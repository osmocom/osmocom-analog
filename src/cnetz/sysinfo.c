#include <stdint.h>
#include <string.h>
#include "sysinfo.h"

cnetz_si si[2];

void init_sysinfo(void)
{
	memset(&si, 0, sizeof(si));

	si[0].flip_polarity = 0;
	si[1].flip_polarity = 1;

	si[0].fuz_nat = 1;
	si[1].fuz_nat = 1;

	si[0].fuz_fuvst = 1;
	si[1].fuz_fuvst = 1;

	si[0].fuz_rest = 38;
	si[1].fuz_rest = 39;

	si[0].mittel_umschalten = 5;
	si[1].mittel_umschalten = 5;

	si[0].grenz_umschalten = 0;
	si[1].grenz_umschalten = 0;

	si[0].mittel_ausloesen = 5;
	si[1].mittel_ausloesen = 5;

	si[0].grenz_ausloesen = 0;
	si[1].grenz_ausloesen = 0;

	si[0].sperre = 0;
	si[1].sperre = 0;

	si[0].genauigkeit = 1; /* bedingte Genauigkeit */
	si[1].genauigkeit = 1; /* bedingte Genauigkeit */

	si[0].entfernung = 3;
	si[1].entfernung = 3;

	si[0].grenz_einbuchen = 1; /* worst case */
	si[1].grenz_einbuchen = 1; /* worst case */

	si[0].fufst_prio = 1; /* normal pio */
	si[1].fufst_prio = 1; /* normal pio */

	si[0].nachbar_prio = 0;
	si[1].nachbar_prio = 0;

	si[0].bewertung = 1; /* pegel */
	si[1].bewertung = 1; /* pegel */

	si[0].reduzierung = 0;
	si[1].reduzierung = 0;
}

