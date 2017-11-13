/* C-Netz main
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
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include "../common/sample.h"
#include "../common/main_mobile.h"
#include "../common/debug.h"
#include "../common/call.h"
#include "../common/mncc_sock.h"
#include "../common/freiton.h"
#include "../common/besetztton.h"
#include "cnetz.h"
#include "database.h"
#include "sysinfo.h"
#include "dsp.h"
#include "telegramm.h"
#include "image.h"
#include "ansage.h"

/* settings */
int num_chan_type = 0;
enum cnetz_chan_type chan_type[MAX_SENDER] = { CHAN_TYPE_OGK_SPK };
int measure_speed = 0;
double clock_speed[2] = { 0.0, 0.0 };
int set_clock_speed = 0;
const char *flip_polarity = "auto";
int ms_power = 0; /* 0..3 */
int auth = 0;
int warteschlange = 1;
uint8_t fuz_nat = 1;
uint8_t fuz_fuvst = 1;
uint8_t fuz_rest = 38;
uint8_t kennung_fufst = 1; /* normal prio */
uint8_t ws_kennung = 0; /* no queue */
uint8_t fuvst_sperren = 0; /* no blocking registration/calls */
uint8_t grenz_einbuchen = 1; /* > 15 SNR */
uint8_t grenz_umschalten = 15; /* < 18 SNR */
uint8_t grenz_ausloesen = 15; /* < 18 SNR */
uint8_t mittel_umschalten = 5; /* 64 Frames */
uint8_t mittel_ausloesen = 5; /* 64 Frames */
uint8_t genauigkeit = 1; /* limited accuracy */
uint8_t bewertung = 1; /* rating by level */
uint8_t entfernung = 3; /* 3km */
uint8_t reduzierung = 0; /* factor 4 */
uint8_t nachbar_prio = 0;
int8_t	futln_sperre_start = -1; /* no blocking */
int8_t	futln_sperre_end = -1; /* no range */
enum demod_type demod = FSK_DEMOD_AUTO;
int metering = 20;

