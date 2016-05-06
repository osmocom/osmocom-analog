/* C-Netz main
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
#include "../common/main.h"
#include "../common/debug.h"
#include "../common/timer.h"
#include "../common/call.h"
#include "../common/mncc_sock.h"
#include "../common/freiton.h"
#include "../common/besetztton.h"
#include "cnetz.h"
#include "sysinfo.h"
#include "dsp.h"
#include "telegramm.h"
#include "image.h"
#include "ansage.h"

/* settings */
int num_chan_type = 0;
enum cnetz_chan_type chan_type[MAX_SENDER] = { CHAN_TYPE_OGK_SPK };
int measure_speed = 0;
double clock_speed[2] = { 0.0, 0.0 };
int set_clock_speed = 0;
double deviation = 0.5, noise = 0.0;
int ms_power = 0; /* 0..3 */
int auth = 0;

void print_help(const char *arg0)
{
	print_help_common(arg0, "");
	/*      -                                                                             - */
	printf(" -t --channel-type <channel type> | list\n");
	printf("        Give channel type, use 'list' to get a list. (default = '%s')\n", chan_type_short_name(chan_type[0]));
	printf(" -M --measure-speed\n");
	printf("        Measures clock speed. THIS IS REQUIRED! See documentation!\n");
	printf(" -S --clock-speed <rx ppm>,<tx ppm>\n");
	printf("        Correct speed of sound card's clock. Use '-M' to measure speed for\n");
	printf("        some hours after temperature has settled. The use these results to\n");
	printf("	correct signal processing speed. After adjustment, the clock must match\n");
	printf("	+- 1ppm or better. CORRECTING CLOCK SPEED IS REQUIRED! See\n");
	printf("        documentation on how to measure correct value.\n");
	printf(" -F --flip-polarity\n");
	printf("	Adjust, so the transmitter increases frequency, when positive levels\n");
	printf("	are sent to sound device\n");
	printf(" -N --noise 0.0 .. 1.0 (default = %.1f)\n", noise);
	printf("	Between frames on OgK, send noise at given level. Use 0.0 for Silence.\n");
	printf(" -P --ms-power <power level>\n");
	printf("        Give power level of the mobile station 0..3. (default = '%d')\n", ms_power);
	printf("	0 = 50-125 mW;  1 = 0.5-1 W;  2 = 4-8 W;  3 = 10-20 W\n");
	printf(" -A --authentication\n");
	printf("        Enable authentication on the base station. Since we cannot\n");
	printf("	authenticate, because we don't know the secret key and the algorithm,\n");
	printf("	we just accept any card. With this we get the vendor IDs of the phone.\n");
}

static int handle_options(int argc, char **argv)
{
	int skip_args = 0;
	int rc;
	const char *p;

	static struct option long_options_special[] = {
		{"channel-type", 1, 0, 't'},
		{"measure-speed", 0, 0, 'M'},
		{"clock-speed", 1, 0, 'S'},
		{"flip-polarity", 0, 0, 'F'},
		{"noise", 1, 0, 'N'},
		{"ms-power", 1, 0, 'P'},
		{"authentication", 0, 0, 'A'},
		{0, 0, 0, 0}
	};

	set_options_common("t:MS:FN:P:A", long_options_special);

	while (1) {
		int option_index = 0, c;

		c = getopt_long(argc, argv, optstring, long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
		case 't':
			if (!strcmp(optarg, "list")) {
				cnetz_channel_list();
				exit(0);
			}
			rc = cnetz_channel_by_short_name(optarg);
			if (rc < 0) {
				fprintf(stderr, "Error, channel type '%s' unknown. Please use '-t list' to get a list. I suggest to use the default.\n", optarg);
				exit(0);
			}
			OPT_ARRAY(num_chan_type, chan_type, rc)
			skip_args += 2;
			break;
		case 'M':
			measure_speed = 1;
			skip_args++;
			break;
		case 'S':
			p = strchr(optarg, ',');
			if (!p) {
				fprintf(stderr, "Illegal clock speed, use two values, seperated by comma and no spaces!\n");
				exit(0);
			}
			clock_speed[0] = strtold(optarg, NULL);
			clock_speed[1] = strtold(p + 1, NULL);
			set_clock_speed = 1;
			skip_args += 2;
			break;
		case 'F':
			deviation = -deviation;
			skip_args += 1;
			break;
		case 'N':
			noise = strtold(optarg, NULL);
			skip_args += 2;
			break;
		case 'P':
			ms_power = atoi(optarg);
			if (ms_power > 3)
				ms_power = 3;
			if (ms_power < 0)
				ms_power = 0;
			skip_args += 2;
			break;
		case 'A':
			auth = 1;
			skip_args += 1;
			break;
		default:
			opt_switch_common(c, argv[0], &skip_args);
		}
	}

	free(long_options);

	return skip_args;
}

