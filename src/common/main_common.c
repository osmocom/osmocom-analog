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
#include "main.h"
#include "debug.h"
#include "sender.h"
#include "timer.h"
#include "call.h"
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
int interval = 1;
int latency = 50;
int uses_emphasis = 1;
int do_pre_emphasis = 0;
int do_de_emphasis = 0;
double rx_gain = 1.0;
int use_mncc_sock = 0;
int send_patterns = 1;
int release_on_disconnect = 1;
int loopback = 0;
int rt_prio = 0;
const char *write_rx_wave = NULL;
const char *write_tx_wave = NULL;
const char *read_rx_wave = NULL;
static const char *sdr_args = "";
double sdr_rx_gain = 0, sdr_tx_gain = 0;

void print_help_common(const char *arg0, const char *ext_usage)
{
	printf("Usage: %s -k <kanal/channel> %s[options] [station-id]\n", arg0, ext_usage);
	printf("\noptions:\n");
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
	printf(" -a --audio-device hw:<card>,<device> | sdr\n");
	printf("        Sound card and device number (default = '%s')\n", audiodev[0]);
	printf("        SDR device, if supported\n");
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
	printf("        with the mixer settings.\n");
	printf(" -m --mncc-sock\n");
	printf("        Disable built-in call contol and offer socket (to LCR)\n");
	printf(" -c --call-device hw:<card>,<device>\n");
	printf("        Sound card and device number for headset (default = '%s')\n", call_audiodev);
	printf(" -t --tones 0 | 1\n");
	printf("        Connect call on setup/release to provide classic tones towards fixed\n");
	printf("        network (default = '%d')\n", send_patterns);
	printf(" -l --loopback <type>\n");
	printf("        Loopback test: 1 = internal | 2 = external | 3 = echo\n");
	printf(" -r --realtime <prio>\n");
	printf("        Set prio: 0 to diable, 99 for maximum (default = %d)\n", rt_prio);
	printf("    --write-rx-wave <file>\n");
	printf("        Write received audio to given wav audio file.\n");
	printf("    --write-tx-wave <file>\n");
	printf("        Write transmitted audio to given wav audio file.\n");
	printf("    --read-rx-wave <file>\n");
	printf("        Replace received audio by given wav audio file.\n");
#ifdef HAVE_SDR
	printf("    --sdr-args <args>\n");
	printf("        Optional SDR device arguments\n");
	printf("    --sdr-rx-gain <gain>\n");
	printf("        SDR device's RX gain in dB (default = %.1f)\n", sdr_rx_gain);
	printf("    --sdr-tx-gain <gain>\n");
	printf("        SDR device's TX gain in dB (default = %.1f)\n", sdr_tx_gain);
#endif
}

void print_hotkeys_common(void)
{
	printf("\n");
	printf("Press digits '0'..'9' and then 'd' key to dial towards mobile station\n");
	printf("Press 'h' key to hangup.\n");
	printf("Press 'w' key to toggle display of wave form of RX signal.\n");
}

#define	OPT_CHANNEL		1000
#define	OPT_WRITE_RX_WAVE	1001
#define	OPT_WRITE_TX_WAVE	1002
#define	OPT_READ_RX_WAVE	1003
#define	OPT_SDR_ARGS		1004
#define	OPT_SDR_RX_GAIN		1005
#define	OPT_SDR_TX_GAIN		1006

static struct option long_options_common[] = {
	{"help", 0, 0, 'h'},
	{"debug", 1, 0, 'v'},
	{"kanal", 1, 0, 'k'},
	{"channel", 1, 0, OPT_CHANNEL},
	{"call-device", 1, 0, 'a'},
	{"samplerate", 1, 0, 's'},
	{"interval", 1, 0, 'i'},
	{"buffer", 1, 0, 'b'},
	{"pre-emphasis", 0, 0, 'p'},
	{"de-emphasis", 0, 0, 'd'},
	{"rx-gain", 0, 0, 'g'},
	{"mncc-sock", 0, 0, 'm'},
	{"call-device", 1, 0, 'c'},
	{"tones", 0, 0, 't'},
	{"loopback", 1, 0, 'l'},
	{"realtime", 1, 0, 'r'},
	{"write-rx-wave", 1, 0, OPT_WRITE_RX_WAVE},
	{"write-tx-wave", 1, 0, OPT_WRITE_TX_WAVE},
	{"read-rx-wave", 1, 0, OPT_READ_RX_WAVE},
	{"sdr-args", 1, 0, OPT_SDR_ARGS},
	{"sdr-rx-gain", 1, 0, OPT_SDR_RX_GAIN},
	{"sdr-tx-gain", 1, 0, OPT_SDR_TX_GAIN},
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
			fprintf(stderr, "A-Netz does not use emphasis, please do not enable pre- or de-emphasis! Disable emphasis on transceiver, if possible.\n");
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
			fprintf(stderr, "Given gain is below 0. Tto reduce RX signal, use sound card's mixer (or resistor net)!\n");
			exit(0);
		}
		rx_gain = pow(10, gain_db / 20.0);
		*skip_args += 2;
		break;
	case 'm':
		use_mncc_sock = 1;
		*skip_args += 1;
		break;
	case 'c':
		call_audiodev = strdup(optarg);
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
	case OPT_SDR_ARGS:
		sdr_args = strdup(optarg);
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
void main_common(int *quit, int latency, int interval, void (*myhandler)(void))
{
	int latspl;
	sender_t *sender;
	double last_time = 0, now;
	struct termios term, term_orig;
	int c;

#ifdef HAVE_SDR
	if (sdr_init(sdr_args, sdr_rx_gain, sdr_tx_gain))
		return;
#endif

	/* open audio */
	if (sender_open_audio())
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

	while(!(*quit)) {
		/* process sound of all transceivers */
		for (sender = sender_head; sender; sender = sender->next) {
			/* do not process audio for an audio slave, since it is done by audio master */
			if (sender->master) /* if master is set, we are an audio slave */
				continue;
			latspl = sender->samplerate * latency / 1000;
			process_sender_audio(sender, quit, latspl);
		}

		/* process timers */
		process_timer();

		/* process audio for mncc call instances */
		now = get_time();
		if (now - last_time >= 0.1)
			last_time = now;
		if (now - last_time >= 0.020) {
			last_time += 0.020;
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
			display_wave_on(-1);
			goto next_char;
		case 'i':
			/* dump info */
			dump_info();
			goto next_char;
		}

		/* process audio of built-in call control */
		process_call(c);

		if (myhandler)
			myhandler();

		/* sleep a while */
		usleep(interval * 1000);
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
}

