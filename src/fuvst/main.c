/* FuVSt main
 *
 * (C) 2020 by Andreas Eversberg <jolly@eversberg.eu>
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
#include "../libtimer/timer.h"
#include "../liboptions/options.h"
#include "../libfm/fm.h"
#include "../anetz/freiton.h"
#include "../anetz/besetztton.h"
#include "../cnetz/ansage.h"
#include "fuvst.h"

static int num_chan_type = 0;
static enum fuvst_chan_type chan_type[MAX_SENDER] = { CHAN_TYPE_ZZK };
static uint8_t sio = 0xcd;
static uint16_t uele_pc = 1400;
static uint16_t fuko_pc = 1466;
static int ignore_link_monitor = 0;
static const char *config_name;
static int config_loaded = 0;
double gebuehren = 12.0;
int authentication = 0;
int alarms = 1;
int warmstart = 0;

void print_help(const char *arg0)
{
	main_mobile_print_help(arg0, "");
	/*      -                                                                             - */
	printf(" -T --channel-type ZZK / SpK\n");
	printf("        The channel type to use. One ZZK is default.\n");
	printf(" -S --sio <hex value>\n");
	printf("        Service Indicator Object. Must be 0xcd! (default = 0x%02x).\n", sio);
	printf(" -U --uele <value>\n");
	printf("        Point Code of MSC, which must match the EEPROM configuration data of\n");
	printf("        the base station. (default = %d).\n", uele_pc);
	printf(" -F --fuko <value>\n");
	printf("        Point Code of BS, which must match the EEPROM configuration data of\n");
	printf("        the base station. (default = %d).\n", fuko_pc);
	printf(" -A --authentication 1 | 0\n");
	printf("        Enable or disable authentication procedure. (default = %d).\n", authentication);
	printf(" -E --emergency <prefix>\n");
	printf("        Add given prefix to the list of emergency numbers.\n");
	printf("    --alarms 1 | 0\n");
	printf("        Enable or disable alarm messages from BS to MSC (default = %d).\n", alarms);
	printf(" -G --gebuehren <duration>\n");
	printf("       Give Metering pulse duration is seconds (default = %.2f).\n", gebuehren);
	printf("    --ignore-link-monitor\n");
	printf("        Don't do any link error checking at MTP.\n");
	printf(" -C --bs-config <filename>\n");
	printf("       Give DKO config file (6 KBytes tape file) to be loaded at boot time.\n");
	main_mobile_print_hotkeys();
}

#define OPT_ALARMS		256
#define OPT_IGNORE_LINK_MONITOR	257

static void add_options(void)
{
	main_mobile_add_options();
	option_add('T', "channel-type", 1);
	option_add('S', "sio", 1);
	option_add('U', "uele", 1);
	option_add('F', "fuko", 1);
	option_add('A', "auth", 1);
	option_add('E', "emergency", 1);
	option_add('G', "gebuehren", 1);
	option_add(OPT_ALARMS, "alarms", 1);
	option_add(OPT_IGNORE_LINK_MONITOR, "ignore-link-monitor", 0);
	option_add('C', "bs-config", 1);
}

