#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "stations.h"

static struct anetz_stations {
	const char	*standort;
	int		kanal21; /* Channel for prefix 21 */
	int		kanal22; /* Channel for prefix 22 (0 = use previous channel) */
	int		kanal23; /* ... */
	int		kanal24;
	int		kanal25;
	const char	*coordinates;
} anetz_stations[] = {
/*	  Standort			Prefix 21..25 	Coorinates */
	{ "Niebuell",			32, 0, 0, 0, 0,	"54o47 08o50" },
	{ "Flensburg",			39, 0, 0, 0, 0,	"54o47 09o26" },
	{ "Schleswig",			35, 0, 0, 0, 0,	"54o31 09o34" },
	{ "Kiel",			36, 0, 0,38, 0,	"54o20 10o08" },
	{ "Toenning",			37, 0, 0, 0, 0,	"54o19 08o57" },
	{ "Heide",			34, 0, 0, 0, 0,	"54o12 09o06" },
	{ "Eutin",			30, 0, 0, 0, 0,	"54o08 10o37" },
	{ "Neumuenster",		32, 0, 0, 0, 0,	"54o04 09o59" },
	{ "Luebeck",			37, 0, 0,42, 0,	"53o52 10o41" },
	{ "Cuxhaven",			30, 0, 0, 0, 0,	"53o52 08o41" },
	{ "Hemmoor",			40, 0, 0, 0, 0,	"53o42 09o08" },
	{ "Ratzeburg",			40, 0, 0, 0, 0,	"53o42 10o45" },
	{ "Hamburg",			39,35,31,41,33,	"53o34 10o00" },
	{ "Bremerhaven",		39, 0, 0,41, 0,	"53o33 08o35" },
	{ "Wilhelmshaven",		36, 0, 0, 0, 0,	"53o31 08o08" },
	{ "Aurich",			32, 0, 0, 0, 0,	"53o28 07o29" },
	{ "Lueneburg",			32, 0, 0, 0, 0,	"53o15 10o25" },
	{ "Leer",			35, 0, 0, 0, 0,	"53o14 07o27" },
	{ "Oldenburg",			30, 0, 0,31, 0,	"53o09 08o13" },
	{ "Rotenburg",			30, 0, 0, 0, 0,	"53o06 09o24" },
	{ "Bremen",			37,44,33,42, 0,	"53o05 08o48" },
	{ "Soltau",			35, 0, 0, 0, 0,	"52o59 09o50" },
	{ "Luechow",			30, 0, 0, 0, 0,	"52o58 11o09" },
	{ "Uelzen",			34, 0, 0, 0, 0,	"52o58 10o33" },
	{ "Werlte",			34, 0, 0, 0, 0,	"52o51 07o41" },
	{ "Hoya",			32, 0, 0, 0, 0,	"52o48 09o08" },
	{ "Lingen",			36, 0, 0, 0, 0,	"52o31 07o19" },
	{ "Berlin",			39,33,35,41, 0,	"52o31 13o23" },
	{ "Wolfsburg",			30, 0, 0, 0, 0,	"52o25 10o47" },
	{ "Hannover",			36,44,33,38, 0,	"52o22 09o43" },
	{ "Osnabrueck",			39, 0, 0,41, 0,	"52o17 08o03" },
	{ "Minden",			30, 0, 0,40, 0,	"52o17 08o55" },
	{ "Braunschweig",		37, 0, 0,42, 0,	"52o16 10o31" },
	{ "Burgsteinfurt",		37, 0, 0, 0, 0,	"52o09 07o21" },
	{ "Bielefeld",			34,33, 0,43, 0,	"52o01 08o31" },
	{ "Alfeld",			39, 0, 0, 0, 0,	"51o59 09o50" },
	{ "Muenster",			32, 0, 0,42, 0,	"51o58 07o38" },
	{ "Kleve",			32, 0,31,38, 0,	"51o47 06o08" },
	{ "Paderborn",			37, 0, 0, 0, 0,	"51o43 08o46" },
	{ "Wesel",			45, 0, 0, 0, 0,	"51o40 06o37" },
	{ "Goettingen",			34, 0, 0, 0, 0,	"51o32 09o56" },
	{ "Dortmund",			36, 0, 31,38,0,	"51o31 07o28" },
	{ "Essen",			34, 0, 0, 0,43,	"51o27 07o01" },
	{ "Duisburg",			39,44,33,41, 0,	"51o26 06o46" },
	{ "Meschede",			35, 0, 0, 0, 0,	"51o21 08o17" },
	{ "Kassel",			32, 0, 0, 0, 0,	"51o19 09o30" },
	{ "Wuppertal",			30, 0, 0, 0,40,	"51o16 07o11" },
	{ "Korbach",			38, 0, 0, 0, 0,	"51o17 08o52" },
	{ "Luedenscheid",		39, 0, 0, 0, 0,	"51o13 07o38" },
	{ "Winterberg",			33, 0, 0, 0, 0,	"51o12 08o31" },
	{ "Duesseldorf",		37, 0, 31,42,0,	"51o14 06o47" },
	{ "Eschwege",			39, 0, 0, 0, 0,	"51o11 10o03" },
	{ "Koeln",			36,35,33,38, 0,	"50o56 06o57" },
	{ "Siegen",			36, 0, 0, 0, 0,	"50o53 08o01" },
	{ "Aachen",			34, 0, 0, 0, 0,	"50o47 06o05" },
	{ "Alsfeld",			36, 0, 0,40, 0,	"50o45 09o16" },
	{ "Bonn",			32, 0, 0,41, 0,	"50o44 07o06" },
	{ "Dillenburg",			42, 0, 0, 0, 0,	"50o44 08o17" },
	{ "Giessen",			34, 0, 0, 0, 0,	"50o35 08o40" },
	{ "Fulda",			30, 0, 0, 0, 0,	"50o33 09o41" },
	{ "Montabaur",			46, 0, 0, 0, 0,	"50o26 07o50" },
	{ "Usingen",			30, 0, 0, 0, 0,	"50o20 08o32" },
	{ "Mayen",			34, 0, 0, 0, 0,	"50o20 07o13" },
	{ "Hof",			39, 0, 0, 0, 0,	"50o19 11o55" },
	{ "Coburg",			35, 0, 0, 0, 0,	"50o16 10o58" },
	{ "Boppard",			39, 31,0, 35,0,	"50o14 07o35" },
	{ "Koblenz",			40, 0, 0, 0, 0,	"50o22 07o36" },
	{ "Pruem",			35, 0, 0, 0, 0,	"50o12 06o25" },
	{ "Gelnhausen",			41, 0, 0, 0, 0,	"50o12 09o10" },
	{ "Wiesbaden",			30, 0, 0, 0,40,	"50o05 08o14" },
	{ "Hanau",			39, 0, 0, 0, 0,	"50o08 08o55" },
	{ "Frankfurt",			37,35,31,42,33,	"50o07 08o41" },
	{ "Schweinfurt",		34, 0, 0, 0, 0,	"50o03 10o14" },
	{ "Aschaffenburg",		45, 0, 0, 0, 0,	"49o58 09o09" },
	{ "Bingen",			36, 0, 0,41, 0,	"49o58 07o54" },
	{ "Bayreuth",			30, 0, 0, 0, 0,	"49o57 11o35" },
	{ "Bernkastel",			37, 0, 0, 0, 0,	"49o55 07o04" },
	{ "Bamberg",			37, 0, 0, 0, 0,	"49o54 10o54" },
	{ "Darmstadt",			32, 0, 0,38, 0,	"49o52 08o39" },
	{ "Marktheidenfeld",		36, 0, 0, 0, 0,	"49o51 09o36" },
	{ "Wuerzburg",			39, 0, 0,44, 0,	"49o47 09o56" },
	{ "Pegnitz",			34, 0, 0, 0, 0,	"49o45 11o34" },
	{ "Trier",			39, 0, 0, 0, 0,	"49o45 06o38" },
	{ "Kitzingen",			32, 0, 0, 0, 0,	"49o44 10o10" },
	{ "Idar-Oberstein",		32, 0, 0, 0, 0,	"49o43 07o19" },
	{ "Lindenfels",			34, 0, 0, 0, 0,	"49o41 08o47" },
	{ "Weiden",			32, 0, 0, 0, 0,	"49o40 12o09" },
	{ "Mannheim",			39,35,44,41, 0,	"49o29 08o28" },
	{ "Fuerth",			44, 0, 0, 0, 0,	"49o28 11o00" },
	{ "Kaiserslautern",		34, 0, 0,43, 0,	"49o27 07o46" },
	{ "Nuernberg",			39, 0, 33,41,0,	"49o27 11o05" },
	{ "Heidelberg",			37, 0, 0,42, 0,	"49o25 08o43" },
	{ "Ansbach",			30, 0, 0, 0, 0,	"49o18 10o35" },
	{ "Neumarkt",			35, 0, 0, 0, 0,	"49o17 11o28" },
	{ "Saarbruecken",		30, 0, 0,40, 0,	"49o14 07o00" },
	{ "Heilbronn",			32, 0, 0,43, 0,	"49o09 09o13" },
	{ "Schwaebisch Hall",		41, 0, 0, 0, 0,	"49o07 09o44" },
	{ "Weissenburg",		36, 0, 0, 0, 0,	"49o02 10o58" },
	{ "Regensburg",			30, 0, 0, 0, 0,	"49o01 12o05" },
	{ "Karlsruhe",			30, 0, 0, 0,40,	"49o01 08o24" },
	{ "Pforzheim",			46, 0, 0, 0, 0,	"48o54 08o43" },
	{ "Deggendorf",			37, 0, 0, 0, 0,	"48o50 12o58" },
	{ "Schwaebisch Gmuend",		34, 0, 0, 0, 0,	"48o48 09o48" },
	{ "Stuttgart",			35,44,33,38,40,	"48o47 09o11" },
	{ "Calw",			31, 0, 0, 0, 0,	"48o43 08o44" },
	{ "Geislingen",			35, 0, 0, 0, 0,	"48o37 09o50" },
	{ "Passau",			36, 0, 0, 0, 0,	"48o34 13o28" },
	{ "Landshut",			34, 0, 0, 0, 0,	"48o32 12o09" },
	{ "Pfaffenhofen",		32, 0, 0, 0, 0,	"48o32 11o31" },
	{ "Tuebingen",			37, 0, 0, 0, 0,	"48o31 09o03" },
	{ "Offenburg",			32, 0, 0,38, 0,	"48o28 07o56" },
	{ "Freudenstadt",		39, 0, 0, 0, 0,	"48o28 08o25" },
	{ "Pfarrkirchen",		35, 0, 0, 0, 0,	"48o25 12o55" },
	{ "Ulm",			39, 0, 0,41, 0,	"48o24 09o59" },
	{ "Augsburg",			37, 0, 0,42, 0,	"48o22 10o54" },
	{ "Rottweil",			34, 0, 0,43, 0,	"48o10 08o37" },
	{ "Riedlingen",			33, 0, 0, 0, 0,	"48o09 09o28" },
	{ "Muenchen",			39,35,33,41,44,	"48o08 11o34" },
	{ "Wasserburg",			36, 0, 0,42, 0,	"48o03 12o14" },
	{ "Freiburg",			37, 0, 0,31, 0,	"47o59 07o51" },
	{ "Donaueschingen",		37, 0, 0, 0, 0,	"47o57 08o30" },
	{ "Neustadt",			30, 0, 0, 0, 0,	"47o55 08o13" },
	{ "Weilheim",			30, 0, 0, 0, 0,	"47o50 11o09" },
	{ "Ravensburg",			30, 0, 0, 0, 0,	"47o47 09o37" },
	{ "Singen",			31, 0, 0, 0, 0,	"47o46 08o50" },
	{ "Bad Toelz",			37, 0, 0, 0, 0,	"47o46 11o33" },
	{ "Kempten",			34, 0, 0,43, 0,	"47o44 10o19" },
	{ "Bad Reichenhall",		41, 0, 0, 0, 0,	"47o43 12o53" },
	{ "Loerrach",			44, 0, 0, 0, 0,	"47o37 07o40" },
	{ "Saeckingen",			34, 0, 0, 0, 0,	"47o33 07o57" },
	{ "Garmisch-Partenkirchen",	36, 0, 0, 0, 0,	"47o30 11o05" },
	{ NULL, 0, 0, 0, 0, 0, NULL }
};

