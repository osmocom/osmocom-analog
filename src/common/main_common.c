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
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sched.h>
#include <unistd.h>
#include <math.h>
#include <termios.h>
#include <errno.h>
#include "sample.h"
#include "main.h"
#include "debug.h"
#include "sender.h"
#include "timer.h"
#include "call.h"
#include "mncc_sock.h"
#ifdef HAVE_SDR
#include "sdr.h"
#endif

/* common settings */
int num_kanal = 0;
int kanal[MAX_SENDER];
int num_audiodev = 0;
const char *audiodev[MAX_SENDER] = { "hw:0,0" };
const char *call_audiodev = "";
int samplerate = 48000;
int call_samplerate = 48000;
int interval = 1;
int latency = 50;
int uses_emphasis = 1;
int do_pre_emphasis = 0;
int do_de_emphasis = 0;
double rx_gain = 1.0;
int use_mncc_sock = 0;
const char *mncc_name = "";
int send_patterns = 1;
int release_on_disconnect = 1;
int loopback = 0;
int rt_prio = 0;
const char *write_tx_wave = NULL;
const char *write_rx_wave = NULL;
const char *read_tx_wave = NULL;
const char *read_rx_wave = NULL;
int use_sdr = 0;
int sdr_channel = 0;
static const char *sdr_device_args = "",  *sdr_stream_args = "",  *sdr_tune_args = "";
static int sdr_samplerate = 0;
static double sdr_bandwidth = 0.0;
#ifdef HAVE_SDR
static int sdr_uhd = 0;
static int sdr_soapy = 0;
#endif
double sdr_tx_gain = 0, sdr_rx_gain = 0;
const char *sdr_tx_antenna = "", *sdr_rx_antenna = "";
const char *write_iq_tx_wave = NULL;
const char *write_iq_rx_wave = NULL;
const char *read_iq_tx_wave = NULL;
const char *read_iq_rx_wave = NULL;
int sdr_swap_links = 0;
int sdr_uhd_tx_timestamps = 0;

