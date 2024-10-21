#include <stdio.h>
#include "../amps/main.h"
#include "../libmobile/main_mobile.h"

const int tacs = 1;
const int jtacs = 1;

const struct number_lengths number_lengths[] = {
	{ 10, "JTACS number (440-XXXXXXX)" },
	{ 0, NULL }
};

const char *number_prefixes[] = { NULL };

int main(int argc, char *argv[])
{
	return main_amps_tacs("jtacs", argc, argv, "japan");
}
