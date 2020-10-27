/* NMT country settings
 *
 * (C) 2017 by Andreas Eversberg <jolly@eversberg.eu>
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
#include <string.h>
#include "countries.h"

struct nmt_frequency {
	int first_channel, last_channel;	/* first and last channel number */
	double first_frequency;			/* frequency of first channel */
	double channel_spacing;			/* spacing between channels */
	double deviation_factor;		/* factor of frequency deviation, relative to scandinavia */
	double duplex_spacing;			/* distance of downlink above uplink frequency */
	int scandinavia;			/* if set, this frequency belongs to scandinavia */
};

/* channel allocation used in scandinavian countries */
static struct nmt_frequency frq_450_scandinavia[] = {
	{   1,	180,	463.000,	0.025,	1.0,	10.0,	1 },
	{   181,200,	462.500,	0.025,	1.0,	10.0,	1 },
	{   201,380,	463.0125,	0.025,	1.0,	10.0,	1 },
	{   381,399,	462.5125,	0.025,	1.0,	10.0,	1 },
	{   0,	0,	0.0,		0.0,	0.0,	0.0,	0 }
};

static struct nmt_frequency frq_900_scandinavia[] = {
	{   1,	1000,	935.0125,	0.025,	1.0,	45.0,	0 },
	{   1025,2023,	935.025,	0.025,	1.0,	45.0,	0 },
	{   0,	0,	0.0,		0.0,	0.0,	0.0,	0 }
};

/* channel allocation used in Netherlands, Luxemburg, Belgium */
static struct nmt_frequency frq_450_nl_l_b[] = {
	{   1,	222,	461.310,	0.020,	0.8,	10.0,	0 },
	{   0,	0,	0.0,		0.0,	0.0,	0.0,	0 }
};

static struct nmt_frequency frq_900_nl[] = {
	{   1,	1000,	935.0125,	0.025,	1.0,	45.0,	0 },
	{   0,	0,	0.0,		0.0,	0.0,	0.0,	0 }
};

/* channel allocation used in Malaysia */
static struct nmt_frequency frq_450_mal[] = {
	{   1,	180,	462.000,	0.025,	1.0,	10.0,	0 },
	{   0,	0,	0.0,		0.0,	0.0,	0.0,	0 }
};

/* channel allocation used in Thailand, Indonesia */
static struct nmt_frequency frq_450_t_ri[] = {
	{   1,	224,	489.000,	0.020,	0.8,	10.0,	0 },
	{   0,	0,	0.0,		0.0,	0.0,	0.0,	0 }
};

/* channel allocation used in Spain */
static struct nmt_frequency frq_450_e[] = {
	{   1,	180,	464.325,	0.025,	1.0,	10.0,	0 },
	{   0,	0,	0.0,		0.0,	0.0,	0.0,	0 }
};

/* channel allocation used in Austria */
static struct nmt_frequency frq_450_a[] = {
	{   1,	180,	465.730,	-0.020,	0.8,	10.0,	0 },
	{   0,	0,	0.0,		0.0,	0.0,	0.0,	0 }
};

/* channel allocation used in Czech Republic and Slovakia */
static struct nmt_frequency frq_450_cz_sk[] = {
	{   1,	222,	465.730,	-0.020,	0.8,	10.0,	0 },
	{   0,	0,	0.0,		0.0,	0.0,	0.0,	0 }
};

/* channel allocation used in Hungary */
static struct nmt_frequency frq_450_hu[] = {
	{   132,239,	467.370,	-0.020,	0.8,	10.0,	0 },
	{   1,	72,	469.990,	-0.020,	0.8,	10.0,	0 },
	{   0,	0,	0.0,		0.0,	0.0,	0.0,	0 }
};

/* channel allocation used in Turkey */
static struct nmt_frequency frq_900_tr[] = {
	{   1,	180,	461.500,	0.025,	1.0,	10.0,	0 },
	{   0,	0,	0.0,		0.0,	0.0,	0.0,	0 }
};

