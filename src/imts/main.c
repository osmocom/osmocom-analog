/* MTS/IMTS main
 *
 * (C) 2019 by Andreas Eversberg <jolly@eversberg.eu>
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
#include "../libdebug/debug.h"
#include "../libtimer/timer.h"
#include "../libmobile/call.h"
#include "../liboptions/options.h"
#include "../amps/tones.h"
#include "../amps/outoforder.h"
#include "../amps/noanswer.h"
#include "../amps/invalidnumber.h"
#include "../amps/congestion.h"
#include "imts.h"
#include "dsp.h"

/* settings */
static double squelch_db = -INFINITY;
static int ptt = 0;
static double fast_seize = 0.0;
static enum mode mode = MODE_IMTS;
static char operator[32] = "010";
static double detector_test_length_1 = 0.0;
static double detector_test_length_2 = 0.0;
static double detector_test_length_3 = 0.0;

void print_help(const char *arg0)
{
	main_mobile_print_help(arg0, "-b 5 -i 0.1 ");
	/*      -                                                                             - */
	printf(" -S --squelch <dB> | auto\n");
	printf("        Use given RF level to detect loss of signal. When the signal gets lost\n");
	printf("        and stays below this level, the connection is released.\n");
	printf("        Use 'auto' to do automatic noise floor calibration to detect loss.\n");
	printf("        Only works with SDR! (disabled by default)\n");
	printf(" -P --push-to-talk\n");
	printf("        Allow push-to-talk operation for IMTS mode. (MTS always uses it.)\n");
	printf("        This adds extra delay to received audio, to eliminate noise when the\n");
	printf("        transmitter of the phone is turned off. Also this disables release on\n");
	printf("        loss of RF signal. (Squelch is required for this to operate.)\n");
	printf(" -F --fast-seize <delay in ms>\n");
	printf("        To compensate audio processing latency, give delay when to respond,\n");
	printf("        after detection of Guard tone from mobile phone.\n");
	printf("        Run software in loopback mode '-l 2' to measure round trip delay.\n");
	printf("        Subtract delay from 350 ms. If the phone has different Guard tone\n");
	printf("        length, subtract from that value.\n");
	printf(" -D --detector-test <idle length> <seize length> <silence length>\n");
	printf("        Transmit detector test signal, to adjust decoder inside mobile phone.\n");
	printf("        Give length of idle / seize and silence in seconds. Listen to it with\n");
	printf("        a radio receiver. To exclude an element, set its length to '0'.\n");
	printf("        Example: '-D 0.5 0.5 0' plays alternating idle/seize tone.\n");
	printf("\nMTS mode options\n");
	printf(" -M --mts\n");
	printf("        Run base station in MTS mode, rather than in IMTS mode.\n");
	printf(" -O --operator <number>\n");
	printf("        Give number to dial when mobile station initiated a call in MTS mode.\n");
	printf("        Because there is no dial on the mobile phone, operator assistance is\n");
	printf("        required to complete the call.\n");
	printf("        By default, the operator '%s' is dialed.\n", operator);
	printf(" -D --detector-test <600 Hz length> <1500 Hz length> <silence length>\n");
	printf("        Transmit detector test signal, to adjust decoder inside MTS phone.\n");
	printf("        Give length of 600/1500 Hz and silence in seconds. Listen to it with\n");
	printf("        a radio receiver. To exclude an element, set its length to '0'.\n");
	printf("        Example: '-D 0.5 0.5 0' plays alternating 600/1500 Hz tone.\n");
	main_mobile_print_station_id();
	main_mobile_print_hotkeys();
}

static void add_options(void)
{
	main_mobile_add_options();
	option_add('S', "squelch", 1);
	option_add('P', "push-to-talk", 0);
	option_add('F', "fast-seize", 1);
	option_add('D', "decoder-test", 3);
	option_add('M', "mts", 0);
	option_add('O', "operator", 1);
}

static int handle_options(int short_option, int argi, char **argv)
{
	switch (short_option) {
	case 'S':
		if (!strcasecmp(argv[argi], "auto"))
			squelch_db = 0.0;
		else
			squelch_db = atof(argv[argi]);
		break;
	case 'P':
		ptt = 1;
		break;
	case 'F':
		fast_seize = atof(argv[argi]) / 1000.0;
		if (fast_seize < 0.0)
			fast_seize = 0.0;
		break;
	case 'D':
		detector_test_length_1 = atof(argv[argi++]);
		detector_test_length_2 = atof(argv[argi++]);
		detector_test_length_3 = atof(argv[argi++]);
		break;
	case 'M':
		mode = MODE_MTS;
		ptt = 1;
		break;
	case 'O':
		strncpy(operator, argv[argi], sizeof(operator) - 1);
		operator[sizeof(operator) - 1] = '\0';
		break;
	default:
		return main_mobile_handle_options(short_option, argi, argv);
	}

	return 1;
}

static const struct number_lengths number_lengths[] = {
	{ 5, "MTS number format" },
	{ 7, "IMTS number format" },
	{ 10, "IMTS number (digits 4..6 will br removed)" },
	{ 0, NULL }
};

