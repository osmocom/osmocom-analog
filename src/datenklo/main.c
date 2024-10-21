/* osmo datenklo main file
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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include "../libsample/sample.h"
#include "../libmobile/sender.h"
#include <osmocom/core/timer.h>
#include <osmocom/cc/misc.h>
#include "../liboptions/options.h"
#include "../liblogging/logging.h"
#include "../libfsk/fsk.h"
#include "am791x.h"
#include "uart.h"
#include "datenklo.h"

#define MAX_DEVICES 2

#define OPT_ARRAY(num_name, name, value) \
{ \
	if (num_name == MAX_DEVICES) { \
		fprintf(stderr, "Too many devices defined!\n"); \
		exit(0); \
	} \
	name[num_name++] = value; \
}

/* dummy functions */
int num_kanal = 1; /* only one channel used for debugging */
sender_t *get_sender_by_empfangsfrequenz(double __attribute__((unused)) freq) { return NULL; }

static datenklo_t datenklo[MAX_DEVICES];
static enum am791x_type am791x_type = AM791X_TYPE_7911;
static int num_mc = 0;
static uint8_t mc[MAX_DEVICES] = { 0 };
static int auto_rts = 0;
static int num_tx_baudrate = 0, num_rx_baudrate = 0;
static int tx_baudrate[MAX_DEVICES] = { 0 }, rx_baudrate[MAX_DEVICES] = { 0 };
static int num_ttydev = 0;
static const char *ttydev[MAX_DEVICES] = { "/dev/ttyDATENKLO0" };
static const char *audiodev = "hw:0,0";
static int dsp_samplerate = 48000;
static int dsp_buffer = 50;
static int stereo = 0;
static int loopback = 0;
static int fast_math = 0;
const char *write_tx_wave = NULL;
const char *write_rx_wave = NULL;
const char *read_tx_wave = NULL;
const char *read_rx_wave = NULL;

static void print_help(const char *arg0)
{
	printf("Usage: %s [options] -M <mode>\n\n", arg0);
	/*      -                                                                             - */
	printf(" -h --help\n");
	printf("        This help\n");
	printf(" --config [~/]<path to config file>\n");
	printf("        Give a config file to use. If it starts with '~/', path is at home dir.\n");
	printf("        Each line in config file is one option, '-' or '--' must not be given!\n");
	logging_print_help();
	printf(" -T --am791x-type 7910 | 7911\n");
	printf("        Give modem chip type. (Default = 791%d)\n", am791x_type);
	printf(" -M --mc <mode>\n");
	printf("        Give mode setting of AM7910/AM71911, use 'list' to list all modes.\n");
	printf(" -A --auto-rts\n");
	printf("        Automatically rais and drop modem's RTS line for half duplex operation.\n");
	printf("        TX data will be queued while remote carrier is detected.\n");
	printf(" -B --baudrate <RX rate> <TX rate>\n");
	printf("        Given baud rate will override the baud rate given by ioctl.\n");
	printf("        A baud rate of <= 150 Bps for TX and/or RX will attach TX and/or RX to\n");
	printf("        the back channel instead of the main channel.\n");
	printf(" -D --device\n");
	printf("        Device name. The prefix '/dev/ is automatically added, if not given.\n");
	printf("        (default = '%s')\n", ttydev[0]);
	printf(" -S --stereo\n");
	printf("        Generate two devices. One device connects to the left and the other to\n");
	printf("        the right channel of the audio device. The device number in the device\n");
	printf("        name is automatically increased by one. You must also define the mode\n");
	printf("        twice. (-M <mode> -M <mode>)\n");
	printf(" -a --audio-device hw:<card>,<device>\n");
	printf("        Sound card and device number (default = '%s')\n", audiodev);
	printf(" -s --samplerate <rate>\n");
	printf("        Sample rate of sound device (default = '%d')\n", dsp_samplerate);
	printf(" -b --buffer <ms>\n");
	printf("        How many milliseconds are processed in advance (default = '%d')\n", dsp_buffer);
	printf(" -l --loopback <type>\n");
	printf("        Perform audio loopback to test modem.\n");
	printf("        type 1: Audio from transmitter is fed into receiver (analog loopback)\n");
	printf("        type 2: Audio is crossed between two modem instances. (use with -S)\n");
	printf("    --fast-math\n");
	printf("        Use fast math approximation for slow CPU / ARM based systems.\n");
        printf("    --write-rx-wave <file>\n");
        printf("        Write received audio to given wave file.\n");
        printf("    --write-tx-wave <file>\n");
        printf("        Write transmitted audio to given wave file.\n");
        printf("    --read-rx-wave <file>\n");
        printf("        Replace received audio by given wave file.\n");
        printf("    --read-tx-wave <file>\n");
        printf("        Replace transmitted audio by given wave file.\n");
}

#define	OPT_WRITE_RX_WAVE	1001
#define	OPT_WRITE_TX_WAVE	1002
#define	OPT_READ_RX_WAVE	1003
#define	OPT_READ_TX_WAVE	1004
#define	OPT_MNCC_NAME		1006
#define	OPT_FAST_MATH		1007

static void add_options(void)
{
	option_add('h', "help", 0);
	option_add('v', "debug", 1);
	option_add('T', "am791x_type", 1);
	option_add('M', "mc", 1);
	option_add('A', "auto-rts", 0);
	option_add('B', "baudrate", 2);
	option_add('D', "device", 1);
	option_add('S', "stereo", 0);
	option_add('a', "audio-device", 1);
	option_add('s', "samplerate", 1);
	option_add('b', "buffer", 1);
	option_add('l', "loopback", 1);
	option_add(OPT_WRITE_RX_WAVE, "write-rx-wave", 1);
	option_add(OPT_WRITE_TX_WAVE, "write-tx-wave", 1);
	option_add(OPT_READ_RX_WAVE, "read-rx-wave", 1);
	option_add(OPT_READ_TX_WAVE, "read-tx-wave", 1);
	option_add(OPT_FAST_MATH, "fast-math", 0);
}

