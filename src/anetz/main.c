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
#include <math.h>
#include "../libsample/sample.h"
#include "../common/main_mobile.h"
#include "../common/debug.h"
#include "../libtimer/timer.h"
#include "../common/call.h"
#include "freiton.h"
#include "besetztton.h"
#include "anetz.h"
#include "dsp.h"
#include "stations.h"
#include "image.h"

/* settings */
static char operator[32] = "010";
static double page_gain = 1;
static int page_sequence = 0;
static double squelch_db = -INFINITY;

void print_help(const char *arg0)
{
	main_mobile_print_help(arg0, "[-V 12] ");
	/*      -                                                                             - */
	printf(" -O --operator <number>\n");
	printf("        Give number to dial when mobile station initiated a call. A-Netz does\n");
	printf("        not support automatic dialing, so operator assistance is required.\n");
	printf("        By default, the operator '%s' is dialed.\n", operator);
	printf(" -G --geo <lat>,<lon>\n");
	printf("        Give your coordinates of your location, to find closest base station.\n");
	printf("	(e.g. '--geo 51.186959,7.080194') Or use '--geo list' to get a list of\n");
	printf("        all base station locations.\n");
	printf(" -V --page-gain <dB>\n");
	printf("        Raise the gain of paging tones to compensate loss due to pre-emphasis\n");
	printf("        of the transmitter. (If you can't disable it.)\n");
	printf(" -P --page-sequence 0 | <ms>\n");
	printf("        Cycle paging tones, rather than sending simultaniously. Try 100.\n");
	printf("        (default = '%d')\n", page_sequence);
	printf(" -S --squelch <dB> | auto\n");
	printf("        Use given RF level to detect loss of signal. When the signal gets lost\n");
	printf("        and stays below this level, the connection is released.\n");
	printf("        Use 'auto' to do automatic noise floor calibration to detect loss.\n");
	printf("        Only works with SDR! (disabled by default)\n");
	printf("\nstation-id: Give (last) 5 digits of station-id, you don't need to enter it\n");
	printf("        for every start of this program.\n");
	main_mobile_print_hotkeys();
}

static int handle_options(int argc, char **argv)
{
	int skip_args = 0;
	char *p;
	double gain_db;

	static struct option long_options_special[] = {
		{"operator", 1, 0, 'O'},
		{"geo", 1, 0, 'G'},
		{"page-gain", 1, 0, 'V'},
		{"page-sequence", 1, 0, 'P'},
		{"squelch", 1, 0, 'S'},
		{0, 0, 0, 0}
	};

	main_mobile_set_options("O:G:V:P:S:", long_options_special);

	while (1) {
		int option_index = 0, c;

		c = getopt_long(argc, argv, optstring, long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
		case 'O':
			strncpy(operator, optarg, sizeof(operator) - 1);
			operator[sizeof(operator) - 1] = '\0';
			skip_args += 2;
			break;
		case 'G':
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
		case 'V':
			gain_db = atof(optarg);
			page_gain = pow(10, gain_db / 20.0);
			skip_args += 2;
			break;
		case 'P':
			page_sequence = atoi(optarg);
			skip_args += 2;
			break;
		case 'S':
			if (!strcasecmp(optarg, "auto"))
				squelch_db = 0.0;
			else
				squelch_db = atof(optarg);
			skip_args += 2;
			break;
		default:
			main_mobile_opt_switch(c, argv[0], &skip_args);
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

	/* a-netz does not use emphasis, so disable it */
	uses_emphasis = 0;

	/* init common tones */
	init_freiton();
	init_besetzton();

	main_mobile_init();

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
	if (use_sdr) {
		/* set audiodev */
		for (i = 0; i < num_kanal; i++)
			audiodev[i] = "sdr";
		num_audiodev = num_kanal;
	}
	if (num_kanal == 1 && num_audiodev == 0)
		num_audiodev = 1; /* use default */
	if (num_kanal != num_audiodev) {
		fprintf(stderr, "You need to specify as many sound devices as you have channels.\n");
		exit(0);
	}

	if (!loopback)
		print_image();

	dsp_init();
	anetz_init();

	/* create transceiver instance */
	for (i = 0; i < num_kanal; i++) {
		rc = anetz_create(kanal[i], audiodev[i], use_sdr, samplerate, rx_gain, page_gain, page_sequence, do_pre_emphasis, do_de_emphasis, write_rx_wave, write_tx_wave, read_rx_wave, read_tx_wave, loopback, squelch_db, operator);
		if (rc < 0) {
			fprintf(stderr, "Failed to create \"Sender\" instance. Quitting!\n");
			goto fail;
		}
		printf("Base station on channel %d ready, please tune transmitter to %.3f MHz and receiver to %.3f MHz. (%.3f MHz offset)\n", kanal[i], anetz_kanal2freq(kanal[i], 0) / 1e6, anetz_kanal2freq(kanal[i], 1) / 1e6, anetz_kanal2freq(kanal[i], 2) / 1e6);
	}

	main_mobile(&quit, latency, interval, NULL, station_id, 5);

fail:
	/* destroy transceiver instance */
	while (sender_head)
		anetz_destroy(sender_head);

	return 0;
}

