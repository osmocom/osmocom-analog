#include <stdio.h>
#include "main.h"
#include "tones.h"
#include "noanswer.h"
#include "outoforder.h"
#include "invalidnumber.h"
#include "congestion.h"
#include "../libmobile/main_mobile.h"

const int tacs = 0;
const int jtacs = 0;

const struct number_lengths number_lengths[] = {
	{ 10, "AMPS number (NPA-XXX-XXXX)" },
	{ 0, NULL }
};

const char *number_prefixes[] = {
	"1xxxxxxxxxx",
	"+1xxxxxxxxxx",
	NULL
};

int main(int argc, char *argv[])
{
	/* init common tones */
	init_tones();
	init_outoforder();
	init_noanswer();
	init_invalidnumber();
	init_congestion();

	return main_amps_tacs("amps", argc, argv);
}
