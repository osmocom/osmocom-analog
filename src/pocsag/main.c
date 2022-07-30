/* POCSAG (Radio-Paging Code #1) main
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
#include "../libdebug/debug.h"
#include "../libmobile/call.h"
#include "../libmobile/main_mobile.h"
#include "../liboptions/options.h"
#include "../libfm/fm.h"
#include "../anetz/besetztton.h"
#include "pocsag.h"
#include "dsp.h"

#define MSG_SEND "/tmp/pocsag_msg_send"
#define MSG_RECEIVED "/tmp/pocsag_msg_received"
static int msg_send_fd = -1;

static int tx = 0;		/* we transmit */
static int rx = 0;		/* we receive */
static int baudrate = 1200;
static int baudrate_given = 0;
static double deviation = 4500;
static int deviation_given = 0;
static double polarity = -1;
static int polarity_given = 0;
static enum pocsag_function function = POCSAG_FUNCTION_NUMERIC;
static const char *message = "1234";
static enum pocsag_language language = LANGUAGE_DEFAULT;
static uint32_t scan_from = 0;
static uint32_t scan_to = 0;

void print_help(const char *arg0)
{
	main_mobile_print_help(arg0, "-k 466.230 | -k list");
	/*      -                                                                             - */
	printf(" -T --tx\n");
	printf("        Transmit POCSAG signal on given channel, to page a receiver. (default)\n");
	printf(" -R --rx\n");
	printf("        Receive POCSAG signal on given channel, so we are the receiver.\n");
	printf("        If none of the options -T nor -R is given, only transmitter is enabled.\n");
	printf(" -B --baud-rate 512 | 1200 | 2400\n");
	printf("        Choose baud rate of transmitter.\n");
	printf(" -D --deviation wide | 4.5 | narrow | 2.5 | <other KHz>\n");
	printf("        Choose deviation of FFSK signal (default %.0f KHz).\n", deviation / 1000.0);
	printf(" -P --polarity -1 | nagative | 1 | positive\n");
	printf("        Choose polarity of FFSK signal. 'negative' means that a binary 0 uses\n");
	printf("        positive and a binary 1 negative deviation. (default %s KHz).\n", (polarity < 0) ? "negative" : "positive");
	printf(" -F --function 0..3 | A..D | numeric | beep1 | beep2 | alphanumeric\n");
	printf("        Choose default function when 7 digit only number is dialed.\n");
	printf("        (default %d = %s)\n", function, pocsag_function_name[function]);
	printf(" -M --message \"...\"\n");
	printf("        Send this message, if no caller ID was given or of built-in console\n");
	printf("        is used. (default \"%s\").\n", message);
	printf(" -L --language\n");
	printf("        Translate German spcial characters from/to UTF-8.\n");
	printf(" -S --scan <from> <to>\n");
	printf("        Scan through given IDs once (no repetition). This can be useful to find\n");
	printf("        the RIC of a vintage pager. Note that scanning all RICs from 0 through\n");
	printf("        2097151 would take about 16.5 Hours at 1200 Baud and known sub RIC.\n");
	printf("        Use -F to select function of the pager. Short messages with 5 numeric\n");
	printf("        or 2 alphanumeric characters are sent without increase in scanning\n");
	printf("        time. The upper 5 digits of the RIC are sent as message, if numeric\n");
	printf("        function was selected. The upper 3 digits of the RIC are sent as\n");
	printf("        message (2 digits hexadecimal), if alphanumeric function was selected.\n");
	printf("\n");
	printf("File: %s\n", MSG_SEND);
	printf("        Write \"<ric>,0,message\" to it to send a numerical message.\n");
	printf("        Write \"<ric>,3,message\" to it to send an alphanumerical message.\n");
	printf("File: %s\n", MSG_RECEIVED);
	printf("        Read from it to see received messages.\n");
	main_mobile_print_station_id();
	main_mobile_print_hotkeys();
}

static void add_options(void)
{
	main_mobile_add_options();
	option_add('T', "tx", 0);
	option_add('R', "rx", 0);
	option_add('B', "baud-rate", 1);
	option_add('D', "deviation", 1);
	option_add('F', "function", 1);
	option_add('P', "polarity", 1);
	option_add('M', "message", 1);
	option_add('L', "language", 0);
	option_add('S', "scan", 2);
}

