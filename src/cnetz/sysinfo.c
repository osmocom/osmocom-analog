#include <stdint.h>
#include <string.h>
#include "sysinfo.h"

cnetz_si si;

void init_sysinfo(uint32_t timeslots, uint8_t fuz_nat, uint8_t fuz_fuvst, uint8_t fuz_rest, uint8_t kennung_fufst, uint8_t bahn_bs, uint8_t authentifikationsbit, uint8_t ws_kennung, uint8_t vermittlungstechnische_sperren, uint8_t grenz_einbuchen, uint8_t grenz_umschalten, uint8_t grenz_ausloesen, uint8_t mittel_umschalten, uint8_t mittel_ausloesen, uint8_t genauigkeit, uint8_t bewertung, uint8_t entfernung, uint8_t reduzierung, uint8_t nachbar_prio, int8_t teilnehmergruppensperre, uint8_t anzahl_gesperrter_teilnehmergruppen, int meldeinterval, int meldeaufrufe)
{
	memset(&si, 0, sizeof(si));

	/* timeslot to use */
	si.timeslots = timeslots;

	/* ID of base station */
	si.fuz_nat = fuz_nat;
	si.fuz_fuvst = fuz_fuvst;
	si.fuz_rest = fuz_rest;

	/* a low value causes quicker measurement results */
	si.mittel_umschalten = mittel_umschalten; /* 0..5 */

	/* a high value is tollerant to bad quality */
	si.grenz_umschalten = grenz_umschalten; /* 0..15 */

	/* a low value causes quicker measurement results */
	si.mittel_ausloesen = mittel_ausloesen; /* 0..5 */

	/* a high value is tollerant to bad quality */
	si.grenz_ausloesen = grenz_ausloesen; /* 0..15 */

	si.vermittlungstechnische_sperren = vermittlungstechnische_sperren;

	si.genauigkeit = genauigkeit; /* 1 = bedingte Genauigkeit */

	si.entfernung = entfernung;

	/* a low value is tollerant to bad quality */
	si.grenz_einbuchen = grenz_einbuchen; /* 1..7 */

	if (bahn_bs)
		kennung_fufst = 0;
	si.kennung_fufst = kennung_fufst;
	si.bahn_bs = bahn_bs;

	si.authentifikationsbit = authentifikationsbit;

	si.ws_kennung = ws_kennung;

	si.nachbar_prio = nachbar_prio;

	si.bewertung = bewertung; /* 0 = relative entfernung, 1 = pegel */

	si.reduzierung = reduzierung;

	/* deny group of subscribers. (used to balance subscribers between base stations) */
	si.teilnehmergruppensperre = teilnehmergruppensperre;
	si.anzahl_gesperrter_teilnehmergruppen = anzahl_gesperrter_teilnehmergruppen;

	si.meldeinterval = meldeinterval;
	si.meldeaufrufe = meldeaufrufe;
}

