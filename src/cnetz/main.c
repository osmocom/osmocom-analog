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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../libsample/sample.h"
#include "../libmobile/main_mobile.h"
#include "../libdebug/debug.h"
#include "../libmobile/call.h"
#include "../anetz/freiton.h"
#include "../anetz/besetztton.h"
#include "../liboptions/options.h"
#include "../libfm/fm.h"
#include "cnetz.h"
#include "database.h"
#include "sysinfo.h"
#include "dsp.h"
#include "telegramm.h"
#include "ansage.h"
#include "stations.h"

/* settings */
int num_chan_type = 0;
enum cnetz_chan_type chan_type[MAX_SENDER] = { CHAN_TYPE_OGK_SPK };
int measure_speed = 0;
double clock_speed[2] = { 0.0, 0.0 };
int set_clock_speed = 0;
const char *flip_polarity = "auto";
int ms_power = 6; /* 1..8 */
int warteschlange = 1;
int challenge_valid;
uint64_t challenge;
int response_valid;
uint64_t response;
uint8_t timeslot = 0;
uint8_t fuz_nat = 1;
uint8_t fuz_fuvst = 4;
uint8_t fuz_rest = 66;
const char *fuz_name = NULL;
uint8_t kennung_fufst = 1; /* normal prio */
uint8_t authentifikationsbit = 0;
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
double speech_deviation = 4000.0; /* best results with all my equipment */

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
	printf("        base station uses double time slots with alternating polarity.\n");
	printf("        Once a mobile registers, the correct polarity is selected and used.\n");
	printf("        (default = %s)\n", flip_polarity);
	printf("	Note: This has no effect with SDR.\n");
	printf(" -P --ms-power <power level>\n");
	printf("        Give power level of the mobile station: 1, 2, 4, 6, 8 (default = '%d')\n", ms_power);
	printf("	1 = 7.5-20 W; 2 = 4-8 W; 4 = 0.5-1 W; 6 = 50-125 mW; 8 = 2-10 mW\n");
	printf("	Power level 8 starts with level 6 and is then reduced on SpK.\n");
	printf(" -A --authentication <challenge>\n");
	printf("        Enable authorization flag on the base station and use given challenge\n");
	printf("        as authorization random. Depending on the key inside the card you will\n");
	printf("        get a response. Any response is accepted. Phone must have smart card!\n");
	printf("        The challenge can be any 64 bit (hex) number like: 0x0123456789abcdef\n");
	printf("        Note: Authentication is automatically enabled for the base station\n");
	printf(" -A --authentication <challenge>,<response>\n");
	printf("        Same as above, but the given response must match the response from\n");
	printf("        smart card. The response can be any 64 bit (hex) number.\n");
	printf("        Note: Authentication is automatically enabled for the base station\n");
	printf(" -Q --queue | --warteschlange 1 | 0\n");
	printf("        Enable queue support. If no channel is available, calls will be kept\n");
	printf("        in a queue for maximum of 60 seconds. (default = %d)\n", warteschlange);
	printf(" -G --gebuehren <seconds> | 0\n");
	printf("        Increment  metering counter every given number of seconds.\n");
	printf("        To turn off, use 0. (default = %d)\n", metering);
	printf(" -V --voice-deviation <2400..4000 Hz>\n");
	printf("        It is unclear what the actual voice deviation is. Please decrease, if\n");
	printf("        mobile's microphone is too loud and speaker is too quiet.\n");
	printf("        (default = %.0f)\n", speech_deviation);
	printf(" -S --sysinfo timeslot=<0..31>\n");
	printf("        Set time slot of OgK broadcast. There are 32 time slots, but every 8th\n");
	printf("	slot is used. This means if you select time slot 0, also slots 8, 16\n");
	printf("	and 24 will be used. If you select slot 14, also slots 6, 22 and 30\n");
	printf("	will be used. (default = %d)\n", timeslot);
	printf(" -S --sysinfo fuz-nat=<nat>\n");
	printf("        Set country ID of base station. All IDs were used inside Germany only.\n");
	printf("        (default = %d)\n", fuz_nat);
	printf(" -S --sysinfo fuz-fuvst=<id>\n");
	printf("        Set switching center ID of base station. (default = %d)\n", fuz_fuvst);
	printf(" -S --sysinfo fuz-rest=<id>\n");
	printf("        Set cell ID of base station. (default = %d)\n", fuz_rest);
	printf(" -S --sysinfo fuz=<nat>,<fuvst>,<rest>\n");
	printf("        Set country, switching center and cell ID of base station at once.\n");
	printf(" -S --sysinfo fuz-name=<name>\n");
	printf("        Set country, switching center and cell ID by providing name or prefix\n");
	printf("        Use 'list' to get a list of all base sstation names.\n");
	printf(" -S --sysinfo kennung-fufst=<id>\n");
	printf("        Set priority for selecting base station. (default = %d)\n", kennung_fufst);
	printf("        0 = Test (Only special mobile stations may register.)\n");
	printf("        1 = Normal priority base station.\n");
	printf("        2 = Higher priority base station.\n");
	printf("        3 = Highest priority base station.\n");
	printf("	Note: Priority has no effect, because there is only one base station.\n");
	printf(" -S --sysinfo auth=<auth>\n");
	printf("        Enable authentication flag on the base station. Since we cannot\n");
	printf("	authenticate, because we don't know the secret key and the algorithm,\n");
	printf("	we just accept any card. Useful to get the vendor IDs of the phone.\n");
	printf("        0 = Disable. Even chip card phones behave like magnetic card phones.\n");
	printf("        1 = Enable. Chip card phones send their card ID.\n");
	printf("        (default = %d)\n", authentifikationsbit);
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

