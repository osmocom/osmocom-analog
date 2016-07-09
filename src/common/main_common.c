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
#include <unistd.h>
#include <math.h>
#include <termios.h>
#include "main.h"
#include "debug.h"
#include "sender.h"
#include "timer.h"
#include "call.h"

/* common settings */
int num_kanal = 0;
int kanal[MAX_SENDER];
int num_sounddev = 0;
const char *sounddev[MAX_SENDER] = { "hw:0,0" };
const char *call_sounddev = "";
int samplerate = 48000;
int interval = 1;
int latency = 50;
int cross_channels = 0;
int do_pre_emphasis = 0;
int do_de_emphasis = 0;
double rx_gain = 1.0;
int use_mncc_sock = 0;
int send_patterns = 1;
int loopback = 0;
int rt_prio = 0;
const char *read_wave = NULL;
const char *write_wave = NULL;

void print_help_common(const char *arg0, const char *ext_usage)
{
	printf("Usage: %s -k <kanal/channel> %s[options] [station-id]\n", arg0, ext_usage);
	printf("\noptions:\n");
	/*      -                                                                             - */
	printf(" -h --help\n");
	printf("        This help\n");
	printf(" -D --debug <level> | <level>,<category>[,<category>[,...]] | list\n");
	printf("        Use 'list' to get a list of all levels and categories\n");
	printf("        Debug level: digit of debug level (default = '%d')\n", debuglevel);
	printf("        Debug level+category: level digit followed by one or more categories\n");
	printf("        -> If no category is specified, all categories are selected\n");
	printf(" -k --kanal <channel>\n");
	printf("        Channel number of \"Sender\"\n");
	printf(" -d --device hw:<card>,<device>\n");
	printf("        Sound card and device number (default = '%s')\n", sounddev[0]);
	printf(" -s --samplerate <rate>\n");
	printf("        Sample rate of sound device (default = '%d')\n", samplerate);
	printf(" -i --interval 1..25\n");
	printf("        Interval of processing loop in ms (default = '%d' ms)\n", interval);
	printf("        Use 25 to drastically reduce CPU usage. In case of buffer underrun,\n");
	printf("        increase latency accordingly.\n");
	printf(" -l --latency <delay>\n");
	printf("        How many milliseconds processed in advance (default = '%d')\n", latency);
	printf(" -x --cross\n");
	printf("        Cross channels on sound card. 1st channel (right) is swapped with\n");
	printf("        second channel (left)\n");
	printf(" -E --pre-emphasis\n");
	printf("        Enable pre-emphasis, if you directly connect to the oscillator of the\n");
	printf("        transmitter. (No pre-emphasis done by the transmitter.)\n");
	printf(" -e --de-emphasis\n");
	printf("        Enable de-emphasis, if you directly connect to the discriminator of\n");
	printf("        the receiver. (No de-emphasis done by the receiver.)\n");
	printf(" -G --rx-gain <dB>\n");
	printf("        Raise receiver RX level by given gain in dB. This is useful if input\n");
	printf("        level of the sound device is too low, even after setting maximum level\n");
	printf("        with the mixer settings.\n");
	printf(" -m --mncc-sock\n");
	printf("        Disable built-in call contol and offer socket (to LCR)\n");
	printf(" -c --call-device hw:<card>,<device>\n");
	printf("        Sound card and device number for headset (default = '%s')\n", call_sounddev);
	printf(" -p --send-patterns 0 | 1\n");
	printf("        Connect call on setup/release to provide classic tones (default = '%d')\n", send_patterns);
	printf(" -L --loopback <type>\n");
	printf("        Loopback test: 1 = internal | 2 = external | 3 = echo\n");
	printf(" -r --realtime <prio>\n");
	printf("        Set prio: 0 to diable, 99 for maximum (default = %d)\n", rt_prio);
	printf(" -W --write-wave <file>\n");
	printf("        Write received audio to given wav audio file.\n");
	printf(" -R --read-wave <file>\n");
	printf("        Replace received audio by given wav audio file.\n");
}