static const char *number_prefixes[] = {
	"1xxxxxxxxxx",
	"+1xxxxxxxxxx",
	NULL
};

int main(int argc, char *argv[])
{
	int rc, argi;
	const char *station_id = "";
	int i;

	/* init common tones */
	init_tones();
	init_outoforder();
	init_noanswer();
	init_invalidnumber();
	init_congestion();

	/* init mobile interface */
	main_mobile_init("0123456789", number_lengths, number_prefixes, NULL);

	/* handle options / config file */
	add_options();
	rc = options_config_file(argc, argv, "~/.osmocom/analog/imts.conf", handle_options);
	if (rc < 0)
		return 0;
	argi = options_command_line(argc, argv, handle_options);
	if (argi <= 0)
		return argi;

	/* set check for MTS mode */
	if (mode == MODE_MTS)
		main_mobile_set_number_check_valid(mts_number_valid);

	if (argi < argc) {
		station_id = argv[argi];
		rc = main_mobile_number_ask(station_id, "station ID");
		if (rc)
			return rc;
	}

	if (!num_kanal) {
		printf("No channel (\"Kanal\") is specified, Use '-k list' to get a list of all channels.\n\n");
		print_help(argv[0]);
		return 0;
	}
	if (!strcasecmp(kanal[0], "list")) {
		imts_list_channels();
		goto fail;
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

	/* SDR always requires emphasis */
	if (use_sdr) {
		do_pre_emphasis = 1;
		do_de_emphasis = 1;
	}

	if (!do_pre_emphasis || !do_de_emphasis) {
		fprintf(stderr, "*******************************************************************************\n");
		fprintf(stderr, "I suggest to let me do pre- and de-emphasis (options -p -d)!\n");
		fprintf(stderr, "Use a transmitter/receiver without emphasis and let me do that!\n");
		fprintf(stderr, "Because FSK signaling does not use emphasis, I like to control emphasis by\n");
		fprintf(stderr, "myself for best results.\n");
		fprintf(stderr, "*******************************************************************************\n");
	}

	if (mode == MODE_IMTS && !fast_seize && dsp_buffer > 5 && loopback == 0) {
		fprintf(stderr, "*******************************************************************************\n");
		fprintf(stderr, "It is required to have a low latency in order to respond to phone's seizure\n");
		fprintf(stderr, "fast enough! Please reduce buffer size to 5 ms via option: '-b 5 -i 0.1'\n");
		fprintf(stderr, "If this causes buffer underruns, use the 'Fast Seize' mode, see help.\n");
		fprintf(stderr, "*******************************************************************************\n");
		exit(0);
	}

	if (mode == MODE_MTS && !use_sdr && loopback == 0) {
		fprintf(stderr, "*******************************************************************************\n");
		fprintf(stderr, "MTS mode requires use of SDR, because base station is controlled by Squelch.\n");
		fprintf(stderr, "*******************************************************************************\n");
		exit(0);
	}
	if (mode == MODE_MTS && isinf(squelch_db) < 0 && loopback == 0) {
		fprintf(stderr, "*******************************************************************************\n");
		fprintf(stderr, "MTS mode requires use of Squelch. Please set Squelch level, see help.\n");
		fprintf(stderr, "*******************************************************************************\n");
		exit(0);
	}

	if (ptt && isinf(squelch_db) < 0 && loopback == 0) {
		fprintf(stderr, "*******************************************************************************\n");
		fprintf(stderr, "Cannot use push-to-talk feature without Squelch option.\n");
		fprintf(stderr, "*******************************************************************************\n");
		exit(0);
	}

	/* no squelch in loopback mode */
	if (loopback)
		squelch_db = -INFINITY;

	/* inits */
	fm_init(fast_math);
	dsp_init();
	imts_init();

	/* create transceiver instance */
	for (i = 0; i < num_kanal; i++) {
		rc = imts_create(kanal[i], dsp_device[i], use_sdr, dsp_samplerate, rx_gain, tx_gain, do_pre_emphasis, do_de_emphasis, write_rx_wave, write_tx_wave, read_rx_wave, read_tx_wave, loopback, squelch_db, ptt, fast_seize, mode, operator, detector_test_length_1, detector_test_length_2, detector_test_length_3);
		if (rc < 0) {
			fprintf(stderr, "Failed to create \"Sender\" instance. Quitting!\n");
			goto fail;
		}
		printf("Base station on channel %s ready, please tune transmitter to %.3f MHz and receiver to %.3f MHz. (%.3f MHz offset)\n", kanal[i], imts_channel2freq(kanal[i], 0) / 1e6, imts_channel2freq(kanal[i], 1) / 1e6, imts_channel2freq(kanal[i], 2) / 1e6);
	}

	main_mobile_loop((mode == MODE_IMTS) ? "imts" : "mts", &quit, NULL, station_id);

fail:
	/* destroy transceiver instance */
	while (sender_head)
		imts_destroy(sender_head);

	/* exits */
	fm_exit();

	options_free();

	return 0;
}

