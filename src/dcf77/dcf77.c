
/* implementation of DCF77 transmitter and receiver, including weather
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
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <math.h>
#include "../libdebug/debug.h"
#include "dcf77.h"
#include "weather.h"

double get_time(void);

#define CARRIER_FREQUENCY	77500
#define TEST_FREQUENCY		1000
#define CARRIER_BANDWIDTH	10.0
#define SAMPLE_CLOCK		1000
#define CLOCK_BANDWIDTH		0.1
#define REDUCTION_FACTOR	0.15
#define REDUCTION_TH		0.575
#define TX_LEVEL		0.9

#define level2db(level)		(20 * log10(level))

/* uncomment to speed up transmission by factor 10 and feed the data to the receiver */
//#define DEBUG_LOOP

static int fast_math = 0;
static float *sin_tab = NULL, *cos_tab = NULL;

const char *time_zone[4] = { "???", "CEST", "CET", "???" };
const char *week_day[8] = { "???", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
const char *month_name[13] = { "???", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

const char *datasets_0_59[8] = {
	"Maximum values 1st day (today)",
	"Minimum values 1st day (today)",
	"Maximum values 2nd day (tomorrow)",
	"Minimum values 2nd day (tomorrow)",
	"Maximum values 3rd day (in two days)",
	"Minimum values 3rd day (in two days)",
	"Maximum values 4th day (in three days)",
	"Wind and weather anomaly 4th day (in three days)",
};

const char *datasets_60_89[2] = {
	"Maximum values 1st day (following day)",
	"Maximum values 2nd day (2nd following day)",
};

const char *region[90] = {
	"F - Bordeaux, Aquitaine (Suedwestfrankreich)",
	"F - La Rochelle, Poitou-Charentes (Westkueste Frankreichs)",
	"F - Paris, Ile-de-France (Pariser Becken)",
	"F - Brest, Bretagne",
	"F - Clermont-Ferrand (Massif Central), Auvergne (Zentralmassiv)",
	"F - Beziers, Languedoc-Roussillon",
	"B - Bruxelles, Brussel (Benelux)",
	"F - Dijon (Bourgogne), Bourgogne (Ostfrankreich / Burgund)",
	"F - Marseille, Provence-Alpes-Côte d'Azur",
	"F - Lyon (Rhone-Alpes), Rhone-Alpes (Rhonetal)",
	"F - Grenoble (Savoie), Rhone-Alpes (Franz. Alpen)",
	"CH - La Chaux de Fond, Jura",
	"D - Frankfurt am Main, Hessen (Unterer Rheingraben)",
	"D - Trier, Westliches Mittelgebirge",
	"D - Duisburg, Nordrhein-Westfalen",
	"GB - Swansea, Wales (Westl. England / Wales)",
	"GB - Manchester, England (Noerdliches England)",
	"F - le Havre, Haute-Normandie (Normandie)",
	"GB - London, England (Suedostengland / London)",
	"D - Bremerhaven, Bremen (Nordseekueste)",
	"DK - Herning, Ringkobing (Nordwestliches Juetland)",
	"DK - Arhus, Arhus (Oestliches Juetland)",
	"D - Hannover, Niedersachsen (Norddeutschland)",
	"DK - Kobenhavn, Staden Kobenhaven (Seeland)",
	"D - Rostock, Mecklenburg-Vorpommern (Ostseekueste)",
	"D - Ingolstadt, Bayern (Donautal)",
	"D - Muenchen, Bayern (Suedbayern)",
	"I - Bolzano, Trentino-Alto Adige (Suedtirol)",
	"D - Nuernberg, Bayern (Nordbayern)",
	"D - Leipzig, Sachsen",
	"D - Erfurt, Thueringen",
	"CH - Lausanne, Genferseeregion (Westl. Schweizer Mitteland)",
	"CH - Zuerich (Oestl. Schweizer Mittelland)",
	"CH - Adelboden (Westl. Schweizer Alpennordhang)",
	"CH - Sion, Wallis",
	"CH - Glarus, Oestlicher Schweizer Alpennordhang",
	"CH - Davos, Graubuenden",
	"D - Kassel, Hessen (Mittelgebirge Ost)",
	"CH - Locarno, Tessin",
	"I - Sestriere, Piemont. Alpen",
	"I - Milano, Lombardia (Poebene)",
	"I - Roma, Lazio (Toskana)",
	"NL - Amsterdam, Noord-Holland (Holland)",
	"I - Genova, Liguria (Golf von Genua)",
	"I - Venezia, Veneto (Pomuendung)",
	"F - Strasbourg, Alsace (Oberer Rheingraben)",
	"A - Klagenfurt, Kaernten (Oesterreich. Alpensuedhang)",
	"A - Innsbruck, Tirol (Inneralpine Gebiete Oesterreichs)",
	"A - Salzburg, Bayr. / Oesterreich. Alpennordhang",
	"SK (Oesterreich / Slovakia) - Wien / Bratislava",
	"CZ - Praha, Prag (Tschechisches Becken)",
	"CZ - Decin, Severocesky (Erzgebirge)",
	"D - Berlin, Ostdeutschland",
	"S - Goeteborg, Goeteborgs och Bohus Laen (Westkueste Schweden)",
	"S - Stockholm, Stockholms Laen (Stockholm)",
	"S - Kalmar, Kalmar Laen (Schwedische Ostseekueste)",
	"S - Joenkoeping, Joenkoepings Laen (Suedschweden)",
	"D - Donaueschingen, Baden-Wuerttemberg (Schwarzwald / Schwaebische Alb)",
	"N - Oslo",
	"D - Stuttgart, Baden-Wuerttemberg (Noerdl. Baden Wuerttemberg)",
	"I - Napoli",
	"I - Ancona",
	"I - Bari",
	"HU - Budapest",
	"E - Madrid",
	"E - Bilbao",
	"I - Palermo",
	"E - Palma de Mallorca",
	"E - Valencia",
	"E - Barcelona",
	"AND - Andorra",
	"E - Sevilla",
	"P - Lissabon",
	"I - Sassari, (Sardinien / Korsika)",
	"E - Gijon",
	"IRL - Galway",
	"IRL - Dublin",
	"GB - Glasgow",
	"N - Stavanger",
	"N - Trondheim",
	"S - Sundsvall",
	"PL - Gdansk",
	"PL - Warszawa",
	"PL - Krakow",
	"S - Umea",
	"S - Oestersund",
	"CH - Samedan",
	"CR - Zagreb",
	"CH - Zermatt",
	"CR - Split",
};

const char *weathers_day[16] = {
	"Reserved",
	"Sunny",
	"Partly clouded",
	"Mostly clouded",
	"Overcast",
	"Heat storms",
	"Heavy Rain",
	"Snow",
	"Fog",
	"Sleet",
	"Rain shower",
	"Light rain",
	"Snow showers",
	"Frontal storms",
	"Stratus cloud",
	"Sleet storms",
};

const char *weathers_night[16] = {
	"Reserved",
	"Clear",
	"Partly clouded",
	"Mostly clouded",
	"Overcast",
	"Heat storms",
	"Heavy Rain",
	"Snow",
	"Fog",
	"Sleet",
	"Rain shower",
	"Light rain",
	"Snow showers",
	"Frontal storms",
	"Stratus cloud",
	"Sleet storms",
};

const char *extremeweathers[16] = {
	"None",
	"Heavy Weather 24 hrs.",
	"Heavy weather Day",
	"Heavy weather Night",
	"Storm 24hrs.",
	"Storm Day",
	"Storm Night",
	"Wind gusts Day",
	"Wind gusts Night",
	"Icy rain morning",
	"Icy rain evening",
	"Icy rain night",
	"Fine dust",
	"Ozon",
	"Radiation",
	"High water",
};

const char *probabilities[8] = {
	"0 %",
	"15 %",
	"30 %",
	"45 %",
	"60 %",
	"75 %",
	"90 %",
	"100 %",
};

const char *winddirections[16] = {
	"North",
	"Northeast",
	"East",
	"Southeast",
	"South",
	"Southwest",
	"West",
	"Northwest",
	"Changeable",
	"Foen",
	"Biese N/O",
	"Mistral N",
	"Scirocco S",
	"Tramont W",
	"reserved",
	"reserved",
};

const char *windstrengths[8] = {
	"0",
	"0-2",
	"3-4",
	"5-6",
	"7",
	"8",
	"9",
	">=10",
};

const char *yesno[2] = {
	"No",
	"Yes",
};
const char *anomaly1[4] = {
	"Same Weather",
	"Jump 1",
	"Jump 2",
	"Jump 3",
};
const char *anomaly2[4] = {
	"0-2 hrs",
	"2-4 hrs",
	"5-6 hrs",
	"7-8 hrs",
};

/* show a list of weather data values */
void list_weather(void)
{
	time_t timestamp, t;
	struct tm *tm;
	int i, j;

	/* get time stamp of this day, but at 22:00 UTC */
	timestamp = floor(get_time());
	timestamp -= timestamp % 86400;
	timestamp += 79200;

	printf("\n");
	printf("List of all regions\n");
	printf("-------------------\n");
	for (i = 0; i < 90; i++) {
		printf("Region: %2d = %s\n", i, region[i]);
		for (j = 0; j < 8; j++) {
			/* get local time where transmission starts */
			if (i < 60) {
				t = timestamp + 180 * i + 10800 * j;
				tm = localtime(&t);
				printf(" -> Transmission at %02d:%02d of %s\n", tm->tm_hour, tm->tm_min, datasets_0_59[j]);
			} else if (j < 2) {
				t = timestamp + 180 * (i - 60) + 10800 * 7 + 5400 * j;
				tm = localtime(&t);
				printf(" -> Transmission at %02d:%02d of %s\n", tm->tm_hour, tm->tm_min, datasets_60_89[j]);
			}
		}
	}

	printf("\n");
	printf("List of all weathers\n");
	printf("--------------------\n");
	for (i = 0; i < 16; i++) {
		if (i == 1)
			printf("Weather: %2d = %s  (day) %s  (night)\n", i, weathers_day[i], weathers_night[i]);
		else
			printf("Weather: %2d = %s  (day and night)\n", i, weathers_day[i]);
	}
	printf("\n");

	printf("List of all extreme weathers\n");
	printf("----------------------------\n");
	for (i = 1; i < 16; i++) {
		printf("Extreme: %2d = %s\n", i, extremeweathers[i]);
	}
	printf("\n");
}

static const char *show_bits(uint64_t value, int bits)
{
	static char bit[128];
	int i;

	for (i = 0; i < bits; i++)
		bit[i] = '0' + ((value >> i) & 1);
	sprintf(bit + i, "(%" PRIu64 ")", value);

	return bit;
}

/* global init */
int dcf77_init(int _fast_math)
{
	fast_math = _fast_math;

	if (fast_math) {
		int i;

		sin_tab = calloc(65536+16384, sizeof(*sin_tab));
		if (!sin_tab) {
			fprintf(stderr, "No mem!\n");
			return -ENOMEM;
		}
		cos_tab = sin_tab + 16384;

		/* generate sine and cosine */
		for (i = 0; i < 65536+16384; i++)
			sin_tab[i] = sin(2.0 * M_PI * (double)i / 65536.0);
	}

	return 0;
}

/* global exit */
void dcf77_exit(void)
{
	if (sin_tab) {
		free(sin_tab);
		sin_tab = cos_tab = NULL;
	}
}

/* instance creation */
dcf77_t *dcf77_create(int samplerate, int use_tx, int use_rx, int test_tone)
{
	dcf77_t *dcf77 = NULL;
	dcf77_tx_t *tx;
	dcf77_rx_t *rx;

	dcf77 = calloc(1, sizeof(*dcf77));
	if (!dcf77) {
		PDEBUG(DDCF77, DEBUG_ERROR, "No mem!\n");
		return NULL;
	}
	tx = &dcf77->tx;
	rx = &dcf77->rx;

	/* measurement */
	display_wave_init(&dcf77->dispwav, (double)samplerate, "DCF77");
	display_measurements_init(&dcf77->dispmeas, samplerate, "DCF77");

	/* prepare tx */
	if (use_tx) {
		tx->enable = 1;
		if (fast_math)
			tx->phase_360 = 65536.0;
		else
			tx->phase_360 = 2.0 * M_PI;

		/* carrier generation */
		tx->carrier_phase_step = tx->phase_360 * (double)CARRIER_FREQUENCY / ((double)samplerate);
		tx->test_phase_step = tx->phase_360 * (double)TEST_FREQUENCY / ((double)samplerate);
		tx->waves_0 = CARRIER_FREQUENCY / 10;
		tx->waves_1 = CARRIER_FREQUENCY / 5;
		tx->waves_sec = CARRIER_FREQUENCY;

		tx->test_tone = test_tone;
	}

	/* prepare rx */
	if (use_rx) {
		rx->enable = 1;
		if (fast_math)
			rx->phase_360 = 65536.0;
		else
			rx->phase_360 = 2.0 * M_PI;

		/* carrier filter */
		rx->carrier_phase_step = rx->phase_360 * (double)CARRIER_FREQUENCY / ((double)samplerate);
		/* use fourth order (2 iter) filter, since it is as fast as second order (1 iter) filter */
		iir_lowpass_init(&rx->carrier_lp[0], CARRIER_BANDWIDTH, (double)samplerate, 2);
		iir_lowpass_init(&rx->carrier_lp[1], CARRIER_BANDWIDTH, (double)samplerate, 2);

		/* signal rate */
		rx->sample_step = (double)SAMPLE_CLOCK / (double)samplerate;

		/* delay buffer */
		rx->delay_size = ceil((double)SAMPLE_CLOCK * 0.1);
		rx->delay_buffer = calloc(rx->delay_size, sizeof(*rx->delay_buffer));
		if (!rx->delay_buffer) {
			PDEBUG(DDCF77, DEBUG_ERROR, "No mem!\n");
			return NULL;
		}

		/* count clock signal */
		rx->clock_count = -1;

		/* measurement parameters */
		dcf77->dmp_input_level = display_measurements_add(&dcf77->dispmeas, "Input Level", "%.0f dB", DISPLAY_MEAS_AVG, DISPLAY_MEAS_LEFT, -100.0, 0.0, -INFINITY);
		dcf77->dmp_signal_level = display_measurements_add(&dcf77->dispmeas, "Signal Level", "%.0f dB", DISPLAY_MEAS_AVG, DISPLAY_MEAS_LEFT, -100.0, 0.0, -INFINITY);
		dcf77->dmp_signal_quality = display_measurements_add(&dcf77->dispmeas, "Signal Qualtiy", "%.0f %%", DISPLAY_MEAS_LAST, DISPLAY_MEAS_LEFT, 0.0, 100.0, -INFINITY);
		dcf77->dmp_current_second = display_measurements_add(&dcf77->dispmeas, "Current Second", "%.0f", DISPLAY_MEAS_LAST, DISPLAY_MEAS_LEFT, 0.0, 59.0, -INFINITY);
	}

	if (tx->enable)
		PDEBUG(DDCF77, DEBUG_INFO, "DCF77 transmitter has been created.\n");
	if (rx->enable)
		PDEBUG(DDCF77, DEBUG_INFO, "DCF77 receiver has been created.\n");

#if 0
	void rx_frame_test(dcf77_t *dcf77, const char *string);
	rx_frame_test(dcf77, "00001000011111100100101101001010000110000110011100001010000");
	rx_frame_test(dcf77, "00101010111111000100111101000010000110000110011100001010000");
	rx_frame_test(dcf77, "00011000101111000100100011000010000110000110011100001010000");
#endif

	return dcf77;
}

/* instance destruction */
void dcf77_destroy(dcf77_t *dcf77)
{
	if (dcf77) {
		dcf77_rx_t *rx = &dcf77->rx;
		free(rx->delay_buffer);
		free(dcf77);
	}

	PDEBUG(DDCF77, DEBUG_INFO, "DCF77 has been destroyed.\n");
}

static void display_weather_temperature(const char *desc, uint32_t weather)
{
	int value;

	value = (weather >> 16) & 0x3f;
	switch (value) {
	case 0:
		printf("%s%s = < -21 degrees C\n", desc, show_bits(value, 6));
		break;
	case 63:
		printf("%s%s = > 40 degrees C\n", desc, show_bits(value, 6));
		break;
	default:
		printf("%s%s = %d degrees C\n", desc, show_bits(value, 6), value - 22);
	}
}

/*
 * TX part
 */

/* Adjust given time stamp to the time stamp when the weather of the given
 * region istransmitted. An offset is used to start several minutes before
 * the transmission of the region starts, so the receiver has time to sync
 * to the signal first, so it will not miss that weather info.
 *
 * Note that this will only set the start of transmitting weather of the
 * current day (and day temperature).
 */
time_t dcf77_start_weather(time_t timestamp, int region, int offset)
{
	int hour, min;

	/* hour+min at UTC */
	if (region < 60) {
		/* first dataset starts at 22:00 UTC */
		hour = (22 + region / 20) % 24;
	} else {
		/* seventh dataset starts at 19:00 UTC */
		hour = (19 + (region - 60) / 20) % 24;
	}
	min = (region % 20) * 3;
	PDEBUG(DDCF77, DEBUG_INFO, "Setting UTC time for region %d to %02d:%02d minutes.\n", region, hour, min);

	/* reset to 0:00 UTC at same day */
	timestamp -= (timestamp % 86400);

	/* add time to start */
	timestamp += hour * 3600 + min * 60;

	/* substract offset */
	PDEBUG(DDCF77, DEBUG_INFO, "Setting timestamp offset to %d minutes.\n", offset);
	timestamp -= 60 * offset;

	return timestamp;
}

/* set weather to transmit on all regions */
void dcf77_set_weather(dcf77_t *dcf77, int weather_day, int weather_night, int extreme, int rain, int wind_dir, int wind_bft, int temperature_day, int temperature_night)
{
	dcf77_tx_t *tx = &dcf77->tx;

	tx->weather = 1;
	tx->weather_day = weather_day;
	tx->weather_night = weather_night;
	tx->rain = rain;
	tx->extreme = extreme;
	tx->wind_dir = wind_dir;
	tx->wind_bft = wind_bft;
	tx->temperature_day = temperature_day;
	tx->temperature_night = temperature_night;
}

/* generate weather frame from weather data */
static uint64_t generate_weather(time_t timestamp, int minute, int utc_hour, int weather_day, int weather_night, int extreme, int rain, int wind_dir, int wind_bft, int temperature_day, int temperature_night)
{
	int dataset = ((utc_hour + 2) % 24) * 20 + (minute / 3); /* data sets since 22:00 UTC */
	struct tm *tm;
	uint64_t key;
	uint32_t weather;
	int value, temperature;
	int i;
	int best, diff;

	/* generate key from time stamp of next minute */
	timestamp += 60;
	tm = localtime(&timestamp);
	key = 0;
	key |= (uint64_t)(tm->tm_min % 10) << 0;
	key |= (uint64_t)(tm->tm_min / 10) << 4;
	key |= (uint64_t)(tm->tm_hour % 10) << 8;
	key |= (uint64_t)(tm->tm_hour / 10) << 12;
	key |= (uint64_t)(tm->tm_mday % 10) << 16;
	key |= (uint64_t)(tm->tm_mday / 10) << 20;
	key |= (uint64_t)((tm->tm_mon + 1) % 10) << 24;
	key |= (uint64_t)((tm->tm_mon + 1) / 10) << 28;
	if (tm->tm_wday > 0)
		key |= (uint64_t)(tm->tm_wday) << 29;
	else
		key |= (uint64_t)(7) << 29;
	key |= (uint64_t)(tm->tm_year % 10) << 32;
	key |= (uint64_t)((tm->tm_year / 10) % 10) << 36;

	/* generate weather data */
	timestamp -= 120;
	weather = 0;
	PDEBUG(DFRAME, DEBUG_INFO, "Encoding weather for dataset %d/480\n", dataset);
	printf("Peparing Weather INFO\n");
	printf("---------------------\n");
	printf("Time (UTC):          %02d:%02d\n", (int)(timestamp / 3600) % 24, (int)(timestamp / 60) % 60);
	/* dataset and region for 0..59 */
	printf("Dataset:             %s\n", datasets_0_59[dataset / 60]);
	value = dataset % 60;
	printf("Region:              %d = %s\n", value, region[value]);
	/* calc. weather of region */
	if (weather_day < 0 || weather_day > 15)
		weather_day = 1;
	weather |= weather_day << 0;
	if (weather_night < 0 || weather_night > 15)
		weather_night = 1;
	weather |= weather_night << 4;
	/* calc. temperature of region 0..59 (day/night) or region 60..89 (day) */
	if (((dataset / 60) & 1) == 0 || (dataset / 60) == 7)
		temperature = temperature_day + 22;
	else
		temperature = temperature_night + 22;
	if (temperature < 0)
		temperature = 0;
	if (temperature > 63)
		value = 63;
	weather |= temperature << 16;
	/* show weather of region 0..59 */
	if ((dataset / 60) < 7) {
		printf("Weather (day):       %s = %s\n", show_bits(weather_day, 4), weathers_day[weather_day]);
		printf("Weather (night):     %s = %s\n", show_bits(weather_night, 4), weathers_night[weather_night]);
	}
	/* show extreme/wind/rain of region 0..59 */
	if (((dataset / 60) & 1) == 0) {
		/* even datasets, this is 'Day' data */
		if (extreme < 0 || extreme > 15)
			value = 0;
		else
			value = extreme;
		printf("Extreme weather:     %s = %s\n", show_bits(value, 4), extremeweathers[value]);
		weather |= value << 8;
		best = 0;
		for (i = 0; i < 8; i++) {
			diff = abs(atoi(probabilities[i]) - rain);
			if (i == 0 || diff < best) {
				best = diff;
				value = i;
			}
		}
		printf("Rain Probability:    %s = %s (best match for %d)\n", show_bits(value, 3), probabilities[value], rain);
		weather |= value << 12;
		value = 0;
		printf("Anomaly:             %s = %s\n", show_bits(value, 1), yesno[value]);
		weather |= value << 15;
		display_weather_temperature("Temperature (day):   ", weather);
	} else {
		/* odd datasets, this is 'Night' data */
		if (wind_dir < 0 || wind_dir > 15)
			value = 8;
		else
			value = wind_dir;
		printf("Wind direction:      %s = %s\n", show_bits(value, 4), winddirections[value]);
		weather |= value << 8;
		if (wind_bft < 1)
			value = 0;
		else
		if (wind_bft < 7)
			value = (wind_bft + 1) / 2;
		else
		if (wind_bft < 10)
			value = wind_bft - 3;
		else
			value = 7;
		printf("Wind strength:       %s = %s (best match for %d)\n", show_bits(value, 3), windstrengths[value], wind_bft);
		weather |= value << 12;
		value = 0;
		printf("Anomaly:             %s = %s\n", show_bits(value, 1), yesno[value]);
		weather |= value << 15;
		value = temperature_night + 22;
		if (value < 0)
			value = 0;
		if (value > 63)
			value = 63;
		weather |= value << 16;
		if ((dataset / 60) < 7)
			display_weather_temperature("Temperature (night): ", weather);
	}
	/* show weather and temperature of of region 60..89 */
	if ((dataset / 60) == 7) {
		printf("Dataset:             %s\n", 60 + datasets_60_89[(dataset % 60) / 30]);
		value = 60 + (dataset % 30);
		printf("Region:              %d = %s\n", value, region[value]);
		printf("Weather (day):       %s = %s\n", show_bits(weather_day, 4), weathers_day[weather_day]);
		printf("Weather (night):     %s = %s\n", show_bits(weather_night, 4), weathers_night[weather_night]);
		display_weather_temperature("Temperature:         ", weather);
	}

	/* the magic '10' bit string */
	weather |= 0x1 << 22;

	/* encode */
	return weather_encode(weather, key);
}

/* transmit chunk of weather data for each minute */
static uint16_t tx_weather(dcf77_tx_t *tx, time_t timestamp, int minute, int hour, int zone)
{
	int index = (minute + 2) % 3;
	int utc_hour;
	uint16_t chunk;

	if (index == 0) {
		/* convert hour to UTC */
		utc_hour = hour - 1;
		if (zone & 1)
			utc_hour--;
		if (utc_hour < 0)
			utc_hour += 24;
		/* in index 0 we transmit minute + 1 (next minute), so we substract 1 */
		tx->weather_cipher = generate_weather(timestamp, (minute + 59) % 60, utc_hour, tx->weather_day, tx->weather_night, tx->extreme, tx->rain, tx->wind_dir, tx->wind_bft, tx->temperature_day, tx->temperature_night);
		PDEBUG(DFRAME, DEBUG_INFO, "Transmitting first chunk of weather info.\n");
		chunk = (tx->weather_cipher & 0x3f) << 1; /* bit 2-7 */
		chunk |= (tx->weather_cipher & 0x0fc0) << 2; /* bit 9-14 */
		tx->weather_cipher >>= 12;
		return chunk;
	}

	PDEBUG(DFRAME, DEBUG_INFO, "Transmitting %s chunk of weather info.\n", (index == 1) ? "second" : "third");
	chunk = tx->weather_cipher & 0x3fff;
	tx->weather_cipher >>= 14;
	return chunk;
}

/* set inital time stamp at the moment the stream starts */
void dcf77_tx_start(dcf77_t *dcf77, time_t timestamp, double sub_sec)
{
	dcf77_tx_t *tx = &dcf77->tx;

	/* current second within minute */
	tx->second = timestamp % 60;
	/* time stamp of next minute */
	tx->timestamp = timestamp - tx->second + 60;
	/* wave within current second */
	tx->wave = sub_sec * (double)tx->waves_sec;
	/* silence until next second begins */
	tx->symbol = 'm'; tx->level = 0;
}

/* transmit one symbol = one second */
static char tx_symbol(dcf77_t *dcf77, time_t timestamp, int second)
{
	dcf77_tx_t *tx = &dcf77->tx;
	char symbol;
	int i, j;

	/* generate frame */
	if (second == 0 || !tx->data_frame) {
		struct tm *tm;
		int isdst_next_hour, wday, zone;
		uint64_t frame = 0, p;

		/* get DST next hour */
		timestamp += 3600;
		tm = localtime(&timestamp);
		timestamp -= 3600;
		isdst_next_hour = tm->tm_isdst;
		tm = localtime(&timestamp);

		/* get weather data */
		if (tx->weather) {
			frame |= tx_weather(tx, timestamp, tm->tm_min, tm->tm_hour, (tm->tm_isdst > 0) ? 1 : 2) << 1;
			/* calling tx_weather() destroys tm, because it is a pointer to a global variable. now we fix it */
			tm = localtime(&timestamp);
		}

		if (tm->tm_wday > 0)
			wday = tm->tm_wday;
		else
			wday = 7;

		if (tm->tm_isdst > 0)
			zone = 1;
		else
			zone = 2;

		PDEBUG(DDCF77, DEBUG_NOTICE, "The time transmitting: %s %s %d %02d:%02d:%02d %s %02d\n", week_day[wday], month_name[tm->tm_mon + 1], tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, time_zone[zone], tm->tm_year + 1900);

		if ((tm->tm_isdst > 0) != (isdst_next_hour > 0))
			frame |= (uint64_t)1 << 16;
		if (tm->tm_isdst > 0)
			frame |= (uint64_t)1 << 17;
		else
			frame |= (uint64_t)2 << 17;
		frame |= 1 << 20;

		frame |= (uint64_t)(tm->tm_min % 10) << 21;
		frame |= (uint64_t)(tm->tm_min / 10) << 25;
		p = (frame >> 21) & 0x7f;
		p = p ^ (p >> 4);
		p = p ^ (p >> 2);
		p = p ^ (p >> 1);
		frame |= (uint64_t)(p & 1) << 28;

		frame |= (uint64_t)(tm->tm_hour % 10) << 29;
		frame |= (uint64_t)(tm->tm_hour / 10) << 33;
		p = (frame >> 29) & 0x3f;
		p = p ^ (p >> 4);
		p = p ^ (p >> 2);
		p = p ^ (p >> 1);
		frame |= (uint64_t)(p & 1) << 35;

		frame |= (uint64_t)(tm->tm_mday % 10) << 36;
		frame |= (uint64_t)(tm->tm_mday / 10) << 40;
		frame |= (uint64_t)(wday) << 42;
		frame |= (uint64_t)((tm->tm_mon + 1) % 10) << 45;
		frame |= (uint64_t)((tm->tm_mon + 1) / 10) << 49;
		frame |= (uint64_t)(tm->tm_year % 10) << 50;
		frame |= (uint64_t)((tm->tm_year / 10) % 10) << 54;
		p = (frame >> 36) & 0x3fffff;
		p = p ^ (p >> 16);
		p = p ^ (p >> 8);
		p = p ^ (p >> 4);
		p = p ^ (p >> 2);
		p = p ^ (p >> 1);
		frame |= (uint64_t)(p & 1) << 58;

		tx->data_frame = frame;

		for (i = 0, j = 0; i < 59; i++) {
			if (i == 1 || i == 15 || i == 21 || i == 29 || i == 36 || i == 42 || i == 45 || i == 50)
				tx->data_string[j++] = ' ';
			tx->data_string[j++] = '0' + ((frame >> i) & 1);
		}
		tx->data_string[j] = '\0';
		PDEBUG(DDSP, DEBUG_INFO, "Start transmission of frame:\n");
		PDEBUG(DDSP, DEBUG_INFO, "0 Wetterdaten    Info 1 Minute P StundeP Tag    WoT Monat Jahr    P\n");
		PDEBUG(DDSP, DEBUG_INFO, "%s\n", tx->data_string);
	}

	if (second == 59)
		symbol = 'm';
	else
		symbol = ((tx->data_frame >> second) & 1) + '0';

	PDEBUG(DDSP, DEBUG_DEBUG, "Trasmitting symbol '%c' (Bit %d)\n", symbol, second);

	return symbol;
}

static void rx_symbol(dcf77_t *dcf77, char symbol);

/* encode one minute */
void dcf77_encode(dcf77_t *dcf77, sample_t *samples, int length)
{
#ifdef DEBUG_LOOP
	/* mute and speed up by factor 20 */
	memset(samples, 0, sizeof(*samples) * length);
	sample_t test_sample[length * 20];
	samples = test_sample;
	length *= 20;
#endif
	dcf77_tx_t *tx = &dcf77->tx;
	double carrier_phase, test_phase;
	int i;

	if (!tx->enable) {
		memset(samples, 0, sizeof(*samples) * length);
		return;
	}

	carrier_phase = tx->carrier_phase;
	test_phase = tx->test_phase;
	for (i = 0; i < length; i++) {
		if (fast_math)
			samples[i] = sin_tab[(uint16_t)carrier_phase] * tx->level;
		else
			samples[i] = sin(carrier_phase) * tx->level;
		carrier_phase += tx->carrier_phase_step;
		if (carrier_phase >= tx->phase_360) {
			carrier_phase -= tx->phase_360;
			tx->wave++;
			if (tx->wave >= tx->waves_sec) {
				tx->wave -= tx->waves_sec;
				if (++tx->second == 60) {
					tx->second = 0;
					tx->timestamp += 60;
				}
				tx->symbol = tx_symbol(dcf77, tx->timestamp, tx->second);
#ifdef DEBUG_LOOP
				rx_symbol(dcf77, tx->symbol);
#endif
			}
			switch (tx->symbol) {
			case '0':
				if (tx->wave < tx->waves_0)
					tx->level = TX_LEVEL * REDUCTION_FACTOR;
				else
					tx->level = TX_LEVEL;
				break;
			case '1':
				if (tx->wave < tx->waves_1)
					tx->level = TX_LEVEL * REDUCTION_FACTOR;
				else
					tx->level = TX_LEVEL;
				break;
			case 'm':
				tx->level = TX_LEVEL;
				break;
			}
			if (tx->test_tone)
				tx->level *= 0.9; /* 90 % */
		}
		if (tx->test_tone) {
			if (fast_math)
				samples[i] += sin_tab[(uint16_t)test_phase] * tx->level / 10.0; /* 10 % */
			else
				samples[i] += sin(test_phase) * tx->level / 10.0; /* 10 % */
			if (test_phase >= tx->phase_360)
				test_phase -= tx->phase_360;
			test_phase += tx->test_phase_step;
		}
	}
	tx->carrier_phase = carrier_phase;
	tx->test_phase = test_phase;
}

/*
 * RX part
 */

/* display weather data from weather frame */
static void display_weather(uint32_t weather, int minute, int utc_hour)
{
	int dataset = ((utc_hour + 2) % 24) * 20 + (minute / 3); /* data sets since 22:00 UTC */
	int value;

	PDEBUG(DFRAME, DEBUG_INFO, "Decoding weather for dataset %d/480\n", dataset);
	printf("Received Weather INFO\n");
	printf("---------------------\n");
	printf("Time (UTC):          %02d:%02d\n", utc_hour, minute);
	printf("Dataset:             %s\n", datasets_0_59[dataset / 60]);
	value = dataset % 60;
	printf("Region:              %d = %s\n", value, region[value]);
	if ((dataset / 60) < 7) {
		value = (weather >> 0) & 0xf;
		printf("Weather (day):       %s = %s\n", show_bits(value, 4), weathers_day[value]);
		value = (weather >> 4) & 0xf;
		printf("Weather (night):     %s = %s\n", show_bits(value, 4), weathers_night[value]);
	}
	if (((dataset / 60) & 1) == 0) {
		/* even datasets, this is 'Day' data */
		if (((weather >> 15) & 1) == 0) {
			value = (weather >> 8) & 0xf;
			printf("Extreme weather:     %s = %s\n", show_bits(value, 4), extremeweathers[value]);
		} else {
			value = (weather >> 8) & 0x3;
			printf("Relative weather:    %s = %s\n", show_bits(value, 2), anomaly1[value]);
			value = (weather >> 10) & 0x3;
			printf("Sunshine:            %s = %s\n", show_bits(value, 2), anomaly1[value]);
		}
		value = (weather >> 12) & 0x7;
		printf("Rain Probability:    %s = %s\n", show_bits(value, 3), probabilities[value]);
		value = (weather >> 15) & 0x1;
		printf("Anomaly:             %s = %s\n", show_bits(value, 1), yesno[value]);
		display_weather_temperature("Temperature (day):   ", weather);
	} else {
		/* odd datasets, this is 'Night' data */
		if (((weather >> 15) & 1) == 0) {
			value = (weather >> 8) & 0xf;
			printf("Wind direction:      %s = %s\n", show_bits(value, 4), winddirections[value]);
		} else {
			value = (weather >> 8) & 0x3;
			printf("Relative weather:    %s = %s\n", show_bits(value, 2), anomaly1[value]);
			value = (weather >> 10) & 0x3;
			printf("Sunshine:            %s = %s\n", show_bits(value, 2), anomaly1[value]);
		}
		value = (weather >> 12) & 0x7;
		printf("Wind strength:       %s = %s\n", show_bits(value, 3), windstrengths[value]);
		value = (weather >> 15) & 0x1;
		printf("Anomaly:             %s = %s\n", show_bits(value, 1), yesno[value]);
		if ((dataset / 60) < 7)
			display_weather_temperature("Temperature (night): ", weather);
	}
	if ((dataset / 60) == 7) {
		printf("Dataset:             %s\n", 60 + datasets_60_89[(dataset % 60) / 30]);
		value = 60 + (dataset % 30);
		printf("Region:              %d = %s\n", value, region[value]);
		value = (weather >> 0) & 0xf;
		printf("Weather (day):       %s = %s\n", show_bits(value, 4), weathers_day[value]);
		value = (weather >> 4) & 0xf;
		printf("Weather (night):     %s = %s\n", show_bits(value, 4), weathers_night[value]);
		display_weather_temperature("Temperature:         ", weather);
	}
}

/* reset weather frame */
static void rx_weather_reset(dcf77_rx_t *rx)
{
	rx->weather_index = 0;
	rx->weather_cipher = 0;
	rx->weather_key = 0;
}

/* receive weather chunk */
static void rx_weather(dcf77_rx_t *rx, int minute, int hour, int zone, uint64_t frame)
{
	int index = (minute + 2) % 3;
	int utc_hour;
	int32_t weather;

	if (rx->weather_index == 0 && index != 0) {
		PDEBUG(DFRAME, DEBUG_INFO, "Skipping weather info chunk, waiting for new start of weather info.\n");
		return;
	}

	if (index == 0) {
		rx_weather_reset(rx);
		rx->weather_cipher |= (frame >> 2) & 0x3f; /* bit 2-7 */
		rx->weather_cipher |= (frame >> 3) & 0x0fc0; /* bit 9-14 */
		rx->weather_index++;
		if (((frame & 0x0002)) || ((frame & 0x0100)) || !rx->weather_cipher) {
			PDEBUG(DFRAME, DEBUG_INFO, "There is no weather info in this received minute.\n");
			rx_weather_reset(rx);
			return;
		}
		PDEBUG(DFRAME, DEBUG_INFO, "Got first chunk of weather info.\n");
		return;
	}
	if (rx->weather_index == 1 && index == 1) {
		PDEBUG(DFRAME, DEBUG_INFO, "Got second chunk of weather info.\n");
		rx->weather_cipher |= (frame << 11) & 0x3fff000; /* bit 1-14 */
		rx->weather_key |= (frame >> 21) & 0x7f;
		rx->weather_key |= ((frame >> 29) & 0x3f) << 8;
		rx->weather_key |= ((frame >> 36) & 0x3f) << 16;
		rx->weather_key |= ((frame >> 45) & 0x1f) << 24;
		rx->weather_key |= ((frame >> 42) & 0x07) << 29;
		rx->weather_key |= ((frame >> 50) & 0xff) << 32;
		rx->weather_index++;
		return;
	}
	if (rx->weather_index == 2 && index == 2) {
		PDEBUG(DFRAME, DEBUG_INFO, "Got third chunk of weather info.\n");
		rx->weather_cipher |= (frame << 25) & 0xfffc000000; /* bit 1-14 */
		weather = weather_decode(rx->weather_cipher, rx->weather_key);
		if (weather < 0)
			PDEBUG(DFRAME, DEBUG_NOTICE, "Failed to decrypt weather info, checksum error.\n");
		else {
			/* convert hour to UTC */
			utc_hour = hour - 1;
			if (zone & 1)
				utc_hour--;
			if (utc_hour < 0)
				utc_hour += 24;
			/* in index 2 we transmit minute + 3 (next minute), so we substract 3 */
			display_weather(weather, (minute + 57) % 60, utc_hour);
		}
		rx_weather_reset(rx);
		return;
	}

	rx_weather_reset(rx);
	PDEBUG(DFRAME, DEBUG_INFO, "Got weather info chunk out of order, waiting for new start of weather info.\n");
}

/* decode time from received data */
static void rx_frame(dcf77_rx_t *rx, uint64_t frame)
{
	int zone;
	int minute_one, minute_ten, minute = -1;
	int hour_one, hour_ten, hour = -1;
	int day_one, day_ten, day = -1;
	int wday = -1;
	int month_one, month_ten, month = -1;
	int year_one, year_ten, year = -1;
	uint64_t p;

	PDEBUG(DFRAME, DEBUG_INFO, "Bit 0 is '0'?    : %s\n", ((frame >> 0) & 1) ? "no" : "yes");
	PDEBUG(DFRAME, DEBUG_INFO, "Bits 1..14       : 0x%04x\n", (int)(frame >> 1) & 0x3fff);
	PDEBUG(DFRAME, DEBUG_INFO, "Call Bit         : %d\n", (int)(frame >> 15) & 1);
	PDEBUG(DFRAME, DEBUG_INFO, "Change Time Zone : %s\n", ((frame >> 16) & 1) ? "yes" : "no");
	zone = ((frame >> 17) & 3);
	PDEBUG(DFRAME, DEBUG_INFO, "Time Zone        : %s\n", time_zone[zone]);
	PDEBUG(DFRAME, DEBUG_INFO, "Add Leap Second  : %s\n", ((frame >> 19) & 1) ? "yes" : "no");
	PDEBUG(DFRAME, DEBUG_INFO, "Bit 20 is '1'?   : %s\n", ((frame >> 20) & 1) ? "yes" : "no");

	minute_one = (frame >> 21 & 0xf);
	minute_ten = ((frame >> 25) & 0x7);
	p = (frame >> 21) & 0xff;
	p = p ^ (p >> 4);
	p = p ^ (p >> 2);
	p = p ^ (p >> 1);
	if (minute_one > 9 || minute_ten > 5 || (p & 1))
		PDEBUG(DFRAME, DEBUG_INFO, "Minute           : ???\n");
	else {
		minute = minute_ten * 10 + minute_one;
		PDEBUG(DFRAME, DEBUG_INFO, "Minute           : %02d\n", minute);
	}

	hour_one = (frame >> 29 & 0xf);
	hour_ten = ((frame >> 33) & 0x3);
	p = (frame >> 29) & 0x7f;
	p = p ^ (p >> 4);
	p = p ^ (p >> 2);
	p = p ^ (p >> 1);
	if (hour_one > 9 || hour_ten > 2 || (hour_ten == 2 && hour_one > 3) || (p & 1))
		PDEBUG(DFRAME, DEBUG_INFO, "Hour             : ???\n");
	else {
		hour = hour_ten * 10 + hour_one;
		PDEBUG(DFRAME, DEBUG_INFO, "Hour             : %02d\n", hour);
	}

	day_one = (frame >> 36 & 0xf);
	day_ten = ((frame >> 40) & 0x3);
	wday = (frame >> 42 & 0x7);
	month_one = (frame >> 45 & 0xf);
	month_ten = ((frame >> 49) & 0x1);
	year_one = (frame >> 50 & 0xf);
	year_ten = ((frame >> 54) & 0xf);
	p = (frame >> 36) & 0x7fffff;
	p = p ^ (p >> 16);
	p = p ^ (p >> 8);
	p = p ^ (p >> 4);
	p = p ^ (p >> 2);
	p = p ^ (p >> 1);
	if (day_one > 9 || day_ten > 3 || (day_ten == 3 && day_one > 1) || (day_ten == 0 && day_one == 0) || (p & 1))
		PDEBUG(DFRAME, DEBUG_INFO, "Day              : ???\n");
	else {
		day = day_ten * 10 + day_one;
		PDEBUG(DFRAME, DEBUG_INFO, "Day              : %d\n", day);
	}
	if (wday < 1 || wday > 7 || (p & 1)) {
		PDEBUG(DFRAME, DEBUG_INFO, "Week Day         : ???\n");
		wday = -1;
	} else
		PDEBUG(DFRAME, DEBUG_INFO, "Week Day         : %s\n", week_day[wday]);
	if (month_one > 9 || month_ten > 1 || (month_ten == 1 && month_one > 2) || (month_ten == 0 && month_one == 0) || (p & 1))
		PDEBUG(DFRAME, DEBUG_INFO, "Month            : ???\n");
	else {
		month = month_ten * 10 + month_one;
		PDEBUG(DFRAME, DEBUG_INFO, "Month            : %d\n", month);
	}
	if (year_one > 9 || year_ten > 9 || (p & 1))
		PDEBUG(DFRAME, DEBUG_INFO, "Year             : ???\n");
	else {
		year = year_ten * 10 + year_one;
		PDEBUG(DFRAME, DEBUG_INFO, "Year             : %02d\n", year);
	}

	if (minute >= 0 && hour >= 0 && day >= 0 && wday >= 0 && month >= 0 && year >= 0) {
		PDEBUG(DDCF77, DEBUG_NOTICE, "The received time is: %s %s %d %02d:%02d:00 %s 20%02d\n", week_day[wday], month_name[month], day, hour, minute, time_zone[zone], year);
		rx_weather(rx, minute, hour, zone, frame);
	} else {
		PDEBUG(DDCF77, DEBUG_NOTICE, "The received time is invalid!\n");
		rx_weather_reset(rx);
	}
}

/* test routing for test data */
void rx_frame_test(dcf77_t *dcf77, const char *string)
{
	uint64_t frame = 0;
	int i;

	puts(string);
	for (i = 0; i < 59; i++) {
		frame |= (uint64_t)(string[i] & 1) << i;
	}

	rx_frame(&dcf77->rx, frame);
}

/* receive one symbol = one second */
static void rx_symbol(dcf77_t *dcf77, char symbol)
{
	dcf77_rx_t *rx = &dcf77->rx;
	double second = -NAN;

	PDEBUG(DDSP, DEBUG_DEBUG, "Received symbol '%c'\n", symbol);

	if (!rx->data_receive) {
		if (symbol == 'm') {
			PDEBUG(DDSP, DEBUG_INFO, "Reception of frame has started\n");
			rx->data_receive = 1;
			rx->data_index = 0;
			rx->string_index = 0;
			second = 0;
		}
	} else {
		if (symbol == 'm') {
			if (rx->data_index == 59) {
				rx->data_string[rx->string_index] = '\0';
				rx->data_index = 0;
				rx->string_index = 0;
				PDEBUG(DDSP, DEBUG_INFO, "Received complete frame:\n");
				PDEBUG(DDSP, DEBUG_INFO, "0 Wetterdaten    Info 1 Minute P StundeP Tag    WoT Monat Jahr    P\n");
				PDEBUG(DDSP, DEBUG_INFO, "%s\n", rx->data_string);
				rx_frame(rx, rx->data_frame);
				second = 0;
			} else {
				PDEBUG(DDSP, DEBUG_INFO, "Short read, frame too short\n");
				rx->data_index = 0;
				rx->string_index = 0;
				rx_weather_reset(rx);
			}
		} else {
			if (rx->data_index == 59) {
				PDEBUG(DDSP, DEBUG_INFO, "Long read, frame too long\n");
				rx->data_receive = 0;
				rx_weather_reset(rx);
			} else {
				if (rx->data_index == 1 || rx->data_index == 15 || rx->data_index == 21 || rx->data_index == 29 || rx->data_index == 36 || rx->data_index == 42 || rx->data_index == 45 || rx->data_index == 50)
					rx->data_string[rx->string_index++] = ' ';
				rx->data_string[rx->string_index++] = symbol;
				rx->data_index++;
				rx->data_frame >>= 1;
				rx->data_frame |= (uint64_t)(symbol & 1) << 58;
				second = rx->data_index;
			}
		}
	}
	display_measurements_update(dcf77->dmp_current_second, second, 0.0);
}

//#define DEBUG_SAMPLE

/* decode radio wave and extract each bit / second */
void dcf77_decode(dcf77_t *dcf77, sample_t *samples, int length)
{
	dcf77_rx_t *rx = &dcf77->rx;
	sample_t I[length], Q[length];
	double phase, level, delayed_level, reduction, quality;
	int i;

	display_wave(&dcf77->dispwav, samples, length, 1.0);

#ifdef DEBUG_LOOP
	return;
#endif
	if (!rx->enable)
		return;

	/* rotate spectrum */
	phase = rx->carrier_phase;
	for (i = 0; i < length; i++) {
		/* mix with carrier frequency */
		if (fast_math) {
			I[i] = cos_tab[(uint16_t)phase] * samples[i];
			Q[i] = sin_tab[(uint16_t)phase] * samples[i];
		} else {
			I[i] = cos(phase) * samples[i];
			Q[i] = sin(phase) * samples[i];
		}
		phase += rx->carrier_phase_step;
		if (phase >= rx->phase_360)
			phase -= rx->phase_360;
	}
	rx->carrier_phase = phase;

	level = sqrt(I[0] * I[0] + Q[0] * Q[0]);
	if (level > 0.0) // don't average with level of 0.0 (-inf dB)
		display_measurements_update(dcf77->dmp_input_level, level2db(level), 0.0);

	/* filter carrier */
	iir_process(&rx->carrier_lp[0], I, length);
	iir_process(&rx->carrier_lp[1], Q, length);

	for (i = 0; i < length; i++) {
		rx->sample_counter += rx->sample_step;
		if (rx->sample_counter >= 1.0) {
			rx->sample_counter -= 1.0;
			/* level */
			level = sqrt(I[i] * I[i] + Q[i] * Q[i]);
			if (level > 0.0) // don't average with level of 0.0 (-inf dB)
				display_measurements_update(dcf77->dmp_signal_level, level2db(level), 0.0);

#ifdef DEBUG_SAMPLE
			printf("%s amplitude= %.6f\n", debug_amplitude(level/rx->value_level), level/rx->value_level);
#endif

			/* delay sample */
			delayed_level = rx->delay_buffer[rx->delay_index];
			rx->delay_buffer[rx->delay_index] = level;
			if (++rx->delay_index == rx->delay_size)
				rx->delay_index = 0;

			if (rx->clock_count < 0 || rx->clock_count > 900) {
				if (level / delayed_level < REDUCTION_TH)
					rx->clock_count = 0;
			}
			if (rx->clock_count >= 0) {
				if (rx->clock_count == 0) {
#ifdef DEBUG_SAMPLE
					puts("got clock");
#endif
					rx->value_level = delayed_level;
				}
				if (rx->clock_count == 50) {
#ifdef DEBUG_SAMPLE
					puts("*short*");
#endif
					rx->value_short = level;
					reduction = rx->value_short / rx->value_level;
					if (reduction < REDUCTION_TH) {
#ifdef DEBUG_SAMPLE
						printf("reduction is %.3f\n", reduction);
#endif
						if (reduction < REDUCTION_FACTOR)
							reduction = REDUCTION_FACTOR;
						quality = 1.0 - (reduction - REDUCTION_FACTOR) / (REDUCTION_TH - REDUCTION_FACTOR);
						display_measurements_update(dcf77->dmp_signal_quality, quality * 100.0, 0.0);
					}
				}
				if (rx->clock_count == 150) {
#ifdef DEBUG_SAMPLE
					puts("*long*");
#endif
					rx->value_long = level;
					if (rx->value_long / rx->value_level < REDUCTION_TH)
						rx_symbol(dcf77, '1');
					else
						rx_symbol(dcf77, '0');
				}
				if (rx->clock_count == 1100) {
#ifdef DEBUG_SAMPLE
					puts("*missing clock*");
#endif
					rx->clock_count = -1;
					rx_symbol(dcf77, 'm');
				}
			}
			if (rx->clock_count >= 0)
				rx->clock_count++;


		}
	}
}



