/* A-Netz main
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
#include "anetz.h"
#include "dsp.h"
#include "stations.h"
#include "image.h"

/* settings */
int page_sequence = 0;
double lossdetect = 0;

void print_help(const char *arg0)
{
	print_help_common(arg0, "");
	/*      -                                                                             - */
	printf(" -g --geo <lat>,<lon>\n");
	printf("        Give your coordinates of your location, to find closest base station.\n");
	printf("	(e.g. '--geo 51.186959,7.080194') Or use '--geo list' to get a list of\n");
	printf("        all base station locations.\n");
	printf(" -P --page-sequence 0 | <ms>\n");
	printf("        Cycle paging tones, rather than sending simultaniously.\n");
	printf("        (default = '%d')\n", page_sequence);
	printf(" -0 --loss <volume>\n");
	printf("        Detect loss of carrier by detecting steady noise above given volume in\n");
	printf("        percent. (disabled by default)\n");
	printf("\nstation-id: Give (last) 5 digits of station-id, you don't need to enter it\n");
	printf("        for every start of this program.\n");
}

static int handle_options(int argc, char **argv)
{
	int skip_args = 0;
	char *p;

	static struct option long_options_special[] = {
		{"geo", 1, 0, 'g'},
		{"page-sequence", 1, 0, 'P'},
		{"loss", 1, 0, '0'},
		{0, 0, 0, 0}
	};

	set_options_common("g:P:0:", long_options_special);

	while (1) {
		int option_index = 0, c;

		c = getopt_long(argc, argv, optstring, long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
		case 'g':
			if (!strcasecmp(optarg, "list")) {
				station_list();
				exit(0);
			}
			if ((p = strchr(optarg, ','))) {
				get_station_by_coordinates(atof(optarg), atof(p + 1));
				exit(0);
			}
			fprintf(stderr, "Invalid geo parameter\n");
			exit(0);
			break;
		case 'P':
			page_sequence = atoi(optarg);
			skip_args += 2;
			break;
		case '0':
			lossdetect = atoi(optarg);
			skip_args += 2;
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
	int i;

	/* init common tones */
	init_freiton();
	init_besetzton();

	skip_args = handle_options(argc, argv);
	argc -= skip_args;
	argv += skip_args;

	if (argc > 1) {
		station_id = argv[1];
		if (strlen(station_id) != 5 && strlen(station_id) != 7) {
			printf("Given station ID '%s' does not have 7 or (the last) 5 digits\n", station_id);
			return 0;
		}
		if (strlen(station_id) > 5)
			station_id += strlen(station_id) - 5;
	}

	if (!num_kanal) {
		printf("No channel (\"Kanal\") is specified, I suggest channel 30.\n\n");
		print_help(argv[-skip_args]);
		return 0;
	}
	if (num_kanal == 1 && num_sounddev == 0)
		num_sounddev = 1; /* use defualt */
	if (num_kanal != num_sounddev) {
		fprintf(stdout, "You need to specify as many sound devices as you have channels.\n");
		exit(0);
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
	dsp_init();
	anetz_init();
	rc = call_init(station_id, call_sounddev, samplerate, latency, 5, loopback);
	if (rc < 0) {
		fprintf(stderr, "Failed to create call control instance. Quitting!\n");
		goto fail;
	}

	/* create transceiver instance */
	for (i = 0; i < num_kanal; i++) {
		rc = anetz_create(kanal[i], sounddev[i], samplerate, cross_channels, rx_gain, page_sequence, do_pre_emphasis, do_de_emphasis, write_wave, read_wave, loopback, lossdetect / 100.0);
		if (rc < 0) {
			fprintf(stderr, "Failed to create \"Sender\" instance. Quitting!\n");
			goto fail;
		}
		printf("Base station on channel %d ready, please tune transmitter to %.3f MHz and receiver to %.3f MHz.\n", kanal[i], anetz_kanal2freq(kanal[i], 0), anetz_kanal2freq(kanal[i], 1));
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

	main_loop(&quit, latency, interval, NULL);

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
		anetz_destroy(sender_head);

	return 0;
}

