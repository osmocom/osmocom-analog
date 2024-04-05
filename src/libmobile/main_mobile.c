/* Common part for main.c of each base station type
 *
 * (C) 2016 by Andreas Eversberg <jolly@eversberg.eu>
 * All Rights Reserved
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sched.h>
#include <unistd.h>
#include <math.h>
#include <termios.h>
#include <errno.h>
#include "../libsample/sample.h"
#include "../liblogging/logging.h"
#include "sender.h"
#include <osmocom/core/timer.h>
#include <osmocom/core/select.h>
#include <osmocom/cc/endpoint.h>
#include "call.h"
#include "console.h"
#include "get_time.h"
#ifdef HAVE_SDR
#include "../libsdr/sdr.h"
#include "../libsdr/sdr_config.h"
#endif
#include "../liboptions/options.h"
#include "../libfm/fm.h"
#include "../libaaimage/aaimage.h"

#define DEFAULT_LO_OFFSET -1000000.0

static int got_init = 0;

/* common mobile settings */
int num_kanal = 0;
const char *kanal[MAX_SENDER];
int num_device = 0;
const char *dsp_device[MAX_SENDER] = { "hw:0,0" };
int allow_sdr = 1;
int use_sdr = 0;
int dsp_samplerate = 48000;
double dsp_interval = 1.0;
int dsp_buffer = 50;
static const char *call_device = "";
static int call_samplerate = 48000;
static int call_buffer = 50;
int uses_emphasis = 1;
int do_pre_emphasis = 0;
int do_de_emphasis = 0;
double rx_gain = 1.0;
double tx_gain = 1.0;
static int echo_test = 0;
static int use_osmocc_cross = 0;
static int use_osmocc_sock = 0;
#define MAX_CC_ARGS 1024
static int cc_argc = 0;
static const char *cc_argv[MAX_CC_ARGS];
int no_l16 = 0;
int send_patterns = 1;
static int release_on_disconnect = 1;
int loopback = 0;
int rt_prio = 0;
int fast_math = 0;
const char *write_tx_wave = NULL;
const char *write_rx_wave = NULL;
const char *read_tx_wave = NULL;
const char *read_rx_wave = NULL;

static const char *number_digits;
static const struct number_lengths *number_lengths;
static const char **number_prefixes;

const char *mobile_number_remove_prefix(const char *number)
{
	size_t len;
	int i, j;

	if (!number_prefixes)
		return number;

	len = strlen(number);
	for (i = 0; number_prefixes[i]; i++) {
		/* skip different lengths */
		if (len != strlen(number_prefixes[i]))
			continue;
		/* match prefix, stop at 'x' */
		for (j = 0; number_prefixes[i][j]; j++) {
			if (number_prefixes[i][j] == 'x')
				break;
			if (number_prefixes[i][j] != number[j])
				break;
		}
		/* if prefix matches, return suffix */ 
		if (number_prefixes[i][j] == 'x')
			return number + j;
	}

	/* return number, if there is no prefix matching */
	return number;
}

const char *mobile_number_check_length(const char *number)
{
	size_t len;
	int i;
	static char invalid[256];

	if (!number_lengths)
		return NULL;

	len = strlen(number);
	for (i = 0; number_lengths[i].usage; i++) {
		if ((int)len == number_lengths[i].digits)
			break;
	}
	if (!number_lengths[i].usage) {
		sprintf(invalid, "Number does not have");
		for (i = 0; number_lengths[i].usage; i++) {
			sprintf(strchr(invalid, '\0'), " %d", number_lengths[i].digits);
			if (number_lengths[i + 1].usage) {
				if (number_lengths[i + 2].usage)
					strcat(invalid, ",");
				else
					strcat(invalid, " or");
			}
		}
		sprintf(strchr(invalid, '\0'), " digits.");
		return invalid;
	}

	return NULL;
}

const char *mobile_number_check_digits(const char *number)
{
	int i;
	static char invalid[256];

	for (i = 0; number[i]; i++) {
		if (!strchr(number_digits, number[i])) {
			sprintf(invalid, "Digit #%d of number has digit '%c' which is not in the set of allowed digits. ('%s')\n", i + 1, number[i], number_digits);
			return invalid;
		}
	}

	return NULL;
}