static double lat_from_coordinates(const char *string)
{
	if (strlen(string) != 11)
		abort();
	if (string[0] < '0' || string[0] > '9')
		abort();
	if (string[1] < '0' || string[1] > '9')
		abort();
	if (string[2] != 'o')
		abort();
	if (string[3] < '0' || string[3] > '9')
		abort();
	if (string[4] < '0' || string[4] > '9')
		abort();

	return	(double)(string[0] - '0') * 10.0 +
		(double)(string[1] - '0') +
		(double)(string[3] - '0') / 6.0 +
		(double)(string[4] - '0') / 60.0;
}

static double lon_from_coordinates(const char *string)
{
	if (strlen(string) != 11)
		abort();
	if (string[6] < '0' || string[6] > '9')
		abort();
	if (string[7] < '0' || string[7] > '9')
		abort();
	if (string[8] != 'o')
		abort();
	if (string[9] < '0' || string[9] > '9')
		abort();
	if (string[10] < '0' || string[10] > '9')
		abort();

	return	(double)(string[6] - '0') * 10.0 +
		(double)(string[7] - '0') +
		(double)(string[9] - '0') / 6.0 +
		(double)(string[10] - '0') / 60.0;
}

#define EQUATOR_RADIUS  6378137.0
#define POLE_RADIUS     6356752.314

