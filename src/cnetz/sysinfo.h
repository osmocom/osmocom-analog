
typedef struct system_information {
	int		flip_polarity;		/* use negative polarity for FSK transmission */
	uint8_t		fuz_nat;		/* national network ID */
	uint8_t		fuz_fuvst;		/* id of switching center */
	uint8_t		fuz_rest;		/* rest of base station id */
	uint8_t		mittel_umschalten;
	uint8_t		grenz_umschalten;
	uint8_t		mittel_ausloesen;
	uint8_t		grenz_ausloesen;
	uint8_t		sperre;
	uint8_t		genauigkeit;
	uint8_t		entfernung;
	uint8_t		grenz_einbuchen;
	uint8_t		fufst_prio;		/* prio of base station */
	uint8_t		nachbar_prio;
	uint8_t		bewertung;
	uint8_t		reduzierung;
} cnetz_si;

extern cnetz_si si[];

void init_sysinfo(void);

