#include "../amps/main.h"
#include "../amps/tones.h"
#include "../amps/outoforder.h"

const int tacs = 1;

int main(int argc, char *argv[])
{
	/* init common tones */
	init_tones();
	init_outoforder();

	return main_amps_tacs(argc, argv);
}
