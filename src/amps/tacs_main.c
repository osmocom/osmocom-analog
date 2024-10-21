#include <stdio.h>
#include "../amps/main.h"
#include "../libmobile/main_mobile.h"

const int tacs = 1;
const int jtacs = 0;

const struct number_lengths number_lengths[] = {
	{ 10, "TACS number (AREA-XXXXXXX)" },
	{ 0, NULL }
};

const char *number_prefixes[] = {
	"0xxxxxxxxxx",
	"+44xxxxxxxxxx",
	NULL
};

int main(int argc, char *argv[])
{
	return main_amps_tacs("tacs", argc, argv, "uk");
}