void print_help_common(const char *arg0, const char *ext_usage)
{
	printf("Usage: %s -k <kanal/channel> %s[options] [station-id]\n", arg0, ext_usage);
	printf("\nGlobal options:\n");
	/*      -                                                                             - */
	printf(" -h --help\n");
	printf("        This help\n");
	printf(" -v --verbose <level> | <level>,<category>[,<category>[,...]] | list\n");
	printf("        Use 'list' to get a list of all levels and categories\n");
	printf("        Verbose level: digit of debug level (default = '%d')\n", debuglevel);
	printf("        Verbose level+category: level digit followed by one or more categories\n");
	printf("        -> If no category is specified, all categories are selected\n");
	printf(" -k --kanal <channel>\n");
	printf(" -k --channel <channel>\n");
	printf("        Channel (German = Kanal) number of \"Sender\" (German = Transceiver)\n");
	printf(" -a --audio-device hw:<card>,<device>\n");
	printf("        Sound card and device number (default = '%s')\n", audiodev[0]);
	printf("        Don't set it for SDR!\n");
	printf(" -s --samplerate <rate>\n");
	printf("        Sample rate of sound device (default = '%d')\n", samplerate);
	printf(" -i --interval 1..25\n");
	printf("        Interval of processing loop in ms (default = '%d' ms)\n", interval);
	printf("        Use 25 to drastically reduce CPU usage. In case of buffer underrun,\n");
	printf("        increase latency accordingly.\n");
	printf(" -b --buffer <ms>\n");
	printf("        How many milliseconds are processed in advance (default = '%d')\n", latency);
    if (uses_emphasis) {
	printf(" -p --pre-emphasis\n");
	printf("        Enable pre-emphasis, if you directly connect to the oscillator of the\n");
	printf("        transmitter. (No pre-emphasis done by the transmitter.)\n");
	printf(" -d --de-emphasis\n");
	printf("        Enable de-emphasis, if you directly connect to the discriminator of\n");
	printf("        the receiver. (No de-emphasis done by the receiver.)\n");
    }
	printf(" -g --rx-gain <dB>\n");
	printf("        Raise receiver RX level by given gain in dB. This is useful if input\n");
	printf("        level of the sound device is too low, even after setting maximum level\n");
	printf("        with the mixer settings. (Works with sound card only.)\n");
	printf(" -m --mncc-sock\n");
	printf("        Disable built-in call contol and offer socket (to LCR)\n");
	printf(" --mncc-name <name>\n");
	printf("        '/tmp/bsc_mncc' is used by default, give name to change socket to\n");
	printf("        '/tmp/bsc_mncc_<name>'. (Useful to run multiple networks.)\n");
	printf(" -c --call-device hw:<card>,<device>\n");
	printf("        Sound card and device number for headset (default = '%s')\n", call_audiodev);
	printf("    --call-samplerate <rate>\n");
	printf("        Sample rate of sound device for headset (default = '%d')\n", call_samplerate);
	printf(" -t --tones 0 | 1\n");
	printf("        Connect call on setup/release to provide classic tones towards fixed\n");
	printf("        network (default = '%d')\n", send_patterns);
	printf(" -l --loopback <type>\n");
	printf("        Loopback test: 1 = internal | 2 = external | 3 = echo\n");
	printf(" -r --realtime <prio>\n");
	printf("        Set prio: 0 to diable, 99 for maximum (default = %d)\n", rt_prio);
	printf("    --write-rx-wave <file>\n");
	printf("        Write received audio to given wave file.\n");
	printf("    --write-tx-wave <file>\n");
	printf("        Write transmitted audio to given wave file.\n");
	printf("    --read-rx-wave <file>\n");
	printf("        Replace received audio by given wave file.\n");
	printf("    --read-tx-wave <file>\n");
	printf("        Replace transmitted audio by given wave file.\n");
#ifdef HAVE_SDR
	printf("\nSDR options:\n");
	/*      -                                                                             - */
#ifdef HAVE_UHD
	printf("    --sdr-uhd\n");
	printf("        Force UHD driver\n");
#endif
#ifdef HAVE_SOAPY
	printf("    --sdr-soapy\n");
	printf("        Force SoapySDR driver\n");
#endif
	printf("    --sdr-channel <channel #>\n");
	printf("        Give channel number for multi channel SDR device (default = %d)\n", sdr_channel);
	printf("    --sdr-device-args <args>\n");
	printf("    --sdr-stream-args <args>\n");
	printf("    --sdr-tune-args <args>\n");
	printf("        Optional SDR device arguments, seperated by comma\n");
	printf("        e.g. --sdr-device-args <key>=<value>[,<key>=<value>[,...]]\n");
	printf("    --sdr-samplerate <samplerate>\n");
	printf("        Sample rate to use with SDR. By default it equals the regular sample\n");
	printf("        rate.\n");
	printf("    --sdr-bandwidth <bandwidth>\n");
	printf("        Give IF filter bandwidth to use. If not, sample rate is used.\n");
	printf("    --sdr-rx-antenna <name>\n");
	printf("        SDR device's RX antenna name, use 'list' to get a list\n");
	printf("    --sdr-tx-antenna <name>\n");
	printf("        SDR device's TX antenna name, use 'list' to get a list\n");
	printf("    --sdr-rx-gain <gain>\n");
	printf("        SDR device's RX gain in dB (default = %.1f)\n", sdr_rx_gain);
	printf("    --sdr-tx-gain <gain>\n");
	printf("        SDR device's TX gain in dB (default = %.1f)\n", sdr_tx_gain);
	printf("    --write-iq-rx-wave <file>\n");
	printf("        Write received IQ data to given wave file.\n");
	printf("    --write-iq-tx-wave <file>\n");
	printf("        Write transmitted IQ data to given wave file.\n");
	printf("    --read-iq-rx-wave <file>\n");
	printf("        Replace received IQ data by given wave file.\n");
	printf("    --read-iq-tx-wave <file>\n");
	printf("        Replace transmitted IQ data by given wave file.\n");
	printf("    --sdr-swap-links\n");
	printf("        Swap RX and TX frequencies for loopback tests over the air.r\n");
#ifdef HAVE_UHD
	printf("    --sdr-uhd-tx-timestamps\n");
	printf("        Use TX timestamps on UHD device. (May not work with some devices!)\n");
#endif
#endif
	printf("\nNetwork specific options:\n");
}

void print_hotkeys_common(void)
{
	printf("\n");
	printf("Press digits '0'..'9' and then 'd' key to dial towards mobile station\n");
	printf("Press 'h' key to hangup.\n");
	printf("Press 'w' key to toggle display of RX wave form.\n");
	printf("Press 'c' key to toggle display of channel status.\n");
#ifdef HAVE_SDR
	printf("Press 'i' key to toggle display of RX I/Q vector.\n");
	printf("Press 's' key to toggle display of RX spectrum.\n");
#endif
}

