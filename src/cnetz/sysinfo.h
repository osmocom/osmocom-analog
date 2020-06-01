
typedef struct system_information {
	uint8_t		timeslot;		/* timeslot to use */
	uint8_t		fuz_nat;		/* national network ID */
	uint8_t		fuz_fuvst;		/* id of switching center */
	uint8_t		fuz_rest;		/* rest of base station id */
	uint8_t		mittel_umschalten;
	uint8_t		grenz_umschalten;
	uint8_t		mittel_ausloesen;
	uint8_t		grenz_ausloesen;
	uint8_t		vermittlungstechnische_sperren;
	uint8_t		genauigkeit;
	uint8_t		entfernung;
	uint8_t		grenz_einbuchen;
	uint8_t		kennung_fufst;		/* prio of base station */
	uint8_t		authentifikationsbit;	/* base station suppoerts authentication */
	uint8_t		ws_kennung;		/* queue setting sof base station */
	uint8_t		nachbar_prio;
	uint8_t		bewertung;
	uint8_t		reduzierung;
	int8_t		teilnehmergruppensperre;
	int8_t		anzahl_gesperrter_teilnehmergruppen;
} cnetz_si;

extern cnetz_si si;

void init_sysinfo(uint8_t timeslot, uint8_t fuz_nat, uint8_t fuz_fuvst, uint8_t fuz_rest, uint8_t kennung_fufst, uint8_t authentifikationsbit, uint8_t ws_kennung, uint8_t vermittlungstechnische_sperren, uint8_t grenz_einbuchen, uint8_t grenz_umschalten, uint8_t grenz_ausloesen, uint8_t mittel_umschalten, uint8_t mittel_ausloesen, uint8_t genauigkeit, uint8_t bewertung, uint8_t entfernung, uint8_t reduzierung, uint8_t nachbar_prio, int8_t teilnehmergruppensperre, uint8_t anzahl_gesperrter_teilnehmergruppen);

