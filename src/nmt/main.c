/* NMT main
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
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "../libsample/sample.h"
#include "../libmobile/main_mobile.h"
#include "../libdebug/debug.h"
#include "../liboptions/options.h"
#include "nmt.h"
#include "frame.h"
#include "dsp.h"
#include "image.h"
#include "tones.h"
#include "announcement.h"
#include "countries.h"

#define SMS_DELIVER "/tmp/nmt_sms_deliver"
#define SMS_SUBMIT "/tmp/nmt_sms_submit"
static int sms_deliver_fd = -1;

/* settings */
int nmt_system = 450;
int num_chan_type = 0;
enum nmt_chan_type chan_type[MAX_SENDER] = { CHAN_TYPE_CC_TC };
int ms_power = 1; /* 0..3 */
char country[16] = "";
uint8_t traffic_area;
int traffic_area_given = 0;
char area_no = 0;
int compandor = 1;
int num_supervisory = 0;
int supervisory[MAX_SENDER] = { 1 };
const char *smsc_number = "767";
int send_callerid = 0;

void print_help(const char *arg0)
{
	main_mobile_print_help(arg0, "[-N 900] -Y <traffic area> | list  [-I 1] [-0 1] ");
	/*      -                                                                             - */
	printf(" -N --nmt-system 450/900\n");
	printf("        Give NMT type as first paramer. (default = '%d')\n", nmt_system);
	printf("        Note: This option must be given at first!\n");
	printf(" -T --channel-type <channel type> | list\n");
	printf("        Give channel type, use 'list' to get a list. (default = '%s')\n", chan_type_short_name(nmt_system, chan_type[0]));
	printf(" -P --ms-power <power level>\n");
	printf("        Give power level of the mobile station 0..3. (default = '%d')\n", ms_power);
    if (nmt_system == 450) {
	printf("        NMT-450: 3 = 15 W / 7 W (handheld), 2 = 1.5 W, 1/0 = 150 mW\n");
    } else {
	printf("        NMT-900: 3/2 = 6 W, 1 = 1.5 W, 0 = 150 mW\n");
    }
	printf("        Note: Power level < 2 causes mobile to search longer for a cell.\n");
	printf(" -Y --traffic-area <traffic area> | list\n");
	printf("        NOTE: MUST MATCH WITH YOUR ROAMING SETTINGS IN THE PHONE!\n");
	printf("              Your phone will not connect, if country code is different!\n");
	printf("        Give short country code and traffic area seperated by comma.\n");
	printf("        (Example: Give 'SE,1' for Sweden, traffic area 1)\n");
	printf("        Add '!' to force traffic area that is not supported by country.\n");
	printf("        (Example: Give 'B,12!' for Belgium, traffic area 12)\n");
	printf("        Use 'list' to get a list of available country code names\n");
	printf(" -A --area-number <area no> | 0\n");
	printf("        Give area number 1..4 or 0 for no area number. (default = '%d')\n", area_no);
	printf(" -C --compandor 1 | 0\n");
	printf("        Make use of the compandor to reduce noise during call. (default = '%d')\n", compandor);
	printf(" -0 --supervisory 1..4 | 0\n");
	printf("        Use supervisory signal 1..4 to detect loss of signal from mobile\n");
	printf("        station, use 0 to disable. (default = '%d')\n", supervisory[0]);
	printf(" -S --smsc-number <digits>\n");
	printf("        If this number is dialed, the mobile is connected to the SMSC (Short\n");
	printf("        Message Service Center). (default = '%s')\n", smsc_number);
	printf(" -I --caller-id 1 | 0\n");
	printf("        If set, the caller ID is sent while ringing the phone. (default = '%d')\n", send_callerid);
	printf("\nstation-id: Give 7 digits of station-id, you don't need to enter it\n");
	printf("        for every start of this program.\n");
	main_mobile_print_hotkeys();
}

static void add_options(void)
{
	main_mobile_add_options();
	option_add('N', "nmt-system", 1);
	option_add('T', "channel-type", 1);
	option_add('P', "ms-power", 1);
	option_add('Y', "traffic-area", 1);
	option_add('A', "area-number", 1);
	option_add('C', "compandor", 1);
	option_add('0', "supervisory", 1);
	option_add('S', "smsc-number", 1);
	option_add('I', "caller-id", 1);
}

