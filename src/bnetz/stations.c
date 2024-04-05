#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "stations.h"

static struct bnetz_stations {
	const char	*standort;
	int		gfs;
	const char	*coordinates;
} bnetz_stations[] = {
/*	  Standort			GFS 	Koorinaten */
	/* Germany */
	{ "@Germany",			0,	"" },
	{ "Westerland",			3,	"54o54 08o19" },
	{ "Flensburg",			8,	"54o47 09o26" },
	{ "Bredstedt",			2,	"54o37 08o58" },
	{ "Eckernfoerde",		5,	"54o28 09o50" },
	{ "Kiel",			6,	"54o20 10o08" },
	{ "Heide",			7,	"54o12 09o06" },
	{ "Eutin",			2,	"54o08 10o37" },
	{ "Neumuenster",		1,	"54o04 09o59" },
	{ "Luebeck Nord",		5,	"53o54 10o38" },
	{ "Cuxhaven",			6,	"53o52 08o41" },
	{ "Luebeck Sued",		8,	"53o52 10o41" },
	{ "Kaltenkirchen",		4,	"53o50 09o58" },
	{ "Hemmoor",			8,	"53o42 09o08" },
	{ "Hamburg",			9,	"53o34 10o00" },
	{ "Bremerhaven",		2,	"53o33 08o35" },
	{ "Wilhelmshaven",		1,	"53o31 08o08" },
	{ "Emden",			9,	"53o22 07o12" },
	{ "Zeven",			5,	"53o18 09o17" },
	{ "Lueneburg",			7,	"53o15 10o25" },
	{ "Leer",			3,	"53o14 07o27" },
	{ "Oldenburg",			8,	"53o09 08o13" },
	{ "Bremen",			7,	"53o05 08o48" },
	{ "Uelzen",			4,	"52o58 10o33" },
	{ "Luechow",			3,	"52o58 11o09" },
	{ "Werlte",			2,	"52o51 07o41" },
	{ "Bergen",			2,	"52o49 09o58" },
	{ "Hoya",			3,	"52o48 09o08" },
	{ "Vechta",			6,	"52o44 08o17" },
	{ "Lingen",			7,	"52o31 07o19" },
	{ "Berlin Nord",		1,	"52o31 13o23" },
	{ "Berlin Sued",		2,	"52o30 13o23" },
	{ "Wolfsburg",			5,	"52o25 10o47" },
	{ "Hannover",			8,	"52o22 09o43" },
	{ "Osnabrueck",			4,	"52o17 08o03" },
	{ "Minden",			5,	"52o17 08o55" },
	{ "Braunschweig",		1,	"52o16 10o31" },
	{ "Burgsteinfurt",		3,	"52o09 07o21" },
	{ "Hildesheim",			7,	"52o09 09o57" },
	{ "Bielefeld",			7,	"52o01 08o31" },
	{ "Alfeld",			6,	"51o59 09o50" },
	{ "Muenster",			6,	"51o58 07o38" },
	{ "Kleve",			4,	"51o47 06o08" },
	{ "Recklinghausen",		5,	"51o35 07o10" },
	{ "Wesel",			8,	"51o40 06o37" },
	{ "Uslar",			4,	"51o40 09o38" },
	{ "Soest",			2,	"51o34 08o07" },
	{ "Goettingen",			5,	"51o32 09o56" },
	{ "Warburg",			3,	"51o30 09o10" },
	{ "Dortmund",			1,	"51o31 07o28" },
	{ "Essen",			7,	"51o27 07o01" },
	{ "Duisburg",			9,	"51o26 06o46" },
	{ "Meschede",			8,	"51o21 08o17" },
	{ "Kassel",			7,	"51o19 09o30" },
	{ "Wuppertal",			3,	"51o16 07o11" },
	{ "Luedenscheid",		4,	"51o13 07o38" },
	{ "Duesseldorf",		2,	"51o14 06o47" },
	{ "Eschwege",			9,	"51o11 10o03" },
	{ "Schmallenberg-Dorlar",	5,	"51o08 08o18" },
	{ "Bad Wildungen",		1,	"51o07 09o07" },
	{ "Gummersbach",		9,	"51o02 07o34" },
	{ "Koeln",			5,	"50o56 06o57" },
	{ "Biedenkopf",			8,	"50o55 08o32" },
	{ "Siegen",			7,	"50o53 08o01" },
	{ "Bad Hersfeld",		2,	"50o52 09o42" },
	{ "Dueren",			6,	"50o48 06o29" },
	{ "Siegburg",			17,	"50o48 07o12" },
	{ "Aachen",			4,	"50o47 06o05" },
	{ "Bonn",			8,	"50o44 07o06" },
	{ "Bad Marienberg",		15,	"50o39 07o57" },
	{ "Lauterbach",			4,	"50o38 09o24" },
	{ "Lahn-Giessen",		3,	"50o35 08o40" },
	{ "Fulda",			6,	"50o33 09o41" },
	{ "Limburg",			2,	"50o23 08o04" },
	{ "Koblenz",			9,	"50o22 07o36" },
	{ "Mayen",			1,	"50o20 07o13" },
	{ "Friedberg",			9,	"50o20 08o45" },
	{ "Bad Brueckenau",		5,	"50o19 09o47" },
	{ "Bad Neustadt",		8,	"50o19 10o13" },
	{ "Hof / Saale",		4,	"50o19 11o55" },
	{ "Bad Koenigshofen",		1,	"50o18 10o25" },
	{ "Coburg",			2,	"50o16 10o58" },
	{ "Pruem",			7,	"50o12 06o25" },
	{ "St. Gorshausen",		4,	"50o09 07o43" },
	{ "Hanau",			7,	"50o08 08o55" },
	{ "Frankfurt",			6,	"50o07 08o41" },
	{ "Wiesbaden",			5,	"50o05 08o14" },
	{ "Bingen",			7,	"49o58 07o54" },
	{ "Aschaffenburg",		8,	"49o58 09o09" },
	{ "Bamberg",			5,	"49o54 10o54" },
	{ "Darmstadt",			1,	"49o52 08o39" },
	{ "Marktheidenfeld",		2,	"49o51 09o36" },
	{ "Wuerzburg",			9,	"49o47 09o56" },
	{ "Trier",			5,	"49o45 06o38" },
	{ "Pegnitz",			1,	"49o45 11o34" },
	{ "Kitzingen",			4,	"49o44 10o10" },
	{ "Idar-Oberstein",		8,	"49o43 07o19" },
	{ "Mannheim",			3,	"49o29 08o28" },
	{ "Kaiserslautern",		2,	"49o27 07o46" },
	{ "Nuernberg",			9,	"49o27 11o05" },
	{ "Heidelberg",			4,	"49o25 08o43" },
	{ "Ansbach",			2,	"49o18 10o35" },
	{ "Saarbruecken",		3,	"49o14 07o00" },
	{ "Heilbronn",			7,	"49o09 09o13" },
	{ "Schwaebisch Hall",		4,	"49o07 09o44" },
	{ "Karlsruhe",			9,	"49o01 08o24" },
	{ "Regensburg",			7,	"49o01 12o05" },
	{ "Pforzheim",			6,	"48o54 08o43" },
	{ "Deggendorf",			1,	"48o50 12o58" },
	{ "Schwaebisch Gmuend",		1,	"48o48 09o48" },
	{ "Stuttgart",			8,	"48o47 09o11" },
	{ "Rottenburg a.d.L.",		16,	"48o42 12o02" },
	{ "Geislingen",			3,	"48o37 09o50" },
	{ "Passau",			8,	"48o34 13o28" },
	{ "Pfaffenhofen",		1,	"48o32 11o31" },
	{ "Landshut",			3,	"48o32 12o09" },
	{ "Offenburg",			4,	"48o28 07o56" },
	{ "Pfarrkirchen",		6,	"48o25 12o55" },
	{ "Ulm",			6,	"48o24 09o59" },
	{ "Augsburg",			8,	"48o22 10o54" },
	{ "Rottweil",			7,	"48o10 08o37" },
	{ "Riedlingen",			1,	"48o09 09o28" },
	{ "Muenchen",			4,	"48o08 11o34" },
	{ "Wasserburg",			5,	"48o03 12o14" },
	{ "Wittlich",			6,	"49o59 06o53" },
	{ "Traben-Trarbach",		3,	"49o57 07o07" },
	{ "Bayreuth",			6,	"49o57 11o35" },
	{ "Volkach",			4,	"49o52 10o13" },
	{ "Bad Kreuznach",		9,	"49o51 07o52" },
	{ "Weiden",			8,	"49o40 12o09" },
	{ "Wadern",			1,	"49o31 06o52" },
	{ "Buchen",			1,	"49o31 09o19" },
	{ "Bad Mergentheim",		5,	"49o30 09o46" },
	{ "Amberg",			2,	"49o27 11o51" },
	{ "Rothenburg",			6,	"49o23 10o11" },
	{ "Roth",			3,	"49o15 11o05" },
	{ "Pirmasens",			5,	"49o12 07o36" },
	{ "Crailsheim",			8,	"49o08 10o04" },
	{ "Hemau",			4,	"49o03 11o47" },
	{ "Regen",			9,	"48o58 13o08" },
	{ "Backnang",			9,	"48o57 09o26" },
	{ "Baden-Baden",		1,	"48o46 08o14" },
	{ "Ingolstadt",			17,	"48o46 11o25" },
	{ "Wildbad",			2,	"48o45 08o33" },
	{ "Donauwoerth",		5,	"48o42 10o48" },
	{ "Heidenheim",			7,	"48o41 10o09" },
	{ "Markt Schwaben",		9,	"48o11 11o52" },
	{ "Mindelheim",			3,	"48o02 10o28" },
	{ "Freiburg",			5,	"47o59 07o51" },
	{ "Donaueschingen",		2,	"47o57 08o30" },
	{ "Todtnau",			6,	"47o50 07o57" },
	{ "Weilheim",			6,	"47o50 11o09" },
	{ "Ravensburg",			4,	"47o47 09o37" },
	{ "Bad Toelz",			7,	"47o46 11o33" },
	{ "Kempten",			9,	"47o44 10o19" },
	{ "Konstanz",			8,	"47o40 09o11" },
	{ "Bad Reichenhall",		2,	"47o43 12o53" },
	{ "Loerrach",			7,	"47o37 07o40" },
	{ "Saeckingen",			1,	"47o33 07o57" },
	{ "Garmisch-Partenkirchen",	2,	"47o30 11o05" },

	/* Austria */
	{ "@Aaustria",			0,	"" },
	{ "Linz",			1,	"48o18 14o17" },
	{ "Amstetten",			2,	"48o07 14o52" },
	{ "St. Poelten",		3,	"48o12 15o37" },
	{ "Wien-West",			1,	"48o11 16o21" },
	{ "Wien-Ost",			2,	"48o13 16o23" },
	{ "WR-Neustadt",		4,	"48o14 16o24" },
	{ "Voecklamarkt",		4,	"48o00 13o29" },
	{ "Gmunden",			3,	"47o55 13o48" },
	{ "Schwaz",			8,	"47o21 11o42" },
	{ "Salzburg",			3,	"47o48 13o02" },
	{ "Innsbruck",			1,	"47o16 11o23" },
	{ "Bruck",			3,	"47o25 15o16" },
	{ "Graz",			5,	"47o04 15o26" },

	/* Luxemburg */
	{ "@Luxemburg",			0,	"" },
	{ "Neidhausen",			9,	"50o02 06o04" },
	{ "Luxemburg",			9,	"49o37 06o08" },

	/* Netherlands */
	{ "@The Netherlands",		0,	"" },
	{ "Groningen",			8,	"53o13 06o34" },
	{ "Leeuwarden",			2,	"53o12 05o48" },
	{ "Winschoten",			1,	"53o09 07o02" },
	{ "Smilde",			5,	"52o57 06o27" },
	{ "Tjerkgaast",			4,	"52o54 05o41" },
	{ "Wieringermeer",		8,	"52o51 05o02" },
	{ "Coevorden",			9,	"52o40 06o44" },
	{ "Alkmaar",			9,	"52o38 04o45" },
	{ "Lelystad",			3,	"52o31 05o28" },
	{ "Zwolle",			6,	"52o31 06o05" },
	{ "Amsterdam",			5,	"52o22 04o53" },
	{ "Markelo",			2,	"52o15 06o42" },
	{ "Ugchelen",			8,	"52o10 05o54" },
	{ "Utrecht",			1,	"52o05 05o07" },
	{ "'s-Gravenzande",		6,	"52o00 04o10" },
	{ "Rotterdam",			4,	"51o56 04o29" },
	{ "Megen",			7,	"51o49 05o34" },
	{ "Loon op Zand",		9,	"51o38 05o05" },
	{ "Roosendaal",			2,	"51o32 04o27" },
	{ "Goes",			5,	"51o30 03o53" },
	{ "Mierlo",			5,	"51o26 05o37" },
	{ "Venlo",			1,	"51o22 06o10" },
	{ "Eys",			3,	"50o50 05o55" },

	{ NULL,				0,	NULL }
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

	printf("List of all base stations: (from north to south)\n");
	for (i = 0; bnetz_stations[i].standort; i++) {
		if (bnetz_stations[i].standort[0] == '@') {
			printf("\nIn %s:\n", bnetz_stations[i].standort + 1);
			continue;
		}
		printf("%s%s GFS %2d   (%5.2f deg N %5.2f deg E)\n",
			bnetz_stations[i].standort, "                                " + strlen(bnetz_stations[i].standort),
			bnetz_stations[i].gfs,
			lat_from_coordinates(bnetz_stations[i].coordinates),
			lon_from_coordinates(bnetz_stations[i].coordinates));
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
	double dist, min = -1.0; /* -1 means: unset */
	int i, min_i = 0;
	double x, y, z;
	double s_lat, s_lon;
	double s_x, s_y, s_z;

	geo2space(&x, &y, &z, lat, lon);

	for (i = 0; bnetz_stations[i].standort; i++) {
		if (bnetz_stations[i].standort[0] == '@')
			continue;
		s_lat = lat_from_coordinates(bnetz_stations[i].coordinates);
		s_lon = lon_from_coordinates(bnetz_stations[i].coordinates);
		geo2space(&s_x, &s_y, &s_z, s_lat, s_lon);
		dist = distinspace(x, y, z, s_x, s_y, s_z);
		/* if unset or if less than last distance */
		if (min == -1.0 || dist < min) {
			min = dist;
			min_i = i;
		}
	}

	/* don't allow distance more than 100KM */
	if (min > 100000) {
		fprintf(stderr, "Given coordinates are more than 100 km away from base station.\n");
		return 0;
	}
	printf("Closest base station: %s (distance = %.2f km)\n", bnetz_stations[min_i].standort, min / 1000.0);
	printf("  Gruppenfreisignal (GFS) = %d\n", bnetz_stations[min_i].gfs);

	return bnetz_stations[min_i].gfs;
}

