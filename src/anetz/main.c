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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "../libsample/sample.h"
#include "../libmobile/main_mobile.h"
#include "../liblogging/logging.h"
#include <osmocom/core/timer.h>
#include "../libmobile/call.h"
#include "../liboptions/options.h"
#include "../libfm/fm.h"
#include "anetz.h"
#include "dsp.h"
#include "stations.h"

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
	main_mobile_print_station_id();
	main_mobile_print_hotkeys();
}

static void add_options(void)
{
	main_mobile_add_options();
	option_add('O', "operator", 1);
	option_add('G', "geo", 1);
	option_add('V', "page-gain", 1);
	option_add('P', "page-sequence", 1);
	option_add('S', "squelch", 1);
}

static int handle_options(int short_option, int argi, char **argv)
{
	char *p;
	double gain_db;

	switch (short_option) {
	case 'O':
		strncpy(operator, argv[argi], sizeof(operator) - 1);
		operator[sizeof(operator) - 1] = '\0';
		break;
	case 'G':
		if (!strcasecmp(argv[argi], "list")) {
			station_list();
			return 0;
		}
		if ((p = strchr(argv[argi], ','))) {
			get_station_by_coordinates(atof(argv[argi]), atof(p + 1));
			return 0;
		}
		fprintf(stderr, "Invalid geo parameter\n");
		return -EINVAL;
	case 'V':
		gain_db = atof(argv[argi]);
		page_gain = pow(10, gain_db / 20.0);
		break;
	case 'P':
		page_sequence = atoi(argv[argi]);
		break;
	case 'S':
		if (!strcasecmp(argv[argi], "auto"))
			squelch_db = 0.0;
		else
			squelch_db = atof(argv[argi]);
		break;
	default:
		return main_mobile_handle_options(short_option, argi, argv);
	}

	return 1;
}

static const struct number_lengths  number_lengths[] = {
	{ 5, "number without channel prefix" },
	{ 7, "number with channel prefix" },
	{ 0, NULL }
};

int main(int argc, char *argv[])
{
	int rc, argi;
	const char *station_id = "";
	int i;

	/* a-netz does not use emphasis, so disable it */
	uses_emphasis = 0;

	main_mobile_init("0123456789", number_lengths, NULL, anetz_number_valid, "oldgerman");

	/* handle options / config file */
	add_options();
	rc = options_config_file(argc, argv, "~/.osmocom/analog/anetz.conf", handle_options);
	if (rc < 0)
		return 0;
	argi = options_command_line(argc, argv, handle_options);
	if (argi <= 0)
		return argi;

	if (argi < argc) {
		station_id = argv[argi];
		rc = main_mobile_number_ask(station_id, "station ID");
		if (rc)
			return rc;
	}

	if (!num_kanal) {
		printf("No channel (\"Kanal\") is specified, I suggest channel 30.\n\n");
		print_help(argv[0]);
		return 0;
	}
	if (use_sdr) {
		/* set device */
		for (i = 0; i < num_kanal; i++)
			dsp_device[i] = "sdr";
		num_device = num_kanal;
	}
	if (num_kanal == 1 && num_device == 0)
		num_device = 1; /* use default */
	if (num_kanal != num_device) {
		fprintf(stderr, "You need to specify as many sound devices as you have channels.\n");
		exit(0);
	}

	/* inits */
	fm_init(fast_math);
	dsp_init();
	anetz_init();

	/* create transceiver instance */
	for (i = 0; i < num_kanal; i++) {
		rc = anetz_create(kanal[i], dsp_device[i], use_sdr, dsp_samplerate, rx_gain, tx_gain, page_gain, page_sequence, do_pre_emphasis, do_de_emphasis, write_rx_wave, write_tx_wave, read_rx_wave, read_tx_wave, loopback, squelch_db, operator);
		if (rc < 0) {
			fprintf(stderr, "Failed to create \"Sender\" instance. Quitting!\n");
			goto fail;
		}
		printf("Base station on channel %s ready, please tune transmitter to %.3f MHz and receiver to %.3f MHz. (%.3f MHz offset)\n", kanal[i], anetz_kanal2freq(atoi(kanal[i]), 0) / 1e6, anetz_kanal2freq(atoi(kanal[i]), 1) / 1e6, anetz_kanal2freq(atoi(kanal[i]), 2) / 1e6);
	}

	main_mobile_loop("anetz", &quit, NULL, station_id);

fail:
	/* destroy transceiver instance */
	while (sender_head)
		anetz_destroy(sender_head);

	/* exits */
	main_mobile_exit();
	fm_exit();

	options_free();

	return 0;
}