const char *(*mobile_number_check_valid)(const char *);

void main_mobile_init(const char *digits, const struct number_lengths lengths[], const char *prefixes[], const char *(*check_valid)(const char *))
{
	logging_init();

	cc_argv[cc_argc++] = options_strdup("remote auto");

	number_digits = digits;
	number_lengths = lengths;
	number_prefixes = prefixes;
	mobile_number_check_valid = check_valid;

	got_init = 1;
#ifdef HAVE_SDR
	sdr_config_init(DEFAULT_LO_OFFSET);
#endif
}

void main_mobile_exit(void)
{
	if (got_init) {
		enable_limit_scroll(false);
		printf("\n\n");
	}
}

void main_mobile_set_number_check_valid(const char *(*check_valid)(const char *))
{
	mobile_number_check_valid = check_valid;
}

/* ask if number is connect */
int main_mobile_number_ask(const char *number, const char *what)
{
	const char *invalid;

	if (!got_init) {
		fprintf(stderr, "main_mobile_init was not called, please fix!\n");
		abort();
	}

	number = mobile_number_remove_prefix(number);

	invalid = mobile_number_check_length(number);
	if (invalid) {
		printf("Given %s '%s' has invalid length: %s\n", what, number, invalid);
		return -EINVAL;
	}

	invalid = mobile_number_check_digits(number);
	if (invalid) {
		printf("Given %s '%s' has invalid digit: %s\n", what, number, invalid);
		return -EINVAL;
	}

	if (mobile_number_check_valid) {
		invalid = mobile_number_check_valid(number);
		if (invalid) {
			printf("Given %s '%s' is invalid for this network: %s\n", what, number, invalid);
			return -EINVAL;
		}
	}

	return 0;
}