/* channel allocation used in France */
static struct nmt_frequency frq_900_f[] = {
	{   400,578,	440.025,	0.025,	1.0,	-10.0,	0 },
	{   1423,1601,	440.0125,	0.025,	1.0,	-10.0,	0 },
	{   0,	0,	0.0,		0.0,	0.0,	0.0,	0 }
};

/* country selector */
static struct nmt_country {
	int system;				/* 450 or 900 */
	int tested;				/* tested with a real phone */
	int y;					/* country code of traffic area */
	int first_ta, last_ta;			/* range of used traffic areas */
	const char *short_name;
	const char *long_name;
	const char *provider_name;
	struct nmt_frequency *nmt_frequency;	/* list of frequency allocations */
} nmt_country[] = {
	/* 450 */
	{ 450, 1, 5, 1,9,	"DK",	"Denmark",		"Tele Danmark Mobile",		frq_450_scandinavia },
	{ 450, 1, 6, 1,9,	"SE",	"Sweden",		"Telia Mobitel",		frq_450_scandinavia },
	{ 450, 1, 7, 1,9,	"NO",	"Norway",		"Telenor Mobil",		frq_450_scandinavia },
	{ 450, 1, 8, 1,9,	"FI",	"Finland",		"Telecom Finland",		frq_450_scandinavia },
	{ 450, 1, 8, 1,9,	"IS",	"Iceland",		"Post & Telecom",		frq_450_scandinavia },
	{ 450, 1, 5, 1,9,	"FO",	"Faroe Island",		"Faroese Telecom",		frq_450_scandinavia },
	{ 450, 1, 7, 1,9,	"EE",	"Estonia",		"Eesti Mobiiltelefon",		frq_450_scandinavia },
	{ 450, 1, 5, 1,9,	"LV",	"Latvia",		"Latvian Mobile Telephone",	frq_450_scandinavia },
	{ 450, 1, 8, 1,9,	"LT",	"Lithuania",		"COMLIET",			frq_450_scandinavia },
	{ 450, 1, 6, 1,9,	"BY",	"Belarus",		"Belcel",			frq_450_scandinavia },
	{ 450, 1, 5, 1,9,	"MO",	"OSS/Moscow",		"Moscow Cellular Comm.",	frq_450_scandinavia },
	{ 450, 1, 6, 1,9,	"STP",	"OSS/St Petersburg",	"Delta Telecom",		frq_450_scandinavia },
	{ 450, 1, 6, 1,9,	"STP",	"OSS/Leningrads Dist.",	"Delta Telecom",		frq_450_scandinavia },
	{ 450, 1, 7, 1,9,	"CAR",	"OSS/Carelian Rep.",	"Telecom Finland",		frq_450_scandinavia },
	{ 450, 1, 5, 1,9,	"MUR",	"OSS/Murmansk",		"Telecom Finland",		frq_450_scandinavia },
	{ 450, 1, 5, 1,9,	"LED",	"OSS/Leningrads Dist.",	"Telecom Finland",		frq_450_scandinavia },
	{ 450, 1, 5, 1,9,	"KAL",	"Kaliningrad",		"Telecom Finland",		frq_450_scandinavia },
	{ 450, 1, 7, 1,9,	"PL",	"Poland",		"CENTERTEL",			frq_450_scandinavia },
	{ 450, 1, 6, 1,9,	"BG",	"Bulgaria",		"MOBIFON",			frq_450_scandinavia },
	{ 450, 1, 5, 1,9,	"RO",	"Romania",		"Telefonica Romania",		frq_450_scandinavia },
	{ 450, 1, 6, 1,9,	"UA",	"Ukraine",		"Ukraine Mobile Comm.",		frq_450_scandinavia },
	{ 450, 1, 1, 1,9,	"RU1",	"",			"",				frq_450_scandinavia },
	{ 450, 1, 2, 1,9,	"RU2",	"",			"",				frq_450_scandinavia },
	{ 450, 1, 3, 1,9,	"RU3",	"",			"",				frq_450_scandinavia },
	{ 450, 1, 4, 1,9,	"RU4",	"",			"",				frq_450_scandinavia },
	{ 450, 1, 1, 1,9,	"NL",	"Netherlands",		"Royal Dutch Post & Telecom",	frq_450_nl_l_b },
	{ 450, 1, 1, 15,15,	"L",	"Luxemburg",		"Enterprise des P&T Luxembourg",frq_450_nl_l_b },
	{ 450, 1, 2, 1,9,	"B",	"Belgium",		"Belgacom Mobile",		frq_450_nl_l_b },
	{ 450, 1, 7, 1,9,	"CZ",	"Czech Republic",	"Eurotel Prague",		frq_450_cz_sk },
	{ 450, 1, 6, 1,9,	"SK",	"Slovakia",		"Eurotel Bratislava",		frq_450_cz_sk },
	{ 450, 1, 6, 1,15,	"HU",	"Hungary",		"WESTEL 0660",			frq_450_hu },
	/* 900 */
	{ 900, 1, 1, 1,9,	"DK",	"Denmark",		"Tele Danmark Mobile",		frq_900_scandinavia },
	{ 900, 1, 2, 1,9,	"SE",	"Sweden",		"Telia Mobitel",		frq_900_scandinavia },
	{ 900, 1, 3, 1,9,	"NO",	"Norway",		"Telenor Mobil",		frq_900_scandinavia },
	{ 900, 1, 4, 1,9,	"FI",	"Finland",		"Telecom Finland",		frq_900_scandinavia },
	{ 900, 1, 0, 1,9,	"F0",	"France (Group 0)",	"France Telecom",		frq_900_f },
	{ 900, 1, 1, 1,9,	"F1",	"France (Group 1)",	"France Telecom",		frq_900_f },
	{ 900, 1, 2, 1,9,	"F2",	"France (Group 2)",	"France Telecom",		frq_900_f },
	{ 900, 1, 3, 1,9,	"F3",	"France (Group 3)",	"France Telecom",		frq_900_f },
	{ 900, 1, 4, 1,9,	"F4",	"France (Group 4)",	"France Telecom",		frq_900_f },
	{ 900, 1, 5, 1,9,	"F5",	"France (Group 5)",	"France Telecom",		frq_900_f },
	{ 900, 1, 6, 1,9,	"F6",	"France (Group 6)",	"France Telecom",		frq_900_f },
	{ 900, 1, 7, 1,9,	"F7",	"France (Group 7)",	"France Telecom",		frq_900_f },
	/* untested... */
	{ 450, 0, 8, 8,8,	"MAL",	"Malaysia",		"Jabatan Telekom Malaysia",	frq_450_mal },
	{ 450, 0, 4, 1,9,	"T",	"Thailand",		"Telephone Organization of Thailand",frq_450_t_ri },
	{ 450, 0, 8, 1,9,	"E",	"Spain",		"Telefonica Servicios Moviles",	frq_450_e },
	{ 450, 0, 8, 1,1,	"RI",	"Indonesia",		"PT Mobisel",			frq_450_t_ri },
	{ 450, 0, 0, 1,3,	"A",	"Austria",		"PTV",				frq_450_a },
	{ 450, 0, 9, 1,3,	"A2",	"Austria 2",		"PTV",				frq_450_a },
	{ 900, 0, 5, 1,9,	"CH",	"Switzerland",		"PTT",				frq_900_scandinavia },
	{ 900, 0, 6, 1,15,	"NL",	"Netherlands",		"Royal Dutch Post & Telecom",	frq_900_nl },
	{ 900, 0, 1, 1,9,	"TR",	"Turkey",		"Turkcell",			frq_900_tr },
	{ 0,0, 0, 0,0,		NULL,	NULL,			NULL,				NULL }
};

