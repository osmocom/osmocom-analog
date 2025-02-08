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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "../libsample/sample.h"
#include "../liblogging/logging.h"
#include "../libmobile/call.h"
#include "../libmobile/main_mobile.h"
#include "../liboptions/options.h"
#include "bnetz.h"
#include "dsp.h"
#include "stations.h"

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
	printf("        If metering pulses are sent via Osmo-CC interface, pulses are always\n");
	printf("        sent, if mobile station requests it. This overrides this option.\n");
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
	main_mobile_print_station_id();
	main_mobile_print_hotkeys();
}

static void add_options(void)
{
	main_mobile_add_options();
	option_add('G', "gfs", 1);
	option_add('M', "gebuehrenimpuls", 1);
	option_add('P', "paging", 1);
	option_add('S', "squelch", 1);
}

static int handle_options(int short_option, int argi, char **argv)
{
	char *p;

	switch (short_option) {
	case 'G':
		if (!strcasecmp(argv[argi], "list")) {
			station_list();
			return 0;
		}
		if ((p = strchr(argv[argi], ','))) {
			gfs = get_station_by_coordinates(atof(argv[argi]), atof(p + 1));
			if (gfs == 0)
				return -EINVAL;
		} else
			gfs = atoi(argv[argi]);
		break;
	case 'M':
		metering = atoi(argv[argi]);
		break;
	case 'P':
		paging = options_strdup(argv[argi]);
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

static const struct number_lengths number_lengths[] = {
	{ 5, "B-Netz number" },
	{ 0, NULL }
};

static const char *number_prefixes[] = {
	"05xxxxx",
	NULL
};

int main(int argc, char *argv[])
{
	int rc, argi;
	const char *station_id = "";
	int i;

	main_mobile_init("0123456789", number_lengths, number_prefixes, NULL, "oldgerman");

	/* handle options / config file */
	add_options();
	rc = options_config_file(argc, argv, "~/.osmocom/analog/bnetz.conf", handle_options);
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
		printf("No channel (\"Kanal\") is specified, I suggest channel 1 (sound card) or 17 (SDR).\n\n");
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
		num_device = 1; /*deviceuse default */
	if (num_kanal != num_device) {
		fprintf(stderr, "You need to specify as many sound devices as you have channels.\n");
		exit(0);
	}

	/* inits */
	fm_init(fast_math);
	dsp_init();
	bnetz_init();

	/* SDR always requires emphasis */
	if (use_sdr) {
		do_pre_emphasis = 1;
		do_de_emphasis = 1;
	}

	/* use squelch */
	if (!use_sdr || isinf(squelch_db)) {
		fprintf(stderr, "*******************************************************************************\n");
		fprintf(stderr, "I strongly suggest using squelch on your receiver! This prevents false channel\n");
		fprintf(stderr, "allocation, due to received noise. For SDR, add '-S auto' to command line.\n");
		fprintf(stderr, "*******************************************************************************\n");
	}

	/* create transceiver instance */
	for (i = 0; i < num_kanal; i++) {
		rc = bnetz_create(kanal[i], dsp_device[i], use_sdr, dsp_samplerate, rx_gain, tx_gain, gfs, do_pre_emphasis, do_de_emphasis, write_rx_wave, write_tx_wave, read_rx_wave, read_tx_wave, loopback, squelch_db, paging, metering);
		if (rc < 0) {
			fprintf(stderr, "Failed to create \"Sender\" instance. Quitting!\n");
			goto fail;
		}
		printf("Base station for channel %s ready, please tune transmitter to %.3f MHz and receiver " "to %.3f MHz. (%.3f MHz offset)\n", kanal[i], bnetz_kanal2freq(atoi(kanal[i]), 0) / 1e6, bnetz_kanal2freq(atoi(kanal[i]), 1) / 1e6, bnetz_kanal2freq(atoi(kanal[i]), 2) / 1e6);
		printf("To call phone, switch transmitter (using paging signal) to %.3f MHz.\n", bnetz_kanal2freq(19, 0) / 1e6);
	}

	main_mobile_loop("bnetz", &quit, NULL, station_id);

fail:
	/* destroy transceiver instance */
	while(sender_head)
		bnetz_destroy(sender_head);

	/* exits */
	main_mobile_exit();
	fm_exit();

	options_free();

	return 0;
}