void main_mobile_print_help(const char *arg0, const char *ext_usage)
{
	printf("Usage: %s -k <kanal/channel> %s[options] [station_id]\n", arg0, ext_usage);
	printf("\nGlobal options:\n");
	/*      -                                                                             - */
	printf(" -h --help\n");
	printf("        This help\n");
	printf(" --config [~/]<path to config file>\n");
	printf("        Give a config file to use. If it starts with '~/', path is at home dir.\n");
	printf("        Each line in config file is one option, '-' or '--' must not be given!\n");
	printf("        Default: '%s'\n", selected_config_file);
	printf(" --no-config\n");
	printf("        Even if a config file exists, don't use it.\n");
	logging_print_help();
	printf(" -k --kanal <channel>\n");
	printf(" -k --channel <channel>\n");
	printf("        Channel (German = Kanal) number of \"Sender\" (German = Transceiver)\n");
	printf(" -a --audio-device hw:<card>,<device>[/hw:<card>.<rec-device>]\n");
	printf("        Sound card and device number (default = '%s')\n", dsp_device[0]);
	printf("        You may specify a different recording device by using '/'.\n");
	printf("        Don't set it for SDR!\n");
	printf(" -s --samplerate <rate>\n");
	printf("        Sample rate of sound device (default = '%d')\n", dsp_samplerate);
	printf(" -i --interval 0.1..25\n");
	printf("        Interval of processing loop in ms (default = '%.1f' ms)\n", dsp_interval);
	printf("        Use 10 to drastically reduce CPU usage. In case of buffer underrun,\n");
	printf("        increase buffer accordingly.\n");
	printf(" -b --buffer <ms>\n");
	printf("        How many milliseconds are processed in advance (default = '%d')\n", dsp_buffer);
	printf("        A buffer below 10 ms requires low interval like 0.1 ms.\n");
    if (uses_emphasis) {
	printf(" -p --pre-emphasis\n");
	printf("        Enable pre-emphasis, if you directly connect to the oscillator of the\n");
	printf("        transmitter. (No pre-emphasis done by the transmitter.)\n");
	printf(" -d --de-emphasis\n");
	printf("        Enable de-emphasis, if you directly connect to the discriminator of\n");
	printf("        the receiver. (No de-emphasis done by the receiver.)\n");
    }
	printf("    --rx-gain <dB>\n");
	printf("        Raise/lower receiver's RX level by given gain in dB.\n");
	printf("        (Works with sound card only.)\n");
	printf("    --tx-gain <dB>\n");
	printf("        Raise/lower transmitters's RX level by given gain in dB.\n");
	printf("        (Works with sound card only.)\n");
	printf(" -e --echo-test\n");
	printf("        Use echo test, to send back audio from mobile phone's microphone to\n");
	printf("        the speaker. (German: 'Blasprobe').\n");
	printf(" -c --call-device hw:<card>,<device>[/hw:<card>.<rec-device>]\n");
	printf("        Sound card and device number for headset (default = '%s')\n", call_device);
	printf("        You may specify a different recording device by using '/'.\n");
	printf("    --call-samplerate <rate>\n");
	printf("        Sample rate of sound device for headset (default = '%d')\n", call_samplerate);
	printf("    --call-buffer <ms>\n");
	printf("        How many milliseconds are processed in advance (default = '%d')\n", call_buffer);
	printf(" -x --osmocc-cross\n");
	printf("        Enable built-in call forwarding between mobiles. Be sure to have\n");
	printf("        at least one control channel and two voice channels. Alternatively\n");
	printf("        use one combined control+voice channel and one voice channels.\n");
	printf(" -o --osmocc-sock\n");
	printf("        Disable built-in call control and offer socket\n");
	printf("    --cc \"<osmo-cc arg>\" [--cc ...]\n");
	printf("        Pass arguments to Osmo-CC endpoint. Use '-cc help' for description.\n");
	printf("    --no-l16\n");
	printf("        Disable L16 (linear 16 bit) codec.\n");
	printf(" -t --tones 0 | 1\n");
	printf("        Connect call on setup/release to provide classic tones towards fixed\n");
	printf("        network (default = '%d')\n", send_patterns);
	printf(" -l --loopback <type>\n");
	printf("        Loopback test: 1 = internal | 2 = external | 3 = echo\n");
	printf(" -r --realtime <prio>\n");
	printf("        Set prio: 0 to disable, 99 for maximum (default = %d)\n", rt_prio);
	printf("    --fast-math\n");
	printf("        Use fast math approximation for slow CPU / ARM based systems.\n");
	printf("    --write-rx-wave <file>\n");
	printf("        Write received audio to given wave file.\n");
	printf("    --write-tx-wave <file>\n");
	printf("        Write transmitted audio to given wave file.\n");
	printf("    --read-rx-wave <file>\n");
	printf("        Replace received audio by given wave file.\n");
	printf("    --read-tx-wave <file>\n");
	printf("        Replace transmitted audio by given wave file.\n");
#ifdef HAVE_SDR
    if (allow_sdr) {
	printf("    --limesdr\n");
	printf("        Auto-select several required options for LimeSDR\n");
	printf("    --limesdr-mini\n");
	printf("        Auto-select several required options for LimeSDR Mini\n");
	sdr_config_print_help();
    }
#endif
	printf("\nNetwork specific options:\n");
}

void main_mobile_print_station_id(void)
{
	int i;

	if (!number_lengths)
		return;

	printf("\nstation_id: Give");
	for (i = 0; number_lengths[i].usage; i++) {
		printf(" %d", number_lengths[i].digits);
		if (number_lengths[i + 1].usage) {
			if (number_lengths[i + 2].usage)
				printf(",");
			else
				printf(" or");
		}
	}
	printf(" digits of station ID,\n");
	printf("        so you don't need to enter it for every start of this application.\n");
	for (i = 0; number_lengths[i].usage; i++)
		printf("        Give %d digits for %s.\n", number_lengths[i].digits, number_lengths[i].usage);
	if (number_prefixes) {
		for (i = 0; number_prefixes[i]; i++)
			printf("        You may use '%s' as prefix.\n", number_prefixes[i]);
	}
}

void main_mobile_print_hotkeys(void)
{
	printf("\n");
	printf("Press digits '0'..'9' and then 'd' key to dial towards mobile station.\n");
	printf("Press 'h' key to hangup.\n");
	printf("Press 'w' key to toggle display of RX wave form.\n");
	printf("Press 'c' key to toggle display of channel status.\n");
	printf("Press 'm' key to toggle display of measurement value.\n");
#ifdef HAVE_SDR
    if (allow_sdr) {
	sdr_config_print_hotkeys();
    }
#endif
}

