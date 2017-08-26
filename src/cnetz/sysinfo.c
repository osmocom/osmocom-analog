#include <stdint.h>
#include <string.h>
#include "sysinfo.h"

cnetz_si si[2];

void init_sysinfo(uint8_t fuz_nat, uint8_t fuz_fuvst, uint8_t fuz_rest, uint8_t kennung_fufst, uint8_t ws_kennung, uint8_t vermittlungstechnische_sperren, uint8_t grenz_einbuchen, uint8_t grenz_umschalten, uint8_t grenz_ausloesen, uint8_t mittel_umschalten, uint8_t mittel_ausloesen, uint8_t genauigkeit, uint8_t bewertung, uint8_t entfernung, uint8_t reduzierung, uint8_t nachbar_prio, int8_t teilnehmergruppensperre, uint8_t anzahl_gesperrter_teilnehmergruppen)
{
	memset(&si[0], 0, sizeof(cnetz_si));

	/* polarity of TX signal */
	si[0].flip_polarity = 0;

	/* ID of base station */
	si[0].fuz_nat = fuz_nat;
	si[0].fuz_fuvst = fuz_fuvst;
	si[0].fuz_rest = fuz_rest;

	/* a low value causes quicker measurement results */
	si[0].mittel_umschalten = mittel_umschalten; /* 0..5 */

	/* a high value is tollerant to bad quality */
	si[0].grenz_umschalten = grenz_umschalten; /* 0..15 */

	/* a low value causes quicker measurement results */
	si[0].mittel_ausloesen = mittel_ausloesen; /* 0..5 */

	/* a high value is tollerant to bad quality */
	si[0].grenz_ausloesen = grenz_ausloesen; /* 0..15 */

	si[0].vermittlungstechnische_sperren = vermittlungstechnische_sperren;

	si[0].genauigkeit = genauigkeit; /* 1 = bedingte Genauigkeit */

	si[0].entfernung = entfernung;

	/* a low value is tollerant to bad quality */
	si[0].grenz_einbuchen = grenz_einbuchen; /* 1..7 */

	si[0].kennung_fufst = kennung_fufst;

	si[0].ws_kennung = ws_kennung;

	si[0].nachbar_prio = nachbar_prio;

	si[0].bewertung = bewertung; /* 0 = relative entfernung, 1 = pegel */

	si[0].reduzierung = reduzierung;

	/* deny group of subscribers. (used to balance subscribers between base stations) */
	si[0].teilnehmergruppensperre = teilnehmergruppensperre;
	si[0].anzahl_gesperrter_teilnehmergruppen = anzahl_gesperrter_teilnehmergruppen;

	/* second cell uses flipped polarity. different station ID is used to
	 * detect what cell (and what polarity) the mobile responses to. */
	memcpy(&si[1], &si[0], sizeof(cnetz_si));
	si[1].flip_polarity = 1;
	si[1].fuz_rest = si[0].fuz_rest + 1;
}

