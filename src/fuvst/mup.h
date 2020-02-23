
#define OPCODE_SWAF	0xc0	/* Wiederanlaufauftrag der BS */
#define OPCODE_SWQU	0xe0	/* Wiederanlaufquittung des MSC */

#define OPCODE_SSSAF	0xcc	/* Sprechkanal-Sammel-Sperrauftrag der BS */
#define OPCODE_SSSQU	0xec	/* Sprechkanal-Sammel-Sperrquitting des MSC */
#define OPCODE_SSAF	0xd8	/* SPRECHKANAL-SPERR-AUFTRAG DER BS */
#define OPCODE_SSQU	0xd1	/* SPRECHKANAL-SPERR-QUITTUNG VON DER MSC */
#define OPCODE_SFAF	0xda	/* SPRECHKANAL-FREIGABE-AUFTRAG DER BS */
#define OPCODE_SFQU	0xd3	/* SPRECHKANAL-FREIGABE-QUITTUNG VON DER MSC */

#define OPCODE_SUAF	0xc5	/* Datum-Uhrzeit-Auftrag der BS */
#define OPCODE_SUQU	0xe6	/* Datum-Uhrzeit-Quittung des MSC */

#define OPCODE_SVAF	0xc2	/* Vermittlungsfaehig-Auftrag der BS */
#define OPCODE_SVQU	0xe2	/* Vermittlungsfaehig-Quittung des MSC */

#define OPCODE_YLSAF	0xc3	/* Systemmeldungsanforderung an MSC */
#define OPCODE_YLSMU	0xe3	/* Systemmeldungsbestaetigung vom MSC */
#define OPCODE_YLSMF	0xdf	/* Systemmeldung an MSC */
#define OPCODE_YLSEF	0xcb	/* Systemmeldungsuebertragungsende an MSC */

#define OPCODE_STDAF	0xc6	/* Tarifdatenauftrag der BS */
#define OPCODE_XGTAU	0x90	/* Tarifdatensignalisierung vom MSC */

#define OPCODE_EBAF	0x09	/* EINBUCHUNGS-AUFTRAG DER BS */
#define OPCODE_EBPQU	0x01	/* EINBUCHUNGS-POSITIV-QUITTUNG VOM MSC */
#define OPCODE_EBNQU	0x02	/* EINBUCHUNGS-NEGATIV-QUITTUNG VOM MSC */

#define OPCODE_ABAF	0x08	/* AUSBUCHUNGS-AUFTRAG DER BS */

#define OPCODE_GVAF	0x12	/* GEHENDER VERBINDUNGS-AUFTRAG DER BS */
#define OPCODE_GVWAF	0x13	/* GEHENDER VERBINDUNGS-WARTESCHLANGEN-AUFTRAG DER BS */
#define OPCODE_GVPQU	0x22	/* GEHENDE VERBINDUNGS-POSITIV-QUITTUNG VOM MSC */
#define OPCODE_GVNQU	0x23	/* GEHENDE VERBINDUNGS-NEGATIV-QUITTUNG VOM MSC */

#define OPCODE_KVAU	0x20	/* KOMMENDER VERBINDUNGS-AUFTRAG VOM MSC */
#define OPCODE_KVWQF	0x11	/* KOMMENDE VERBINDUNGS-WARTESCHLANGEN-QUITTUNG DER BS */
#define OPCODE_KVBAF	0x10	/* KOMMENDE VERBINDUNGS-BEGINN-AUFTRAG DER BS */

#define OPCODE_STAF	0x18	/* SCHLEIFENTEST-AUFTRAG DER BS */
#define OPCODE_STPQU	0x28	/* SCHLEIFENTEST-POSITIV-QUITTUNG VOM MSC */
#define OPCODE_STNQU	0x29	/* SCHLEIFENTEST-NEGATIV-QUITTUNG VOM MSC */