#define PI              M_PI

void station_list(void)
{
	int i;

	printf("List of all base stations:\n");
	for (i = 0; anetz_stations[i].standort; i++) {
		printf("%s (%.2f deg N %.2f deg E)\n", anetz_stations[i].standort, lat_from_coordinates(anetz_stations[i].coordinates), lon_from_coordinates(anetz_stations[i].coordinates));
		if (anetz_stations[i].kanal21) {
			if (anetz_stations[i].kanal22)
				printf("\tPrefix 21: Channel %d\n", anetz_stations[i].kanal21);
			else if (anetz_stations[i].kanal23)
				printf("\tPrefix 21-22: Channel %d\n", anetz_stations[i].kanal21);
			else if (anetz_stations[i].kanal24)
				printf("\tPrefix 21-23: Channel %d\n", anetz_stations[i].kanal21);
			else if (anetz_stations[i].kanal25)
				printf("\tPrefix 21-24: Channel %d\n", anetz_stations[i].kanal21);
			else
				printf("\tPrefix 21-25: Channel %d\n", anetz_stations[i].kanal21);
		}
		if (anetz_stations[i].kanal22) {
			if (anetz_stations[i].kanal23)
				printf("\tPrefix 22: Channel %d\n", anetz_stations[i].kanal22);
			else if (anetz_stations[i].kanal24)
				printf("\tPrefix 22-23: Channel %d\n", anetz_stations[i].kanal22);
			else if (anetz_stations[i].kanal25)
				printf("\tPrefix 22-24: Channel %d\n", anetz_stations[i].kanal22);
			else
				printf("\tPrefix 22-25: Channel %d\n", anetz_stations[i].kanal22);
		}
		if (anetz_stations[i].kanal23) {
			if (anetz_stations[i].kanal24)
				printf("\tPrefix 23: Channel %d\n", anetz_stations[i].kanal23);
			else if (anetz_stations[i].kanal25)
				printf("\tPrefix 23-24: Channel %d\n", anetz_stations[i].kanal23);
			else
				printf("\tPrefix 23-25: Channel %d\n", anetz_stations[i].kanal23);
		}
		if (anetz_stations[i].kanal24) {
			if (anetz_stations[i].kanal25)
				printf("\tPrefix 24: Channel %d\n", anetz_stations[i].kanal24);
			else
				printf("\tPrefix 24-25: Channel %d\n", anetz_stations[i].kanal24);
		}
		if (anetz_stations[i].kanal25)
			printf("\tPrefix 25: Channel %d\n", anetz_stations[i].kanal25);
	}
}