void print_help(const char *arg0)
{
	main_mobile_print_help(arg0, "[-M] -S <rx ppm>,<tx ppm> -p -d ");
	/*      -                                                                             - */
	printf(" -T --channel-type <channel type> | list\n");
	printf("        Give channel type, use 'list' to get a list. (default = '%s')\n", chan_type_short_name(chan_type[0]));
	printf(" -M --measure-speed\n");
	printf("        Measures clock speed. THIS IS REQUIRED! See documentation!\n");
	printf(" -C --clock-speed <rx ppm>,<tx ppm>\n");
	printf("        Correct speed of sound card's clock. Use '-M' to measure speed for\n");
	printf("        some hours after temperature has settled. The use these results to\n");
	printf("        correct signal processing speed. After adjustment, the clock must match\n");
	printf("        +- 1ppm or better. CORRECTING CLOCK SPEED IS REQUIRED! See\n");
	printf("        documentation on how to measure correct value.\n");
	printf(" -F --flip-polarity no | yes | auto\n");
	printf("        Flip polarity of transmitted FSK signal. If yes, the sound card\n");
	printf("        generates a negative signal rather than a positive one. If auto, the\n");
	printf("        base station generates two virtual base stations with both polarities.\n");
	printf("        Once a mobile registers, the correct polarity is selected and used.\n");
	printf("        (default = %s)\n", flip_polarity);
	printf("	Note: This has no effect with SDR.\n");
	printf(" -P --ms-power <power level>\n");
	printf("        Give power level of the mobile station 0..3. (default = '%d')\n", ms_power);
	printf("	0 = 50-125 mW;  1 = 0.5-1 W;  2 = 4-8 W;  3 = 10-20 W\n");
	printf(" -A --authentication\n");
	printf("        Enable authentication on the base station. Since we cannot\n");
	printf("	authenticate, because we don't know the secret key and the algorithm,\n");
	printf("	we just accept any card. With this we get the vendor IDs of the phone.\n");
	printf(" -Q --queue | --warteschlange 1 | 0\n");
	printf("        Enable queue support. If no channel is available, calls will be kept\n");
	printf("        in a queue for maximum of 60 seconds. (default = %d)\n", warteschlange);
	printf(" -G --gebuehren <seconds> | 0\n");
	printf("        Increment  metering counter every given number of seconds.\n");
	printf("        To turn off, use 0. (default = %d)\n", metering);
	printf(" -S --sysinfo fuz-nat=<nat>\n");
	printf("        Set country ID of base station. All IDs were used inside Germany only.\n");
	printf("        (default = %d)\n", fuz_nat);
	printf(" -S --sysinfo fuz-fuvst=<id>\n");
	printf("        Set switching center ID of base station. (default = %d)\n", fuz_fuvst);
	printf(" -S --sysinfo fuz-rest=<id>\n");
	printf("        Set cell ID of base station. (default = %d)\n", fuz_rest);
	printf(" -S --sysinfo kennung-fufst=<id>\n");
	printf("        Set priority for selecting base station. (default = %d)\n", kennung_fufst);
	printf("        0 = Test (Only special mobile stations may register.)\n");
	printf("        1 = Normal priority base station.\n");
	printf("        2 = Higher priority base station.\n");
	printf("        3 = Highest priority base station.\n");
	printf("	Note: Priority has no effect, because there is only one base station.\n");
	printf(" -S --sysinfo ws-kennung=<value>\n");
	printf("        Queue setting of base station. (default = %d)\n", ws_kennung);
	printf("        0 = No queue, calls will be handled directly.\n");
	printf("        1 = Queue on outgoing calls.\n");
	printf("        2 = Queue blocked, no calls allowed.\n");
	printf("        3 = Reserved, don't use!\n");
	printf(" -S --sysinfo fuvst-sperren=<value>\n");
	printf("        Blocking registration and outgoing calls. (default = %d)\n", fuvst_sperren);
	printf("        0 = Registration and outgoing calls allowed.\n");
	printf("        1 = Only registration alloweed.\n");
	printf("        2 = Only outgoing calls allowed. (Cannot work without registration!)\n");
	printf("        3 = No registration and no outgoing calls allowed.\n");
	printf(" -S --sysinfo grenz-einbuchen=<value>\n");
	printf("        Minimum SNR to allow registration of mobile (default = %d)\n", grenz_einbuchen);
	printf("        0 = No limit;  1 = >15 dB;  2 = >17 dB;  3 = >19 dB\n");
	printf("        4 = >21 dB;    5 = >25 dB;  6 = >28 dB;  7 = >32 dB\n");
	printf(" -S --sysinfo grenz-umschalten=<value>\n");
	printf("        Minimum SNR before phone requests handover (default = %d)\n", grenz_umschalten);
	printf("        15 = 18 dB ... 0 = 26 dB (external)\n");
	printf("        13 = 16 dB ... 0 = 22 dB (internal)\n");
	printf(" -S --sysinfo grenz-ausloesen=<value>\n");
	printf("        Minimum SNR before phone releases of call (default = %d)\n", grenz_ausloesen);
	printf("        15 = 18 dB ... 0 = 26 dB\n");
	printf(" -S --sysinfo mittel-umschalten=<value>\n");
	printf("        Number of frames to measure for handover criterium (default = %d)\n", mittel_umschalten);
	printf("        0 =  2 measurements;  1 =  4 measurements;  2 =  8 measurememnts\n");
	printf("        3 = 16 measurements;  4 = 32 measurements;  5 = 64 measurememnts\n");
	printf(" -S --sysinfo mittel-ausloesen=<value>\n");
	printf("        Number of frames to measure for release criterium (default = %d)\n", mittel_ausloesen);
	printf("        0 =  2 measurements;  1 =  4 measurements;  2 =  8 measurememnts\n");
	printf("        3 = 16 measurements;  4 = 32 measurements;  5 = 64 measurememnts\n");
	printf(" -S --sysinfo genauigkeit=<value>\n");
	printf("        Accuracy of base station (default = %d)\n", genauigkeit);
	printf("        0 = full accuracy;  1 = limited accuracy\n");
	printf("	Note: This has no effect, because there is only one base station.\n");
	printf(" -S --sysinfo bewertung=<value>\n");
	printf("        Rating of base station (default = %d)\n", bewertung);
	printf("        0 = by relative distance;  1 = by received level\n");
	printf("	Note: This has no effect, because there is only one base station.\n");
	printf(" -S --sysinfo entfernung=<value>\n");
	printf("        Base station size (default = %d)\n", entfernung);
	printf("        0 = 1.5km;  1 =   2km;  2 = 2.5km; 3 =   3km;  4 =   5km;  5 =   5km\n");
	printf("        6 =   6km;  7 =   7km;  8 =   8km; 9 =  10km;  10 = 12km;  11 = 14km\n");
	printf("        12 = 16km;  13 = 17km;  14 = 23km; 15 = 30km\n");
	printf("	Note: This has no effect, because there is only one base station.\n");
	printf(" -S --sysinfo reduzierung=<value>\n");
	printf("        See specs value 'y' (default = %d)\n", reduzierung);
	printf("        0 = 4;  1 = 3;  2 = 2;  3 = 1\n");
	printf("	Note: This has no effect, because there is only one base station.\n");
	printf(" -S --sysinfo nachbar-prio=<value>\n");
	printf("        See specs value 'g' (default = %d)\n", nachbar_prio);
	printf("	Note: This has no effect, because there is only one base station.\n");
	printf(" -S --sysinfo futln-sperre=<value>[-<value>]\n");
	printf("        Blocking registration and outgoing calls for selected mobile stations.\n");
	printf("        The 4 least significant bits of the subscriber number can be given to\n");
	printf("        block all matching phones. Alternatively the phone number may be given\n");
	printf("        here, so that the 4 bits get calculated automatically. The optional\n");
	printf("        second value can be given, to define a range - in the same way.\n");
    if (futln_sperre_start < 0) {
	printf("        (default = no value given)\n");
    } else if (futln_sperre_end < 0) {
	printf("        (default = %d)\n", futln_sperre_start);
    } else {
	printf("        (default = %d-%d)\n", futln_sperre_start, futln_sperre_end);
    }
	printf(" -D --demod auto | slope | level\n");
	printf("        Adjust demodulation algorithm. Use 'slope' to detect a level change\n");
	printf("        by finding the highest slope of a bit transition. It is useful, if\n");
	printf("        the receiver drifts to 0 after a while, due to DC decoupling. This\n");
	printf("        happens in every analog receiver and in every sound card input.\n");
	printf("        Use 'level' to detect a level change by passing zero level. This\n");
	printf("        requires a DC coupled signal, which is produced by SDR.\n");
	printf("        Use 'auto' to select 'slope' for sound card input and 'level' for SDR\n");
	printf("        input. (default = '%s')\n", (demod == FSK_DEMOD_LEVEL) ? "level" : (demod == FSK_DEMOD_SLOPE) ? "slope" : "auto");
	printf("\nstation-id: Give 7 digit station-id, you don't need to enter it for every\n");
	printf("        start of this program.\n");
	main_mobile_print_hotkeys();
	printf("Press 'i' key to dump list of currently attached subscribers.\n");
}