#define OPCODE_APF	0x1d	/* AUTORISIERUNGSPARAMETER */

#define OPCODE_GSTAU	0x2a	/* GEBUEHREN-START-AUFTRAG VOM MSC */

#define OPCODE_FAF	0x19	/* FANG-AUFTRAG BS */

#define OPCODE_NAF	0x14	/* NEVATIV-AUFTRAG DER BS */
#define OPCODE_EQU	0x25	/* ENDE-QUITTUNG VOM MSC */

#define OPCODE_AAF	0x16	/* AUSLOESE-AUFTRAG DER BS */
#define OPCODE_AQU	0x27	/* AUSLOESE-QUITTUNG VOM MSC */

#define OPCODE_NAU	0x24	/* NEVATIV-AUFTRAG VOM MSC */
#define OPCODE_EQF	0x15	/* ENDE-QUITTUNG DER BS */

#define OPCODE_AAU	0x26	/* AUSLOESE-AUFTRAG VOM MSC */
#define OPCODE_AQF	0x17	/* AUSLOESE-QUITTUNG DER BS */

#define OPCODE_XADBF	0x86	/* ANFORDERUNG EINES BS-DB-DATENBLOCKES VON DER MSC */
#define OPCODE_XEDBU	0x9b	/* BS-DB-TRANSFER-ERGEBNIS-SIGN. VON DER MSC */

#define OPCODE_YAAAU	0xd4	/* ANLAUF-AKTIVIERUNGS-AUFTRAG DER MSC  (INITIALISIEREN DER BS) */

#define OPCODE_SWAU	0xe1	/* Wiederanlaufauftrag des MSC */
#define OPCODE_SWQF	0xc1	/* Wiederanlauf-Quittung von der BS */

#define OPCODE_SADAU	0xe4	/* Aktivdatei-Auftrag vom MSC */
#define OPCODE_SADQF	0xc4	/* Aktivdateiquittung der BS */

#define VERSION_LM8	6

struct SysMeld {
	uint16_t	FUKO;
	uint8_t		Monat;
	uint8_t		Tag;
	uint8_t		Stunde;
	uint8_t		Minute;
	uint8_t		Kennzeichen_posthum;
	uint16_t	Systemmeldungsnr;
	uint8_t		Indizienlaenge;
	uint8_t		Indizien[10];
	uint8_t		ASCII_Typ;
	uint8_t		Einrichtungstyp;
	uint8_t		Einrichtungsnr;
	uint8_t		Zusatzinfo[4];
};

const char *einrichtrungstyp_string(uint8_t T);