#define	OPT_CHANNEL		1000
#define	OPT_RX_GAIN		1001
#define	OPT_TX_GAIN		1002
#define	OPT_OSMO_CC		1003
#define	OPT_WRITE_RX_WAVE	1004
#define	OPT_WRITE_TX_WAVE	1005
#define	OPT_READ_RX_WAVE	1006
#define	OPT_READ_TX_WAVE	1007
#define	OPT_CALL_SAMPLERATE	1008
#define	OPT_CALL_BUFFER		1009
#define	OPT_FAST_MATH		1010
#define	OPT_NO_L16		1011
#define	OPT_LIMESDR		1100
#define	OPT_LIMESDR_MINI	1101

void main_mobile_add_options(void)
{
	option_add('h', "help", 0);
	option_add('v', "verbose", 1);
	option_add('k', "kanal", 1);
	option_add(OPT_CHANNEL, "channel", 1);
	option_add('a', "audio-device", 1);
	option_add('s', "samplerate", 1);
	option_add('i', "interval", 1);
	option_add('b', "buffer", 1);
	option_add('p', "pre-emphasis", 0);
	option_add('d', "de-emphasis", 0);
	option_add(OPT_RX_GAIN, "rx-gain", 1);
	option_add(OPT_TX_GAIN, "tx-gain", 1);
	option_add('e', "echo-test", 0);
	option_add('x', "osmocc-cross", 0);
	option_add('o', "osmocc-sock", 0);
	option_add(OPT_OSMO_CC, "cc", 1);
	option_add(OPT_NO_L16, "no-l16", 0);
	option_add('c', "call-device", 1);
	option_add(OPT_CALL_SAMPLERATE, "call-samplerate", 1);
	option_add(OPT_CALL_BUFFER, "call-buffer", 1);
	option_add('t', "tones", 1);
	option_add('l', "loopback", 1);
	option_add('r', "realtime", 1);
	option_add(OPT_FAST_MATH, "fast-math", 0);
	option_add(OPT_WRITE_RX_WAVE, "write-rx-wave", 1);
	option_add(OPT_WRITE_TX_WAVE, "write-tx-wave", 1);
	option_add(OPT_READ_RX_WAVE, "read-rx-wave", 1);
	option_add(OPT_READ_TX_WAVE, "read-tx-wave", 1);
#ifdef HAVE_SDR
	option_add(OPT_LIMESDR, "limesdr", 0);
	option_add(OPT_LIMESDR_MINI, "limesdr-mini", 0);
	sdr_config_add_options();
#endif
};

