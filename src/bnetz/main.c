/* B-Netz main
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
#include "../common/sample.h"
#include "../common/debug.h"
#include "../common/call.h"
#include "../common/main_mobile.h"
#include "../anetz/freiton.h"
#include "../anetz/besetztton.h"
#include "bnetz.h"
#include "dsp.h"
#include "stations.h"
#include "image.h"
#include "ansage.h"

int gfs = 2;
int metering = 20;
const char *paging = "tone";
double squelch_db = -INFINITY;

void print_help(const char *arg0)
{
	main_mobile_print_help(arg0, "[-G <gfs>] ");
	/*      -                                                                             - */
	printf(" -G --gfs <gruppenfreisignal> | <lat>,<lon>\n");
	printf("        Gruppenfreisignal\" 1..9 | 19 | 10..18 (default = '%d')\n", gfs);
	printf("        Alternative give your coordinates of your location, to find closest\n");
	printf("        base station. (e.g. '--gfs 54.487291,9.069993') Or use '--gfs list' to\n");
	printf("        get a list of all base station locations.\n");
	printf(" -G --gfs 19\n");
	printf("        Set to 19 in order to make the phone transmit at 100 mW instead of\n");
	printf("        full 15 Watts. If supported, the phone uses the channel with low power\n");
	printf("        (Kanal kleiner Leistung).\n");
	printf(" -M --gebuehrenimpuls <secods> | -<seconds> | 0\n");
	printf("        Send metering pulses every given number of seconds or 0 to turn off.\n");
	printf("        Pulses will be sent on outgoing calls only and only if mobile station\n");
	printf("        requests it. Use negative value to force metering pulses for all calls.\n");
	printf("        (default = %d)\n", metering);
	printf(" -P --paging tone | notone | positive | negative | <file>=<on>:<off>\n");
	printf("        Send a tone, give a signal or write to a file when switching to\n");
	printf("        channel 19. (paging the phone).\n");
	printf("        'tone', 'positive', 'negative' is sent on second audio channel.\n");
	printf("        'tone' sends a tone whenever channel 19 is switched.\n");
	printf("        'notone' sends a tone whenever channel 19 is NOT switched.\n");
	printf("        'positive' sends a positive signal for channel 19, else negative.\n");
	printf("        'negative' sends a negative signal for channel 19, else positive.\n");
	printf("        Example: /sys/class/gpio/gpio17/value=1:0 writes a '1' to\n");
	printf("        /sys/class/gpio/gpio17/value to switching to channel 19 and a '0' to\n");
	printf("        switch back. (default = %s)\n", paging);
	printf(" -S --squelch <dB>\n");
	printf("        Use given RF level to detect loss of signal. When the signal gets lost\n");
	printf("        and stays below this level, the connection is released.\n");
	printf("        Use 'auto' to do automatic noise floor calibration to detect loss.\n");
	printf("        Only works with SDR! (disabled by default)\n");
	printf("\nstation-id: Give 5 digit station-id, you don't need to enter it for every\n");
	printf("        start of this program.\n");
	main_mobile_print_hotkeys();
}

static int handle_options(int argc, char **argv)
{
	int skip_args = 0;
	char *p;

	static struct option long_options_special[] = {
		{"gfs", 1, 0, 'G'},
		{"gebuehrenimpuls", 1, 0, 'M'},
		{"paging", 1, 0, 'P'},
		{"squelch", 1, 0, 'S'},
		{0, 0, 0, 0},
	};

	main_mobile_set_options("G:M:P:S:", long_options_special);

	while (1) {
		int option_index = 0, c;

		c = getopt_long(argc, argv, optstring, long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
		case 'G':
			if (!strcasecmp(optarg, "list")) {
				station_list();
				exit(0);
			}
			if ((p = strchr(optarg, ','))) {
				gfs = get_station_by_coordinates(atof(optarg), atof(p + 1));
				if (gfs == 0)
					exit(0);
			} else
				gfs = atoi(optarg);
			skip_args += 2;
			break;
		case 'M':
			metering = atoi(optarg);
			skip_args += 2;
			break;
		case 'P':
			paging = strdup(optarg);
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
	init_ansage();

	main_mobile_init();

	skip_args = handle_options(argc, argv);
	argc -= skip_args;
	argv += skip_args;

	if (argc > 1) {
		station_id = argv[1];
		if (strlen(station_id) != 5) {
			printf("Given station ID '%s' does not have 5 digits\n", station_id);
			return 0;
		}
	}

	if (!num_kanal) {
		printf("No channel (\"Kanal\") is specified, I suggest channel 1 (sound card) or 17 (SDR).\n\n");
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

	/* init functions */
	dsp_init();
	bnetz_init();

	/* SDR always requires emphasis */
	if (use_sdr) {
		do_pre_emphasis = 1;
		do_de_emphasis = 1;
	}

	/* create transceiver instance */
	for (i = 0; i < num_kanal; i++) {
		rc = bnetz_create(kanal[i], audiodev[i], use_sdr, samplerate, rx_gain, gfs, do_pre_emphasis, do_de_emphasis, write_rx_wave, write_tx_wave, read_rx_wave, read_tx_wave, loopback, squelch_db, paging, metering);
		if (rc < 0) {
			fprintf(stderr, "Failed to create \"Sender\" instance. Quitting!\n");
			goto fail;
		}
		printf("Base station for channel %d ready, please tune transmitter to %.3f MHz and receiver " "to %.3f MHz. (%.3f MHz offset)\n", kanal[i], bnetz_kanal2freq(kanal[i], 0) / 1e6, bnetz_kanal2freq(kanal[i], 1) / 1e6, bnetz_kanal2freq(kanal[i], 2) / 1e6);
		printf("To call phone, switch transmitter (using paging signal) to %.3f MHz.\n", bnetz_kanal2freq(19, 0) / 1e6);
	}

	main_mobile(&quit, latency, interval, NULL, station_id, 5);

fail:
	/* destroy transceiver instance */
	while(sender_head)
		bnetz_destroy(sender_head);

	return 0;
}

