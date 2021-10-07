#include <stdio.h>
#include "../amps/main.h"
#include "../amps/tones.h"
#include "../amps/outoforder.h"

const int tacs = 1;
const int jtacs = 0;

const char *number_prefixes[] = {
	"0xxxxxxxxxx",
	"+44xxxxxxxxxx",
	NULL
};

int main(int argc, char *argv[])
{
	/* init common tones */
	init_tones();
	init_outoforder();

	return main_amps_tacs("tacs", argc, argv);
}
