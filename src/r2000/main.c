/* Radiocom 2000 main
 *
 * (C) 2017 by Andreas Eversberg <jolly@eversberg.eu>
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
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "../libsample/sample.h"
#include "../libmobile/main_mobile.h"
#include "../libdebug/debug.h"
#include "../liboptions/options.h"
#include "r2000.h"
#include "dsp.h"
#include "frame.h"
#include "tones.h"
#include "image.h"

/* settings */
static int band = 1;
static int num_chan_type = 0;
static int relais = 32;
static int deport = 0;
static int agi = 7;
static int sm_power = 0;
static int taxe = 0;
static int crins = 0, destruction = 0; /* neven set CRINS to 3 and destruction to other than 0 here! */
static int nconv = 0;
static int recall = 0;
enum r2000_chan_type chan_type[MAX_SENDER] = { CHAN_TYPE_CC_TC };

void print_help(const char *arg0)
{
	main_mobile_print_help(arg0, "[-R <relais number>] ");
	/*      -                                                                             - */
	printf(" -B --band <number> | list\n");
	printf(" -B --bande <number> | list\n");
	printf("        Give frequency band, use 'list' to get a list. (default = '%d')\n", band);
	printf(" -T --channel-type <channel type> | list\n");
	printf("        Give channel type, use 'list' to get a list. (default = '%s')\n", chan_type_short_name(chan_type[0]));
	printf(" -R --relais <relais number>\n");
	printf("        Give relais number (base station ID) 1..511. (default = '%d')\n", relais);
	printf("        Be sure to set the station mobile to the same relais number!\n");
	printf(" --deport 0..7\n");
	printf("        Supervisory information to tell about sub-stations.\n");
	printf("        The functionality is unknown. (default = '%d')\n", deport);
	printf(" -I --agi 0..7 | list\n");
	printf("        Supervisory information to tell which phone is allowed to register\n");
	printf("        Use 'list' to get a list of possible valued.\n");
	printf("        (default = '%d' = %s)\n", agi, param_agi(agi));
#if 0
       printf(" -A --aga 0..3 | list\n");
       printf("        Supervisory information to tell which phone is allowed to call\n");
       printf("        Use 'list' to get a list of possible valued.\n");
       printf("        (default = '%d' = %s)\n", aga, param_aga(aga));
#endif
	printf(" -P --sm-power <power level>\n");
	printf("        Give power level of the station mobile 0..1. (default = '%d')\n", sm_power);
	printf("        0 = low (about 1 Watts)  1 = high (up to 10 Watts)\n");
	printf(" --taxe 0..1\n");
	printf("        Supervisory information to tell about rate information.\n");
	printf("        The functionality is unknown. (default = '%d')\n", taxe);
	printf(" -C --crins 0..7 | list [--destruction YES]\n");
	printf("        Result that will be returned when the phone registers.\n");
	printf("        NEVER USE '3', IT WILL DESTROY YOUR PHONE, but shows a warning first!\n");
	printf("        Use 'list' to get a list of possible valued.\n");
	printf("        (default = '%d' = %s)\n", crins, param_crins(crins));
	printf(" -N --nconv 0..7\n");
	printf("        Supervisory digit, sent during conversation. (default = '%d')\n", nconv);
	printf("        It is used to detect lost signal. When using multiple traffic\n");
	printf("        channels, this value is incremented per channel.\n");
	printf(" -S --recall\n");
	printf("        Suspend outgoing call after dialing and recall when called party has\n");
	printf("        answered.\n");
	printf("\nstation-id: Give 1 digit of station mobile type + 3 digits of home relais ID\n");
	printf("        + 5 digits of mobile ID.\n");
	printf("        (e.g. 103200819 = type 1, relais ID 32, mobile ID 819)\n");
	main_mobile_print_hotkeys();
}

#define OPT_BANDE	256
#define OPT_DEPORT	257
#define OPT_TAXE	258
#define OPT_DESTRUCTION	259

static void add_options(void)
{
	main_mobile_add_options();
	option_add('B', "band", 1);
	option_add(OPT_BANDE, "bande", 1);
	option_add('T', "channel-type", 1);
	option_add('R', "relais", 1);
	option_add(OPT_DEPORT, "deport", 1);
	option_add('I', "agi", 1);
	option_add('P', "sm-power", 1);
	option_add(OPT_TAXE, "taxe", 1);
	option_add('C', "crins", 1);
	option_add(OPT_DESTRUCTION, "destruction", 1);
	option_add('N', "nconv", 1);
	option_add('S', "recall", 0);
}

