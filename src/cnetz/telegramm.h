
#define OPCODE_EM_R	0
#define OPCODE_UM_R	1
#define OPCODE_UWG_R	2
#define OPCODE_ATO_R	3
#define OPCODE_VWG_R	4
#define OPCODE_SRG_R	5
#define OPCODE_NUG_R	6
#define OPCODE_UWK_R	7
#define OPCODE_MFT_M	8
#define OPCODE_WUE_M	10
#define OPCODE_BEL_K	16
#define OPCODE_VH_K	17
#define OPCODE_RTAQ_K	18
#define OPCODE_AH_K	19
#define OPCODE_VH_V	20
#define OPCODE_AT_K	21
#define OPCODE_AT_V	22
#define OPCODE_DSQ_K	23
#define OPCODE_USAI_V	24
#define OPCODE_USAE_V	25
#define OPCODE_USTLN_K	26
#define OPCODE_ZFZQ_K	27
#define OPCODE_AP_K	28
#define OPCODE_MA_M	32
#define OPCODE_VAK_R	33
#define OPCODE_EBQ_R	35
#define OPCODE_UBQ_R	36
#define OPCODE_WSK_R	37
#define OPCODE_MLR_M	38
#define OPCODE_LR_R	39
#define OPCODE_ATQ_R	40
#define OPCODE_SAR_R	41
#define OPCODE_WAF_M	42
#define OPCODE_WBP_R	43
#define OPCODE_WBN_R	44
#define OPCODE_WWBP_R	45
#define OPCODE_VAG_R	46
#define OPCODE_VA_R	47
#define OPCODE_BQ_K	48
#define OPCODE_VHQ_K	49
#define OPCODE_RTA_K	50
#define OPCODE_AHQ_K	51
#define OPCODE_VHQ1_V	52
#define OPCODE_VHQ2_V	53
#define OPCODE_AF_K	54
#define OPCODE_AF_V	55
#define OPCODE_DSB_K	56
#define OPCODE_DSBI_V	57
#define OPCODE_USF_K	58
#define OPCODE_USBE_V	59
#define OPCODE_ZFZ_K	60

#define BLOCK_I		0
#define BLOCK_R		1
#define BLOCK_M		2
#define BLOCK_K		3
#define BLOCK_V		4

/* data structor of one cnetz-message */
typedef struct telegramm {
	double level;		/* average level of received sync sequence */
	double sync_time;	/* when did we receive the sync for this frame */
	uint8_t	opcode;
	/* used parameters depend on opcode */
	uint8_t fuz_fuvst_nr;
	uint8_t betriebs_art;
	uint8_t ankuendigung_gespraechsende;
	uint8_t teilnehmergruppensperre;
	uint8_t anzahl_gesperrter_teilnehmergruppen;
	uint8_t fuz_rest_nr;
	uint16_t gebuehren_stand;
	uint16_t ogk_vorschlag;
	uint8_t fuz_nationalitaet;
	uint8_t sendeleistungsanpassung;
	uint16_t frequenz_nr;
	uint8_t art_der_signalisierung_im_ogk;
	uint8_t ogk_verkehrsanteil;
	uint8_t futln_nationalitaet;
	uint8_t max_sendeleistung;
	uint8_t kartenkennung;
	uint8_t durchfuehrung_der_ueberlastbehandlung;
	uint8_t sonderruf;
	uint16_t futln_rest_nr;
	uint8_t futln_heimat_fuvst_nr;
	uint16_t sicherungs_code;
	uint8_t ws_kennung;
	char wahlziffern[17];
	uint8_t zeitschlitz_nr;
	uint8_t grenze_fuer_ausloesen;
	uint8_t chipkarten_futelg_bit;
	uint8_t ausloesegrund;
	uint8_t bedingte_genauigkeit_der_fufst;
	uint8_t entfernung;
	uint8_t grenzwert_fuer_einbuchen_und_umbuchen;
	uint8_t nachbarschafts_prioritaets_bit;
	uint8_t herstellerkennung;
	uint8_t hardware_des_futelg;
	uint8_t software_des_futelg;
	uint8_t kennung_fufst;
	uint8_t authentifikationsbit;
	uint8_t mittelungsfaktor_fuer_ausloesen;
	uint8_t mittelungsfaktor_fuer_umschalten;
	uint16_t zufallszahl;
	uint8_t bewertung_nach_pegel_und_entfernung;
	uint64_t authorisierungsparameter;
	uint8_t entfernungsangabe_der_fufst;
	uint8_t gueltigkeit_des_gebuehrenstandes;
	uint8_t test_telefonteilnehmer_geraet;
	uint8_t grenzwert_fuer_umschalten;
	uint8_t vermittlungstechnische_sperren;
	uint8_t erweitertes_frequenzbandbit;
	uint8_t reduzierungsfaktor;
	uint64_t illegaler_opcode;
} telegramm_t;

int init_telegramm(void);
int init_coding(void);
const char *telegramm_name(uint8_t opcode);

const char *telegramm2rufnummer(telegramm_t *telegramm);
int match_fuz(cnetz_t *cnetz, telegramm_t *telegramm, int cell);
int match_futln(telegramm_t *telegramm, uint8_t futln_nat, uint8_t futln_fuvst, uint16_t futln_rest);

int detect_sync(uint64_t bitstream);
void cnetz_decode_telegramm(cnetz_t *cnetz, const char *bits, double level, double sync_time, double stddev);
const char *cnetz_encode_telegramm(cnetz_t *cnetz);

