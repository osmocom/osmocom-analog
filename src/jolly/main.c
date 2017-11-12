/* main
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
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "../libsample/sample.h"
#include "../libmobile/main_mobile.h"
#include "../libdebug/debug.h"
#include "../libtimer/timer.h"
#include "../libmncc/mncc_sock.h"
#include "../anetz/freiton.h"
#include "../anetz/besetztton.h"
#include "jolly.h"
#include "dsp.h"
#include "voice.h"

/* settings */
int num_freq = 0;
double dl_freq = 438.600;
double ul_freq = 431.000;
double step = 25.0;
static double squelch_db = -INFINITY;
int nbfm = 0;
int repeater = 0;

void print_help(const char *arg0)
{
	main_mobile_print_help(arg0, "[-F <downlink MHz>,<uplink MHz>] ");
	/*      -                                                                             - */
	printf(" -F --frequency <downlink MHz>,<uplink MHz>,<step KHz>\n");
	printf("        Downlink and uplink frequency to use for channel 0.\n");
	printf("        Give step for channel spacing. (default = %.3f,%.3f,%.4f)\n", dl_freq, ul_freq, step);
	printf(" -S --squelch <dB> | auto\n");
	printf("        Use given RF level to detect loss of signal. When the signal gets lost\n");
	printf("        and stays below this level, the connection is released.\n");
	printf("        Use 'auto' to do automatic noise floor calibration to detect loss.\n");
	printf("        Only works with SDR! (disabled by default)\n");
	printf(" -N --nbfm\n");
	printf("        Use Narrow band FM with deviation of 2.5 KHz instead of 5.0 KHz.\n");
	printf(" -R --relay\n");
	printf("        Use transceiver as repeater, so multiple radios can communicate with\n");
	printf("        each other. It is still possible to make and receive calls. Multiple\n");
	printf("        radios can talk then to the calling/called party.\n");
	printf("\nstation-id: Give 4 digits of station-id, you don't need to enter it\n");
	printf("        for every start of this program.\n");
	main_mobile_print_hotkeys();
}

static int handle_options(int argc, char **argv)
{
	int skip_args = 0;

	static struct option long_options_special[] = {
		{"frequency", 1, 0, 'F'},
		{"squelch", 1, 0, 'S'},
		{"nbfm", 0, 0, 'N'},
		{"repeater", 0, 0, 'R'},
		{0, 0, 0, 0}
	};

	main_mobile_set_options("F:S:NR", long_options_special);

	while (1) {
		int option_index = 0, c;
		char *string, *string_dl, *string_ul, *string_step;

		c = getopt_long(argc, argv, optstring, long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
		case 'F':
			string = strdup(optarg);
			string_dl = strsep(&string, ",");
			string_ul = strsep(&string, ",");
			string_step = strsep(&string, ",");
			if (!string_dl || !string_ul || !string_step) {
				fprintf(stderr, "Please give 3 values for --frequency, seperated by comma and no space!\n");
				exit(0);
			}
			dl_freq = atof(string_dl);
			ul_freq = atof(string_ul);
			step = atof(string_step);
			skip_args += 2;
			break;
		case 'S':
			if (!strcasecmp(optarg, "auto"))
				squelch_db = 0.0;
			else
				squelch_db = atof(optarg);
			skip_args += 2;
			break;
		case 'N':
			nbfm = 1;
			skip_args += 1;
			break;
		case 'R':
			repeater = 1;
			skip_args += 1;
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
	int mandatory = 0;
	int i;

	/* init tones */
	init_freiton();
	init_besetzton();
//	init_ansage();

	main_mobile_init();

	skip_args = handle_options(argc, argv);
	argc -= skip_args;
	argv += skip_args;

	if (argc > 1) {
		station_id = argv[1];
		if (strlen(station_id) != 4) {
			printf("Given station ID '%s' does not have 4 digits\n", station_id);
			return 0;
		}
	}

	if (!num_kanal) {
		printf("No channel (\"Kanal\") is specified, Please use channel 0.\n\n");
		mandatory = 1;
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

	if (mandatory) {
		print_help(argv[-skip_args]);
		return 0;
	}

	/* SDR always requires emphasis */
	if (use_sdr) {
		do_pre_emphasis = 1;
		do_de_emphasis = 1;
	}

	/* no SDR, no squelch */
	if (!use_sdr && !isinf(squelch_db)) {
		fprintf(stderr, "Cannot use squelch without SDR! Analog receivers don't give use RSSI.\n");
		goto fail;
	}

	init_voice(samplerate);
	dsp_init();

	/* create transceiver instance */
	for (i = 0; i < num_kanal; i++) {
		rc = jolly_create(kanal[i], dl_freq, ul_freq, step, audiodev[i], use_sdr, samplerate, rx_gain, do_pre_emphasis, do_de_emphasis, write_rx_wave, write_tx_wave, read_rx_wave, read_tx_wave, loopback, squelch_db, nbfm, repeater);
		if (rc < 0) {
			fprintf(stderr, "Failed to create transceiver instance. Quitting!\n");
			goto fail;
		}
		printf("base station on channel %d ready, please tune transmitter to %.4f MHz and receiver to %.4f MHz. (%.4f MHz offset)\n", kanal[i], dl_freq + step / 1e3 * (double)kanal[i], ul_freq + step / 1e3 * (double)kanal[i], ul_freq - dl_freq);
	}

	main_mobile(&quit, latency, interval, NULL, station_id, 4);

fail:
	/* destroy transceiver instance */
	while (sender_head)
		jolly_destroy(sender_head);

	return 0;
}