static int handle_options(int short_option, int argi, char **argv)
{
	int rc;

	switch (short_option) {
	case 'B':
	case OPT_BANDE:
		if (!strcmp(argv[argi], "list")) {
			r2000_band_list();
			return 0;
		}
		band = atoi(argv[argi]);
		break;
	case 'T':
		if (!strcmp(argv[argi], "list")) {
			r2000_channel_list();
			return 0;
		}
		rc = r2000_channel_by_short_name(argv[argi]);
		if (rc < 0) {
			fprintf(stderr, "Error, channel type '%s' unknown. Please use '-t list' to get a list. I suggest to use the default.\n", argv[argi]);
			return -EINVAL;
		}
		OPT_ARRAY(num_chan_type, chan_type, rc)
		break;
	case 'R':
		relais = atoi(argv[argi]);
		if (relais > 511)
			relais = 511;
		if (relais < 1)
			relais = 1;
		break;
	case OPT_DEPORT:
		deport = atoi(argv[argi]);
		if (deport > 7)
			deport = 7;
		if (deport < 0)
			deport = 0;
		break;
	case 'I':
		if (!strcmp(argv[argi], "list")) {
			int i;

			printf("\nList of possible AGI (inscription permission) codes:\n\n");
			printf("Value\tDescription\n");
			printf("------------------------------------------------------------------------\n");
			for (i = 0; i < 8; i++)
				printf("%d\t%s\n", i, param_agi(i));
			return 0;
		}
		agi = atoi(argv[argi]);
		if (agi < 0 || agi > 7) {
			fprintf(stderr, "Error, given inscription permission (AGI) %d is invalid, use 'list' to get a list of values!\n", agi);
			return -EINVAL;
		}
		break;
	case 'P':
		sm_power = atoi(argv[argi]);
		if (sm_power > 1)
			sm_power = 1;
		if (sm_power < 0)
			sm_power = 0;
		break;
	case OPT_TAXE:
		taxe = atoi(argv[argi]);
		if (taxe > 1)
			taxe = 1;
		if (taxe < 0)
			taxe = 0;
		break;
#if 0
	case 'A':
		if (!strcmp(argv[argi], "list")) {
			int i;

			printf("\nList of possible AGA (call permission) codes:\n\n");
			printf("Value\tDescription\n");
			printf("------------------------------------------------------------------------\n");
			for (i = 0; i < 4; i++)
				printf("%d\t%s\n", i, param_aga(i));
			return 0;
		}
		aga = atoi(argv[argi]);
		if (aga < 0 || aga > 3) {
			fprintf(stderr, "Error, given call permission (AGA) %d is invalid, use 'list' to get a list of values!\n", aga);
			return -EINVAL;
		}
		break;
#endif
	case 'C':
		if (!strcmp(argv[argi], "list")) {
			int i;

			printf("\nList of possible CRINS (inscription response) codes:\n\n");
			printf("Value\tDescription\n");
			printf("------------------------------------------------------------------------\n");
			for (i = 0; i < 8; i++)
				printf("%d\t%s\n", i, param_crins(i));
			return 0;
		}
		crins = atoi(argv[argi]);
		if (crins < 0 || crins > 7) {
			fprintf(stderr, "Error, given inscription response (CRINS) %d is invalid, use 'list' to get a list of values!\n", crins);
			return -EINVAL;
		}
		break;
	case OPT_DESTRUCTION:
		if (!strcmp(argv[argi], "YES")) {
			destruction = 2342;
		}
		break;
	case 'N':
		nconv = atoi(argv[argi]);
		if (nconv > 7)
			nconv = 7;
		if (nconv < 0)
			nconv = 0;
		break;
	case 'S':
		recall = 1;
		break;
	default:
		return main_mobile_handle_options(short_option, argi, argv);
	}

	return 1;
}