void nmt_country_list(int nmt_system)
{
	int i, j;
	int ch_from = 0, ch_to = 0;
	char ch_string[256];

	printf("TA from\tTA to\tYY Code\tChannels\t\t\tCountry (Provider)\n");
	printf("--------------------------------------------------------------------------------------------------------\n");
	for (i = 0; nmt_country[i].short_name; i++) {
		if (nmt_system != nmt_country[i].system)
			continue;
		printf("%s,%d\t", nmt_country[i].short_name, nmt_country[i].first_ta);
		if (nmt_country[i].first_ta != nmt_country[i].last_ta)
			printf("%s,%d", nmt_country[i].short_name, nmt_country[i].last_ta);
		printf("\t%02x..%02x", (nmt_country[i].y << 4) | nmt_country[i].first_ta, (nmt_country[i].y << 4) | nmt_country[i].last_ta);
		ch_string[0] = '\0';
		for (j = 0; nmt_country[i].nmt_frequency[j].first_frequency; j++) {
			ch_from = nmt_country[i].nmt_frequency[j].first_channel;
			ch_to = nmt_country[i].nmt_frequency[j].last_channel;
			sprintf(strchr(ch_string, '\0'), "%d-%d ", ch_from, ch_to);
		}
		strcpy(strchr(ch_string, '\0'), "                              ");
		ch_string[30] = '\0';
		printf("\t%s", ch_string);
		if (nmt_country[i].long_name[0])
			printf("\t%s (%s)\n", nmt_country[i].long_name, nmt_country[i].provider_name);
		else
			printf("\n");
	}
	printf("\nFor listing channels of the NMT %d system use '-N %d' as first option.\n", 1350 - nmt_system, 1350 - nmt_system);
}

