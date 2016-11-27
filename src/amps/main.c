/* AMPS main
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
#include "../common/main.h"
#include "../common/debug.h"
#include "../common/timer.h"
#include "../common/call.h"
#include "../common/mncc_sock.h"
#include "amps.h"
#include "dsp.h"
#include "frame.h"
#include "image.h"
#include "tones.h"
#include "noanswer.h"
#include "outoforder.h"
#include "invalidnumber.h"
#include "congestion.h"
#include "stations.h"

/* settings */
int num_chan_type = 0;
enum amps_chan_type chan_type[MAX_SENDER] = { CHAN_TYPE_CC_PC_VC };
const char *flip_polarity = "";
int ms_power = 4, dcc = 0, scc = 0, sid = 40, regh = 1, regr = 1, pureg = 1, pdreg = 1, locaid = -1, regincr = 300, bis = 0;
int tolerant = 0;

void print_help(const char *arg0)
{
	print_help_common(arg0, "-p -d -F yes | no ");
	/*      -                                                                             - */
	printf(" -T --channel-type <channel type> | list\n");
	printf("        Give channel type, use 'list' to get a list. (default = '%s')\n", chan_type_short_name(chan_type[0]));
	printf(" -F --flip-polarity no | yes\n");
	printf("        Flip polarity of transmitted FSK signal. If yes, the sound card\n");
	printf("        generates a negative signal rather than a positive one. Be sure that\n");
	printf("        a positive signal causes a positive deviation on your transmitter.\n");
	printf("        If the phone shows 'NoSrv', try the other way.\n");
	printf(" -P --ms-power <power level>\n");
	printf("        Give power level of the mobile station 0..7. (default = '%d')\n", ms_power);
	printf("        0 = 4 W;     1 = 1.6 W;  2 = 630 mW;  3 = 250 mW;\n");
	printf("	4 = 100 mW;  5 = 40 mW;  6 = 16 mW;   7 = 6.3 mW\n");
	printf(" -S --sysinfo sid=<System ID> | sid=list\n");
	printf("        Give system ID of cell broadcast (default = '%d')\n", sid);
	printf("        If it changes, phone re-registers. Use 'sid=list' to get a full list.\n");
	printf(" -S --sysinfo dcc=<digital color code>\n");
	printf("        Give digital color code 0..3 (default = '%d')\n", dcc);
	printf(" -S --sysinfo scc=<SAT color code>\n");
	printf("        Give supervisor tone color code 0..2 (default = '%d')\n", scc);
	printf(" -S --sysinfo regincr\n");
	printf("        Amount to add to REGID after successful registration (default = '%d')\n", regincr);
	printf("        Since REGID is incremented every second, this value define after how\n");
	printf("        many second the phone waits before it re-registers.\n");
	printf(" -S --sysinfo pureg=0 | pureg=1\n");
	printf("        If 1, phone registers on every power on (default = '%d')\n", pureg);
	printf(" -S --sysinfo pdreg=0 | pdreg=1\n");
	printf("        If 1, phone de-registers on every power down (default = '%d')\n", pureg);
	printf(" -S --sysinfo locaid=<location area ID > | locaid=-1 to disable\n");
	printf("        (default = '%d')\n", locaid);
	printf("        If it changes, phone re-registers.\n");
	printf(" -S --sysinfo regh=0 | regh=1\n");
	printf("        If 1, phone registers only if System ID matches (default = '%d')\n", regh);
	printf(" -S --sysinfo regr=0 | regr=1\n");
	printf("        If 1, phone registers only if System ID is different (default = '%d')\n", regr);
	printf(" -S --sysinfo bis=0 | bis=1\n");
	printf("        If 0, phone ignores BUSY/IDLE bit on FOCC (default = '%d')\n", bis);
	printf("        If 1, be sure to have a round-trip delay (latency) not more than 5 ms\n");
	printf(" -O --tolerant\n");
	printf("        Be more tolerant when hunting for sync sequence\n");
	printf("\nstation-id: Give 10 digit station-id, you don't need to enter it for every\n");
	printf("        start of this program.\n");
}

