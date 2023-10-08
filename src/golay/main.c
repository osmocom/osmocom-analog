/* Golay/GSC pager main
 *
 * (C) 2022 by Andreas Eversberg <jolly@eversberg.eu>
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
#include "../libdebug/debug.h"
#include "../libmobile/call.h"
#include "../libmobile/main_mobile.h"
#include "../liboptions/options.h"
#include "../libfm/fm.h"
#include "../amps/tones.h"
#include "../amps/noanswer.h"
#include "../amps/outoforder.h"
#include "../amps/invalidnumber.h"
#include "../amps/congestion.h"
#include "golay.h"

#define MSG_SEND "/tmp/golay_msg_send"
#define MSG_RECEIVED "/tmp/golay_msg_received"
static int msg_send_fd = -1;

static int tx = 0;		/* we transmit */
static int rx = 0;		/* we receive */
static double deviation = 4500;	/* WB confirmed by an email: POCSAG and GSC have same deviation of +-4.5 kHz. */
static int deviation_given = 0;
static double polarity = 1;
static int polarity_given = 0;
static const char *message = "1234";

void print_help(const char *arg0)
{
	main_mobile_print_help(arg0, "| -k 462.900 | -k <MHz> ");
	/*      -                                                                             - */
	printf(" -T --tx\n");
	printf("        Transmit GSC signal on given channel, to page a receiver. (default)\n");
	printf(" -R --rx\n");
	printf("        Receive GSC signal on given channel, so we are the receiver.\n");
	printf("        If none of the options -T nor -R is given, only transmitter is enabled.\n");
	printf(" -D --deviation wide | 4.5 | narrow | 1.0 | <other KHz>\n"); /* NB confirmed by IQ data from signal-id-wiki */
	printf("        Choose deviation of FFSK signal (default %.0f KHz).\n", deviation / 1000.0);
	printf(" -P --polarity -1 | nagative | 1 | positive\n");
	printf("        Choose polarity of FFSK signal. 'positive' means that a binary 1 uses\n");
	printf("        positive and a binary 0 negative deviation. (default %s KHz).\n", (polarity < 0) ? "negative" : "positive");
	printf(" -M --message \"...\"\n");
	printf("        Send this message, if no caller ID was given or if built-in console\n");
	printf("        is used. (default \"%s\").\n", message);
	printf("\n");
	printf("File: %s\n", MSG_SEND);
	printf("        Write \"<address>[,message]\" to it, to send a default message.\n");
	printf("        Write \"<address>,n,message\" to it, to send a numeric message.\n");
	printf("        Write \"<address>,a,message\" to it, to send an alphanumeric message.\n");
	printf("        Write \"<address>,v,<wave file name>\" to it, to send a voice message.\n");
	printf("\n");
	printf("By default, an alphanumic message is sent, if last digit of the functional\n");
	printf("address is 5..8. Otherwise a tone only message is sent.\n");
	printf("\n");
	printf("A numeric message can have up to 24 digits, they are: 0123456789U-* and space\n");
	printf("Also 'shifted' digits can be sent using two digits, they are: ABCDEFGHJLNPR\n");
	printf("\n");
	printf("An aplhanumeric message can have up to 80 digits, sent upper case only.\n");
	main_mobile_print_station_id();
	main_mobile_print_hotkeys();
}

static void add_options(void)
{
	main_mobile_add_options();
	option_add('T', "tx", 0);
	option_add('R', "rx", 0);
	option_add('D', "deviation", 1);
	option_add('P', "polarity", 1);
	option_add('M', "message", 1);
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
		if (argv[argi][0] == 'n' || argv[argi][0] == 'N')
			deviation = 1000.0;
		else if (argv[argi][0] == 'w' || argv[argi][0] == 'W')
			deviation = 4500.0;
		else
			deviation = atof(argv[argi]) * 1000.0;
		if (deviation < 1000.0) {
			fprintf(stderr, "Given deviation is too low, use higher deviation.\n");
			return -EINVAL;
		}
		if (deviation > 10000.0) {
			fprintf(stderr, "Given deviation is too high, use lower deviation.\n");
			return -EINVAL;
		}
		deviation_given = 1;
		break;
	case 'P':
		if (argv[argi][0] == 'n' || argv[argi][0] == 'N')
			polarity = -1.0;
		else if (argv[argi][0] == 'p' || argv[argi][0] == 'P')
			polarity = 1.0;
		else if (atoi(argv[argi]) == -1)
			polarity = -1.0;
		else if (atoi(argv[argi]) == 1)
			polarity = 1.0;
		else {
			fprintf(stderr, "Given polarity is not positive nor negative, use '-h' for help.\n");
			return -EINVAL;
		}
		polarity_given = 1;
		break;
	case 'M':
		message = options_strdup(argv[argi++]);
		break;
	default:
		return main_mobile_handle_options(short_option, argi, argv);
	}

	return 1;
}