#define	OPT_CHANNEL		1000
#define	OPT_WRITE_RX_WAVE	1001
#define	OPT_WRITE_TX_WAVE	1002
#define	OPT_READ_RX_WAVE	1003
#define	OPT_READ_TX_WAVE	1004
#define	OPT_CALL_SAMPLERATE	1005
#define	OPT_MNCC_NAME		1006

#define	OPT_SDR_UHD		1100
#define	OPT_SDR_SOAPY		1101
#define	OPT_SDR_CHANNEL		1102
#define	OPT_SDR_DEVICE_ARGS	1103
#define	OPT_SDR_STREAM_ARGS	1104
#define	OPT_SDR_TUNE_ARGS	1105
#define	OPT_SDR_RX_ANTENNA	1106
#define	OPT_SDR_TX_ANTENNA	1107
#define	OPT_SDR_RX_GAIN		1108
#define	OPT_SDR_TX_GAIN		1109
#define	OPT_SDR_SAMPLERATE	1110
#define	OPT_SDR_BANDWIDTH	1111
#define	OPT_WRITE_IQ_RX_WAVE	1112
#define	OPT_WRITE_IQ_TX_WAVE	1113
#define	OPT_READ_IQ_RX_WAVE	1114
#define	OPT_READ_IQ_TX_WAVE	1115
#define	OPT_SDR_SWAP_LINKS	1116
#define OPT_SDR_UHD_TX_TS	1117

static struct option long_options_common[] = {
	{"help", 0, 0, 'h'},
	{"debug", 1, 0, 'v'},
	{"kanal", 1, 0, 'k'},
	{"channel", 1, 0, OPT_CHANNEL},
	{"audio-device", 1, 0, 'a'},
	{"samplerate", 1, 0, 's'},
	{"interval", 1, 0, 'i'},
	{"buffer", 1, 0, 'b'},
	{"pre-emphasis", 0, 0, 'p'},
	{"de-emphasis", 0, 0, 'd'},
	{"rx-gain", 1, 0, 'g'},
	{"mncc-sock", 0, 0, 'm'},
	{"mncc-name", 1, 0, OPT_MNCC_NAME},
	{"call-device", 1, 0, 'c'},
	{"call-samplerate", 1, 0, OPT_CALL_SAMPLERATE},
	{"tones", 0, 0, 't'},
	{"loopback", 1, 0, 'l'},
	{"realtime", 1, 0, 'r'},
	{"write-rx-wave", 1, 0, OPT_WRITE_RX_WAVE},
	{"write-tx-wave", 1, 0, OPT_WRITE_TX_WAVE},
	{"read-rx-wave", 1, 0, OPT_READ_RX_WAVE},
	{"read-tx-wave", 1, 0, OPT_READ_TX_WAVE},
	{"sdr-uhd", 0, 0, OPT_SDR_UHD},
	{"sdr-soapy", 0, 0, OPT_SDR_SOAPY},
	{"sdr-channel", 1, 0, OPT_SDR_CHANNEL},
	{"sdr-device-args", 1, 0, OPT_SDR_DEVICE_ARGS},
	{"sdr-stream-args", 1, 0, OPT_SDR_STREAM_ARGS},
	{"sdr-tune-args", 1, 0, OPT_SDR_TUNE_ARGS},
	{"sdr-samplerate", 1, 0, OPT_SDR_SAMPLERATE},
	{"sdr-bandwidth", 1, 0, OPT_SDR_BANDWIDTH},
	{"sdr-rx-antenna", 1, 0, OPT_SDR_RX_ANTENNA},
	{"sdr-tx-antenna", 1, 0, OPT_SDR_TX_ANTENNA},
	{"sdr-rx-gain", 1, 0, OPT_SDR_RX_GAIN},
	{"sdr-tx-gain", 1, 0, OPT_SDR_TX_GAIN},
	{"write-iq-rx-wave", 1, 0, OPT_WRITE_IQ_RX_WAVE},
	{"write-iq-tx-wave", 1, 0, OPT_WRITE_IQ_TX_WAVE},
	{"read-iq-rx-wave", 1, 0, OPT_READ_IQ_RX_WAVE},
	{"read-iq-tx-wave", 1, 0, OPT_READ_IQ_TX_WAVE},
	{"sdr-swap-links", 0, 0, OPT_SDR_SWAP_LINKS},
	{"sdr-uhd-tx-timestamps", 0, 0, OPT_SDR_UHD_TX_TS},
	{0, 0, 0, 0}
};