static int atoi_limit(const char *p, int l1, int l2)
{
	int value = atoi(p);

	if (l1 < l2) {
		if (value < l1)
			value = l1;
		if (value > l2)
			value = l2;
	} else {
		if (value < l2)
			value = l2;
		if (value > l1)
			value = l1;
	}

	return value;
}

#define OPT_WARTESCHLANGE	256

static int handle_options(int argc, char **argv)
{
	int skip_args = 0;
	int rc;
	const char *p;

	static struct option long_options_special[] = {
		{"channel-type", 1, 0, 'T'},
		{"measure-speed", 0, 0, 'M'},
		{"clock-speed", 1, 0, 'C'},
		{"flip-polarity", 1, 0, 'F'},
		{"ms-power", 1, 0, 'P'},
		{"authentication", 0, 0, 'A'},
		{"queue", 1, 0, 'Q'},
		{"warteschlange", 1, 0, OPT_WARTESCHLANGE},
		{"gebuehren", 1, 0, 'G'},
		{"sysinfo", 1, 0, 'S'},
		{"demod", 1, 0, 'D'},
		{0, 0, 0, 0}
	};

	main_mobile_set_options("T:MC:F:P:AQ:G:S:D:", long_options_special);

	while (1) {
		int option_index = 0, c;

		c = getopt_long(argc, argv, optstring, long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
		case 'T':
			if (!strcmp(optarg, "list")) {
				cnetz_channel_list();
				exit(0);
			}
			rc = cnetz_channel_by_short_name(optarg);
			if (rc < 0) {
				fprintf(stderr, "Error, channel type '%s' unknown. Please use '-t list' to get a list. I suggest to use the default.\n", optarg);
				exit(0);
			}
			OPT_ARRAY(num_chan_type, chan_type, rc)
			skip_args += 2;
			break;
		case 'M':
			measure_speed = 1;
			skip_args++;
			break;
		case 'C':
			p = strchr(optarg, ',');
			if (!p) {
				fprintf(stderr, "Illegal clock speed, use two values, seperated by comma and no spaces!\n");
				exit(0);
			}
			clock_speed[0] = strtold(optarg, NULL);
			clock_speed[1] = strtold(p + 1, NULL);
			set_clock_speed = 1;
			skip_args += 2;
			break;
		case 'F':
			if (!strcasecmp(optarg, "no"))
				flip_polarity = "no";
			else if (!strcasecmp(optarg, "yes"))
				flip_polarity = "yes";
			else if (!strcasecmp(optarg, "auto"))
				flip_polarity = "auto";
			else {
				fprintf(stderr, "Given polarity '%s' is illegal, see help!\n", optarg);
				exit(0);
			}
			skip_args += 2;
			break;
		case 'P':
			ms_power = atoi_limit(optarg, 0, 3);
			skip_args += 2;
			break;
		case 'A':
			auth = 1;
			skip_args += 1;
			break;
		case 'Q':
		case OPT_WARTESCHLANGE:
			warteschlange = atoi_limit(optarg, 0, 1);;
			skip_args += 2;
			break;
		case 'G':
			metering = atoi(optarg);
			skip_args += 2;
			break;
		case 'S':
			p = strchr(optarg, '=');
			if (!p) {
				fprintf(stderr, "Given sysinfo parameter '%s' requires '=' character to set value, see help!\n", optarg);
				exit(0);
			}
			p++;
			if (!strncasecmp(optarg, "fuz-nat=", p - optarg)) {
				fuz_nat = atoi_limit(p, 0, 7);
			} else
			if (!strncasecmp(optarg, "fuz-fuvst=", p - optarg)) {
				fuz_fuvst = atoi_limit(p, 0, 32);
			} else
			if (!strncasecmp(optarg, "fuz-rest=", p - optarg)) {
				fuz_rest = atoi_limit(p, 0, 255);
			} else
			if (!strncasecmp(optarg, "kennung-fufst=", p - optarg)) {
				kennung_fufst = atoi_limit(p, 0, 3);
			} else
			if (!strncasecmp(optarg, "ws-kennung=", p - optarg)) {
				ws_kennung = atoi_limit(p, 0, 3);
			} else
			if (!strncasecmp(optarg, "fuvst-sperren=", p - optarg)) {
				fuvst_sperren = atoi_limit(p, 0, 3);
			} else
			if (!strncasecmp(optarg, "grenz-einbuchen=", p - optarg)) {
				grenz_einbuchen = atoi_limit(p, 0, 7);
			} else
			if (!strncasecmp(optarg, "grenz-umschalten=", p - optarg)) {
				grenz_umschalten = atoi_limit(p, 0, 15);
			} else
			if (!strncasecmp(optarg, "grenz-ausloesen=", p - optarg)) {
				grenz_ausloesen = atoi_limit(p, 0, 15);
			} else
			if (!strncasecmp(optarg, "mittel-umschalten=", p - optarg)) {
				mittel_umschalten = atoi_limit(p, 0, 5);
			} else
			if (!strncasecmp(optarg, "mittel-ausloesen=", p - optarg)) {
				mittel_ausloesen = atoi_limit(p, 0, 5);
			} else
			if (!strncasecmp(optarg, "genauigkeit=", p - optarg)) {
				genauigkeit = atoi_limit(p, 0, 1);
			} else
			if (!strncasecmp(optarg, "bewertung=", p - optarg)) {
				bewertung = atoi_limit(p, 0, 1);
			} else
			if (!strncasecmp(optarg, "entfernung=", p - optarg)) {
				entfernung = atoi_limit(p, 0, 15);
			} else
			if (!strncasecmp(optarg, "nachbar-prio=", p - optarg)) {
				nachbar_prio = atoi_limit(p, 0, 1);
			} else
			if (!strncasecmp(optarg, "futln-sperre=", p - optarg)) {
				char value[128], *v, *q;
				strncpy(value, p, sizeof(value) - 1);
				value[sizeof(value) - 1] = '\0';
				v = value;
				q = strchr(value, '-');
				if (q)
					*q++ = '\0';
				if (strlen(v) > 5)
					v += strlen(v) - 5;
				futln_sperre_start = atoi(v) & 0xf;
				if (q) {
					if (strlen(q) > 5)
						q += strlen(q) - 5;
					futln_sperre_end = atoi(q) & 0xf;
				}
			} else
			{
				fprintf(stderr, "Given sysinfo parameter '%s' unknown, see help!\n", optarg);
				exit(0);
			}
			skip_args += 2;
			break;
		case 'D':
			if (!strcasecmp(optarg, "auto"))
				demod = FSK_DEMOD_AUTO;
			else if (!strcasecmp(optarg, "slope"))
				demod = FSK_DEMOD_SLOPE;
			else if (!strcasecmp(optarg, "level"))
				demod = FSK_DEMOD_LEVEL;
			else {
				fprintf(stderr, "Given demodulation type '%s' is illegal, see help!\n", optarg);
				exit(0);
			}
			skip_args += 2;
			break;
		default:
			main_mobile_opt_switch(c, argv[0], &skip_args);
		}
	}

	free(long_options);

	return skip_args;
}