static int handle_options(int short_option, int argi, char **argv)
{
	int rc;
	char *p;
	int super;

	switch (short_option) {
	case 'N':
		nmt_system = atoi(argv[argi]);
		if (nmt_system != 450 && nmt_system != 900) {
			fprintf(stderr, "Error, NMT system type '%s' unknown. Please use '-N 450' for NMT-450 or '-N 900' for NMT-900.\n", argv[argi]);
			return -EINVAL;
		}
		if (nmt_system == 900)
			ms_power = 0;
		if (!option_is_first()) {
			fprintf(stderr, "Please specify the NMT system (-N) as first command line option!\n");
			return -EINVAL;
		}
		break;
	case 'T':
		if (!strcmp(argv[argi], "list")) {
			nmt_channel_list(nmt_system);
			return 0;
		}
		rc = nmt_channel_by_short_name(nmt_system, argv[argi]);
		if (rc < 0) {
			fprintf(stderr, "Error, channel type '%s' unknown. Please use '-t list' to get a list. I suggest to use the default.\n", argv[argi]);
			return -EINVAL;
		}
		OPT_ARRAY(num_chan_type, chan_type, rc)
		break;
	case 'P':
		ms_power = atoi(argv[argi]);
		if (ms_power > 3)
			ms_power = 3;
		if (ms_power < 0)
			ms_power = 0;
		break;
	case 'Y':

		if (!strcmp(argv[argi], "list")) {
			nmt_country_list(nmt_system);
			return 0;
		}
		/* digits */
		strncpy(country, argv[argi], sizeof(country) - 1);
		country[sizeof(country) - 1] = '\0';
		p = strchr(country, ',');
		if (!p) {
			fprintf(stderr, "Illegal traffic area '%s', see '-h' for help\n", argv[argi]);
			return -EINVAL;
		}
		*p++ = '\0';
		rc = nmt_country_by_short_name(nmt_system, country);
		if (rc < 0) {
error_ta:
			fprintf(stderr, "Invalid traffic area '%s', use '-Y list' for a list of valid areas\n", argv[argi]);
			return -EINVAL;
		}
		traffic_area = rc << 4;
		if (p[strlen(p) - 1] != '!') {
			rc = nmt_ta_by_short_name(nmt_system, country, atoi(p));
			if (rc < 0)
				goto error_ta;
		}
		if (atoi(p) < 0 || atoi(p) > 15)
			goto error_ta;
		traffic_area |= (nmt_system == 450) ? nmt_flip_ten(atoi(p)) : atoi(p);
		traffic_area_given = 1;
		break;
	case 'A':
		area_no = argv[argi][0] - '0';
		if (area_no > 4) {
			fprintf(stderr, "Area number '%s' out of range, please use 1..4 or 0 for no area\n", argv[argi]);
			return -EINVAL;
		}
		break;
	case 'C':
		compandor = atoi(argv[argi]);
		break;
	case '0':
		super = atoi(argv[argi]);
		if (super < 0 || super > 4) {
			fprintf(stderr, "Given supervisory signal is wrong, use '-h' for help!\n");
			return -EINVAL;
		}
		OPT_ARRAY(num_supervisory, supervisory, super)
		break;
	case 'S':
		smsc_number = strdup(argv[argi]);
		break;
	case 'I':
		send_callerid = atoi(argv[argi]);
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

	rc = read(sms_deliver_fd, buffer + pos, space);
	if (rc > 0) {
		pos += rc;
		if (pos == space) {
			fprintf(stderr, "SMS buffer overflow!\n");
			pos = 0;
		}
		/* check for end of line */
		for (i = 0; i < pos; i++) {
			if (buffer[i] == '\r' || buffer[i] == '\n')
				break;
		}
		/* send sms */
		if (i < pos) {
			buffer[i] = '\0';
			pos = 0;
			deliver_sms(buffer);
		}
	}
}

int submit_sms(const char *sms)
{
	FILE *fp;

	fp = fopen(SMS_SUBMIT, "a");
	if (!fp) {
		fprintf(stderr, "Failed to open SMS submit file '%s'!\n", SMS_SUBMIT);
		return -1;
	}

	fprintf(fp, "%s\n", sms);

	fclose(fp);

	return 0;
}

int main(int argc, char *argv[])
{
	int rc, argi;
	const char *station_id = "";
	int mandatory = 0;
	int i;

	/* init common tones */
	init_nmt_tones();
	init_announcement();

	main_mobile_init();

	/* handle options / config file */
	add_options();
	rc = options_config_file("~/.osmocom/analog/nmt.conf", handle_options);
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

	if (!num_kanal) {
		printf("No channel (\"Kanal\") is specified, I suggest channel 1 (-k 1).\n\n");
		mandatory = 1;
	}
	if (use_sdr) {
		/* set audiodev */
		for (i = 0; i < num_kanal; i++)
			audiodev[i] = "sdr";
		num_audiodev = num_kanal;
		/* set channel types for more than 1 channel */
		if (num_kanal > 1 && num_chan_type == 0) {
			if (loopback)
				chan_type[0] = CHAN_TYPE_TEST;
			else
				chan_type[0] = CHAN_TYPE_CC;
			for (i = 1; i < num_kanal; i++) {
				if (loopback)
					chan_type[i] = CHAN_TYPE_TEST;
				else
					chan_type[i] = CHAN_TYPE_TC;
			}
			num_chan_type = num_kanal;
		}
		if (num_supervisory == 0) {
			/* set supervisory signal */
			for (i = 0; i < num_kanal; i++)
				supervisory[i] = (i % 4) + 1;
			num_supervisory = num_kanal;
		}
	}
	if (num_kanal == 1 && num_audiodev == 0)
		num_audiodev = 1; /* use default */
	if (num_kanal != num_audiodev) {
		fprintf(stderr, "You need to specify as many sound devices as you have channels.\n");
		return -EINVAL;
	}
	if (num_kanal == 1 && num_chan_type == 0) {
		num_chan_type = 1; /* use default */
		if (loopback)
			chan_type[0] = CHAN_TYPE_TEST;
	}
	if (num_kanal != num_chan_type) {
		fprintf(stderr, "You need to specify as many channel types as you have channels.\n");
		return -EINVAL;
	}
	if (num_kanal == 1 && num_supervisory == 0)
		num_supervisory = 1; /* use default */
	if (num_kanal != num_supervisory) {
		fprintf(stderr, "You need to specify as many supervisory signals as you have channels.\n");
		fprintf(stderr, "They shall be different at channels that are close to each other.\n");
		return -EINVAL;
	}
	if (num_kanal) {
		uint8_t super[5] = { 0, 0, 0, 0, 0 };

		for (i = 0; i < num_kanal; i++) {
			if (supervisory[i] == 0) {
				fprintf(stderr, "No supervisory signal given for channel %d. This is ok, but signal loss dannot be detected. \n", kanal[i]);
				continue;
			}
			if (super[supervisory[i]]) {
				fprintf(stderr, "Supervisory signal %d is selected for both cannels #%d and #%d. I advice to use different signal, to avoid co-channel interferences.\n", supervisory[i], kanal[i], super[supervisory[i]]);
				continue;
			}
			super[supervisory[i]] = kanal[i];
		}
	}

	if (!traffic_area_given) {
		printf("No traffic area is specified, I suggest to use Sweden (-Y SE,1) and set the phone's roaming to 'SE' also.\n\n");
		mandatory = 1;
	}

	if (mandatory) {
		print_help(argv[0]);
		return 0;
	}

	/* create pipe for SMS delivery */
	unlink(SMS_DELIVER);
	rc = mkfifo(SMS_DELIVER, 0666);
	if (rc < 0) {
		fprintf(stderr, "Failed to create SMS deliver FIFO '%s'!\n", SMS_DELIVER);
		goto fail;
	} else {
		sms_deliver_fd = open(SMS_DELIVER, O_RDONLY | O_NONBLOCK);
		if (sms_deliver_fd < 0) {
			fprintf(stderr, "Failed to open SMS deliver FIFO! '%s'\n", SMS_DELIVER);
			goto fail;
		}
	}

	if (!loopback)
		print_image();

	/* inits */
	fm_init(fast_math);
	rc = init_frame();
	if (rc < 0) {
		fprintf(stderr, "Failed to setup frames. Quitting!\n");
		return -1;
	}
	dsp_init();

	/* SDR always requires emphasis */
	if (use_sdr) {
		do_pre_emphasis = 1;
		do_de_emphasis = 1;
	}

	/* create transceiver instance */
	for (i = 0; i < num_kanal; i++) {
		rc = nmt_create(nmt_system, country, kanal[i], chan_type[i], audiodev[i], use_sdr, samplerate, rx_gain, do_pre_emphasis, do_de_emphasis, write_rx_wave, write_tx_wave, read_rx_wave, read_tx_wave, ms_power, traffic_area, area_no, compandor, supervisory[i], smsc_number, send_callerid, loopback);
		if (rc < 0) {
			fprintf(stderr, "Failed to create transceiver instance. Quitting!\n");
			goto fail;
		}
		printf("base station on channel %d ready, please tune transmitter to %.4f MHz and receiver to %.4f MHz. (%.3f MHz offset)\n", kanal[i], nmt_channel2freq(nmt_system, country, kanal[i], 0, NULL, NULL, NULL) / 1e6, nmt_channel2freq(nmt_system, country, kanal[i], 1, NULL, NULL, NULL) / 1e6, nmt_channel2freq(nmt_system, country, kanal[i], 2, NULL, NULL, NULL) / 1e6);
	}

	nmt_check_channels(nmt_system);

	main_mobile(&quit, latency, interval, myhandler, station_id, 7);

fail:
	/* fifo */
	if (sms_deliver_fd > 0)
		close(sms_deliver_fd);
	unlink(SMS_DELIVER);

	/* destroy transceiver instance */
	while (sender_head)
		nmt_destroy(sender_head);

	/* exits */
	fm_exit();

	return 0;
}