static void add_options(void)
{
	main_mobile_add_options();
	option_add('T', "channel-type", 1);
	option_add('M', "measure-speed", 0);
	option_add('C', "clock-speed", 1);
	option_add('F', "flip-polarity", 1);
	option_add('P', "ms-power", 1);
	option_add('A', "authentifikationsbit", 1);
	option_add('Q', "queue", 1);
	option_add(OPT_WARTESCHLANGE, "warteschlange", 1);
	option_add('G', "gebuehren", 1);
	option_add('V', "voice-deviation", 1);
	option_add('S', "sysinfo", 1);
	option_add('D', "demod", 1);
}

static int handle_options(int short_option, int argi, char **argv)
{
	int rc;
	const char *p, *q;

	switch (short_option) {
	case 'T':
		if (!strcmp(argv[argi], "list")) {
			cnetz_channel_list();
			return 0;
		}
		rc = cnetz_channel_by_short_name(argv[argi]);
		if (rc < 0) {
			fprintf(stderr, "Error, channel type '%s' unknown. Please use '-t list' to get a list. I suggest to use the default.\n", argv[argi]);
			return -EINVAL;
		}
		OPT_ARRAY(num_chan_type, chan_type, rc)
		break;
	case 'M':
		measure_speed = 1;
		break;
	case 'C':
		p = strchr(argv[argi], ',');
		if (!p) {
			fprintf(stderr, "Illegal clock speed, use two values, separated by comma and no spaces!\n");
			return -EINVAL;
		}
		clock_speed[0] = strtold(argv[argi], NULL);
		clock_speed[1] = strtold(p + 1, NULL);
		set_clock_speed = 1;
		break;
	case 'F':
		if (!strcasecmp(argv[argi], "no"))
			flip_polarity = "no";
		else if (!strcasecmp(argv[argi], "yes"))
			flip_polarity = "yes";
		else if (!strcasecmp(argv[argi], "auto"))
			flip_polarity = "auto";
		else {
			fprintf(stderr, "Given polarity '%s' is illegal, use '-h' for help!\n", argv[argi]);
			return -EINVAL;
		}
		break;
	case 'P':
		ms_power = atoi_limit(argv[argi], 0, 9);
		if (ms_power < 1 || ms_power == 3 || ms_power == 5 || ms_power == 7 || ms_power > 8) {
			fprintf(stderr, "Given power level '%s' is illegal, use '-h' for help!\n", argv[argi]);
			return -EINVAL;
		}
		break;
	case 'A':
		authentifikationsbit = 1;
		challenge_valid = 1;
		challenge = strtoul(argv[argi], NULL, 0);
		p = strchr(argv[argi], ',');
		if (p) {
			response_valid = 1;
			response = strtoul(p + 1, NULL, 0);
		}
		break;
	case 'Q':
	case OPT_WARTESCHLANGE:
		warteschlange = atoi_limit(argv[argi], 0, 1);
		break;
	case 'G':
		metering = atoi(argv[argi]);
		break;
	case 'V':
		speech_deviation = atoi_limit(argv[argi], 2400, 4000);
		break;
	case 'S':
		p = strchr(argv[argi], '=');
		if (!p) {
			fprintf(stderr, "Given sysinfo parameter '%s' requires '=' character to set value, use '-h' for help!\n", argv[argi]);
			return -EINVAL;
		}
		p++;
		if (!strncasecmp(argv[argi], "timeslot=", p - argv[argi])) {
			timeslot = atoi_limit(p, 0, 31);
		} else
		if (!strncasecmp(argv[argi], "fuz-nat=", p - argv[argi])) {
			fuz_nat = atoi_limit(p, 0, 7);
		} else
		if (!strncasecmp(argv[argi], "fuz-fuvst=", p - argv[argi])) {
			fuz_fuvst = atoi_limit(p, 0, 31);
		} else
		if (!strncasecmp(argv[argi], "fuz-rest=", p - argv[argi])) {
			fuz_rest = atoi_limit(p, 0, 255);
		} else
		if (!strncasecmp(argv[argi], "fuz=", p - argv[argi])) {
			q = strchr(p, ',');
			if (!q) {
error_fuz:
				fprintf(stderr, "You must give 3 values for 'fuz'.\n");
				return -EINVAL;
			}
			fuz_nat = atoi_limit(p, 0, 7);
			p = q + 1;
			q = strchr(p, ',');
			if (!q)
				goto error_fuz;
			fuz_fuvst = atoi_limit(p, 0, 31);
			p = q + 1;
			fuz_rest = atoi_limit(p, 0, 255);
		} else
		if (!strncasecmp(argv[argi], "fuz-name=", p - argv[argi])) {
			fuz_name = options_strdup(p);
		} else
		if (!strncasecmp(argv[argi], "kennung-fufst=", p - argv[argi])) {
			kennung_fufst = atoi_limit(p, 0, 3);
		} else
		if (!strncasecmp(argv[argi], "auth=", p - argv[argi])) {
			authentifikationsbit = atoi_limit(p, 0, 1);
		} else
		if (!strncasecmp(argv[argi], "ws-kennung=", p - argv[argi])) {
			ws_kennung = atoi_limit(p, 0, 3);
		} else
		if (!strncasecmp(argv[argi], "fuvst-sperren=", p - argv[argi])) {
			fuvst_sperren = atoi_limit(p, 0, 3);
		} else
		if (!strncasecmp(argv[argi], "grenz-einbuchen=", p - argv[argi])) {
			grenz_einbuchen = atoi_limit(p, 0, 7);
		} else
		if (!strncasecmp(argv[argi], "grenz-umschalten=", p - argv[argi])) {
			grenz_umschalten = atoi_limit(p, 0, 15);
		} else
		if (!strncasecmp(argv[argi], "grenz-ausloesen=", p - argv[argi])) {
			grenz_ausloesen = atoi_limit(p, 0, 15);
		} else
		if (!strncasecmp(argv[argi], "mittel-umschalten=", p - argv[argi])) {
			mittel_umschalten = atoi_limit(p, 0, 5);
		} else
		if (!strncasecmp(argv[argi], "mittel-ausloesen=", p - argv[argi])) {
			mittel_ausloesen = atoi_limit(p, 0, 5);
		} else
		if (!strncasecmp(argv[argi], "genauigkeit=", p - argv[argi])) {
			genauigkeit = atoi_limit(p, 0, 1);
		} else
		if (!strncasecmp(argv[argi], "bewertung=", p - argv[argi])) {
			bewertung = atoi_limit(p, 0, 1);
		} else
		if (!strncasecmp(argv[argi], "entfernung=", p - argv[argi])) {
			entfernung = atoi_limit(p, 0, 15);
		} else
		if (!strncasecmp(argv[argi], "nachbar-prio=", p - argv[argi])) {
			nachbar_prio = atoi_limit(p, 0, 1);
		} else
		if (!strncasecmp(argv[argi], "futln-sperre=", p - argv[argi])) {
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
			fprintf(stderr, "Given sysinfo parameter '%s' unknown, use '-h' for help!\n", argv[argi]);
			return -EINVAL;
		}
		break;
	case 'D':
		if (!strcasecmp(argv[argi], "auto"))
			demod = FSK_DEMOD_AUTO;
		else if (!strcasecmp(argv[argi], "slope"))
			demod = FSK_DEMOD_SLOPE;
		else if (!strcasecmp(argv[argi], "level"))
			demod = FSK_DEMOD_LEVEL;
		else {
			fprintf(stderr, "Given demodulation type '%s' is illegal, use '-h' for help!\n", argv[argi]);
			return -EINVAL;
		}
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
	int polarity;
	int teilnehmergruppensperre = 0;
	int anzahl_gesperrter_teilnehmergruppen = 0;
	int i;

	/* init common tones */
	init_freiton();
	init_besetzton();
	init_ansage();

	init_station();

	main_mobile_init();

	/* handle options / config file */
	add_options();
	rc = options_config_file("~/.osmocom/analog/cnetz.conf", handle_options);
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

	/* resolve name of base station */
	if (fuz_name) {
		const char *error;

		/* get data from name */
		if (!strcmp(fuz_name, "list")) {
			station_list();
			return 0;
		}
		/* resolve */
		error = get_station_id(fuz_name, &fuz_nat, &fuz_fuvst, &fuz_rest);
		if (error) {
			fprintf(stderr, "%s\n", error);
			return -EINVAL;
		}
	}
	/* set or complete name (in case of prefix was given) */
	fuz_name = get_station_name(fuz_nat, fuz_fuvst, fuz_rest);

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

	if (!set_clock_speed && !measure_speed && !use_sdr) {
		printf("No clock speed given. You need to measure clock using '-M' and later correct clock using '-S <rx ppm>,<tx ppm>'. See documentation for help!\n\n");
		mandatory = 1;
	}

	if (mandatory) {
		print_help(argv[0]);
		return 0;
	}

	/* inits */
	fm_init(fast_math);
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
	init_sysinfo(timeslot, fuz_nat, fuz_fuvst, fuz_rest, kennung_fufst, authentifikationsbit, ws_kennung, fuvst_sperren, grenz_einbuchen, grenz_umschalten, grenz_ausloesen, mittel_umschalten, mittel_ausloesen, genauigkeit, bewertung, entfernung, reduzierung, nachbar_prio, teilnehmergruppensperre, anzahl_gesperrter_teilnehmergruppen);
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
		fprintf(stderr, "The first channel you define must be OgK (control) or OgK/SPK (control/speech) channel type. Quitting!\n");
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
		rc = cnetz_create(kanal[i], chan_type[i], audiodev[i], use_sdr, demod, samplerate, rx_gain, tx_gain, challenge_valid, challenge, response_valid, response, warteschlange, metering, speech_deviation, ms_power, (i == 0) ? measure_speed : 0, clock_speed, polarity, do_pre_emphasis, do_de_emphasis, write_rx_wave, write_tx_wave, read_rx_wave, read_tx_wave, loopback);
		if (rc < 0) {
			fprintf(stderr, "Failed to create \"Sender\" instance. Quitting!\n");
			goto fail;
		}
		if ((atoi(kanal[i]) & 1)) {
			printf("Base station on channel %s ready, please tune transmitter to %.3f MHz and receiver to %.3f MHz. (%.3f MHz offset)\n", kanal[i], cnetz_kanal2freq(atoi(kanal[i]), 0) / 1e6, cnetz_kanal2freq(atoi(kanal[i]), 1) / 1e6, cnetz_kanal2freq(atoi(kanal[i]), 2) / 1e6);
		} else {
			printf("Base station on channel %s ready, please tune transmitter to %.4f MHz and receiver to %.4f MHz. (%.3f MHz offset)\n", kanal[i], cnetz_kanal2freq(atoi(kanal[i]), 0) / 1e6, cnetz_kanal2freq(atoi(kanal[i]), 1) / 1e6, cnetz_kanal2freq(atoi(kanal[i]), 2) / 1e6);
		}
	}

	main_mobile("cnetz", &quit, latency, interval, NULL, station_id, 7);

fail:
	flush_db();

	/* destroy transceiver instance */
	while (sender_head)
		cnetz_destroy(sender_head);

	/* exits */
	fm_exit();

	options_free();

	return 0;
}