int main(int argc, char *argv[])
{
	int rc;
	int skip_args;
	const char *station_id = "";
	int mandatory = 0;
	int polarity;
	int teilnehmergruppensperre = 0;
	int anzahl_gesperrter_teilnehmergruppen = 0;
	int i;

	/* init common tones */
	init_freiton();
	init_besetzton();
	init_ansage();

	main_mobile_init();

	skip_args = handle_options(argc, argv);
	argc -= skip_args;
	argv += skip_args;

	if (argc > 1) {
		station_id = argv[1];
		if (strlen(station_id) != 7) {
			printf("Given station ID '%s' does not have 7 digits\n", station_id);
			return 0;
		}
	}

	if (!num_kanal) {
		printf("No channel (\"Kanal\") is specified, I suggest channel %d.\n\n", CNETZ_OGK_KANAL);
		mandatory = 1;
	}
	if (use_sdr) {
		/* set audiodev */
		for (i = 0; i < num_kanal; i++)
			audiodev[i] = "sdr";
		num_audiodev = num_kanal;
		/* set channel types for more than 1 channel */
		if (num_kanal > 1 && num_chan_type == 0) {
			chan_type[0] = CHAN_TYPE_OGK;
			for (i = 1; i < num_kanal; i++)
				chan_type[i] = CHAN_TYPE_SPK;
			num_chan_type = num_kanal;
		}
	}
	if (num_kanal == 1 && num_audiodev == 0)
		num_audiodev = 1; /* use default */
	if (num_kanal != num_audiodev) {
		fprintf(stderr, "You need to specify as many sound devices as you have channels.\n");
		exit(0);
	}
	if (num_kanal == 1 && num_chan_type == 0)
		num_chan_type = 1; /* use default */
	if (num_kanal != num_chan_type) {
		fprintf(stderr, "You need to specify as many channel types as you have channels.\n");
		exit(0);
	}

	if (!set_clock_speed && !measure_speed) {
		printf("No clock speed given. You need to measure clock using '-M' and later correct clock using '-S <rx ppm>,<tx ppm>'. See documentation for help!\n\n");
		mandatory = 1;
	}

	if (mandatory) {
		print_help(argv[-skip_args]);
		return 0;
	}

	if (!loopback)
		print_image();

	/* init functions */
	scrambler_init();
	if (futln_sperre_start >= 0) {
		teilnehmergruppensperre = futln_sperre_start;
		if (futln_sperre_end >= 0)
			anzahl_gesperrter_teilnehmergruppen = ((futln_sperre_end - futln_sperre_start) & 0xf) + 1;
		else
			anzahl_gesperrter_teilnehmergruppen = 1;
	}
    	if (anzahl_gesperrter_teilnehmergruppen)
		printf("Blocked subscriber with number's last 4 bits from 0x%x to 0x%x\n", teilnehmergruppensperre, (teilnehmergruppensperre + anzahl_gesperrter_teilnehmergruppen - 1) & 0xf);
	init_sysinfo(fuz_nat, fuz_fuvst, fuz_rest, kennung_fufst, ws_kennung, fuvst_sperren, grenz_einbuchen, grenz_umschalten, grenz_ausloesen, mittel_umschalten, mittel_ausloesen, genauigkeit, bewertung, entfernung, reduzierung, nachbar_prio, teilnehmergruppensperre, anzahl_gesperrter_teilnehmergruppen);
	dsp_init();
	rc = init_telegramm();
	if (rc < 0) {
		fprintf(stderr, "Error in Telegramm structure. Quitting!\n");
		goto fail;
	}
	init_coding();
	cnetz_init();

	/* check for mandatory OgK */
	for (i = 0; i < num_kanal; i++) {
		if (chan_type[i] == CHAN_TYPE_OGK || chan_type[i] == CHAN_TYPE_OGK_SPK)
			break;
	}
	if (i == num_kanal) {
		fprintf(stderr, "You must define at least one OgK (control) or OgK/SPK (control/speech) channel type. Quitting!\n");
		goto fail;
	}

	/* OgK must be the first channel, so it becomes master. This is required for syncing SPK channels */
	if (i != 0) {
		fprintf(stderr, "The first channel you defined must be OgK (control) or OgK/SPK (control/speech) channel type. Quitting!\n");
		goto fail;
	}

	/* check for mandatory OgK */
	for (i = 0; i < num_kanal; i++) {
		if (chan_type[i] == CHAN_TYPE_SPK || chan_type[i] == CHAN_TYPE_OGK_SPK)
			break;
	}
	if (i == num_kanal)
		fprintf(stderr, "You did not define any SpK (speech) channel. You will not be able to make any call.\n");

	/* SDR always requires emphasis */
	if (use_sdr) {
		do_pre_emphasis = 1;
		do_de_emphasis = 1;
	}

	if (!do_pre_emphasis || !do_de_emphasis) {
		fprintf(stderr, "*******************************************************************************\n");
		fprintf(stderr, "I strongly suggest to let me do pre- and de-emphasis (options -p -d)!\n");
		fprintf(stderr, "Use a transmitter/receiver without emphasis and let me do that!\n");
		fprintf(stderr, "Because carrier FSK signaling and scrambled voice (default) does not use\n");
		fprintf(stderr, "emphasis, I like to control emphasis by myself for best results.\n");
		fprintf(stderr, "*******************************************************************************\n");
	}

	polarity = 0; /* auto */
	if (!strcmp(flip_polarity, "no"))
		polarity = 1; /* positive */
	if (!strcmp(flip_polarity, "yes"))
		polarity = -1; /* negative */
	if (use_sdr && polarity == 0)
		polarity = 1; /* SDR is always positive */

	/* demodulation algorithm */
	if (demod == FSK_DEMOD_AUTO)
		demod = (use_sdr) ? FSK_DEMOD_LEVEL : FSK_DEMOD_SLOPE;
	if (demod == FSK_DEMOD_LEVEL && !use_sdr) {
		fprintf(stderr, "*******************************************************************************\n");
		fprintf(stderr, "I strongly suggest to use 'slope' demodulation algorithm!!!\n");
		fprintf(stderr, "Using sound card will cause the DC levels to return to 0. Using 'level' assumes\n");
		fprintf(stderr, "that a frequency offset never returns to 0. (Use this only with SDR.)\n");
		fprintf(stderr, "*******************************************************************************\n");
	}

	/* create transceiver instance */
	for (i = 0; i < num_kanal; i++) {
		rc = cnetz_create(kanal[i], chan_type[i], audiodev[i], use_sdr, demod, samplerate, rx_gain, auth, warteschlange, metering, ms_power, (i == 0) ? measure_speed : 0, clock_speed, polarity, do_pre_emphasis, do_de_emphasis, write_rx_wave, write_tx_wave, read_rx_wave, read_tx_wave, loopback);
		if (rc < 0) {
			fprintf(stderr, "Failed to create \"Sender\" instance. Quitting!\n");
			goto fail;
		}
		if ((kanal[i] & 1)) {
			printf("Base station on channel %d ready, please tune transmitter to %.3f MHz and receiver to %.3f MHz. (%.3f MHz offset)\n", kanal[i], cnetz_kanal2freq(kanal[i], 0) / 1e6, cnetz_kanal2freq(kanal[i], 1) / 1e6, cnetz_kanal2freq(kanal[i], 2) / 1e6);
		} else {
			printf("Base station on channel %d ready, please tune transmitter to %.4f MHz and receiver to %.4f MHz. (%.3f MHz offset)\n", kanal[i], cnetz_kanal2freq(kanal[i], 0) / 1e6, cnetz_kanal2freq(kanal[i], 1) / 1e6, cnetz_kanal2freq(kanal[i], 2) / 1e6);
		}
	}

	main_mobile(&quit, latency, interval, NULL, station_id, 7);

fail:
	flush_db();

	/* destroy transceiver instance */
	while (sender_head)
		cnetz_destroy(sender_head);

	return 0;
}

