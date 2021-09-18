/* MPT1327 main
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
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "../libsample/sample.h"
#include "../libmobile/main_mobile.h"
#include "../libdebug/debug.h"
#include "../libtimer/timer.h"
#include "../anetz/freiton.h"
#include "../anetz/besetztton.h"
#include "../liboptions/options.h"
#include "mpt1327.h"
#include "dsp.h"
#include "message.h"

/* settings */
int num_freq = 0;
static int num_chan_type = 0;
static double squelch_db = -INFINITY;
static enum mpt1327_band band = BAND_REGIONET43_SUB1;
static enum mpt1327_chan_type chan_type[MAX_SENDER] = { CHAN_TYPE_CC_TC };
static int16_t sys = -1;
static int wt = 10;
static int per = 5;
static int pon = 1;
static int timeout = 30;

void print_image(void) {}

void print_help(const char *arg0)
{
	main_mobile_print_help(arg0, "-O ... | -I ... ");
	/*      -                                                                             - */
	printf(" -B --band <name> | list\n");
	printf("        Select frequency Band (default = '%s')\n", mpt1327_band_name(band));
	printf(" -T --channel-type <channel type> | list\n");
	printf("        Give channel type, use 'list' to get a list. (default = '%s')\n", chan_type_short_name(chan_type[0]));
	printf(" -O --operator <OPID> <NDD> <LAB>\n");
	printf("         -> decimal, '0x' for hex or all binary digits\n");
	printf("        Give System Identity Code of regional network (1st bit = 0)\n");
	printf("        OPID: Operator Identity (7 binary digits)\n");
	printf("	 -> Check subscription data of mobile unit\n");
	printf("	NDD: Network Dependent Data (4 binary digts)\n");
	printf("	 -> Check subscription data of mobile unit (must be '0001' or greater)\n");
	printf("	 -> Change it to force re-registering of mobile unit.\n");
	printf("	LAB: Label for multiple control channels (3 binary digits)\n");
	printf("	 -> Use '001' to allow all categories\n");
	printf(" -N --net <NET> <NDD> <LAB>\n");
	printf("         -> decimal, '0x' for hex or all binary digits\n");
	printf("        Give System Identity Code of national network (1st bit = 1)\n");
	printf("        NET: Network Identity (2 binary digits)\n");
	printf("	 -> Check subscription data of mobile unit (must be '000000001' or greater)\n");
	printf("	 -> Change it to force re-registering of mobile unit.\n");
	printf("	NDD: Network Dependent Data (9 binary digts)\n");
	printf("	LAB: Label for multiple control channels (3 binary digits)\n");
	printf("	 -> Use '001' to allow all categories\n");
	printf(" -S --sysdef wt=5 | wt=10 | wt=15\n");
	printf("        Number of slots the Radio Unit waits for response. A slot lasts about\n");
	printf("        107 ms. (default = %d)\n", wt);
	printf(" -S --sysdef per=<secs> | per=0\n");
	printf("        Interval of periodic messages from the Radio Unit while transmitting\n");
	printf("        speech. Use 1..31 to enable and 0 to disable. Also the 'timeout' value\n");
	printf("        must be greater than value given here. (default = %d)\n", per);
	printf(" -S --sysdef pon=1 | pon=0\n");
	printf("        The Radio Unit must send 'Pressel On' message to unmute the uplink.\n");
	printf("        If disabled, squelch must be enabled. (default = %d)\n", pon);
	printf(" -S --sysdef timeout=<secs> | timeout=off\n");
	printf("        The Traffic Channel is released, if no radio transmits for given amount of time.\n");
	printf("        (default = %d)\n", timeout);
	printf(" -Q --squelch <dB> | auto\n");
	printf("        Use given RF level to detect transmission on Traffic Channel, if\n");
        printf("        'Pressel On' is disabled.\n");
	printf("        and stays below this level, the connection is released.\n");
	printf("        Use 'auto' to do automatic noise floor calibration to detect loss.\n");
	printf("        Only works with SDR! (disabled by default)\n");

	printf("\nstation-id: Give 7 digits of Radio Unit's prefix/ident, you don't need to\n");
	printf("        enter it for every start of this program.\n");
	main_mobile_print_hotkeys();
	printf("Press 'i' key to dump list of seen Radio Units.\n");
}

static void add_options(void)
{
	main_mobile_add_options();
	option_add('B', "band", 1);
	option_add('T', "channel-type", 1);
	option_add('O', "operator", 3);
	option_add('N', "net", 3);
	option_add('S', "sysdef", 1);
	option_add('Q', "squelch", 1);
}