int main(int argc, char *argv[])
{
	int rc, argi;
	const char *station_id = "";
	int mandatory = 0;
	int i;

	/* init tones */
	init_radiocom_tones();

	main_mobile_init();

	/* handle options / config file */
	add_options();
	rc = options_config_file("~/.osmocom/analog/radiocom2000.conf", handle_options);
	if (rc < 0)
		return 0;
	argi = options_command_line(argc, argv, handle_options);
	if (argi <= 0)
		return argi;

	if (argi < argc) {
		station_id = argv[argi];
		if (strlen(station_id) != 9) {
			printf("Given station ID '%s' does not have 9 digits\n", station_id);
			return 0;
		}
	}

	if (!num_kanal) {
		printf("No channel (\"Kanal\") is specified, I suggest channel 160 (-k 160).\n\n");
		mandatory = 1;
	}
	if (use_sdr) {
		/* set audiodev */
		for (i = 0; i < num_kanal; i++)
			audiodev[i] = "sdr";
		num_audiodev = num_kanal;
		/* set channel types for more than 1 channel */
		if (num_kanal > 1 && num_chan_type == 0) {
			chan_type[0] = CHAN_TYPE_CC;
			for (i = 1; i < num_kanal; i++)
				chan_type[i] = CHAN_TYPE_TC;
			num_chan_type = num_kanal;
		}

	}
	if (num_kanal == 1 && num_audiodev == 0)
		num_audiodev = 1; /* use default */
	if (num_kanal != num_audiodev) {
		fprintf(stderr, "You need to specify as many sound devices as you have channels.\n");
		exit(0);
	}
	if (num_kanal == 1 && num_chan_type == 0)
		num_chan_type = 1; /* use default */
	if (num_kanal != num_chan_type) {
		fprintf(stderr, "You need to specify as many channel types as you have channels.\n");
		exit(0);
	}

	if (mandatory) {
		print_help(argv[0]);
		return 0;
	}

	/* check for destruction of the phone (crins 3 will brick it) */
	if (crins == 3) {
		fprintf(stderr, "\n*******************************************************************************\n");
		fprintf(stderr, "You selected inscription response '3'!\n\n");
		fprintf(stderr, "This feature was used by the operators to destroy a stolen/modified phone.\n");
		fprintf(stderr, "This will brick/destroy/kill/make ALL PHONES USELESS, if registering!\n");
		fprintf(stderr, "PHONE WILL LOCK AND/OR SUBSCRIBER DATA WILL BE ERASED!!! IS THAT WHAT YOU WANT?\n");
		fprintf(stderr, "I had to hack the firmware of my phone to unbrick it. Can you do that too?\n");
		if (!destruction)
			fprintf(stderr, "If you can unlock your phone later, then use '--destruction YES' to confirm.\n");
		else
			fprintf(stderr, "\n **** PRESS CTRL+c TO ABORT THIS FEATURE, NOW! ****  Press enter to continue.\n\n");
		fprintf(stderr, "*******************************************************************************\n\n");
		if (!destruction)
			exit(0);
		else
			getchar();
	}

	if (!loopback && crins != 3)
		print_image();

	/* init functions */
	dsp_init();

	/* SDR always requires emphasis */
	if (use_sdr) {
		do_pre_emphasis = 1;
		do_de_emphasis = 1;
	}

	if (!do_pre_emphasis || !do_de_emphasis) {
		fprintf(stderr, "*******************************************************************************\n");
		fprintf(stderr, "I strongly suggest to let me do pre- and de-emphasis (options -p -d)!\n");
		fprintf(stderr, "Use a transmitter/receiver without emphasis and let me do that!\n");
		fprintf(stderr, "Because 50 baud supervisory signalling arround 150 Hz will not be tranmitted by\n");
		fprintf(stderr, "regular radio, use direct input to the PLL of your transmitter (or use SDR).\n");
		fprintf(stderr, "*******************************************************************************\n");
	}

	/* create transceiver instance */
	for (i = 0; i < num_kanal; i++) {
		rc = r2000_create(band, kanal[i], chan_type[i], audiodev[i], use_sdr, samplerate, rx_gain, do_pre_emphasis, do_de_emphasis, write_rx_wave, write_tx_wave, read_rx_wave, read_tx_wave, relais, deport, agi, sm_power, taxe, crins, destruction, nconv, recall, loopback);
		if (rc < 0) {
			fprintf(stderr, "Failed to create transceiver instance. Quitting!\n");
			goto fail;
		}
		printf("base station on channel %d ready, please tune transmitter to %.4f MHz and receiver to %.4f MHz. (%.3f MHz offset)\n", kanal[i], r2000_channel2freq(band, kanal[i], 0) / 1e6, r2000_channel2freq(band, kanal[i], 1) / 1e6, r2000_channel2freq(band, kanal[i], 2) / 1e6);
		nconv = (nconv + 1) & 7;
	}

	r2000_check_channels();

	main_mobile(&quit, latency, interval, NULL, station_id, 9);

fail:
	/* destroy transceiver instance */
	while (sender_head)
		r2000_destroy(sender_head);

	return 0;
}