static int handle_options(int short_option, int argi, char **argv)
{
	int rc;

	switch (short_option) {
	case 'h':
		print_help(argv[0]);
		return 0;
	case 'v':
		rc = parse_logging_opt(argv[argi]);
		if (rc > 0)
			return 0;
		if (rc < 0) {
			fprintf(stderr, "Failed to parse logging option, please use -h for help.\n");
			return rc;
		}
		break;
	case 'T':
		am791x_type = atoi(argv[argi]);
		if (am791x_type < 0 || am791x_type > 1) {
			fprintf(stderr, "Given type parameter '%s' is invalid, use '-h' for help!\n", argv[argi]);
			return -EINVAL;
		}
		break;
	case 'M':
		if (!strcasecmp(argv[argi], "list")) {
			am791x_list_mc(am791x_type);
			return 0;
		} else
		if (argv[argi][0] >= '0' && argv[argi][0] <= '9') {
			OPT_ARRAY(num_mc, mc, atoi(argv[argi]))
			if (mc[num_mc - 1] > 31)
				goto mc_inval;
		} else
		{
			mc_inval:
			fprintf(stderr, "Given mode parameter '%s' is invalid, use '-h' for help!\n", argv[argi]);
			return -EINVAL;
		}
		break;
	case 'A':
		auto_rts = 1;
		break;
	case 'B':
		OPT_ARRAY(num_rx_baudrate, rx_baudrate, atoi(argv[argi]))
		OPT_ARRAY(num_tx_baudrate, tx_baudrate, atoi(argv[argi + 1]))
		break;
	case 'D':
		OPT_ARRAY(num_ttydev, ttydev, options_strdup(argv[argi]))
		break;
	case 'S':
		stereo = 1;
		break;
	case 'a':
		audiodev = options_strdup(argv[argi]);
		break;
	case 's':
		dsp_samplerate = atoi(argv[argi]);
		break;
	case 'b':
		dsp_buffer = atoi(argv[argi]);
		break;
	case 'l':
		loopback = atoi(argv[argi]);
		break;
	case OPT_FAST_MATH:
		fast_math = 1;
		break;
	case OPT_WRITE_RX_WAVE:
		write_rx_wave = options_strdup(argv[argi]);
		break;
	case OPT_WRITE_TX_WAVE:
		write_tx_wave = options_strdup(argv[argi]);
		break;
	case OPT_READ_RX_WAVE:
		read_rx_wave = options_strdup(argv[argi]);
		break;
	case OPT_READ_TX_WAVE:
		read_tx_wave = options_strdup(argv[argi]);
		break;
	}

	return 1;
}

static const char *inc_dev_name(const char *dev_name)
{
	char *new_name, *number;
	int integer;

	/* clone */
	new_name = malloc(256);
	strcpy(new_name, dev_name);

	/* find number and remove, if any */
	number = new_name;
	while(*number < '0' || *number > '9')
		number++;
	if (!(*number))
		integer = 2;
	else
		integer = atoi(number) + 1;

	/* change number */
	sprintf(number, "%d", integer);

	return new_name;
}

int main(int argc, char *argv[])
{
	int rc, argi;
	int i;

	logging_init();

	/* handle options / config file */
	add_options();
	rc = options_config_file(argc, argv, "~/.osmocom/analog/datenklo.conf", handle_options);
	if (rc < 0)
		return 0;
	argi = options_command_line(argc, argv, handle_options);
	if (argi <= 0)
		return argi;

	/* inits */
	datenklo_init_global();
	fm_init(fast_math);

	if (stereo) {
		num_kanal = 2;
	}
	if (num_mc == 0) {
		fprintf(stderr, "You need to set the mode of the modem chip. See '--help'.\n");
		exit(0);
	}
	if (num_mc < num_kanal) {
		fprintf(stderr, "You need to specify as many mode settings as you have channels.\n");
		exit(0);
	}

	/* create modem instance */
	for (i = 0; i < num_kanal; i++) {
		/* remove /dev/ */
		if (ttydev[i] && !strncmp(ttydev[i], "/dev/", 5))
			ttydev[i] += 5;
		/* increment last name */
		if (i && ttydev[i] == NULL)
			ttydev[i] = inc_dev_name(ttydev[i - 1]);
		rc = datenklo_init(&datenklo[i], ttydev[i], am791x_type, mc[i], auto_rts, tx_baudrate[i], rx_baudrate[i], dsp_samplerate, loopback);
		if (rc < 0) {
			fprintf(stderr, "Failed to create \"Datenklo\" instance. Quitting!\n");
			goto fail;
		}
		if (i)
			datenklo[i - 1].slave = &datenklo[i];
		printf("Datenklo on device '/dev/%s' ready. (using sound device '%s')\n", ttydev[i], audiodev);
	}

	rc = datenklo_open_audio(&datenklo[0], audiodev, dsp_buffer, write_rx_wave, write_tx_wave, read_rx_wave, read_tx_wave);
	if (rc < 0) {
		fprintf(stderr, "Failed to initialize audio. Quitting!\n");
		goto fail;
	}

	datenklo_main(&datenklo[0], loopback);

fail:
	for (i = 0; i < num_kanal; i++)
		datenklo_exit(&datenklo[i]);

	options_free();

	return 0;
}

void osmo_cc_set_log_cat(int __attribute__((unused)) cc_log_cat) {}