static int handle_options(int short_option, int argi, char **argv)
{
	int rc;

	switch (short_option) {
	case 'T':
		tx = 1;
		break;
	case 'R':
		rx = 1;
		break;
	case 'B':
		baudrate = atoi(argv[argi]);
		if (baudrate != 512 && baudrate != 1200 && baudrate != 2400) {
			fprintf(stderr, "Given baud-rate is not 512, 1200 nor 2400, use '-h' for help.\n");
			return -EINVAL;
		}
		baudrate_given = 1;
		break;
	case 'D':
		if (argv[argi][0] == 'n' || argv[argi][0] == 'N')
			deviation = 2500.0;
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
	case 'F':
		rc = pocsag_function_name2value(argv[argi]);
		if (rc < 0) {
			fprintf(stderr, "Given function is invalid, use '-h' for help.\n");
			return rc;
		}
		function = rc;
		break;
	case 'M':
		message = options_strdup(argv[argi++]);
		break;
	case 'L':
		language = LANGUAGE_GERMAN;
		break;
	case 'S':
		scan_from = atoi(argv[argi++]);
		if (scan_from > 2097151) {
			fprintf(stderr, "Given RIC to scan from is out of range!\n");
			return -EINVAL;
		}
		scan_to = atoi(argv[argi++]) + 1;
		if (scan_to > 2097151 + 1) {
			fprintf(stderr, "Given RIC to scan to is out of range!\n");
			return -EINVAL;
		}
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
				pocsag_msg_send(language, buffer);
			else
				PDEBUG(DPOCSAG, DEBUG_ERROR, "Failed to send message, transmitter is not enabled!\n");
		}
	}
}

int msg_receive(const char *text)
{
	FILE *fp;

	fp = fopen(MSG_RECEIVED, "a");
	if (!fp) {
		fprintf(stderr, "Failed to open MSG receive file '%s'!\n", MSG_RECEIVED);
		return -1;
	}

	fprintf(fp, "%s\n", text);

	fclose(fp);

	return 0;
}

static const struct number_lengths number_lengths[] = {
	{ 7, "RIC with default function" },
	{ 8, "RIC with function (append 0..3)" },
	{ 0, NULL }
};

int main(int argc, char *argv[])
{
	int rc, argi;
	const char *station_id = "";
	int i;
	double frequency;

	/* pocsag does not use emphasis, so disable it */
	uses_emphasis = 0;

	/* init common tones */
	init_besetzton();

	/* init mobile interface */
	main_mobile_init("0123456789", number_lengths, NULL, pocsag_number_valid);

	/* handle options / config file */
	add_options();
	rc = options_config_file(argc, argv, "~/.osmocom/analog/pocsag.conf", handle_options);
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
	if (!strcasecmp(kanal[0], "list")) {
		pocsag_list_channels();
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
		goto fail;
	}

	/* TX is default */
	if (!tx && !rx)
		tx = 1;

	/* TX & RX if loopback */
	if (loopback)
		tx = rx = 1;

	/* no TX, no scanning */
	if (!tx && scan_to > scan_from) {
		fprintf(stderr, "You need to enable TX, in order to scan.\n");
		goto fail;
	}

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
	pocsag_init();

	/* create transceiver instance */
	for (i = 0; i < num_kanal; i++) {
		frequency = pocsag_channel2freq(kanal[i], (deviation_given) ? NULL : &deviation, (polarity_given) ? NULL : &polarity, (baudrate_given) ? NULL : &baudrate);
		if (frequency == 0.0) {
			printf("Invalid channel '%s', Use '-k list' to get a list of all channels.\n\n", kanal[i]);
			goto fail;
		}
		rc = pocsag_create(kanal[i], frequency, dsp_device[i], use_sdr, dsp_samplerate, rx_gain, tx_gain, tx, rx, language, baudrate, deviation, polarity, function, message, scan_from, scan_to, write_rx_wave, write_tx_wave, read_rx_wave, read_tx_wave, loopback);
		if (rc < 0) {
			fprintf(stderr, "Failed to create \"Sender\" instance. Quitting!\n");
			goto fail;
		}
		printf("Base station ready, please tune transmitter (or receiver) to %.4f MHz\n", frequency / 1e6);
	}

	main_mobile_loop("pocsag", &quit, myhandler, station_id);

fail:
	/* pipe */
	if (msg_send_fd > 0)
		close(msg_send_fd);
	unlink(MSG_SEND);

	/* destroy transceiver instance */
	while(sender_head)
		pocsag_destroy(sender_head);

	/* exits */
	fm_exit();
	pocsag_exit();

	options_free();

	return 0;
}

