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
#include "main.h"
#include "debug.h"

/* common settings */
int kanal = 0;
const char *sounddev = "hw:0,0";
const char *call_sounddev = "";
int samplerate = 48000;
int latency = 50;
int use_mncc_sock = 0;
int send_patterns = 1;
int loopback = 0;
double lossdetect = 0;
int rt_prio = 0;

void print_help_common(const char *arg0)
{
	printf("Usage: %s -k kanal [options] [station-id]\n", arg0);
	printf("\noptions:\n");
	/*      -                                                                             - */
	printf(" -h --help\n");
	printf("        This help\n");
	printf(" -D --debug <level>\n");
	printf("        Debug level:  0 = debug | 1 = info | 2 = notice (default = '%d')\n", debuglevel);
	printf(" -k --kanal <channel>\n");
	printf("        Channel number of \"Sender\" (default = '%d')\n", kanal);
	printf(" -d --device hw:<card>,<device>\n");
	printf("        Sound card and device number (default = '%s')\n", sounddev);
	printf(" -s --samplerate <rate>\n");
	printf("        Sample rate of sound device (default = '%d')\n", samplerate);
	printf(" -l --latency <delay>\n");
	printf("        How many milliseconds processed in advance  (default = '%d')\n", latency);
	printf(" -0 --loss <volume>\n");
	printf("        Detect loss of carrier by detecting steady noise above given volume in\n");
	printf("        percent. (disabled by default)\n");
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
}

static struct option long_options_common[] = {
	{"help", 0, 0, 'h'},
	{"debug", 1, 0, 'D'},
	{"kanal", 1, 0, 'k'},
	{"device", 1, 0, 'd'},
	{"call-device", 1, 0, 'c'},
	{"samplerate", 1, 0, 's'},
	{"latency", 1, 0, 'l'},
	{"loss", 1, 0, '0'},
	{"mncc-sock", 0, 0, 'm'},
	{"send-patterns", 0, 0, 'p'},
	{"loopback", 1, 0, 'L'},
	{"realtime", 1, 0, 'r'},
	{0, 0, 0, 0}
};

const char *optstring_common = "hD:k:d:s:c:l:0:mp:L:r:";

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
	switch (c) {
	case 'h':
		print_help(arg0);
		exit(0);
	case 'D':
		debuglevel = atoi(optarg);
		if (debuglevel > 2)
			debuglevel = 2;
		if (debuglevel < 0)
			debuglevel = 0;
		*skip_args += 2;
		break;
	case 'k':
		kanal = atoi(optarg);
		*skip_args += 2;
		break;
	case 'd':
		sounddev = strdup(optarg);
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
	case 'l':
		latency = atoi(optarg);
		*skip_args += 2;
		break;
	case '0':
		lossdetect = atoi(optarg);
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