const char *optstring_common = "hv:k:a:s:i:b:pdg:mc:t:l:r:";

struct option *long_options;
char *optstring;

static void check_duplicate_option(int num, struct option *option)
{
	int i;

	for (i = 0; i < num; i++) {
		if (long_options[i].val == option->val) {
			fprintf(stderr, "Duplicate option %d. Please fix!\n", option->val);
			abort();
		}
	}
}

void set_options_common(const char *optstring_special, struct option *long_options_special)
{
	int i;

	long_options = calloc(sizeof(*long_options), 100);
	for (i = 0; long_options_common[i].name; i++) {
		check_duplicate_option(i, &long_options_common[i]);
		memcpy(&long_options[i], &long_options_common[i], sizeof(*long_options));
	}
	for (; long_options_special->name; i++) {
		check_duplicate_option(i, long_options_special);
		memcpy(&long_options[i], long_options_special++, sizeof(*long_options));
	}
	
	optstring = calloc(strlen(optstring_common) + strlen(optstring_special) + 1, 1);
	strcpy(optstring, optstring_common);
	strcat(optstring, optstring_special);
}

void opt_switch_common(int c, char *arg0, int *skip_args)
{
	double gain_db;

	switch (c) {
	case 'h':
		print_help(arg0);
		exit(0);
	case 'v':
		if (!strcasecmp(optarg, "list")) {
	                debug_list_cat();
			exit(0);
		}
		if (parse_debug_opt(optarg)) {
			fprintf(stderr, "Failed to parse debug option, please use -h for help.\n");
			exit(0);
		}
		*skip_args += 2;
		break;
	case 'k':
	case OPT_CHANNEL:
		OPT_ARRAY(num_kanal, kanal, atoi(optarg))
		*skip_args += 2;
		break;
	case 'a':
		OPT_ARRAY(num_audiodev, audiodev, strdup(optarg))
		*skip_args += 2;
		break;
	case 's':
		samplerate = atoi(optarg);
		*skip_args += 2;
		break;
	case 'i':
		interval = atoi(optarg);
		*skip_args += 2;
		if (interval < 1)
			interval = 1;
		if (interval > 25)
			interval = 25;
		break;
	case 'b':
		latency = atoi(optarg);
		*skip_args += 2;
		break;
	case 'p':
		if (!uses_emphasis) {
			no_emph:
			fprintf(stderr, "This network does not use emphasis, please do not enable pre- or de-emphasis! Disable emphasis on transceiver, if possible.\n");
			exit(0);
		}
		do_pre_emphasis = 1;
		*skip_args += 1;
		break;
	case 'd':
		if (!uses_emphasis)
			goto no_emph;
		do_de_emphasis = 1;
		*skip_args += 1;
		break;
	case 'g':
		gain_db = atof(optarg);
		if (gain_db < 0.0) {
			fprintf(stderr, "Given gain is below 0. To reduce RX signal, use sound card's mixer (or resistor net)!\n");
			exit(0);
		}
		rx_gain = pow(10, gain_db / 20.0);
		*skip_args += 2;
		break;
	case 'm':
		use_mncc_sock = 1;
		*skip_args += 1;
		break;
	case OPT_MNCC_NAME:
		mncc_name = strdup(optarg);
		*skip_args += 2;
		break;
	case 'c':
		call_audiodev = strdup(optarg);
		*skip_args += 2;
		break;
	case OPT_CALL_SAMPLERATE:
		call_samplerate = atoi(optarg);
		*skip_args += 2;
		break;
	case 't':
		send_patterns = atoi(optarg);
		*skip_args += 2;
		break;
	case 'l':
		loopback = atoi(optarg);
		*skip_args += 2;
		break;
	case 'r':
		rt_prio = atoi(optarg);
		*skip_args += 2;
		break;
	case OPT_WRITE_RX_WAVE:
		write_rx_wave = strdup(optarg);
		*skip_args += 2;
		break;
	case OPT_WRITE_TX_WAVE:
		write_tx_wave = strdup(optarg);
		*skip_args += 2;
		break;
	case OPT_READ_RX_WAVE:
		read_rx_wave = strdup(optarg);
		*skip_args += 2;
		break;
	case OPT_READ_TX_WAVE:
		read_tx_wave = strdup(optarg);
		*skip_args += 2;
		break;
	case OPT_SDR_UHD:
#ifdef HAVE_SDR
		use_sdr = 1;
		sdr_uhd = 1;
#else
		fprintf(stderr, "UHD SDR support not compiled in!\n");
		exit(0);
#endif
		*skip_args += 1;
		break;
	case OPT_SDR_SOAPY:
#ifdef HAVE_SDR
		use_sdr = 1;
		sdr_soapy = 1;
#else
		fprintf(stderr, "SoapySDR support not compiled in!\n");
		exit(0);
#endif
		*skip_args += 1;
		break;
	case OPT_SDR_CHANNEL:
		sdr_channel = atoi(optarg);
		*skip_args += 2;
		break;
	case OPT_SDR_DEVICE_ARGS:
		sdr_device_args = strdup(optarg);
		*skip_args += 2;
		break;
	case OPT_SDR_STREAM_ARGS:
		sdr_stream_args = strdup(optarg);
		*skip_args += 2;
		break;
	case OPT_SDR_TUNE_ARGS:
		sdr_tune_args = strdup(optarg);
		*skip_args += 2;
		break;
	case OPT_SDR_SAMPLERATE:
		sdr_samplerate = atoi(optarg);
		*skip_args += 2;
		break;
	case OPT_SDR_BANDWIDTH:
		sdr_bandwidth = atof(optarg);
		*skip_args += 2;
		break;
	case OPT_SDR_RX_ANTENNA:
		sdr_rx_antenna = strdup(optarg);
		*skip_args += 2;
		break;
	case OPT_SDR_TX_ANTENNA:
		sdr_tx_antenna = strdup(optarg);
		*skip_args += 2;
		break;
	case OPT_SDR_RX_GAIN:
		sdr_rx_gain = atof(optarg);
		*skip_args += 2;
		break;
	case OPT_SDR_TX_GAIN:
		sdr_tx_gain = atof(optarg);
		*skip_args += 2;
		break;
	case OPT_WRITE_IQ_RX_WAVE:
		write_iq_rx_wave = strdup(optarg);
		*skip_args += 2;
		break;
	case OPT_WRITE_IQ_TX_WAVE:
		write_iq_tx_wave = strdup(optarg);
		*skip_args += 2;
		break;
	case OPT_READ_IQ_RX_WAVE:
		read_iq_rx_wave = strdup(optarg);
		*skip_args += 2;
		break;
	case OPT_READ_IQ_TX_WAVE:
		read_iq_tx_wave = strdup(optarg);
		*skip_args += 2;
		break;
	case OPT_SDR_SWAP_LINKS:
		sdr_swap_links = 1;
		*skip_args += 1;
		break;
	case OPT_SDR_UHD_TX_TS:
		sdr_uhd_tx_timestamps = 1;
		*skip_args += 1;
		break;
	default:
		exit (0);
	}
}

