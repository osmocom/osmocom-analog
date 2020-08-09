#include "../amps/main.h"
#include "../amps/tones.h"
#include "../amps/outoforder.h"

const int tacs = 1;
const int jtacs = 1;

int main(int argc, char *argv[])
{
	/* init common tones */
	init_tones();

	return main_amps_tacs("jtacs", argc, argv);
}