static int handle_options(int short_option, int argi, char **argv)
{
	int rc;

	switch (short_option) {
	case 'T':
		if (!strcasecmp(argv[argi], "zzk")) {
			OPT_ARRAY(num_chan_type, chan_type, CHAN_TYPE_ZZK);
		} else if (!strcasecmp(argv[argi], "spk")) {
			OPT_ARRAY(num_chan_type, chan_type, CHAN_TYPE_SPK);
		} else {
			fprintf(stderr, "Illegal channel type '%s', see help!\n", argv[argi]);
			return -EINVAL;
		}
		break;
	case 'S':
		sio = strtoul(argv[argi], NULL, 16);
		break;
	case 'U':
		uele_pc = atoi(argv[argi]);
		break;
	case 'F':
		fuko_pc = atoi(argv[argi]);
		break;
	case 'A':
		authentication = atoi(argv[argi]);
		break;
	case 'E':
		add_emergency(argv[argi]);
		break;
	case 'G':
		gebuehren = atof(argv[argi]);
		if (gebuehren < 1.0) {
			fprintf(stderr, "Metering duration too small!\n");
			return -EINVAL;
		}
		if (gebuehren > 2458.0) {
			fprintf(stderr, "Metering duration too large!\n");
			return -EINVAL;
		}
		break;
	case OPT_ALARMS:
		alarms = atoi(argv[argi]);
		break;
	case OPT_IGNORE_LINK_MONITOR:
		ignore_link_monitor = 1;
		break;
	case 'C':
		rc = config_file(config_name = argv[argi]);
		if (rc < 0)
			return rc;
		config_loaded = 1;
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
	int i = 0;
	int any_zzk = 0, any_spk = 0;

	/* init system specific tones */
        init_freiton();
        init_besetzton();
        init_ansage();

	allow_sdr = 0;
	uses_emphasis = 0;
	check_channel = 0;
	main_mobile_init();

	config_init();

	add_emergency("110");
	add_emergency("112");
	add_emergency("*110");
	add_emergency("*112");

	/* handle options / config file */
	add_options();
	rc = options_config_file(argc, argv, "~/.osmocom/cnetz/fuvst.conf", handle_options);
	if (rc < 0)
		return 0;
	argi = options_command_line(argc, argv, handle_options);
	if (argi <= 0)
		return argi;

	if (argi < argc) {
		station_id = argv[argi];
		if (strlen(station_id) != 7) {
			printf("Given station ID '%s' does not have 7 digits\n", station_id);
			return 0;
		}
	}

	if (num_kanal == 1 && num_device == 0)
		num_device = 1; /* use default */
	if (num_kanal != num_device) {
		fprintf(stderr, "You need to specify as many sound devices as you have channels.\n");
		return -EINVAL;
	}
	if (num_kanal == 1 && num_chan_type == 0)
		num_chan_type = 1; /* use default */
	if (num_kanal != num_chan_type) {
		fprintf(stderr, "You need to specify as many channel types as you have channels.\n");
		return -EINVAL;
	}
	for (i = 0; i < num_kanal; i++) {
		if (chan_type[i] == CHAN_TYPE_ZZK) {
			if (!!strcmp(kanal[i], "1") && !!strcmp(kanal[i], "2")) {
				fprintf(stderr, "A ZZK must have a channel 1 or 2.\n");
				return -EINVAL;
			}
			any_zzk = 1;
		}
		if (chan_type[i] == CHAN_TYPE_SPK)
			any_spk = 1;
	}
	if (!any_zzk) {
		fprintf(stderr, "You need to specify at least one Control Channel (ZZK).\n");
		return -EINVAL;
	}
	if (!any_spk) {
		fprintf(stderr, "You need to specify at least one Speech Channel (SpK)\n");
		fprintf(stderr, "in order to make or receive any call.\n");
	}

	/* inits */
	fm_init(fast_math);

	for (i = 0; i < num_kanal; i++) {
		rc = fuvst_create(kanal[i], chan_type[i], dsp_device[i], dsp_samplerate, rx_gain, tx_gain, write_rx_wave, write_tx_wave, read_rx_wave, read_tx_wave, loopback, ignore_link_monitor, sio, uele_pc, fuko_pc);
		if (rc < 0) {
			fprintf(stderr, "Failed to create \"Kanal\" instance. Quitting!\n");
			goto fail;
		}
	}

#if 0
	Does not work! Reset works, but not config is loaded from BS
	if (config_loaded) {
		char buffer[10];

		printf("*******************************************************************************\n");
		printf("You have selected a BS config file. The BS loads this config after cold start.\n");
		printf("At warm start or reset it doesn't. Press 'y' to force a warm start and load the\n");
		printf("config, otherwise press 'n'.\n\n");
		printf("*******************************************************************************\n");
		do {
			printf("Enter 'y' or 'n': "); fflush(stdout);
			if (fgets(buffer, sizeof(buffer), stdin)) 
				buffer[sizeof(buffer) - 1] = '\0';
		} while (buffer[0] != 'y' && buffer[0] != 'n');
		if (buffer[0] == 'y')
			warmstart = 1;
	}
#endif

	printf("\n");
	printf("FUVST ready.\n");
	for (i = 0; i < num_kanal; i++) {
		if (chan_type[i] == CHAN_TYPE_ZZK)
			printf("Using Signaling Channel: ZZK-%s\n", kanal[i]);
		if (chan_type[i] == CHAN_TYPE_SPK)
			printf("Using Speech Channel:    SPK-%s\n", kanal[i]);
	}
	printf("Point codes: BS=%d (FUKO/FUFST) MSC=%d (FUVST)\n", fuko_pc, uele_pc);
	if (config_loaded)
		printf("BS-Config: %s\n", config_name);

	main_mobile("fuvst", &quit, NULL, station_id, 7);
fail:

	/* destroy transceiver instance */
	while (sender_head)
		fuvst_destroy(sender_head);

	/* exits */
//	zeit_exit();
	fm_exit();

	options_free();

	return 0;
}