int nmt_country_by_short_name(int nmt_system, const char *short_name)
{
	int i;

	for (i = 0; nmt_country[i].short_name; i++) {
		if (nmt_system != nmt_country[i].system)
			continue;
		if (!strcasecmp(nmt_country[i].short_name, short_name))
			return nmt_country[i].y;
	}

	return -1;
}

const char *nmt_long_name_by_short_name(int nmt_system, const char *short_name)
{
	int i;

	for (i = 0; nmt_country[i].short_name; i++) {
		if (nmt_system != nmt_country[i].system)
			continue;
		if (!strcasecmp(nmt_country[i].short_name, short_name))
			return nmt_country[i].long_name;
	}

	return NULL;
}

int nmt_ta_by_short_name(int nmt_system, const char *short_name, int ta)
{
	int i;

	for (i = 0; nmt_country[i].short_name; i++) {
		if (nmt_system != nmt_country[i].system)
			continue;
		if (!strcasecmp(nmt_country[i].short_name, short_name) && ta >= nmt_country[i].first_ta && ta <= nmt_country[i].last_ta)
			return ta;
	}

	return -1;
}

/* Convert channel number to frequency number of base station.
   Set 'uplink' to 1 to get frequency of mobile station. */
double nmt_channel2freq(int nmt_system, const char *short_name, int channel, int uplink, double *deviation_factor, int *scandinavia, int *tested)
{
	int i, j;
	double freq;

	for (i = 0; nmt_country[i].short_name; i++) {
		if (nmt_system != nmt_country[i].system)
			continue;
		if (!strcasecmp(nmt_country[i].short_name, short_name)) {
			for (j = 0; nmt_country[i].nmt_frequency[j].first_frequency; j++) {
				if (channel >= nmt_country[i].nmt_frequency[j].first_channel
				 && channel <= nmt_country[i].nmt_frequency[j].last_channel) {
					if (uplink == 2)
						return -nmt_country[i].nmt_frequency[j].duplex_spacing * 1e6;
				 	/* add "channel offset" * "channel spacing" to "first frequency" */
					freq = nmt_country[i].nmt_frequency[j].first_frequency;
					freq += (double)(channel - nmt_country[i].nmt_frequency[j].first_channel) * nmt_country[i].nmt_frequency[j].channel_spacing;
					if (uplink)
						freq -= nmt_country[i].nmt_frequency[j].duplex_spacing;
					if (deviation_factor)
						*deviation_factor = nmt_country[i].nmt_frequency[j].deviation_factor;
					if (scandinavia)
						*scandinavia = nmt_country[i].nmt_frequency[j].scandinavia;
					if (tested)
						*tested = nmt_country[i].tested;
				 	return freq * 1e6;
				}
			}
			return 0.0;
		}
	}

	return 0.0;
}