int main_mobile_handle_options(int short_option, int argi, char **argv)
{
	double gain_db;
	int rc;

	switch (short_option) {
	case 'h':
		print_help(argv[0]);
		return 0;
	case 'v':
		rc = parse_logging_opt(argv[argi]);
		if (rc > 0)
			return 0;
		if (rc < 0) {
			fprintf(stderr, "Failed to parse logging option, please use -h for help.\n");
			return rc;
		}
		break;
	case 'k':
	case OPT_CHANNEL:
		OPT_ARRAY(num_kanal, kanal, argv[argi])
		break;
	case 'a':
		OPT_ARRAY(num_device, dsp_device, options_strdup(argv[argi]))
		break;
	case 's':
		dsp_samplerate = atoi(argv[argi]);
		break;
	case 'i':
		dsp_interval = atof(argv[argi]);
		if (dsp_interval < 0.1)
			dsp_interval = 0.1;
		if (dsp_interval > 10)
			dsp_interval = 10;
		break;
	case 'b':
		dsp_buffer = atoi(argv[argi]);
		break;
	case 'p':
		if (!uses_emphasis) {
			no_emph:
			fprintf(stderr, "This network does not use emphasis, please do not enable pre- or de-emphasis! Disable emphasis on transceiver, if possible.\n");
			return -EINVAL;
		}
		do_pre_emphasis = 1;
		break;
	case 'd':
		if (!uses_emphasis)
			goto no_emph;
		do_de_emphasis = 1;
		break;
	case OPT_RX_GAIN:
		gain_db = atof(argv[argi]);
		rx_gain = pow(10, gain_db / 20.0);
		break;
	case OPT_TX_GAIN:
		gain_db = atof(argv[argi]);
		tx_gain = pow(10, gain_db / 20.0);
		break;
	case 'e':
		echo_test = 1;
		break;
	case 'x':
		use_osmocc_cross = 1;
		break;
	case 'o':
		use_osmocc_sock = 1;
		break;
	case OPT_OSMO_CC:
		if (!strcasecmp(argv[argi], "help")) {
			osmo_cc_help();
			return 0;
		}
		if (cc_argc == MAX_CC_ARGS) {
			fprintf(stderr, "Too many osmo-cc args!\n");
			break;
		}
		cc_argv[cc_argc++] = options_strdup(argv[argi]);
		break;
	case OPT_NO_L16:
		no_l16 = 1;
		break;
	case 'c':
		call_device = options_strdup(argv[argi]);
		break;
	case OPT_CALL_SAMPLERATE:
		call_samplerate = atoi(argv[argi]);
		break;
	case OPT_CALL_BUFFER:
		call_buffer = atoi(argv[argi]);
		break;
	case 't':
		send_patterns = atoi(argv[argi]);
		break;
	case 'l':
		loopback = atoi(argv[argi]);
		break;
	case 'r':
		rt_prio = atoi(argv[argi]);
		break;
	case OPT_FAST_MATH:
		fast_math = 1;
		break;
	case OPT_WRITE_RX_WAVE:
		write_rx_wave = options_strdup(argv[argi]);
		break;
	case OPT_WRITE_TX_WAVE:
		write_tx_wave = options_strdup(argv[argi]);
		break;
	case OPT_READ_RX_WAVE:
		read_rx_wave = options_strdup(argv[argi]);
		break;
	case OPT_READ_TX_WAVE:
		read_tx_wave = options_strdup(argv[argi]);
		break;
#ifdef HAVE_SDR
	case OPT_LIMESDR:
		if (allow_sdr) {
			char *argv_lime[] = { argv[0],
				"--sdr-soapy",
				"--sdr-device-args", "driver=lime",
				"--sdr-rx-antenna", "LNAL",
				"--sdr-rx-gain", "30",
				"--sdr-tx-gain", "30",
				"--sdr-samplerate", "5000000",
				"--sdr-bandwidth", "15000000",
				"-s", "200000",
			};
			int argc_lime = sizeof(argv_lime) / sizeof (*argv_lime);
			return options_command_line(argc_lime, argv_lime, main_mobile_handle_options);
		}
		break;
	case OPT_LIMESDR_MINI:
		if (allow_sdr) {
			char *argv_lime[] = { argv[0],
				"--sdr-soapy",
				"--sdr-device-args", "driver=lime",
				"--sdr-rx-antenna", "LNAW",
				"--sdr-tx-antenna", "BAND2",
				"--sdr-rx-gain", "25",
				"--sdr-tx-gain", "30",
				"--sdr-samplerate", "5000000",
				"--sdr-bandwidth", "15000000",
				"-s", "200000",
			};
			int argc_lime = sizeof(argv_lime) / sizeof (*argv_lime);
			return options_command_line(argc_lime, argv_lime, main_mobile_handle_options);
		}
		break;
#endif
	default:
#ifdef HAVE_SDR
		if (allow_sdr)
			return sdr_config_handle_options(short_option, argi, argv);
#endif
		return -EINVAL;
	}

	return 1;
}

/* global variable to quit main loop */
int quit = 0;

void sighandler(int sigset)
{
	if (sigset == SIGHUP)
		return;
	if (sigset == SIGPIPE)
		return;

	printf("Signal received: %d\n", sigset);

	quit = 1;
}

static int get_char()
{
	struct timeval tv = {0, 0};
	fd_set fds;
	char c = 0;
	int __attribute__((__unused__)) rc;

	FD_ZERO(&fds);
	FD_SET(0, &fds);
	select(0+1, &fds, NULL, NULL, &tv);
	if (FD_ISSET(0, &fds)) {
		rc = read(0, &c, 1);
		return c;
	} else
		return -1;
}