static int handle_options(int argc, char **argv)
{
	const char *p;
	int skip_args = 0;
	int rc;

	static struct option long_options_special[] = {
		{"channel-type", 1, 0, 'T'},
		{"flip-polarity", 1, 0, 'F'},
		{"ms-power", 1, 0, 'P'},
		{"sysinfo", 1, 0, 'S'},
		{"tolerant", 0, 0, 'O'},
		{0, 0, 0, 0}
	};

	set_options_common("T:F:P:S:O", long_options_special);

	while (1) {
		int option_index = 0, c;

		c = getopt_long(argc, argv, optstring, long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
		case 'T':
			if (!strcmp(optarg, "list")) {
				amps_channel_list();
				exit(0);
			}
			rc = amps_channel_by_short_name(optarg);
			if (rc < 0) {
				fprintf(stderr, "Error, channel type '%s' unknown. Please use '-t list' to get a list. I suggest to use the default.\n", optarg);
				exit(0);
			}
			OPT_ARRAY(num_chan_type, chan_type, rc)
			skip_args += 2;
			break;
		case 'F':
			if (!strcasecmp(optarg, "no"))
				flip_polarity = "no";
			else if (!strcasecmp(optarg, "yes"))
				flip_polarity = "yes";
			else {
				fprintf(stderr, "Given polarity '%s' is illegal, see help!\n", optarg);
				exit(0);
			}
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
		case 'S':
			p = strchr(optarg, '=');
			if (!p) {
				fprintf(stderr, "Given sysinfo parameter '%s' requires '=' character to set value, see help!\n", optarg);
				exit(0);
			}
			p++;
			if (!strncasecmp(optarg, "sid=", p - optarg)) {
				if (!strcasecmp(p, "list")) {
					list_stations();
					exit(0);
				}
				sid = atoi(p);
				if (sid > 32767)
					sid = 32767;
				if (sid < 0)
					sid = 0;
			} else
			if (!strncasecmp(optarg, "dcc=", p - optarg)) {
				dcc = atoi(p);
				if (dcc > 3)
					dcc = 3;
				if (dcc < 0)
					dcc = 0;
			} else
			if (!strncasecmp(optarg, "scc=", p - optarg)) {
				scc = atoi(p);
				if (scc > 2)
					scc = 2;
				if (scc < 0)
					scc = 0;
			} else
			if (!strncasecmp(optarg, "regincr=", p - optarg)) {
				regincr = atoi(p);
			} else
			if (!strncasecmp(optarg, "pureg=", p - optarg)) {
				pureg = atoi(p) & 1;
			} else
			if (!strncasecmp(optarg, "pdreg=", p - optarg)) {
				pdreg = atoi(p) & 1;
			} else
			if (!strncasecmp(optarg, "locaid=", p - optarg)) {
				locaid = atoi(p);
				if (locaid > 4095)
					locaid = 4095;
			} else
			if (!strncasecmp(optarg, "regh=", p - optarg)) {
				regh = atoi(p) & 1;
			} else
			if (!strncasecmp(optarg, "regr=", p - optarg)) {
				regr = atoi(p) & 1;
			} else
			if (!strncasecmp(optarg, "bis=", p - optarg)) {
				bis = atoi(p) & 1;
			} else {
				fprintf(stderr, "Given sysinfo parameter '%s' unknown, see help!\n", optarg);
				exit(0);
			}
			skip_args += 2;
			break;
		case 'O':
			tolerant = 1;
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
	int polarity;
	int i;

	/* override default */
	samplerate = 96000;

	/* init common tones */
	init_tones();
	init_noanswer();
	init_outoforder();
	init_invalidnumber();
	init_congestion();

	skip_args = handle_options(argc, argv);
	argc -= skip_args;
	argv += skip_args;

	if (argc > 1) {
		station_id = argv[1];
		if (strlen(station_id) != 10) {
			printf("Given station ID '%s' does not have 10 digits\n", station_id);
			return 0;
		}
	}

	if (!num_kanal) {
		printf("No channel (\"Kanal\") is specified, I suggest channel 334.\n\n");
		print_help(argv[-skip_args]);
		return 0;
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

	if (bis && latency > 5) {
		fprintf(stdout, "If you use BUSY/IDLE bit, you need to lower the round-trip delay to 5 ms (--latenc 5).\n");
		exit(0);
	}

	if (!loopback)
		print_image();
	sid_stations(sid);

	/* init functions */
	if (use_mncc_sock) {
		rc = mncc_init("/tmp/bsc_mncc");
		if (rc < 0) {
			fprintf(stderr, "Failed to setup MNCC socket. Quitting!\n");
			return -1;
		}
	}
	dsp_init();
	init_frame();
	rc = call_init(station_id, call_sounddev, samplerate, latency, 10, loopback);
	if (rc < 0) {
		fprintf(stderr, "Failed to create call control instance. Quitting!\n");
		goto fail;
	}

	/* check for mandatory CC */
	for (i = 0; i < num_kanal; i++) {
		if (chan_type[i] == CHAN_TYPE_CC || chan_type[i] == CHAN_TYPE_CC_PC || chan_type[i] == CHAN_TYPE_CC_PC_VC)
			break;
	}
	if (i == num_kanal) {
		fprintf(stderr, "You must define at least one CC (control) or combined channel type. Quitting!\n");
		goto fail;
	}

	/* check for mandatory PC */
	for (i = 0; i < num_kanal; i++) {
		if (chan_type[i] == CHAN_TYPE_CC_PC || chan_type[i] == CHAN_TYPE_CC_PC_VC)
			break;
	}
	if (i == num_kanal) {
		fprintf(stderr, "You must define at least one PC (paging) or combined channel type. Quitting!\n");
		goto fail;
	}

	/* check for mandatory VC */
	for (i = 0; i < num_kanal; i++) {
		if (chan_type[i] == CHAN_TYPE_VC || chan_type[i] == CHAN_TYPE_CC_PC_VC)
			break;
	}
	if (i == num_kanal)
		fprintf(stderr, "You did not define any VC (voice) channel. You will not be able to make any call.\n");

	if (!do_pre_emphasis || !do_de_emphasis) {
		fprintf(stderr, "*******************************************************************************\n");
		fprintf(stderr, "I strongly suggest to let me do pre- and de-emphasis (options -p -d)!\n");
		fprintf(stderr, "Use a transmitter/receiver without emphasis and let me do that!\n");
		fprintf(stderr, "Because carrier FSK signaling does not use emphasis, I like to control\n");
		fprintf(stderr, "emphasis by myself for best results.\n");
		fprintf(stderr, "*******************************************************************************\n");
	}

	if (!strcmp(flip_polarity, "no"))
		polarity = 1; /* positive */
	else if (!strcmp(flip_polarity, "yes"))
		polarity = -1; /* negative */
	else {
		fprintf(stderr, "You must define, if the the TX deviation polarity has to be flipped. (-F yes | no) See help.\n");
		exit(0);
	}

	/* create transceiver instance */
	for (i = 0; i < num_kanal; i++) {
		amps_si si;

		init_sysinfo(&si, ms_power, ms_power, dcc, sid >> 1, regh, regr, pureg, pdreg, locaid, regincr, bis);
		rc = amps_create(kanal[i], chan_type[i], sounddev[i], samplerate, cross_channels, rx_gain, do_pre_emphasis, do_de_emphasis, write_rx_wave, write_tx_wave, read_rx_wave, &si, sid, scc, polarity, tolerant, loopback);
		if (rc < 0) {
			fprintf(stderr, "Failed to create \"Sender\" instance. Quitting!\n");
			goto fail;
		}
		printf("Base station on channel %d ready (%s), please tune transmitter to %.3f MHz and receiver to %.3f MHz.\n", kanal[i], chan_type_long_name(chan_type[i]), amps_channel2freq(kanal[i], 0), amps_channel2freq(kanal[i], 1));
	}

	main_loop(&quit, latency, interval, NULL);

fail:
	/* cleanup functions */
	call_cleanup();
	if (use_mncc_sock)
		mncc_exit();

	/* destroy transceiver instance */
	while (sender_head)
		amps_destroy(sender_head);

	return 0;
}

