#include <stdio.h>
#include <string.h>
#include "../amps/stations.h"

void numbering(const char __attribute__((unused)) *number, const char __attribute__((unused)) **carrier, const char **country, const char __attribute__((unused)) **national_number)
{
	*country = "Japan";
}

void list_stations(void)
{
	printf("Unknown for JTACS\n");
}

void sid_stations(int __attribute__((unused)) aid)
{
}