/* Loop through all transceiver instances of one network. */
void main_mobile_loop(const char *name, int *quit, void (*myhandler)(void), const char *station_id)
{
	int buffer_size;
	sender_t *sender;
	double last_time_call = 0, begin_time, now, sleep;
	struct termios term, term_orig;
	int num_chan, i;
	int c;
	int rc;

	if (!got_init) {
		fprintf(stderr, "main_mobile_init was not called, please fix!\n");
		abort();
	}

	/* station id preset */
	if (station_id)
		station_id = mobile_number_remove_prefix(station_id);

	/* size of dsp buffer in samples */
	buffer_size = dsp_samplerate * dsp_buffer / 1000;

	/* check OSMO-CC support */
	if (use_osmocc_cross && num_kanal == 1) {
		fprintf(stderr, "You selected built-in call forwarding, but only channel is used. Does this makes sense?\n");
		return;
	}
	if (use_osmocc_sock && use_osmocc_cross) {
		fprintf(stderr, "You selected OSMO-CC socket interface and built-in call forwarding, but only one can be selected.\n");
		return;
	}
	if (echo_test && call_device[0]) {
		fprintf(stderr, "You selected call device (headset) and echo test, but only one can be selected.\n");
		return;
	}
	if (use_osmocc_sock && call_device[0]) {
		fprintf(stderr, "You selected OSMO-CC socket interface, but it cannot be used with call device (headset).\n");
		return;
	}
	if (use_osmocc_cross && call_device[0]) {
		fprintf(stderr, "You selected built-in call forwarding, but it cannot be used with call device (headset).\n");
		return;
	}
	if (use_osmocc_sock && echo_test) {
		fprintf(stderr, "You selected OSMO-CC socket interface, but it cannot be used with echo test.\n");
		return;
	}
	if (use_osmocc_cross && echo_test) {
		fprintf(stderr, "You selected built-in call forwarding, but it cannot be used with echo test.\n");
		return;
	}

	/* OSMO-CC crossover */
	if (use_osmocc_cross) {
		use_osmocc_sock = 1;
		cc_argc = 0;
		cc_argv[cc_argc++] = options_strdup("local 127.0.0.1:4200");
		cc_argv[cc_argc++] = options_strdup("remote 127.0.0.1:4200");
	}

	/* init OSMO-CC */
	if (!use_osmocc_sock)
		console_init(call_device, call_samplerate, call_buffer, loopback, echo_test, number_digits, number_lengths, station_id);

	/* init call control instance */
	rc = call_init(name, (use_osmocc_sock) ? send_patterns : 0, release_on_disconnect, use_osmocc_sock, cc_argc, cc_argv, no_l16);
	if (rc < 0) {
		fprintf(stderr, "Failed to create call control instance. Quitting!\n");
		return;
	}

#ifdef HAVE_SDR
	rc = sdr_configure(dsp_samplerate);
	if (rc < 0)
		return;
#endif

	/* open audio */
	if (sender_open_audio(buffer_size, dsp_interval))
		return;
	if (console_open_audio(buffer_size, dsp_interval))
		return;

	/* alloc memory for audio processing */
	for (num_chan = 0, sender = sender_head; sender; num_chan++, sender = sender->next);
	sample_t *samples[num_chan];
	uint8_t *powers[num_chan];
	for (i = 0; i < num_chan; i++) {
		samples[i] = calloc(buffer_size, sizeof(**samples));
		powers[i] = calloc(buffer_size, sizeof(**powers));
	}

	/* real time priority */
	if (rt_prio > 0) {
		struct sched_param schedp;
		int rc;

		memset(&schedp, 0, sizeof(schedp));
		schedp.sched_priority = rt_prio;
		rc = sched_setscheduler(0, SCHED_RR, &schedp);
		if (rc) {
			fprintf(stderr, "Error setting SCHED_RR with prio %d\n", rt_prio);
			return;
		}
	}

	if (!loopback)
		print_aaimage();

	/* prepare terminal */
	tcgetattr(0, &term_orig);
	term = term_orig;
	term.c_lflag &= ~(ISIG|ICANON|ECHO);
	term.c_cc[VMIN]=1;
	term.c_cc[VTIME]=2;
	tcsetattr(0, TCSANOW, &term);

	/* catch signals */
	signal(SIGINT, sighandler);
	signal(SIGHUP, sighandler);
	signal(SIGTERM, sighandler);
	signal(SIGPIPE, sighandler);

	/* start streaming */
	if (sender_start_audio())
		*quit = 1;
	if (console_start_audio())
		*quit = 1;

	while(!(*quit)) {
		int work;
		begin_time = get_time();

		/* process sound of all transceivers */
		for (sender = sender_head; sender; sender = sender->next) {
			/* do not process audio for an audio slave, since it is done by audio master */
			if (sender->master) /* if master is set, we are an audio slave */
				continue;
			process_sender_audio(sender, quit, samples, powers, buffer_size);
		}

		/* process audio for call instances */
		now = get_time();
		if (now - last_time_call >= 0.1)
			last_time_call = now;
		if (now - last_time_call >= 0.020) {
			last_time_call += 0.020;
			/* call clock every 20ms */
			call_clock();
		}

next_char:
		c = get_char();
		switch (c) {
		case 3:
			/* quit */
			printf("CTRL+c received, quitting!\n");
			*quit = 1;
			goto next_char;
		case 'w':
			/* toggle wave display */
			display_status_on(0);
			display_measurements_on(0);
#ifdef HAVE_SDR
			display_iq_on(0);
			display_spectrum_on(0);
#endif
			display_wave_on(-1);
			goto next_char;
		case 'c':
			/* toggle call state display */
			display_wave_on(0);
			display_measurements_on(0);
#ifdef HAVE_SDR
			display_iq_on(0);
			display_spectrum_on(0);
#endif
			display_status_on(-1);
			goto next_char;
		case 'm':
			/* toggle measurements display */
			display_wave_on(0);
			display_status_on(0);
#ifdef HAVE_SDR
			display_iq_on(0);
			display_spectrum_on(0);
#endif
			display_measurements_on(-1);
			goto next_char;
#ifdef HAVE_SDR
		case 'q':
			/* toggle IQ display */
			if (!use_sdr)
				goto next_char;
			display_wave_on(0);
			display_status_on(0);
			display_measurements_on(0);
			display_spectrum_on(0);
			display_iq_on(-1);
			goto next_char;
		case 's':
			/* toggle spectrum display */
			if (!use_sdr)
				goto next_char;
			display_wave_on(0);
			display_status_on(0);
			display_measurements_on(0);
			display_iq_on(0);
			display_spectrum_on(-1);
			goto next_char;
#endif
		case 'i':
			/* dump info */
			dump_info();
			goto next_char;
#ifdef HAVE_SDR
		case 'b':
			calibrate_bias();
			goto next_char;
#endif
		}

		/* handle all handlers until no more events */
		do {
			work = 0;
			work |= osmo_cc_handle();
			work |= osmo_select_main(1);
		} while (work);

		if (!use_osmocc_sock)
			process_console(c);

		if (myhandler)
			myhandler();

		display_measurements(dsp_interval / 1000.0);

		now = get_time();

		/* sleep interval */
		sleep = (dsp_interval / 1000.0) - (now - begin_time);
		if (sleep > 0)
			usleep(sleep * 1000000.0);

//		now = get_time();
//		printf("duration =%.6f\n", now - begin_time);
	}

	/* reset signals */
	signal(SIGINT, SIG_DFL);
	signal(SIGHUP, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);

	for (i = 0; i < num_chan; i++) {
		free(samples[i]);
		free(powers[i]);
	}

	/* reset terminal */
	tcsetattr(0, TCSANOW, &term_orig);
	
	/* reset real time prio */
	if (rt_prio > 0) {
		struct sched_param schedp;

		memset(&schedp, 0, sizeof(schedp));
		schedp.sched_priority = 0;
		sched_setscheduler(0, SCHED_OTHER, &schedp);
	}

	//* cleanup call control */
	call_exit();

	/* cleanup console */
	if (!use_osmocc_sock)
		console_cleanup();
}