static struct option long_options_common[] = {
	{"help", 0, 0, 'h'},
	{"debug", 1, 0, 'D'},
	{"kanal", 1, 0, 'k'},
	{"device", 1, 0, 'd'},
	{"call-device", 1, 0, 'c'},
	{"samplerate", 1, 0, 's'},
	{"interval", 1, 0, 'i'},
	{"latency", 1, 0, 'l'},
	{"cross", 0, 0, 'x'},
	{"pre-emphasis", 0, 0, 'E'},
	{"de-emphasis", 0, 0, 'e'},
	{"rx-gain", 0, 0, 'G'},
	{"mncc-sock", 0, 0, 'm'},
	{"send-patterns", 0, 0, 'p'},
	{"loopback", 1, 0, 'L'},
	{"realtime", 1, 0, 'r'},
	{"write-wave", 1, 0, 'W'},
	{"read-wave", 1, 0, 'R'},
	{0, 0, 0, 0}
};

const char *optstring_common = "hD:k:d:s:c:i:l:xEeG:mp:L:r:W:R:";

struct option *long_options;
char *optstring;

void set_options_common(const char *optstring_special, struct option *long_options_special)
{
	int i;

	long_options = calloc(sizeof(*long_options), 100);
	for (i = 0; long_options_common[i].name; i++)
		memcpy(&long_options[i], &long_options_common[i], sizeof(*long_options));
	for (; long_options_special->name; i++)
		memcpy(&long_options[i], long_options_special++, sizeof(*long_options));
	
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
	case 'D':
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
		OPT_ARRAY(num_kanal, kanal, atoi(optarg))
		*skip_args += 2;
		break;
	case 'd':
		OPT_ARRAY(num_sounddev, sounddev, strdup(optarg))
		*skip_args += 2;
		break;
	case 's':
		samplerate = atoi(optarg);
		*skip_args += 2;
		break;
	case 'c':
		call_sounddev = strdup(optarg);
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
	case 'l':
		latency = atoi(optarg);
		*skip_args += 2;
		break;
	case 'x':
		cross_channels = 1;
		*skip_args += 1;
		break;
	case 'E':
		do_pre_emphasis = 1;
		*skip_args += 1;
		break;
	case 'e':
		do_de_emphasis = 1;
		*skip_args += 1;
		break;
	case 'G':
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
	case 'p':
		send_patterns = atoi(optarg);
		*skip_args += 2;
		break;
	case 'L':
		loopback = atoi(optarg);
		*skip_args += 2;
		break;
	case 'r':
		rt_prio = atoi(optarg);
		*skip_args += 2;
		break;
	case 'W':
		write_wave = strdup(optarg);
		*skip_args += 2;
		break;
	case 'R':
		read_wave = strdup(optarg);
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

	fprintf(stderr, "Signal received: %d\n", sigset);

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
void main_loop(int *quit, int latency, int interval, void (*myhandler)(void))
{
	int latspl;
	sender_t *sender;
	double last_time = 0, now;
	struct termios term, term_orig;
	int c;

	/* prepare terminal */
	tcgetattr(0, &term_orig);
	term = term_orig;
	term.c_lflag &= ~(ISIG|ICANON|ECHO);
	term.c_cc[VMIN]=1;
	term.c_cc[VTIME]=2;
	tcsetattr(0, TCSANOW, &term);

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
			*quit = 1;
			goto next_char;
		case 'w':
			/* toggle display */
			display_wave_on(-1);
			goto next_char;
		}

		/* process audio of built-in call control */
		process_call(c);

		if (myhandler)
			myhandler();

		/* sleep a while */
		usleep(interval * 1000);
	}

	/* reset terminal */
	tcsetattr(0, TCSANOW, &term_orig);
}

