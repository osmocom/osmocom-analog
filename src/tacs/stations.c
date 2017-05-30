#include <stdio.h>
#include <string.h>
#include "../amps/stations.h"

/* area codes */
static struct tacs_areas {
	const char *number;		/* country code (first 3 digits of subscriber number) + area code */
	const char *carrier;		/* name of carrier */
	const char *country;		/* name of country */
	const char *national_prefix; /* digits to dial inside country */
} tacs_areas[] = {
	{ "2220",	"Telecom Italia Mobile",	"Italy",		"0336" },
	{ "2222",	"Telecom Italia Mobile",	"Italy",		"0337" },
	{ "2224",	"Telecom Italia Mobile",	"Italy",		"0330" },
	{ "2225",	"Telecom Italia Mobile",	"Italy",		"0360" },
	{ "2226",	"Telecom Italia Mobile",	"Italy",		"0368" },
	{ "2340",	"Vodafone",			"United Kingdom",	"0836" },
	{ "2341",	"Vodafone",			"United Kingdom",	"0421" },
	{ "2342",	"CellNet",			"United Kingdom",	"0860" },
	{ "2343",	"Vodafone",			"United Kingdom",	"0378" },
	{ "2344",	"Vodafone",			"United Kingdom",	"0831" },
	{ "2345",	"Vodafone",			"United Kingdom",	"0374" },
	{ "2346",	"CellNet",			"United Kingdom",	"0850" },
	{ "2347",	"CellNet",			"United Kingdom",	"0589" },
	{ "2348",	"CellNet",			"United Kingdom",	"0402" },
	{ "2349",	"CellNet",			"United Kingdom",	"0585" },
	{ NULL, NULL, NULL, NULL }
};

void numbering(const char *number, const char **carrier, const char **country, const char **national_number)
{
	int i;
	static char digits[64];

	for (i = 0; tacs_areas[i].carrier; i++) {
		if (!strncmp(number, tacs_areas[i].number, 4)) {
			*carrier = tacs_areas[i].carrier;
			*country = tacs_areas[i].country;
			if (tacs_areas[i].national_prefix) {
				strcpy(digits, tacs_areas[i].national_prefix);
				strcat(digits, number + 4);
				*national_number = digits;
			}
		}
	}
}

/*
	1: the AID, system Identification number
	2: the telephone company name
	3: the country
*/
static struct tacs_stations {
	int aid;
	const char *carrier, *country;
} tacs_stations[] = {
	{ -1,		"Telecom Italia Mobile",	"Italy" },
	{ 3600,		"CellNet",			"United Kingdom" },
	{ 2051,		"VodaFone",			"United Kingdom" },
	{ 0, NULL, NULL }
};

void list_stations(void)
{
	int i;

	for (i = 0; tacs_stations[i].carrier; i++) {
		if (tacs_stations[i].aid >= 0)
			printf("AID:%5d", tacs_stations[i].aid);
		else
			printf("AID:  ???");
		printf(" Carrier: %s, %s\n", tacs_stations[i].carrier, tacs_stations[i].country);
	}
}

void sid_stations(int aid)
{
	int i, first = 1;

	for (i = 0; tacs_stations[i].carrier; i++) {
		if (aid == tacs_stations[i].aid) {
			if (first)
				printf("Selected Area ID (AID) %d belongs to:\n", tacs_stations[i].aid);
			first = 0;
			printf("\t%s, %s\n",  tacs_stations[i].carrier, tacs_stations[i].country);
		}
	}
}

