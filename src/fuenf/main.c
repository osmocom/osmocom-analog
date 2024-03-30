/* 5-Ton-Folge main
 *
 * (C) 2021 by Andreas Eversberg <jolly@eversberg.eu>
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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "../libsample/sample.h"
#include "../liblogging/logging.h"
#include "../libmobile/call.h"
#include "../libmobile/main_mobile.h"
#include "../liboptions/options.h"
#include "../anetz/besetztton.h"
#include "fuenf.h"
#include "dsp.h"

static int tx = 0;		/* we transmit */
static int rx = 0;		/* we receive */
static double max_deviation = 4000;
static double signal_deviation = 2400;
static enum fuenf_funktion funktion = FUENF_FUNKTION_FEUER;
static uint32_t scan_from = 0;
static uint32_t scan_to = 0;

void print_help(const char *arg0)
{
	main_mobile_print_help(arg0, "-k <kanal> | -k list");
	/*      -                                                                             - */
	printf(" -T --tx\n");
	printf("        Transmit 5-tones on given channel, to page a receiver. (default)\n");
	printf(" -R --rx\n");
	printf("        Receive 5-tones on given channel, so we are the receiver.\n");
	printf("        If none of the options -T nor -R is given, only transmitter is enabled.\n");
	printf(" -D --deviation <KHz>\n");
	printf("        Choose deviation of FM signal (default %.0f KHz).\n", signal_deviation / 1000.0);
	printf(" -F --funktion 0 | ruf | 1 | feuer | 2 | probe | 3 | warnung | 4 | abc\n");
	printf("        | 5 | entwarnung | 6 | katastrophe | 7 | turbo\n");
	printf("        Choose default function when 5 digit only number is dialed.\n");
	printf("        (default %d = %s)\n", funktion, fuenf_funktion_name[funktion]);
	printf(" -S --scan <from> <to>\n");
	printf("        Scan through given IDs once (no repetition). This can be useful to find\n");
	printf("        the call sign of a vintage receiver. Note that scanning all call signs\n");
	printf("        from 000000 through 99999 would take about 8.7 days.\n");
	printf("        If 'turbo' function is selected, only a single 5-tone sequence is sent\n");
	printf("        per call sign. A full scan would takte about 26.4 hours then.\n");
	main_mobile_print_station_id();
	main_mobile_print_hotkeys();
}

static void add_options(void)
{
	main_mobile_add_options();
	option_add('T', "tx", 0);
	option_add('R', "rx", 0);
	option_add('D', "deviation", 1);
	option_add('F', "funktion", 1);
	option_add('S', "scan", 2);
}

static int handle_options(int short_option, int argi, char **argv)
{
	switch (short_option) {
	case 'T':
		tx = 1;
		break;
	case 'R':
		rx = 1;
		break;
	case 'D':
		signal_deviation = atof(argv[argi]) * 1000.0;
		if (signal_deviation < 1000.0) {
			fprintf(stderr, "Given deviation is too low, use higher deviation.\n");
			return -EINVAL;
		}
		if (signal_deviation > max_deviation)
			max_deviation = signal_deviation;
		break;
	case 'F':
		switch (argv[argi][0]) {
		case '0':
		case 'r':
		case 'R':
			funktion = FUENF_FUNKTION_RUF;
			break;
		case '1':
		case 'f':
		case 'F':
			funktion = FUENF_FUNKTION_FEUER;
			break;
		case '2':
		case 'p':
		case 'P':
			funktion = FUENF_FUNKTION_PROBE;
			break;
		case '3':
		case 'w':
		case 'W':
			funktion = FUENF_FUNKTION_WARNUNG;
			break;
		case '4':
		case 'a':
		case 'A':
			funktion = FUENF_FUNKTION_ABC;
			break;
		case '5':
		case 'e':
		case 'E':
			funktion = FUENF_FUNKTION_ENTWARNUNG;
			break;
		case '6':
		case 'k':
		case 'K':
			funktion = FUENF_FUNKTION_KATASTROPHE;
			break;
		case '7':
		case 't':
		case 'T':
			funktion = FUENF_FUNKTION_TURBO;
			break;
		default:
			fprintf(stderr, "Given 'Funktion' is invalid, use '-h' for help.\n");
			return -EINVAL;
		}
		break;
	case 'S':
		scan_from = atoi(argv[argi++]);
		if (scan_from > 99999) {
			fprintf(stderr, "Given call sign to scan from is out of range!\n");
			return -EINVAL;
		}
		scan_to = atoi(argv[argi++]) + 1;
		if (scan_to > 99999) {
			fprintf(stderr, "Given call sign to scan to is out of range!\n");
			return -EINVAL;
		}
		break;
	default:
		return main_mobile_handle_options(short_option, argi, argv);
	}

	return 1;
}

static const struct number_lengths number_lengths[] = {
	{ 5, "5-Ton-Folge" },
	{ 6, "5-Ton-Folge mit Ruf (0) oder Sirenenalarm (1..6)" },
	{ 0, NULL }
};

int main(int argc, char *argv[])
{
	int rc, argi;
	const char *station_id = "";
	int i;
	const char *k;
	double f;

	/* BOS does not use emphasis, so disable it */
	uses_emphasis = 0;

	/* init common tones */
	init_besetzton();

	/* init mobile interface */
	main_mobile_init("0123456789", number_lengths, NULL, bos_number_valid);

	/* handle options / config file */
	add_options();
	rc = options_config_file(argc, argv, "~/.osmocom/analog/5-ton-folge.conf", handle_options);
	if (rc < 0)
		return 0;
	argi = options_command_line(argc, argv, handle_options);
	if (argi <= 0)
		return argi;

	if (argi < argc) {
		station_id = argv[argi];
		rc = main_mobile_number_ask(station_id, "station ID (RIC)");
		if (rc)
			return rc;
	}

	if (!num_kanal) {
		printf("No channel is specified, Use '-k list' to get a list of all channels.\n\n");
		print_help(argv[0]);
		return 0;
	}
	for (i = 0; i < num_kanal; i++) {
		if (!strcasecmp(kanal[i], "list")) {
			bos_list_channels();
			goto fail;
		}
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

	/* TX is default */
	if (!tx && !rx)
		tx = 1;

	/* TX & RX if loopback */
	if (loopback)
		tx = rx = 1;

	/* inits */
	fm_init(fast_math);
	fuenf_init();

	/* create transceiver instance */
	for (i = 0; i < num_kanal; i++) {
		k = bos_freq2kanal(kanal[i]);
		f = bos_kanal2freq(k);
		if (f == 0.0) {
			printf("Invalid channel '%s', Use '-k list' to get a list of all channels.\n\n", k);
			goto fail;
		}
		rc = fuenf_create(k, f, dsp_device[i], use_sdr, dsp_samplerate, rx_gain, tx_gain, tx, rx, max_deviation, signal_deviation, funktion, scan_from, scan_to, write_rx_wave, write_tx_wave, read_rx_wave, read_tx_wave, loopback);
		if (rc < 0) {
			fprintf(stderr, "Failed to create \"Sender\" instance. Quitting!\n");
			goto fail;
		}
		printf("Base station ready, please tune transmitter (or receiver) to %.4f MHz\n", f / 1e6);
	}

	main_mobile_loop("5-ton-folge", &quit, NULL, station_id);

fail:
	/* destroy transceiver instance */
	while(sender_head)
		fuenf_destroy(sender_head);

	/* exits */
	main_mobile_exit();
	fm_exit();
	fuenf_exit();

	options_free();

	return 0;
}