/* global variable to quit main loop */
int quit = 0;

void sighandler(int sigset)
{
	if (sigset == SIGHUP)
		return;
	if (sigset == SIGPIPE)
		return;

	clear_console_text();
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
void main_common(int *quit, int latency, int interval, void (*myhandler)(void), const char *station_id, int station_id_digits)
{
	int latspl;
	sender_t *sender;
	double last_time_call = 0, begin_time, now, sleep;
	struct termios term, term_orig;
	int c;
	int rc;

	/* latency of send buffer in samples */
	latspl = samplerate * latency / 1000;

	/* init mncc */
	if (use_mncc_sock) {
		char mncc_sock_name[64];
		if (mncc_name[0]) {
			snprintf(mncc_sock_name, sizeof(mncc_sock_name), "/tmp/bsc_mncc_%s", mncc_name);
			mncc_sock_name[sizeof(mncc_sock_name) - 1] = '\0';
		} else
			strcpy(mncc_sock_name, "/tmp/bsc_mncc");
		rc = mncc_init(mncc_sock_name);
		if (rc < 0) {
			fprintf(stderr, "Failed to setup MNCC socket. Quitting!\n");
			return;
		}
	}

	/* init call device */
	rc = call_init(station_id, call_audiodev, call_samplerate, latency, station_id_digits, loopback, use_mncc_sock, send_patterns, release_on_disconnect);
	if (rc < 0) {
		fprintf(stderr, "Failed to create call control instance. Quitting!\n");
		return;
	}

#ifdef HAVE_SDR
	if ((sdr_uhd == 1 && sdr_soapy == 1)) {
		fprintf(stderr, "You must choose which one you want: --sdr-uhd or --sdr-soapy\n");
		return;
	}

	if (sdr_samplerate == 0.0)
		sdr_samplerate = samplerate;
	if (sdr_bandwidth == 0.0)
		sdr_bandwidth = sdr_samplerate;
	rc = sdr_init(sdr_uhd, sdr_soapy, sdr_channel, sdr_device_args, sdr_stream_args, sdr_tune_args, sdr_tx_antenna, sdr_rx_antenna, sdr_tx_gain, sdr_rx_gain, sdr_samplerate, sdr_bandwidth, write_iq_tx_wave, write_iq_rx_wave, read_iq_tx_wave, read_iq_rx_wave, latspl, sdr_swap_links, sdr_uhd_tx_timestamps);
	if (rc < 0)
		return;
#endif

	/* open audio */
	if (sender_open_audio())
		return;
	if (call_open_audio())
		return;

	/* real time priority */
	if (rt_prio > 0) {
		struct sched_param schedp;
		int rc;

		memset(&schedp, 0, sizeof(schedp));
		schedp.sched_priority = rt_prio;
		rc = sched_setscheduler(0, SCHED_RR, &schedp);
		if (rc)
			fprintf(stderr, "Error setting SCHED_RR with prio %d\n", rt_prio);
	}

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
	if (call_start_audio())
		*quit = 1;

	while(!(*quit)) {
		begin_time = get_time();

		/* process sound of all transceivers */
		for (sender = sender_head; sender; sender = sender->next) {
			/* do not process audio for an audio slave, since it is done by audio master */
			if (sender->master) /* if master is set, we are an audio slave */
				continue;
			process_sender_audio(sender, quit, latspl);
		}

		/* process timers */
		process_timer();

		/* process audio for mncc call instances */
		now = get_time();
		if (now - last_time_call >= 0.1)
			last_time_call = now;
		if (now - last_time_call >= 0.020) {
			last_time_call += 0.020;
			/* call clock every 20ms */
			call_mncc_clock();
		}

next_char:
		c = get_char();
		switch (c) {
		case 3:
			/* quit */
			clear_console_text();
			printf("CTRL+c received, quitting!\n");
			*quit = 1;
			goto next_char;
		case 'w':
			/* toggle display */
#ifdef HAVE_SDR
			display_iq_on(0);
			display_spectrum_on(0);
#endif
			display_status_on(0);
			display_wave_on(-1);
			goto next_char;
		case 'c':
			/* toggle display */
#ifdef HAVE_SDR
			display_iq_on(0);
			display_spectrum_on(0);
#endif
			display_wave_on(0);
			display_status_on(-1);
			goto next_char;
#ifdef HAVE_SDR
		case 'q':
			/* toggle display */
			display_wave_on(0);
			display_status_on(0);
			display_spectrum_on(0);
			display_iq_on(-1);
			goto next_char;
		case 's':
			/* toggle spectrum */
			display_wave_on(0);
			display_status_on(0);
			display_iq_on(0);
			display_spectrum_on(-1);
			goto next_char;
#endif
		case 'i':
			/* dump info */
			dump_info();
			goto next_char;
		}

		/* process audio of built-in call control */
		process_call(c);

		if (myhandler)
			myhandler();

		now = get_time();

		/* sleep interval */
		sleep = ((double)interval / 1000.0) - (now - begin_time);
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

	/* get rid of last entry */
	clear_console_text();

	/* reset terminal */
	tcsetattr(0, TCSANOW, &term_orig);
	
	/* reset real time prio */
	if (rt_prio > 0) {
		struct sched_param schedp;

		memset(&schedp, 0, sizeof(schedp));
		schedp.sched_priority = 0;
		sched_setscheduler(0, SCHED_OTHER, &schedp);
	}

	/* cleanup call control */
	call_cleanup();

	/* close mncc socket */
	if (use_mncc_sock)
		mncc_exit();
}

