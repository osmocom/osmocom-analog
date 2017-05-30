#include "main.h"
#include "tones.h"
#include "noanswer.h"
#include "outoforder.h"
#include "invalidnumber.h"
#include "congestion.h"

const int tacs = 0;

int main(int argc, char *argv[])
{
	/* init common tones */
	init_tones();
	init_outoforder();
	init_noanswer();
	init_invalidnumber();
	init_congestion();

	return main_amps_tacs(argc, argv);
}