int main(int argc, char *argv[])
{
	int rc;
	int skip_args;
	const char *station_id = "";
	int mandatory = 0;
	int i;

	/* init common tones */
	init_freiton();
	init_besetzton();
	init_ansage();

	skip_args = handle_options(argc, argv);
	argc -= skip_args;
	argv += skip_args;

	if (argc > 1) {
	}

	if (!num_kanal) {
		printf("No channel (\"Kanal\") is specified, I suggest channel %d.\n\n", CNETZ_OGK_KANAL);
		mandatory = 1;
	}
	if (num_kanal == 1 && num_sounddev == 0)
		num_sounddev = 1; /* use defualt */
	if (num_kanal != num_sounddev) {
		fprintf(stdout, "You need to specify as many sound devices as you have channels.\n");
		exit(0);
	}
	if (num_kanal == 1 && num_chan_type == 0)
		num_chan_type = 1; /* use defualt */
	if (num_kanal != num_chan_type) {
		fprintf(stdout, "You need to specify as many channel types as you have channels.\n");
		exit(0);
	}

	if (!set_clock_speed && !measure_speed) {
		printf("No clock speed given. You need to measure clock using '-M' and later correct clock using '-S <rx ppm>,<tx ppm>'. See documentation for help!\n\n");
		mandatory = 1;
	}

	if (mandatory) {
		print_help(argv[-skip_args]);
		return 0;
	}

	if (!loopback)
		print_image();

	/* init functions */
	if (use_mncc_sock) {
		rc = mncc_init("/tmp/bsc_mncc");
		if (rc < 0) {
			fprintf(stderr, "Failed to setup MNCC socket. Quitting!\n");
			return -1;
		}
	}
	scrambler_init();
	init_sysinfo();
	dsp_init();
	init_telegramm();
	init_coding();
	cnetz_init();
	rc = call_init(station_id, call_sounddev, samplerate, latency, 7, loopback);
	if (rc < 0) {
		fprintf(stderr, "Failed to create call control instance. Quitting!\n");
		goto fail;
	}

	/* check for mandatory OgK */
	for (i = 0; i < num_kanal; i++) {
		if (chan_type[i] == CHAN_TYPE_OGK || chan_type[i] == CHAN_TYPE_OGK_SPK)
			break;
	}
	if (i == num_kanal) {
		fprintf(stderr, "You must define at least one OgK (control) or OgK/SPK (control/speech) channel type. Quitting!\n");
		goto fail;
	}

	/* check for mandatory OgK */
	for (i = 0; i < num_kanal; i++) {
		if (chan_type[i] == CHAN_TYPE_SPK || chan_type[i] == CHAN_TYPE_OGK_SPK)
			break;
	}
	if (i == num_kanal)
		fprintf(stderr, "You did not define any SpK (speech) channel. You will not be able to make any call.\n");

	/* create transceiver instance */
	for (i = 0; i < num_kanal; i++) {
		rc = cnetz_create(kanal[i], chan_type[i], sounddev[i], samplerate, cross_channels, rx_gain, auth, ms_power, (i == 0) ? measure_speed : 0, clock_speed, deviation, noise, do_pre_emphasis, do_de_emphasis, write_wave, read_wave, loopback);
		if (rc < 0) {
			fprintf(stderr, "Failed to create \"Sender\" instance. Quitting!\n");
			goto fail;
		}
		printf("Base station on channel %d ready, please tune transmitter to %.3f MHz and receiver to %.3f MHz.\n", kanal[i], cnetz_kanal2freq(kanal[i], 0), cnetz_kanal2freq(kanal[i], 1));
	}

	signal(SIGINT,sighandler);
	signal(SIGHUP,sighandler);
	signal(SIGTERM,sighandler);
	signal(SIGPIPE,sighandler);

	if (rt_prio > 0) {
		struct sched_param schedp;
		int rc;

		memset(&schedp, 0, sizeof(schedp));
		schedp.sched_priority = rt_prio;
		rc = sched_setscheduler(0, SCHED_RR, &schedp);
		if (rc)
			fprintf(stderr, "Error setting SCHED_RR with prio %d\n", rt_prio);
	}

	main_loop(&quit, latency);

	if (rt_prio > 0) {
		struct sched_param schedp;

		memset(&schedp, 0, sizeof(schedp));
		schedp.sched_priority = 0;
		sched_setscheduler(0, SCHED_OTHER, &schedp);
	}

fail:
	/* cleanup functions */
	call_cleanup();
	if (use_mncc_sock)
		mncc_exit();

	/* destroy transceiver instance */
	while (sender_head)
		cnetz_destroy(sender_head);

	return 0;
}