void decode_swaf(uint8_t *data, int len, uint8_t *V, uint8_t *N, uint8_t *U, uint8_t *F, uint8_t *C, uint8_t *B);
int encode_swqu(uint8_t *opcode, uint8_t **data, uint8_t A);
void decode_suaf(uint8_t *data, int len, uint8_t *V, uint8_t *N, uint8_t *U, uint8_t *F, uint8_t *C, uint8_t *B);
int encode_suqu(uint8_t *opcode, uint8_t **data, uint8_t Q, uint8_t N, time_t now);
void decode_sssaf(uint8_t *data, int len);
void encode_sssqu(uint8_t *opcode);
void decode_ssaf(uint8_t *data, int len, uint8_t *S);
int encode_ssqu(uint8_t *opcode, uint8_t **data, uint8_t S);
void decode_sfaf(uint8_t *data, int len, uint8_t *S);
int encode_sfqu(uint8_t *opcode, uint8_t **data, uint8_t S);
void decode_svaf(uint8_t *data, int len);
int encode_svqu(uint8_t *opcode, uint8_t **data);
void decode_ylsaf(uint8_t *data, int len);
int encode_ylsmu(uint8_t *opcode, uint8_t **data);
void decode_ylsmf(uint8_t *data, int len, uint8_t *N, uint8_t *C, struct SysMeld *SM);
void decode_ylsef(uint8_t *data, int len);
void decode_stdaf(uint8_t *data, int len);
int encode_xgtau(uint8_t *opcode, uint8_t **data, uint8_t Z, uint32_t T, uint8_t S, uint8_t K, uint16_t CS);
void decode_ebaf(uint8_t *data, int len, uint16_t *T, uint8_t *U, uint8_t *N, uint16_t *s, uint8_t *u, uint8_t *b, uint8_t *l);
int encode_ebpqu(uint8_t *opcode, uint8_t **data);
void decode_abaf(uint8_t *data, int len, uint16_t *T, uint8_t *U, uint8_t *N);
void decode_gvaf(uint8_t *data, int len, uint16_t *T, uint8_t *U, uint8_t *N, char *number);
void decode_gvwaf(uint8_t *data, int len, uint16_t *T, uint8_t *U, uint8_t *N, char *number);
int encode_gvpqu(uint8_t *opcode, uint8_t **data, uint8_t P, uint8_t e);
int encode_gvnqu(uint8_t *opcode, uint8_t **data, uint8_t X, uint8_t Y);
int encode_kvau(uint8_t *opcode, uint8_t **data, uint16_t T, uint8_t U, uint8_t N, uint8_t F, uint8_t e);
void decode_kvwqf(uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len);
void decode_kvbaf(uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len);
void decode_staf(uint8_t *data, int len, uint8_t *Q, uint8_t *V, uint8_t *e, uint64_t *n);
int encode_stpqu(uint8_t *opcode, uint8_t **data, uint8_t Q, uint8_t A, uint8_t K, uint16_t G, uint8_t U, uint8_t X, uint8_t Y, uint8_t mystery);
int encode_stnqu(uint8_t *opcode, uint8_t **data, uint8_t Q);
void decode_apf(uint8_t *data, int len, uint8_t *Q, uint64_t *a);
int encode_gstau(uint8_t *opcode, uint8_t **data, uint8_t Q, uint16_t G, uint8_t U, uint8_t Y, uint8_t A, uint8_t K);
void decode_faf(uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len);
void decode_naf(uint8_t *data, int len, uint8_t *X);
int encode_equ(uint8_t *opcode, uint8_t **data);
void decode_aaf(uint8_t *data, int len, uint8_t *Q, uint8_t *X);
int encode_aqu(uint8_t *opcode, uint8_t **data, uint8_t Q);
int encode_nau(uint8_t *opcode, uint8_t **data, uint8_t X, uint8_t Y);
void decode_eqf(uint8_t *data, int len);
int encode_aau(uint8_t *opcode, uint8_t **data, uint8_t Q, uint8_t X, uint8_t Y);
void decode_aqf(uint8_t *data, int len, uint8_t *Q);
void decode_xadbf(uint8_t *data, int len, uint8_t *PJ, uint16_t *D, uint16_t *L);
int encode_xedbu_1(uint8_t *opcode, uint8_t **data, uint8_t R, uint8_t PJ, uint16_t A);
int encode_xedbu_2(uint8_t *opcode, uint8_t **data, uint8_t S, uint8_t PJ, uint8_t *P);
int encode_xedbu_3(uint8_t *opcode, uint8_t **data, uint8_t S, uint8_t PJ, uint16_t D, uint16_t L, uint32_t CS);
int encode_yaaau(uint8_t *opcode, uint8_t **data, uint8_t J);
int encode_swau(uint8_t *opcode, uint8_t **data, uint8_t V);
void decode_swqf(uint8_t *data, int len, uint8_t *V, uint8_t *N, uint8_t *U, uint8_t *F, uint8_t *C, uint8_t *B);
void encode_sadau(uint8_t *opcode);
int decode_sadqf(uint8_t *data, int len, uint16_t *S, uint8_t *E, uint8_t *l, uint16_t *T, uint8_t *U, uint8_t *N);