static int read_sys(const char *param, const char *value, int digits)
{
	int result = 0;
	int i;

	if ((int)strlen(value) < digits) {
		result = strtoul(value, NULL, 0);
		if (result >= (1 << digits)) {
			fprintf(stderr, "Given '%s' value is out of range for %d binary digits, use '-h' for help!\n", param, digits);
			return -EINVAL;
		}
		return result;
	}

	if ((int)strlen(value) > digits) {
		fprintf(stderr, "Given '%s' value must have exactly %d binary digits, use '-h' for help!\n", param, digits);
		return -EINVAL;
	}

	for (i = 0; i < (int)strlen(value); i++) {
		if (value[i] < '0' || value[i] > '1') {
			fprintf(stderr, "Given '%s' value must only have binary digits of '0' or '1', use '-h' for help!\n", param);
			return -EINVAL;
		}
		result = (result << 1) | (value[i] - '0');
	}

	return result;
}

static int handle_options(int short_option, int argi, char **argv)
{
	int rc;
	const char *p;

	switch (short_option) {
	case 'B':
		if (!strcmp(argv[argi], "list")) {
			mpt1327_band_list();
			return 0;
		}
		rc = mpt1327_band_by_short_name(argv[argi]);
		if (rc < 0) {
			fprintf(stderr, "Given band '%s' is illegal, use '-h' for help!\n", argv[argi]);
			return -EINVAL;
		}
		band = rc;
		break;
	case 'T':
		if (!strcmp(argv[argi], "list")) {
			mpt1327_channel_list();
			return 0;
		}
		rc = mpt1327_channel_by_short_name(argv[argi]);
		if (rc < 0) {
			fprintf(stderr, "Error, channel type '%s' unknown. Please use '-t list' to get a list. I suggest to use the default.\n", argv[argi]);
			return -EINVAL;
		}
		OPT_ARRAY(num_chan_type, chan_type, rc)
		break;
	case 'O':
		sys = 0x0000;
		rc = read_sys("OID", argv[argi + 0], 7);
		if (rc < 0)
			return rc;
		sys = sys | (rc << 7);
		rc = read_sys("NDD", argv[argi + 1], 4);
		if (rc < 0)
			return rc;
		sys = sys | (rc << 3);
		rc = read_sys("LAB", argv[argi + 2], 3);
		if (rc < 0)
			return rc;
		sys = sys | rc;
		break;
	case 'N':
		sys = 0x4000;
		rc = read_sys("NET", argv[argi + 0], 2);
		if (rc < 0)
			return rc;
		sys = sys | (rc << 12);
		rc = read_sys("NDD", argv[argi + 1], 9);
		if (rc < 0)
			return rc;
		sys = sys | (rc << 3);
		rc = read_sys("LAB", argv[argi + 2], 3);
		if (rc < 0)
			return rc;
		sys = sys | rc;
		break;
	case 'S':
		p = strchr(argv[argi], '=');
		if (!p) {
			fprintf(stderr, "Given sysdef parameter '%s' requires '=' character to set value, use '-h' for help!\n", argv[argi]);
			return -EINVAL;
		}
		p++;
		if (!strncasecmp(argv[argi], "wt=", p - argv[argi])) {
			wt = atoi(p);
			if (wt != 5 && wt != 10 && wt != 15) {
sysdef_oor:
				fprintf(stderr, "Given sysdef parameter '%s' out of range, use '-h' for help!\n", argv[argi]);
				return -EINVAL;
			}
		} else
		if (!strncasecmp(argv[argi], "per=", p - argv[argi])) {
			per = atoi(p);
			if (per < 0 || per >31)
				goto sysdef_oor;
		} else
		if (!strncasecmp(argv[argi], "pon=", p - argv[argi])) {
			pon = atoi(p);
			if (pon != 0 && pon != 1)
				goto sysdef_oor;
		} else
		if (!strncasecmp(argv[argi], "timeout=", p - argv[argi])) {
			timeout = atoi(p);
		} else
		{
			fprintf(stderr, "Given sysdef parameter '%s' unknown, use '-h' for help!\n", argv[argi]);
			return -EINVAL;
		}
		break;
	case 'Q':
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

int main(int argc, char *argv[])
{
	int rc, argi;
	const char *station_id = "";
	int mandatory = 0;
	int i;

	/* init tones */
	init_freiton();
	init_besetzton();
//	init_ansage();

	console_digits = "0123456789*#";
	main_mobile_init();

	/* handle options / config file */
	add_options();
	rc = options_config_file(argc, argv, "~/.osmocom/analog/mpt1327.conf", handle_options);
	if (rc < 0)
		return 0;
	argi = options_command_line(argc, argv, handle_options);
	if (argi <= 0)
		return argi;

	if (argi < argc) {
		station_id = argv[argi];
		if (strlen(station_id) != 7) {
			printf("Given station ID '%s' does not have 4 digits\n", station_id);
			return 0;
		}
	}

	if (!num_kanal) {
		printf("No channel (\"Kanal\") is specified, I suggest channel 1.\n\n");
		mandatory = 1;
	}
	if (use_sdr) {
		/* set device */
		for (i = 0; i < num_kanal; i++)
			dsp_device[i] = "sdr";
		num_device = num_kanal;
		/* set channel types for more than 1 channel */
		if (num_kanal > 1 && num_chan_type == 0) {
			chan_type[0] = CHAN_TYPE_CC;
			for (i = 1; i < num_kanal; i++)
				chan_type[i] = CHAN_TYPE_TC;
			num_chan_type = num_kanal;
		}

	}
	if (num_kanal == 1 && num_device == 0)
		num_device = 1; /* use default */
	if (num_kanal != num_device) {
		fprintf(stderr, "You need to specify as many sound devices as you have channels.\n");
		exit(0);
	}
	if (num_kanal == 1 && num_chan_type == 0)
		num_chan_type = 1; /* use default */
	if (num_kanal != num_chan_type) {
		fprintf(stderr, "You need to specify as many channel types as you have channels.\n");
		exit(0);
	}

	if (sys < 0) {
		fprintf(stderr, "No System Identity Code is specified, make them match with your radio unit.\n\n");
		mandatory = 1;
	}
	if (isinf(squelch_db) && pon == 0) {
		fprintf(stderr, "'Pressel On' message (PON) and squelch are turned off. Enable one of them.\n\n");
		mandatory = 1;
	}
	if (!isinf(squelch_db) && pon == 1) {
		fprintf(stderr, "'Pressel On' message (PON) and squelch are turned on. Disable one of them.\n\n");
		mandatory = 1;
	}
	if (pon && timeout <= per) {
		fprintf(stderr, "The defined timeout value is lower than the Periodic message interval (PER). Define a greater timeout.\n\n");
		mandatory = 1;
	}
	if (pon && (timeout && !per)) {
		fprintf(stderr, "You must enable Periodic message interval (PER), if you use timeout (and have no squelch).\n\n");
		mandatory = 1;
	}
	if (!pon && !timeout) {
		fprintf(stderr, "Warning: 'Pressel On' message (PON) and timeout is both disabled. There will be no way to detect loss of Radio Unit.\n\n");
	}

	if (do_de_emphasis || do_pre_emphasis) {
		printf("Don't use pre-/de-emphasis, it is not used for Speech, nor for signaling.\n\n");
		mandatory = 1;
	}

	if (mandatory) {
		print_help(argv[0]);
		return 0;
	}

	/* no SDR, no squelch */
	if (!use_sdr && !isinf(squelch_db)) {
		fprintf(stderr, "Cannot use squelch without SDR! Analog receivers don't give use RSSI.\n");
		goto fail;
	}

	printf("Using Sysdef 0x%04x:\n", sys);
	if (!(sys & 0x4000)) {
		printf("OID=%d NDD=%d LAB=%d\n", (sys >> 7) & 0x7f, (sys >> 3) & 0xf, sys & 0x7);
	} else {
		printf("NET=%d NDD=%d LAB=%d\n", (sys >> 12) & 0x3, (sys >> 3) & 0x1ff, sys & 0x7);
	}

	/* inits */
	fm_init(fast_math);
	dsp_init();
	init_codeword();
	init_sysdef(sys, wt, per, pon, timeout);

	/* create transceiver instance */
	for (i = 0; i < num_kanal; i++) {
		rc = mpt1327_create(band, kanal[i], chan_type[i], dsp_device[i], use_sdr, dsp_samplerate, rx_gain, tx_gain, write_rx_wave, write_tx_wave, read_rx_wave, read_tx_wave, loopback, squelch_db);
		if (rc < 0) {
			fprintf(stderr, "Failed to create transceiver instance. Quitting!\n");
			goto fail;
		}
		printf("base station on channel %s ready, please tune transmitter to %.4f MHz and receiver to %.4f MHz. (%s %.3f MHz offset)\n", kanal[i], mpt1327_channel2freq(band, atoi(kanal[i]), 0) / 1e6, mpt1327_channel2freq(band, atoi(kanal[i]), 1) / 1e6, mpt1327_band_name(band), mpt1327_channel2freq(band, atoi(kanal[i]), 2) / 1e6);
	}

	mpt1327_check_channels();

	main_mobile("mpt1327", &quit, NULL, station_id, 7);

fail:
	/* destroy transceiver instance */
	while (sender_head)
		mpt1327_destroy(sender_head);

	/* exits */
	fm_exit();
	flush_units();

	options_free();

	return 0;
}

