/* Eurosignal main
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
#include "../libsample/sample.h"
#include "../libdebug/debug.h"
#include "../libmobile/call.h"
#include "../libmobile/main_mobile.h"
#include "../liboptions/options.h"
#include "eurosignal.h"
#include "dsp.h"
#include "../anetz/besetztton.h"
#include "es_mitte.h"
#include "es_ges.h"
#include "es_teilges.h"
#include "es_kaudn.h"


static int fm = 0;		/* use fm */
static int tx = 0;		/* we transmit */
static int rx = 0;		/* we receive */
static int repeat = 4;		/* repeat given ID */
static int degraded = 0;	/* if station is degraded */
static int random_id = 0;	/* transmit pseudo random pattern */
static uint32_t scan_from = 0;
static uint32_t scan_to = 0;

void print_help(const char *arg0)
{
	main_mobile_print_help(arg0, "[--random] ");
	/*      -                                                                             - */
	printf(" -F --fm\n");
	printf("        Use frequency modulation instead of amplitude modulation. The carrier\n");
	printf("        frequency will be 7.5 KHz above/below the actual frequency. This\n");
	printf("        ensures that the change in frequency results as a change in amplitude\n");
	printf("        at the demodulator of the receiver.\n");
	printf(" -T --tx\n");
	printf("        Transmit Eurosignal on given channel, to page a receiver. (default)\n");
	printf(" -R --rx\n");
	printf("        Receive Eurosignal on given channel, so we are the receiver.\n");
	printf("        If none of the options -T nor -R is given, only transmitter is enabled.\n");
	printf(" -I --id <id> [-I ...]\n");
	printf("        Give one or more IDs to allow only the given IDs to be transmitted and\n");
	printf("        received. This option can be repeated many times. If this option is not\n");
	printf("        specified, any ID is allowed to transmit.\n");
	printf("        Also, any given ID will cause a beep when received. This requires call\n");
	printf("        device (sound card) to be defined or MNCC interface. (The caller ID will\n");
	printf("        be the received pager ID. The called number will be '1' for the first\n");
	printf("        ID given, '2' for scond, ...)\n");
	printf(" -D --degraded\n");
	printf("        Play the anouncement that the system is degraded due to failure of one\n");
	printf("        or more transmitters. If the caller hangs up during or rigt after the\n");
	printf("        announcement, no paging is performed.\n");
	printf(" -S --scan <from> <to>\n");
	printf("        Scan through given IDs once (no repetition). This can be useful to find\n");
	printf("        the ID of a vintage receiver. Note that scanning all IDs from 000000\n");
	printf("        through 999999 would take almost 10 Days.\n");
	printf("    --random\n");
	printf("        Repeat some pseudo random IDs, to make the transmitted signal sound\n");
	printf("        authentic and as it would handle many calls. It will still be possible\n");
	printf("        to page a recevier.\n");
	printf("    --repeat <num>\n");
	printf("        Repead paging ID <num> times when transmitting. (default = %d)\n", repeat);
	printf("\nstation-id: Give 6 digit station-id, you don't need to enter it for every\n");
	printf("        start of this program.\n");
	main_mobile_print_hotkeys();
}

#define	OPT_RANDOM	255
#define	OPT_REPEAT	258

static void add_options(void)
{
	main_mobile_add_options();
	option_add('F', "fm", 0);
	option_add('T', "tx", 0);
	option_add('R', "rx", 0);
	option_add('I', "id", 1);
	option_add('D', "degraded", 0);
	option_add('S', "scan", 2);
	option_add(OPT_RANDOM, "random", 0);
	option_add(OPT_REPEAT, "repeat", 1);
}

static int check_id(const char *id)
{
	int i;

	if (strlen(id) != 6) {
		fprintf(stderr, "Given paging ID must have exactly 6 digits!\n");
		return -EINVAL;
	}

	for (i = 0; i < 6; i++) {
		if (id[i] < '0' || id[i] > '9') {
			fprintf(stderr, "Given paging ID must have digits (0..9) only!\n");
			return -EINVAL;
		}
	}

	return 0;
}

static int handle_options(int short_option, int argi, char **argv)
{
	switch (short_option) {
	case 'F':
		fm = 1;
		break;
	case 'T':
		tx = 1;
		break;
	case 'R':
		rx = 1;
		break;
	case 'I':
		if (check_id(argv[argi]))
			return -EINVAL;
		euro_add_id(argv[argi]);
		break;
	case 'D':
		degraded = 1;
		break;
	case 'S':
		if (check_id(argv[argi]))
			return -EINVAL;
		scan_from = atoi(argv[argi++]);
		if (check_id(argv[argi]))
			return -EINVAL;
		scan_to = atoi(argv[argi++]) + 1;
		break;
	case OPT_RANDOM:
		random_id = 1;
		break;
	case OPT_REPEAT:
		repeat = atoi(argv[argi]);
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
	int i;

	/* init common tones */
	init_besetzton();

	/* init system specific tones */
	init_es_mitte();
	init_es_ges();
	init_es_teilges();
	init_es_kaudn();

	/* init mobile interface */
	console_digits = "0123456789ABCDE";
	main_mobile_init();

	/* handle options / config file */
	add_options();
	rc = options_config_file("~/.osmocom/analog/eurosignal.conf", handle_options);
	if (rc < 0)
		return 0;
	argi = options_command_line(argc, argv, handle_options);
	if (argi <= 0)
		return argi;

	if (argi < argc) {
		station_id = argv[argi];
		if (strlen(station_id) != 6) {
			printf("Given receiver ID '%s' does not have 6 digits\n", station_id);
			return 0;
		}
	}

	if (!num_kanal) {
		printf("No channel (\"Kanal\") is specified, Use '-k list' to get a list of all channels.\n\n");
		print_help(argv[0]);
		return 0;
	}
	if (!strcasecmp(kanal[0], "list")) {
		euro_list_channels();
		goto fail;
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

	/* inits */
	fm_init(fast_math);
	dsp_init(samplerate);
	euro_init();

	/* TX is default */
	if (!tx && !rx)
		tx = 1;

	/* create transceiver instance */
	for (i = 0; i < num_kanal; i++) {
		rc = euro_create(kanal[i], audiodev[i], use_sdr, samplerate, rx_gain, fm, tx, rx, repeat, degraded, random_id, scan_from, scan_to, write_rx_wave, write_tx_wave, read_rx_wave, read_tx_wave, loopback);
		if (rc < 0) {
			fprintf(stderr, "Failed to create \"Sender\" instance. Quitting!\n");
			goto fail;
		}
		printf("Base station for channel %s ready, please tune transmitter and/or receiver to %.4f MHz\n", kanal[i], euro_kanal2freq(kanal[i], fm) / 1e6);
	}

	main_mobile(&quit, latency, interval, NULL, station_id, 6);

fail:
	/* destroy transceiver instance */
	while(sender_head)
		euro_destroy(sender_head);

	/* exits */
	fm_exit();
	euro_exit();

	return 0;
}