static void myhandler(void)
{
	static char buffer[256];
	static int pos = 0, rc, i;
	int space = sizeof(buffer) - pos;

	rc = read(msg_send_fd, buffer + pos, space);
	if (rc > 0) {
		pos += rc;
		if (pos == space) {
			fprintf(stderr, "Message buffer overflow!\n");
			pos = 0;
		}
		/* check for end of line */
		for (i = 0; i < pos; i++) {
			if (buffer[i] == '\r' || buffer[i] == '\n')
				break;
		}
		/* send msg */
		if (i < pos) {
			buffer[i] = '\0';
			pos = 0;
			if (tx)
				golay_msg_send(buffer);
			else
				PDEBUG(DGOLAY, DEBUG_ERROR, "Failed to send message, transmitter is not enabled!\n");
		}
	}
}

static const struct number_lengths number_lengths[] = {
	{ 7, "functional address" },
	{ 0, NULL }
};

int main(int argc, char *argv[])
{
	int rc, argi;
	const char *station_id = "";
	int i;
	double frequency;

	/* GSC does not use emphasis, so disable it */
	uses_emphasis = 0;

	/* init common tones */
	init_tones();
	init_outoforder();
	init_noanswer();
	init_invalidnumber();
	init_congestion();

	/* init coding tables */
	init_golay();
	init_bch();

	/* init mobile interface */
	main_mobile_init("0123456789", number_lengths, NULL, NULL);

	/* handle options / config file */
	add_options();
	rc = options_config_file(argc, argv, "~/.osmocom/analog/golay.conf", handle_options);
	if (rc < 0)
		return 0;
	argi = options_command_line(argc, argv, handle_options);
	if (argi <= 0)
		return argi;

	if (argi < argc) {
		station_id = argv[argi];
		rc = main_mobile_number_ask(station_id, "functional address");
		if (rc)
			return rc;
	}

	if (!num_kanal) {
		printf("No channel is specified, Use '-k <MHz>' to define frequency.\n\n");
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
		goto fail;
	}

	/* TX is default */
	if (!tx && !rx)
		tx = 1;

	if (rx) {
		fprintf(stderr, "Sorry, but RX is not yet supported and maybe never will.\n");
		goto fail;
	}

	/* TX & RX if loopback */
	if (loopback)
		tx = rx = 1;

	/* create pipe for message sendy */
	unlink(MSG_SEND);
	rc = mkfifo(MSG_SEND, 0666);
	if (rc < 0) {
		fprintf(stderr, "Failed to create mwaaage send FIFO '%s'!\n", MSG_SEND);
		goto fail;
	} else {
		msg_send_fd = open(MSG_SEND, O_RDONLY | O_NONBLOCK);
		if (msg_send_fd < 0) {
			fprintf(stderr, "Failed to open mwaaage send FIFO! '%s'\n", MSG_SEND);
			goto fail;
		}
	}

	/* inits */
	fm_init(fast_math);

	/* create transceiver instance */
	for (i = 0; i < num_kanal; i++) {
		frequency = atof(kanal[i]) * 1e6;
		rc = golay_create(kanal[i], frequency, dsp_device[i], use_sdr, dsp_samplerate, rx_gain, tx_gain, deviation, polarity, message, write_rx_wave, write_tx_wave, read_rx_wave, read_tx_wave, loopback);
		if (rc < 0) {
			fprintf(stderr, "Failed to create \"Sender\" instance. Quitting!\n");
			goto fail;
		}
		printf("Base station ready, please tune transmitter (or receiver) to %.4f MHz\n", frequency / 1e6);
	}

	main_mobile_loop("golay", &quit, myhandler, station_id);

fail:
	/* pipe */
	if (msg_send_fd > 0)
		close(msg_send_fd);
	unlink(MSG_SEND);

	/* destroy transceiver instance */
	while(sender_head)
		golay_destroy(sender_head);

	/* exits */
	fm_exit();

	options_free();

	return 0;
}

