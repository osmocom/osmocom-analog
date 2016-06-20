/* NMT main
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
#include "nmt.h"
#include "frame.h"
#include "dsp.h"
#include "image.h"
#include "tones.h"
#include "announcement.h"

/* settings */
int num_chan_type = 0;
enum nmt_chan_type chan_type[MAX_SENDER] = { CHAN_TYPE_CC_TC };
int ms_power = 1; /* 1..3 */
char traffic_area[3] = "";
char area_no = 0;
int compandor = 1;
int supervisory = 0;

void print_help(const char *arg0)
{
	print_help_common(arg0, "-y <traffic area> | list ");
	/*      -                                                                             - */
	printf(" -t --channel-type <channel type> | list\n");
	printf("        Give channel type, use 'list' to get a list. (default = '%s')\n", chan_type_short_name(chan_type[0]));
	printf(" -P --ms-power <power level>\n");
	printf("        Give power level of the mobile station 1..3. (default = '%d')\n", ms_power);
	printf("        3 = 15 W / 7 W (handheld), 2 = 1.5 W, 1 = 150 mW\n");
	printf(" -y --traffic-area <traffic area> | list\n");
	printf("        NOTE: MUST MATCH WITH YOUR ROAMING SETTINGS IN THE PHONE!\n");
	printf("              Your phone will not connect, if country code is different!\n");
	printf("        Give two digits of traffic area, the first digit defines the country\n");
	printf("        code, the second defines the cell area. (Example: 61 = Sweden, cell 1)\n");
	printf("        Alternatively you can give the short code for country and the cell\n");
	printf("        area seperated by comma. (Example: SE,1 = Sweden, cell 1)\n");
	printf("        Use 'list' to get a list of available short country code names\n");
	printf(" -a --area-number <area no> | 0\n");
	printf("        Give area number 1..4 or 0 for no area number. (default = '%d')\n", area_no);
	printf(" -C --compandor 1 | 0\n");
	printf("        Make use of the compandor to reduce noise during call. (default = '%d')\n", compandor);
	printf(" -0 --supervisory 1..4 | 0\n");
	printf("        Use supervisory signal 1..4 to detect loss of signal from mobile\n");
	printf("        station, use 0 to disable. (default = '%d')\n", supervisory);
	printf("\nstation-id: Give 7 digits of station-id, you don't need to enter it\n");
	printf("        for every start of this program.\n");
}

static int handle_options(int argc, char **argv)
{
	char country[32], *p;
	int skip_args = 0;

	static struct option long_options_special[] = {
		{"channel-type", 1, 0, 't'},
		{"ms-power", 1, 0, 'P'},
		{"area-number", 1, 0, 'a'},
		{"traffic-area", 1, 0, 'y'},
		{"compandor", 1, 0, 'C'},
		{"supervisory", 1, 0, '0'},
		{0, 0, 0, 0}
	};

	set_options_common("t:P:a:y:C:0:", long_options_special);

	while (1) {
		int option_index = 0, c, rc;

		c = getopt_long(argc, argv, optstring, long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
		case 't':
			if (!strcmp(optarg, "list")) {
				nmt_channel_list();
				exit(0);
			}
			rc = nmt_channel_by_short_name(optarg);
			if (rc < 0) {
				fprintf(stderr, "Error, channel type '%s' unknown. Please use '-t list' to get a list. I suggest to use the default.\n", optarg);
				exit(0);
			}
			OPT_ARRAY(num_chan_type, chan_type, rc)
			skip_args += 2;
			break;
		case 'P':
			ms_power = atoi(optarg);
			if (ms_power > 3)
				ms_power = 3;
			if (ms_power < 1)
				ms_power = 1;
			skip_args += 2;
			break;
		case 'a':
			area_no = optarg[0] - '0';
			if (area_no > 4) {
				fprintf(stderr, "Area number '%s' out of range, please use 1..4 or 0 for no area\n", optarg);
				exit(0);
			}
			skip_args += 2;
			break;
		case 'y':

			if (!strcmp(optarg, "list")) {
				nmt_country_list();
				exit(0);
			}
			/* digits */
			if (optarg[0] >= '0' && optarg[0] <= '9') {
				traffic_area[0] = optarg[0];
				if (optarg[1] < '0' || optarg[1] > '9') {
error_ta:
					fprintf(stderr, "Illegal traffic area '%s', see '-h' for help\n", optarg);
					exit(0);
				}
				traffic_area[1] = optarg[1];
				if (optarg[2] != '\0')
					goto error_ta;
				traffic_area[2] = '\0';
			} else {
				strncpy(country, optarg, sizeof(country) - 1);
				country[sizeof(country) - 1] = '\0';
				p = strchr(country, ',');
				if (!p)
					goto error_ta;
				*p++ = '\0';
				rc = nmt_country_by_short_name(country);
				if (!rc)
					goto error_ta;
				if (*p < '0' || *p > '9')
					goto error_ta;
				traffic_area[0] = rc + '0';
				traffic_area[1] = *p;
				traffic_area[2] = '\0';
			}
			skip_args += 2;
			break;
		case 'C':
			compandor = atoi(optarg);
			skip_args += 2;
			break;
		case '0':
			supervisory = atoi(optarg);
			if (supervisory < 0 || supervisory > 4) {
				fprintf(stderr, "Given supervisory signal is wrong, use '-h' for help!\n");
				exit(0);
			}
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
	int mandatory = 0;
	int i;

	/* init common tones */
	init_nmt_tones();
	init_announcement();

	skip_args = handle_options(argc, argv);
	argc -= skip_args;
	argv += skip_args;

	if (argc > 1) {
		station_id = argv[1];
		if (strlen(station_id) != 7) {
			printf("Given station ID '%s' does not have 7 digits\n", station_id);
			return 0;
		}
	}

	if (!num_kanal) {
		printf("No channel (\"Kanal\") is specified, I suggest channel 1 (-k 1).\n\n");
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

	if (!traffic_area[0]) {
		printf("No traffic area is specified, I suggest to use Sweden (-y SE,1) and set the phone's roaming to 'SE' also.\n\n");
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
	init_frame();
	dsp_init();
	rc = call_init(station_id, call_sounddev, samplerate, latency, 7, loopback);
	if (rc < 0) {
		fprintf(stderr, "Failed to create call control instance. Quitting!\n");
		goto fail;
	}

	/* create transceiver instance */
	for (i = 0; i < num_kanal; i++) {
		rc = nmt_create(kanal[i], (loopback) ? CHAN_TYPE_TEST : chan_type[i], sounddev[i], samplerate, cross_channels, rx_gain, do_pre_emphasis, do_de_emphasis, write_wave, read_wave, ms_power, nmt_digits2value(traffic_area, 2), area_no, compandor, supervisory, loopback);
		if (rc < 0) {
			fprintf(stderr, "Failed to create transceiver instance. Quitting!\n");
			goto fail;
		}
		if (kanal[i] > 200) {
			printf("Base station on channel %d ready, please tune transmitter to %.4f MHz and receiver to %.4f MHz.\n", kanal[i], nmt_channel2freq(kanal[i], 0), nmt_channel2freq(kanal[i], 1));
		} else {
			printf("Base station on channel %d ready, please tune transmitter to %.3f MHz and receiver to %.3f MHz.\n", kanal[i], nmt_channel2freq(kanal[i], 0), nmt_channel2freq(kanal[i], 1));
		}
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

	main_loop(&quit, latency, interval);

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
		nmt_destroy(sender_head);

	return 0;
}

