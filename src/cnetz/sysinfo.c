#include <stdint.h>
#include <string.h>
#include "sysinfo.h"

cnetz_si si;

void init_sysinfo(void)
{
	memset(&si, 0, sizeof(si));

	si.ogk_timeslot_mask = 0x01010101; /* 4 slots per super frame */
	si.fuz_nat = 1;
	si.fuz_fuvst = 1;
	si.fuz_rest = 38;
	si.mittel_umschalten = 5;
	si.grenz_umschalten = 0;
	si.mittel_ausloesen = 5;
	si.grenz_ausloesen = 0;
	si.sperre = 0;
	si.genauigkeit = 1; /* bedingte Genauigkeit */
	si.entfernung = 3;
	si.grenz_einbuchen = 1; /* worst case */
	si.fufst_prio = 1; /* normal pio */
	si.nachbar_prio = 0;
	si.bewertung = 1; /* pegel */
	si.reduzierung = 0;
}