/* convert geo coordinates (lon/lat) to space coordinates in meters (x/y/z) */
static void geo2space(double *x, double *y, double *z, double lat, double lon)
{
	*z = sin(lat / 180.0 * PI) * POLE_RADIUS;
	*x = sin(lon / 180.0 * PI) * cos(lat / 180.0 * PI) * EQUATOR_RADIUS;
	*y = -cos(lon / 180.0 * PI) * cos(lat / 180.0 * PI) * EQUATOR_RADIUS;
}

/* calculate distance */
static double distinspace(double x1, double y1, double z1, double x2, double y2, double z2)
{
	double x = x1 - x2;
	double y = y1 - y2;
	double z = z1 - z2;

	return sqrt(x * x + y * y + z * z);
}


int get_station_by_coordinates(double lat, double lon)
{
	double dist, min = 0.0;
	int i, min_i = 0;
	double x, y, z;
	double s_lat, s_lon;
	double s_x, s_y, s_z;
	int kanal[5];

	geo2space(&x, &y, &z, lat, lon);

	for (i = 0; anetz_stations[i].standort; i++) {
		s_lat = lat_from_coordinates(anetz_stations[i].coordinates);
		s_lon = lon_from_coordinates(anetz_stations[i].coordinates);
		geo2space(&s_x, &s_y, &s_z, s_lat, s_lon);
		dist = distinspace(x, y, z, s_x, s_y, s_z);
		if (i == 0 || dist < min) {
			min = dist;
			min_i = i;
		}
	}

	/* don't allow distance more than 100KM */
	if (min > 100000) {
		fprintf(stderr, "Given coordinates are more than 100 km away from base station.\n");
		return 0;
	}
	kanal[0] = anetz_stations[min_i].kanal21;
	kanal[1] = anetz_stations[min_i].kanal22;
	kanal[2] = anetz_stations[min_i].kanal23;
	kanal[3] = anetz_stations[min_i].kanal24;
	kanal[4] = anetz_stations[min_i].kanal25;

	for (i = 0; i < 5; i++) {
		if (i && kanal[i] == 0)
			kanal[i] = kanal[i - 1];
	}

	printf("Closest base station: %s (distance = %.2f km)\n", anetz_stations[min_i].standort, min / 1000.0);
	printf("Frequencies allocated in your area were:\n");
	for (i = 0; i < 5; i++)
		printf("  Channel %d for phone numbers '%dxxxxx'\n", kanal[i], i + 21);

	return 0;
}

