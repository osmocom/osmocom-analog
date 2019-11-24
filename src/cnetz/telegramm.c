/* C-Netz telegramm transcoding
 *
 * (C) 2016 by Andreas Eversberg <jolly@eversberg.eu>
 * All Rights Reserved
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define CHAN cnetz->sender.kanal

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "../libsample/sample.h"
#include "../libdebug/debug.h"
#include "cnetz.h"
#include "dsp.h"
#include "sysinfo.h"
#include "telegramm.h"

/* debug coding */
//#define DEBUG_RAW       /* debug raw bits */
//#define DEBUG_BLOCK     /* debug interleaved blocks */
//#define DEBUG_CODER	/* debug block coder / decoder */

static const char *param_ja[] = {
	"nein",
	"ja",
};

static const char *param_betriebsart[] = {
	"Sprache klar",
	"Sprache invertiert",
	"Illegaler Parameter 2",
	"Illegaler Parameter 3",
};

static const char *param_gespraechsende[] = {
	"kein bevorstehendes Gespraechsende",
	"bevorstehendes Gespraechsende",
};

static const char *param_frequenz[2048];

static const char *param_anpassen[] = {
	"Sendeleistung erniedrigen",
	"Sendeleistung erhoehen",
};

static const char *param_opcode[64];

static const char *param_power[] = {
	"50-125 mW",
	"0.5-1 W",
	"4-8 W",
	"10-20 W",
};

static const char *param_sonderruf[] = {
	"Verbindungsaufbau gehend",
	"Sonderruf",
};

static const char *param_wskennung[] = {
	"keine Warteschlange",
	"Warteschlange",
	"Warteschlange blockiert",
	"Illegaler Parameter 3",
};

static const char *param_ausloesen[] = {
	"26 dB S/N + Pegel",
	"25 dB S/N + Pegel",
	"24 dB S/N + Pegel",
	"23 dB S/N + Pegel",
	"22 dB S/N + Pegel",
	"21 dB S/N + Pegel",
	"20 dB S/N + Pegel",
	"26 dB S/N",
	"25 dB S/N",
	"24 dB S/N",
	"23 dB S/N",
	"22 dB S/N",
	"21 dB S/N",
	"20 dB S/N",
	"19 dB S/N",
	"18 dB S/N",
};

static const char *param_ausloesegrund[] = {
	"gassenbesetzt (oder Sperre)",
	"teilnehmerbesetzt",
	"funktechnisch",
	"Illegaler Parameter 3",
};

static const char *param_genauigkeit[] = {
	"volle Genauigkeit",
	"bedingte Genauigkeit",
};

static const char *param_grenzwert[] = {
	"No limit",
	"> 15 dB S/N",
	"> 17 dB S/N",
	"> 19 dB S/N",
	"> 21 dB S/N",
	"> 25 dB S/N",
	"> 28 dB S/N",
	"> 32 dB S/N",
};

static const char *param_kennung[] = {
	"Test-FuFSt",
	"Normal-FuFSt",
	"Vorzugs-FuFSt",
	"FuFSt hoechster Prioritaet",
};

static const char *param_mittelung[] = {
	"2",
	"4",
	"8",
	"16",
	"32",
	"64",
	"Illegaler Parameter 6",
	"Illegaler Parameter 7",
	"Illegaler Parameter 8",
	"Illegaler Parameter 9",
	"Illegaler Parameter 10",
	"Illegaler Parameter 11",
	"Illegaler Parameter 12",
	"Illegaler Parameter 13",
	"Illegaler Parameter 14",
	"Illegaler Parameter 15",
};

static const char *param_entfernung[] = {
	"1,5 km",
	"2 km",
	"2,5 km",
	"3 km",
	"4 km",
	"5 km",
	"6 km",
	"7 km",
	"8 km",
	"10 km",
	"12 km",
	"14 km",
	"16 km",
	"17 km",
	"23 km",
	"30 km",
};

static const char *param_sperren[] = {
	"Ein- & Umbuchen / Gehende Verbindung",
	"Nur Ein- & Umbuchen",
	"Nur Gehende Verbindung",
	"gesperrt",
};

static const char *param_bewertung[] = {
	"Auswahl nach relativer Entfernungsbewertung",
	"Auswahl nach Pegelkreterium",
};

static const char *param_gueltig[] = {
	"gueltig",
	"ungueltig",
};

static const char *param_verkehrsanteil[] = {
	"Sonderfall",
	"1 Zeitschlitz",
	"2 Zeitschlitze",
	"3 Zeitschlitze",
	"4 Zeitschlitze",
	"5 Zeitschlitze",
	"6 Zeitschlitze",
	"7 Zeitschlitze",
	"8 Zeitschlitze",
	"9 Zeitschlitze",
	"10 Zeitschlitze",
	"11 Zeitschlitze",
	"12 Zeitschlitze",
	"13 Zeitschlitze",
	"14 Zeitschlitze",
	"15 Zeitschlitze",
	"16 Zeitschlitze",
	"17 Zeitschlitze",
	"18 Zeitschlitze",
	"19 Zeitschlitze",
	"10 Zeitschlitze",
	"21 Zeitschlitze",
	"22 Zeitschlitze",
	"23 Zeitschlitze",
	"24 Zeitschlitze",
	"25 Zeitschlitze",
	"26 Zeitschlitze",
	"27 Zeitschlitze",
	"28 Zeitschlitze",
	"29 Zeitschlitze",
	"30 Zeitschlitze",
	"31 Zeitschlitze",
};

static const char *param_signalisierung[] = {
	"Spontansignalisierung",
	"Signalisierung aus Wiederholstellung",
};

static const char *param_chipkarte[] = {
	"Magnetkarte",
	"Chipkarte",
};

static const char *param_auth[] = {
	"Authentifikation nicht durchfuehrbar",
	"Authentifikation durchfuehrbar",
};

static const char *param_reduzierung[] = {
	"4",
	"3",
	"2",
	"1",
};

static struct definition_parameter {
	char		digit;
	const char	*param_name;
	char		bits;
	const char	**value_names; /* points to a list of parameter names, NULL for integer */
} definition_parameter[] = {
	{ 'A',"FuZ-FuVSt-Nr.",				 5, NULL },
	{ 'B',"Betriebs-Art",				 2, param_betriebsart },
	{ 'C',"Ankuendigung Gespraechsende",		 1, param_gespraechsende },
	{ 'D',"Teilnehmergruppensperre",		 4, NULL },
	{ 'E',"Anzahl der gesperrten Teilnehmergruppen", 4, NULL },
	{ 'F',"FuZ-Rest-Nr.",				 8, NULL },
	{ 'G',"Gebuehren-Stand",	 		12, NULL },
	{ 'H',"OgK-Vorschlag",				10, param_frequenz },
	{ 'I',"FuZ-Nationalitaet",			 3, NULL },
	{ 'J',"Sendeleistungsanpassung",		 1, param_anpassen },
	{ 'K',"Frequenz-Nr.",				11, param_frequenz },
	{ 'L',"Art der Signalisierung im OgK",		 1, param_signalisierung },
	{ 'M',"OgK-Verkehrsanteil",			 5, param_verkehrsanteil },
	{ 'N',"FuTln-Nationalitaet",			 3, NULL },
	{ 'O',"OP-Code der Signalisierung",		 6, param_opcode },
	{ 'P',"Max. Sendeleistung",			 2, param_power },
	{ 'Q',"Kartenkennung",	 			 3, NULL },
	{ 'R',"Durchfuehrung der Ueberlastbehandlung",	 1, param_ja },
	{ 'S',"Sonderruf",				 1, param_sonderruf },
	{ 'T',"FuTln-Rest-Nr.",				16, NULL },
	{ 'U',"FuTln-Heimmat FuVSt-Nr.",		 5, NULL },
	{ 'V',"Sicherungs-Code",			16, NULL },
	{ 'W',"WS-Kennung",				 2, param_wskennung },
	{ 'X',"Wahlziffer beliebig 16 Ziffern",		64, NULL },
	{ 'Z',"Zeitschlitz-Nr.",			 5, NULL },
	{ 'a',"Grenzert fuer Ausloesen",		 4, param_ausloesen },
	{ 'b',"Chipkarten-FuTelG-Bit",			 1, param_chipkarte },
	{ 'c',"Ausloesegrund",				 2, param_ausloesegrund },
	{ 'd',"Bedingte Genauigkeit der FuFSt",		 1, param_genauigkeit },
	{ 'e',"Entfernung (generic value)",		 8, NULL },
	{ 'f',"Grenzwert fuer Einbuchen und Umbuchen",	 3, param_grenzwert },
	{ 'g',"Nachbarschafts-Prioritaets-Bit",		 1, NULL },
	{ 'h',"Herstellerkennung",			 5, NULL },
	{ 'i',"Hardwarestand des FuTelG",		 5, NULL },
	{ 'j',"Softwarestand des FuTelG",		 5, NULL },
	{ 'k',"Kennung FuFSt",				 2, param_kennung },
	{ 'l',"Authentifikationsbit",			 1, param_auth },
	{ 'm',"Mittelungs-Faktor fuer Ausloesen",	 4, param_mittelung },
	{ 'n',"Mittelungs-Faktor fuer Umschalten",	 4, param_mittelung },
	{ 'o',"Zufallszahl"			,	64, NULL },
	{ 'p',"Bewertung nach Pegel und Entfernung",	 1, param_bewertung },
	{ 'q',"Autorisierungsparameter",		64, NULL },
	{ 'r',"Entfernungsangabe der FuFSt",		 4, param_entfernung },
	{ 's',"Gueltigkeit des Gebuehrenstandes",	 1, param_gueltig },
	{ 't',"Test-Telefonteilnehmer-Geraet",		 1, param_ja },
	{ 'u',"Grenzwert fuer Umschalten",		 4, param_ausloesen },
	{ 'v',"Vermittlungtechnische Sperren",		 2, param_sperren },
	{ 'w',"Erweitertes Frequenzbandbit",		 1, NULL },
	{ 'y',"Reduzierungsfaktor",			 2, param_reduzierung },
	{ '_',"Illegaler Opcode",			64, NULL },
	{ 0  ,"",					 0, NULL },
};

static struct definition_parameter *get_parameter(char digit)
{
	struct definition_parameter *parameter = definition_parameter;

	for (parameter = definition_parameter; parameter->digit; parameter++) {
		if (parameter->digit == digit)
			return parameter;
	}

	return NULL;
}

static struct definition_opcode {
	const char *no_auth_bits, *auth_bits;
	const char *message_name;
	int block;
	const char *message_text;
} definition_opcode[64] = {
      /*   8888888877777777666666665555555544444444333333332222222211111111         message	block	text */
	{ "-bRLw---VVVVVVVVVVVVVVVVIIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT",
	  "-bRLwQQQ-hhhhhiiiiijjjjjIIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT",       "EM(R)",	BLOCK_R,"Erstmeldung" },
	{ "-bRLw---VVVVVVVVVVVVVVVVIIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT",
	  "-bRLwQQQ-hhhhhiiiiijjjjjIIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT",       "UM(R)",	BLOCK_R,"Umbuchantrag" },
	{ "SbRLw---VVVVVVVVVVVVVVVVIIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT",
	  "SbRLwQQQ-hhhhhiiiiijjjjjIIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT",       "UWG(R)",	BLOCK_R,"Umbuchantrag bei Warteschlange (gehende Verbindung)" },
	{ "--RL-----hhhhhiiiiijjjjjIIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "ATO(R)",	BLOCK_R,"Ausloesen des FuTelG im OgK-Betrieb bei WS" },
	{ "--RL--WW-hhhhhiiiiijjjjjIIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "VWG(R)",	BLOCK_R,"Verbindungswunsch gehend" },
	{ "--RL-----hhhhhiiiiijjjjjIIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "SRG(R)",	BLOCK_R,"Sonderruf (Notruf)" },
	{ "SbRLw---VVVVVVVVVVVVVVVVIIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT",
	  "SbRLwQQQ-hhhhhiiiiijjjjjIIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT",       "NUG(R)",	BLOCK_R,"Verbindungswunsch gehend bei Nachbarschaftsunterstuetzung" },
	{ "-bRLw---VVVVVVVVVVVVVVVVIIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT",
	  "-bRLwQQQ-hhhhhiiiiijjjjjIIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT",       "UWK(R)",	BLOCK_R,"Umbuchantrag bei Warteschlange (kommende Verbindung)" },
	{ "------------------------IIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "MFT(M)",	BLOCK_M,"Meldung: Funktelefonteilnehmer" },
	{ "________________________________________________________________", NULL, "opcode 9",	BLOCK_I,"Illegaler Opcode" },
	{ "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", NULL, "WUE(M)",	BLOCK_M,"Wahluebertragung" },
	{ "________________________________________________________________", NULL, "opcode 11",BLOCK_I,"Illegaler Opcode" },
	{ "________________________________________________________________", NULL, "opcode 12",BLOCK_I,"Illegaler Opcode" },
	{ "________________________________________________________________", NULL, "opcode 13",BLOCK_I,"Illegaler Opcode" },
	{ "________________________________________________________________", NULL, "opcode 14",BLOCK_I,"Illegaler Opcode" },
	{ "________________________________________________________________", NULL, "opcode 15",BLOCK_I,"Illegaler Opcode" },
	{ "------dJ----------------IIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "BEL(K)",	BLOCK_K,"Belegung" },
	{ "------dJ--------eeeeeeeeIIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "VH(K)",	BLOCK_K,"Verbindung halten" },
	{ "------dJ--------eeeeeeeeIIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "RTAQ(K)",	BLOCK_K,"Quittung Rufton anschalten" },
	{ "------dJBB------eeeeeeeeIIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "AH(K)",	BLOCK_K,"Abhebe-Signal" },
	{ "-----wdJBBCt----eeeeeeeeIIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "VH(V)",	BLOCK_V,"Verbindung halten" },
	{ "------dJ----------------IIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "AT(K)",	BLOCK_K,"Ausloesen durch Funktelefonteilnehmer" },
	{ "------dJBBC-------------IIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "AT(V)",	BLOCK_V,"Ausloesen durch Funktelefonteilnehmer" },
	{ "------dJBB------eeeeeeeeIIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "DSQ(K)",	BLOCK_K,"Durchschalten Quittung" },
	{ "-----wdJBBCt----eeeeeeeeIIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "USAI(V)",	BLOCK_V,"Umschaltantrag intern" },
	{ "-----wdJBBCt----eeeeeeeeIIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "USAE(V)",	BLOCK_V,"Umschaltantrag extern" },
	{ "------dJBB--------------IIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "USTLN(K)",	BLOCK_K,"Umschalten Funktelefonteilnehmer" },
	{ "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo", NULL, "ZFZQ(K)",	BLOCK_K,"Zufallszahlquittung" },
	{ "qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq", NULL, "AP(K)",	BLOCK_K,"Autorisierungsparameter" },
	{ "________________________________________________________________", NULL, "opcode 29",BLOCK_I,"Illegaler Opcode" },
	{ "________________________________________________________________", NULL, "opcode 30",BLOCK_I,"Illegaler Opcode" },
	{ "________________________________________________________________", NULL, "opcode 31",BLOCK_I,"Illegaler Opcode" },
	{ "PP-MMMMMDDDDEEEE------HHHHHHHHHHFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "MA(M)",	BLOCK_M,"Meldeaufruf" },
	{ "PPdZZZZZ-----KKKKKKKKKKKIIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "VAK(R)",	BLOCK_R,"Verbindungsaufbau kommend" },
	{ "________________________________________________________________", NULL, "opcode 34",BLOCK_I,"Illegaler Opcode" },
	{ "PPdZZZZZ----------------IIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "EBQ(R)",	BLOCK_R,"Einbuchquittung" },
	{ "PPdZZZZZ----------------IIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "UBQ(R)",	BLOCK_R,"Umbuchquittung" },
	{ "PPdZZZZZ----------------IIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "WSK(R)",	BLOCK_R,"Warteschglange kommend" },
	{ "PP-MMMMMDDDDEEEE------HHHHHHHHHHFFFFFFFF------------------------", NULL, "MLR(M)",	BLOCK_M,"Melde-Leer-Ruf" },
	{ "PPdZZZZZffflvvWW------yyIIIAAAAAFFFFFFFFkkgprrrrmmmmnnnnuuuuaaaa", NULL, "LR(R)",	BLOCK_R,"Leer-Ruf" },
	{ "PPdZZZZZ----------------IIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "ATQ(R)",	BLOCK_R,"Quittung fuer Ausloesen des FuTelG im OgK-Betrieb" },
	{ "PPdZZZZZ----------------IIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "SAR(R)",	BLOCK_R,"Sperraufruf" },
	{ "PP-MMMMMDDDDEEEE------HHHHHHHHHHFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "WAF(M)",	BLOCK_M,"Wahlaufforderung" },
	{ "PPdZZZZZ----------------IIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "WBP(R)",	BLOCK_R,"Wahlbestaetigung positiv" },
	{ "PPdZZZZZ----------------IIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "WBN(R)",	BLOCK_R,"Wahlbestaetigung negativ" },
	{ "PPdZZZZZ----------------IIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "WWBP(R)",	BLOCK_R,"Wahlbestaetigung positiv in Warteschlange" },
	{ "PPdZZZZZ-----KKKKKKKKKKKIIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "VAG(R)",	BLOCK_R,"Verbindungsaufbau gehend" },
	{ "PPdZZZZZ------cc--------IIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "VA(R)",	BLOCK_R,"Vorzeitiges Ausloesen" },
	{ "PP----dJ--------eeeeeeeeIIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "BQ(K)",	BLOCK_K,"Belegungsquittung" },
	{ "PP----dJ--------eeeeeeeeIIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "VHQ(K)",	BLOCK_K,"Quittung Verbindung halten" },
	{ "PP----dJ--------eeeeeeeeIIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "RTA(K)",	BLOCK_K,"Rufton anschalten" },
	{ "PP----dJ--------eeeeeeeeIIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "AHQ(K)",	BLOCK_K,"Abhebe-Quittierung" },
	{ "PP----dJ--C-----eeeeeeeeIIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "VHQ1(V)",	BLOCK_V,"Verbindung halten Quittung 1" },
	{ "PP----dJ--CsGGGGGGGGGGGGIIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "VHQ2(V)",	BLOCK_V,"Verbindung halten Quittung 2" },
	{ "PP----dJ------cc--------IIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "AF(K)",	BLOCK_K,"Ausloesen durch FuFSt in konzentr. Signalisierung" },
	{ "PP----dJ------cc--------IIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "AF(V)",	BLOCK_V,"Ausloesen durch FuFSt in verteilter Signalisierung" },
	{ "PP----dJ--------eeeeeeeeIIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "DSB(K)",	BLOCK_K,"Durchschaltung" },
	{ "PP----dJ-----KKKKKKKKKKKIIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "DSBI(V)",	BLOCK_V,"Umschaltbefehl intern (neuer SpK in der gleichen FuZ)" },
	{ "PP----dJ-----KKKKKKKKKKKIIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "USF(K)",	BLOCK_K,"Umschalten FuFst" },
	{ "PP----dJ-----KKKKKKKKKKKIIIAAAAAFFFFFFFFNNNUUUUUTTTTTTTTTTTTTTTT", NULL, "USBE(V)",	BLOCK_V,"Umschaltbefehl extern (neuer SpK in einer anderen Funkzelle)" },
	{ "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo", NULL, "ZFZ(K)",	BLOCK_K,"Zufallszahl" },
	{ "________________________________________________________________", NULL, "opcode 61",BLOCK_I,"Illegaler Opcode" },
	{ "________________________________________________________________", NULL, "opcode 62",BLOCK_I,"Illegaler Opcode" },
	{ "________________________________________________________________", NULL, "opcode 63",BLOCK_I,"Illegaler Opcode" },
};

const char *telegramm_name(uint8_t opcode)
{
	return definition_opcode[opcode].message_name;
}

int init_telegramm(void)
{
	struct definition_parameter *parameter;
	int i, j;
	const char *bits;
	char last_bit;
	int count_bits;

	/* copy no_auth_bits to auth_bits, if required
	 * check if the number of bits in a message matches the number of bits of a parameter */
	for (i = 0; i < 64; i++) {
		if (definition_opcode[i].auth_bits == NULL)
			definition_opcode[i].auth_bits = definition_opcode[i].no_auth_bits;
		for (bits = definition_opcode[i].no_auth_bits; ; bits = definition_opcode[i].auth_bits) {
			last_bit = '-';
			count_bits = 0;
			for (j = 0; j < 65; j++) { /* include termination character */
				if (last_bit != bits[j]) {
					if (last_bit != '-') {
						parameter = get_parameter(last_bit);
						if (!parameter) {
							printf("Message #%d has invalid digit '%c'\n", i, last_bit);
							return -1;
						}
						if (parameter->bits != count_bits) {
							printf("Message #%d has digit '%c' with %d bits, but parameter has %d bits\n", i, last_bit, count_bits, parameter->bits);
							return -1;
						}
					}
					last_bit = bits[j];
					count_bits = 0;
				}
				count_bits++;
			}
			if (bits == definition_opcode[i].auth_bits)
				break;
		}
	}

	/* generate frequency names */
	for (i = 0; i < 2048; i++) {
		char *frequenz = calloc(16, 1);
		if ((i & 1))
			sprintf(frequenz, "%.4f MHz", 465.750 - (double)(i+1) / 2.0 * 0.010);
		else
			sprintf(frequenz, "%.4f MHz", 465.750 - (double)i / 2.0 * 0.0125);
		param_frequenz[i] = frequenz;
	}

	/* generate opcode names */
	for (i = 0; i < 64; i++)
		param_opcode[i] = definition_opcode[i].message_name;

	return 0;
}

const char *telegramm2rufnummer(telegramm_t *telegramm)
{
	static char rufnummer[32]; /* make GCC happy (overflow check) */

	sprintf(rufnummer, "%d%d%05d", telegramm->futln_nationalitaet, telegramm->futln_heimat_fuvst_nr, telegramm->futln_rest_nr);

	return rufnummer;
}

static void decode_dialstring(char *number, uint64_t value)
{
	int index, max, shift;
	int digit;

	if ((value & 0x000000000000000f) == 0) {
		max = 16;
		index = 1;
		number[0] = '0';
	} else {
		max = value & 0x000000000000000f;
		index = 0;
	}
	shift = 4;
	while (index < max) {
		digit = (value >> shift) & 0xf;
		switch (digit) {
		case 0xb:
			digit = '*';
			break;
		case 0xc:
			digit = '#';
			break;
		case 0xa:
		case 0xd:
		case 0xe:
		case 0xf:
			digit = digit - 0xa + 'a';
			break;
		default:
			digit = digit + '0';
		}
		number[index] = digit;
		index++;
		shift += 4;
	}
	number[index] = '\0';
}

static int encode_dialstring(uint64_t *value, const char *number)
{
	int max, index, shift, digit;

	max = strlen(number);
	if (max > 16) {
		PDEBUG(DFRAME, DEBUG_NOTICE, "Given number '%s' has more than 16 digits\n", number);
		return -EINVAL;
	}

	if (max == 16) {
		if (number[0] != '0') {
			PDEBUG(DFRAME, DEBUG_NOTICE, "Given 16 digit number '%s' does not start with '0'\n", number);
			return -EINVAL;
		}
		*value = 0;
		index = 1;
	} else {
		*value = strlen(number);
		index = 0;
	}
	shift = 4;
	while (index < max) {
		digit = number[index];
		switch (digit) {
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			digit = digit - '0';
			break;
		case 'a':
		case 'd':
		case 'e':
		case 'f':
			digit = digit - 'a' + 0xa;
			break;
		case 'A':
		case 'D':
		case 'E':
		case 'F':
			digit = digit - 'A' + 0xa;
			break;
		case '*':
			digit = 0xb;
			break;
		case '#':
			digit = 0xc;
			break;
		default:
			return -EINVAL;
		}
		*value |= (uint64_t)digit << shift;
		index++;
		shift += 4;
	}

	return 0;
}

int match_fuz(telegramm_t *telegramm)
{
	if (telegramm->fuz_nationalitaet != si.fuz_nat
	 || telegramm->fuz_fuvst_nr != si.fuz_fuvst
	 || telegramm->fuz_rest_nr != si.fuz_rest) {
		PDEBUG(DFRAME, DEBUG_NOTICE, "Ignoring message from mobile phone %d,%d,%d: Cell 'Funkzelle' does not match!\n", telegramm->futln_nationalitaet, telegramm->futln_heimat_fuvst_nr, telegramm->futln_rest_nr);
	 	return 0;
	}

	return 1;
}

int match_futln(telegramm_t *telegramm, uint8_t futln_nat, uint8_t futln_fuvst, uint16_t futln_rest)
{
	if (telegramm->futln_nationalitaet != futln_nat
	 || telegramm->futln_heimat_fuvst_nr != futln_fuvst
	 || telegramm->futln_rest_nr != futln_rest) {
		PDEBUG(DFRAME, DEBUG_NOTICE, "Ignoring message from mobile phone %d,%d,%d: Mobile station 'Funktelefongeraet' does not match!\n", telegramm->futln_nationalitaet, telegramm->futln_heimat_fuvst_nr, telegramm->futln_rest_nr);
	 	return 0;
	}

	return 1;
}

static void debug_parameter(char digit, uint64_t value)
{
	struct definition_parameter *parameter;

	parameter = get_parameter(digit);
	if (!parameter) {
		PDEBUG(DFRAME, DEBUG_ERROR, "Digit '%c' not found in definition_parameter list, please fix!\n", digit);
		abort();
	}
	if (parameter->value_names)
		PDEBUG(DFRAME, DEBUG_DEBUG, " (%c) %s : %s\n", digit, parameter->param_name, parameter->value_names[value]);
	else if (parameter->bits == 64)
		PDEBUG(DFRAME, DEBUG_DEBUG, " (%c) %s : 0x%016" PRIx64 "\n", digit, parameter->param_name, value);
	else if (digit == 'X') {
		char wahlziffern[17];
		decode_dialstring(wahlziffern, value);
		PDEBUG(DFRAME, DEBUG_DEBUG, " (%c) %s : '%s'\n", digit, parameter->param_name, wahlziffern);
	} else
		PDEBUG(DFRAME, DEBUG_DEBUG, " (%c) %s : %" PRIu64 "\n", digit, parameter->param_name, value);
}

/* encode telegram to 70 bits
 * bit order MSB
 */
static char *assemble_telegramm(const telegramm_t *telegramm, int debug)
{
	static char bits[71]; /* + termination char for debug */
	char parameter;
	const char *string;
	uint64_t value, val;
	int i, j;
	int rc;

	if (telegramm->opcode >= 64) {
		PDEBUG(DFRAME, DEBUG_ERROR, "Opcode '0x%x' exceeds bit range, please fix!\n", telegramm->opcode);
		abort();
	}

	PDEBUG(DFRAME, DEBUG_DEBUG, "Coding %s %s\n", definition_opcode[telegramm->opcode].message_name, definition_opcode[telegramm->opcode].message_text);

	/* copy opcode */
	for (i = 0; i < 6; i++)
		bits[5 - i] = (telegramm->opcode & (1 << i)) ? '1' : '0';

	/* copy parameters */
	string = definition_opcode[telegramm->opcode].no_auth_bits;
	for (i = 0; i < 64; i++) {
		parameter = string[63 - i];
		if (parameter == '-') {
			bits[69 - i] = '0';
			continue;
		}
		switch (parameter) {
		case 'A':
			value = telegramm->fuz_fuvst_nr;
			break;
		case 'B':
			value = telegramm->betriebs_art;
			break;
		case 'C':
			value = telegramm->ankuendigung_gespraechsende;
			break;
		case 'D':
			value = telegramm->teilnehmergruppensperre;
			break;
		case 'E':
			value = telegramm->anzahl_gesperrter_teilnehmergruppen;
			break;
		case 'F':
			value = telegramm->fuz_rest_nr;
			break;
		case 'G':
			value = telegramm->gebuehren_stand;
			break;
		case 'H':
			value = telegramm->ogk_vorschlag;
			break;
		case 'I':
			value = telegramm->fuz_nationalitaet;
			break;
		case 'J':
			value = telegramm->sendeleistungsanpassung;
			break;
		case 'K':
			value = telegramm->frequenz_nr;
			break;
		case 'L':
			value = telegramm->art_der_signalisierung_im_ogk;
			break;
		case 'M':
			value = telegramm->ogk_verkehrsanteil;
			break;
		case 'N':
			value = telegramm->futln_nationalitaet;
			break;
		case 'P':
			value = telegramm->max_sendeleistung;
			break;
		case 'Q':
			value = telegramm->kartenkennung;
			break;
		case 'R':
			value = telegramm->durchfuehrung_der_ueberlastbehandlung;
			break;
		case 'S':
			value = telegramm->sonderruf;
			break;
		case 'T':
			value = telegramm->futln_rest_nr;
			break;
		case 'U':
			value = telegramm->futln_heimat_fuvst_nr;
			break;
		case 'V':
			value = telegramm->sicherungs_code;
			break;
		case 'W':
			value = telegramm->ws_kennung;
			break;
		case 'X':
			rc = encode_dialstring(&value, telegramm->wahlziffern);
			if (rc < 0) {
				PDEBUG(DFRAME, DEBUG_ERROR, "Illegal dial string '%s', please fix!\n", telegramm->wahlziffern);
				abort();
			}
			break;
		case 'Z':
			value = telegramm->zeitschlitz_nr;
			break;
		case 'a':
			value = telegramm->grenze_fuer_ausloesen;
			break;
		case 'b':
			value = telegramm->chipkarten_futelg_bit;
			break;
		case 'c':
			value = telegramm->ausloesegrund;
			break;
		case 'd':
			value = telegramm->bedingte_genauigkeit_der_fufst;
			break;
		case 'e':
			value = telegramm->entfernung;
			break;
		case 'f':
			value = telegramm->grenzwert_fuer_einbuchen_und_umbuchen;
			break;
		case 'g':
			value = telegramm->nachbarschafts_prioritaets_bit;
			break;
		case 'h':
			value = telegramm->herstellerkennung;
			break;
		case 'i':
			value = telegramm->hardware_des_futelg;
			break;
		case 'j':
			value = telegramm->software_des_futelg;
			break;
		case 'k':
			value = telegramm->kennung_fufst;
			break;
		case 'l':
			value = telegramm->authentifikationsbit;
			break;
		case 'm':
			value = telegramm->mittelungsfaktor_fuer_ausloesen;
			break;
		case 'n':
			value = telegramm->mittelungsfaktor_fuer_umschalten;
			break;
		case 'o':
			value = telegramm->zufallszahl;
			break;
		case 'p':
			value = telegramm->bewertung_nach_pegel_und_entfernung;
			break;
		case 'q':
			value = telegramm->authorisierungsparameter;
			break;
		case 'r':
			value = telegramm->entfernungsangabe_der_fufst;
			break;
		case 's':
			value = telegramm->gueltigkeit_des_gebuehrenstandes;
			break;
		case 't':
			value = telegramm->test_telefonteilnehmer_geraet;
			break;
		case 'u':
			value = telegramm->grenzwert_fuer_umschalten;
			break;
		case 'v':
			value = telegramm->vermittlungstechnische_sperren;
			break;
		case 'w':
			value = telegramm->erweitertes_frequenzbandbit;
			break;
		case 'y':
			value = telegramm->reduzierungsfaktor;
			break;
		case '_':
			value = telegramm->illegaler_opcode;
			break;
		default:
			PDEBUG(DFRAME, DEBUG_ERROR, "Parameter '%c' does not exist, please fix!\n", parameter);
			abort();
		}
		if (debug && debuglevel <= DEBUG_DEBUG)
			debug_parameter(parameter, value);
		val = value;
		for (j = 0; string[63 - i - j] == parameter; j++) {
			bits[69 - i - j] = (val & 1) ? '1' : '0';
			val >>= 1;
		}
		if (val)
			PDEBUG(DFRAME, DEBUG_ERROR, "Parameter '%c' value '0x%" PRIx64 "' exceeds bit range!\n", parameter, value);
		i += j - 1;
	}
	bits[70] = '\0';

	if (debug) {
		PDEBUG(DFRAME, DEBUG_DEBUG, "OOOOOO%s\n", string);
		PDEBUG(DFRAME, DEBUG_DEBUG, "%s\n", bits);
	}

	return bits;
}

/* decode telegram from 70 bits
 * bit order MSB
 */
static void disassemble_telegramm(telegramm_t *telegramm, const char *bits, int auth)
{
	uint64_t value;
	const char *string;
	char parameter;
	int i, j;

	memset(telegramm, 0, sizeof(*telegramm));

	/* copy opcode */
	value = 0;
	for (i = 0; i < 6; i++)
		value = (value << 1) | (bits[i] == '1');
	telegramm->opcode = value;

	PDEBUG(DFRAME, DEBUG_DEBUG, "Decoding %s %s\n", definition_opcode[telegramm->opcode].message_name, definition_opcode[telegramm->opcode].message_text);

	/* copy parameters */
	if (auth && bits[1]) /* auth flag and chip card flag */
		string = definition_opcode[telegramm->opcode].auth_bits;
	else
		string = definition_opcode[telegramm->opcode].no_auth_bits;
	for (i = 0; i < 64; i++) {
		parameter = string[63 - i];
		if (parameter == '-')
			continue;
		value = 0;
		for (j = 0; i + j < 64 && string[63 - i - j] == parameter; j++)
			value = (value >> 1) | ((uint64_t)(bits[69 - i - j] == '1') << 63);
		value >>= 64 - j;
		i += j - 1;
		if (debuglevel <= DEBUG_DEBUG)
			debug_parameter(parameter, value);
		switch (parameter) {
		case 'A':
			telegramm->fuz_fuvst_nr = value;
			break;
		case 'B':
			telegramm->betriebs_art = value;
			break;
		case 'C':
			telegramm->ankuendigung_gespraechsende = value;
			break;
		case 'D':
			telegramm->teilnehmergruppensperre = value;
			break;
		case 'E':
			telegramm->anzahl_gesperrter_teilnehmergruppen = value;
			break;
		case 'F':
			telegramm->fuz_rest_nr = value;
			break;
		case 'G':
			telegramm->gebuehren_stand = value;
			break;
		case 'H':
			telegramm->ogk_vorschlag = value;
			break;
		case 'I':
			telegramm->fuz_nationalitaet = value;
			break;
		case 'J':
			telegramm->sendeleistungsanpassung = value;
			break;
		case 'K':
			telegramm->frequenz_nr = value;
			break;
		case 'L':
			telegramm->art_der_signalisierung_im_ogk = value;
			break;
		case 'M':
			telegramm->ogk_verkehrsanteil = value;
			break;
		case 'N':
			telegramm->futln_nationalitaet = value;
			break;
		case 'P':
			telegramm->max_sendeleistung = value;
			break;
		case 'Q':
			telegramm->kartenkennung = value;
			break;
		case 'R':
			telegramm->durchfuehrung_der_ueberlastbehandlung = value;
			break;
		case 'S':
			telegramm->sonderruf = value;
			break;
		case 'T':
			telegramm->futln_rest_nr = value;
			break;
		case 'U':
			telegramm->futln_heimat_fuvst_nr = value;
			break;
		case 'V':
			telegramm->sicherungs_code = value;
			break;
		case 'W':
			telegramm->ws_kennung = value;
			break;
		case 'X':
			decode_dialstring(telegramm->wahlziffern, value);
			break;
		case 'Z':
			telegramm->zeitschlitz_nr = value;
			break;
		case 'a':
			telegramm->grenze_fuer_ausloesen = value;
			break;
		case 'b':
			telegramm->chipkarten_futelg_bit = value;
			break;
		case 'c':
			telegramm->ausloesegrund = value;
			break;
		case 'd':
			telegramm->bedingte_genauigkeit_der_fufst = value;
			break;
		case 'e':
			telegramm->entfernung = value;
			break;
		case 'f':
			telegramm->grenzwert_fuer_einbuchen_und_umbuchen = value;
			break;
		case 'g':
			telegramm->nachbarschafts_prioritaets_bit = value;
			break;
		case 'h':
			telegramm->herstellerkennung = value;
			break;
		case 'i':
			telegramm->hardware_des_futelg = value;
			break;
		case 'j':
			telegramm->software_des_futelg = value;
			break;
		case 'k':
			telegramm->kennung_fufst = value;
			break;
		case 'l':
			telegramm->authentifikationsbit = value;
			break;
		case 'm':
			telegramm->mittelungsfaktor_fuer_ausloesen = value;
			break;
		case 'n':
			telegramm->mittelungsfaktor_fuer_umschalten = value;
			break;
		case 'o':
			telegramm->zufallszahl = value;
			break;
		case 'p':
			telegramm->bewertung_nach_pegel_und_entfernung = value;
			break;
		case 'q':
			telegramm->authorisierungsparameter = value;
			break;
		case 'r':
			telegramm->entfernungsangabe_der_fufst = value;
			break;
		case 's':
			telegramm->gueltigkeit_des_gebuehrenstandes = value;
			break;
		case 't':
			telegramm->test_telefonteilnehmer_geraet = value;
			break;
		case 'u':
			telegramm->grenzwert_fuer_umschalten = value;
			break;
		case 'v':
			telegramm->vermittlungstechnische_sperren = value;
			break;
		case 'w':
			telegramm->erweitertes_frequenzbandbit = value;
			break;
		case 'y':
			telegramm->reduzierungsfaktor = value;
			break;
		case '_':
			telegramm->illegaler_opcode = value;
			break;
		default:
			PDEBUG(DFRAME, DEBUG_ERROR, "Parameter '%c' does not exist, please fix!\n", parameter);
			abort();
		}
	}

	if (debuglevel <= DEBUG_DEBUG) {
		char debug_bits[71];

		memcpy(debug_bits, bits, 70);
		debug_bits[70] = '\0';
		PDEBUG(DFRAME, DEBUG_DEBUG, "OOOOOO%s\n", string);
		PDEBUG(DFRAME, DEBUG_DEBUG, "%s\n", debug_bits);
	}

}

static const char *barker_string = "11100010010";
static int16_t barker_code = 0x712; /* 11 bits: 11100010010 */
static uint8_t barker_decode[2048]; /* detected bits */

static char *blockcode[128] = {
/*	 0123456 = Nutzbits */
/*	           01234567 = Redundanzbits */
	"0000000" "00000000",
	"1000000" "11101000",
	"0100000" "01110100",
	"1100000" "10011100",
	"0010000" "00111010",
	"1010000" "11010010",
	"0110000" "01001110",
	"1110000" "10100110",
	"0001000" "00011101",
	"1001000" "11110101",
	"0101000" "01101001",
	"1101000" "10000001",
	"0011000" "00100111",
	"1011000" "11001111",
	"0111000" "01010011",
	"1111000" "10111011",
	"0000100" "11100110",
	"1000100" "00001110",
	"0100100" "10010010",
	"1100100" "01111010",
	"0010100" "11011100",
	"1010100" "00110100",
	"0110100" "10101000",
	"1110100" "01000000",
	"0001100" "11111011",
	"1001100" "00010011",
	"0101100" "10001111",
	"1101100" "01100111",
	"0011100" "11000001",
	"1011100" "00101001",
	"0111100" "10110101",
	"1111100" "01011101",
	"0000010" "01110011",
	"1000010" "10011011",
	"0100010" "00000111",
	"1100010" "11101111",
	"0010010" "01001001",
	"1010010" "10100001",
	"0110010" "00111101",
	"1110010" "11010101",
	"0001010" "01101110",
	"1001010" "10000110",
	"0101010" "00011010",
	"1101010" "11110010",
	"0011010" "01010100",
	"1011010" "10111100",
	"0111010" "00100000",
	"1111010" "11001000",
	"0000110" "10010101",
	"1000110" "01111101",
	"0100110" "11100001",
	"1100110" "00001001",
	"0010110" "10101111",
	"1010110" "01000111",
	"0110110" "11011011",
	"1110110" "00110011",
	"0001110" "10001000",
	"1001110" "01100000",
	"0101110" "11111100",
	"1101110" "00010100",
	"0011110" "10110010",
	"1011110" "01011010",
	"0111110" "11000110",
	"1111110" "00101110",
	"0000001" "11010001",
	"1000001" "00111001",
	"0100001" "10100101",
	"1100001" "01001101",
	"0010001" "11101011",
	"1010001" "00000011",
	"0110001" "10011111",
	"1110001" "01110111",
	"0001001" "11001100",
	"1001001" "00100100",
	"0101001" "10111000",
	"1101001" "01010000",
	"0011001" "11110110",
	"1011001" "00011110",
	"0111001" "10000010",
	"1111001" "01101010",
	"0000101" "00110111",
	"1000101" "11011111",
	"0100101" "01000011",
	"1100101" "10101011",
	"0010101" "00001101",
	"1010101" "11100101",
	"0110101" "01111001",
	"1110101" "10010001",
	"0001101" "00101010",
	"1001101" "11000010",
	"0101101" "01011110",
	"1101101" "10110110",
	"0011101" "00010000",
	"1011101" "11111000",
	"0111101" "01100100",
	"1111101" "10001100",
	"0000011" "10100010",
	"1000011" "01001010",
	"0100011" "11010110",
	"1100011" "00111110",
	"0010011" "10011000",
	"1010011" "01110000",
	"0110011" "11101100",
	"1110011" "00000100",
	"0001011" "10111111",
	"1001011" "01010111",
	"0101011" "11001011",
	"1101011" "00100011",
	"0011011" "10000101",
	"1011011" "01101101",
	"0111011" "11110001",
	"1111011" "00011001",
	"0000111" "01000100",
	"1000111" "10101100",
	"0100111" "00110000",
	"1100111" "11011000",
	"0010111" "01111110",
	"1010111" "10010110",
	"0110111" "00001010",
	"1110111" "11100010",
	"0001111" "01011001",
	"1001111" "10110001",
	"0101111" "00101101",
	"1101111" "11000101",
	"0011111" "01100011",
	"1011111" "10001011",
	"0111111" "00010111",
	"1111111" "11111111",
};

static uint16_t block_code[128];
static uint16_t block_decode[32768]; /* code word + flag / 0xffff=decode error */

int init_coding(void)
{
	int i, j, k;

	/* create table to decode barker code.
	 * ech table entry returns the number of detected bits */
	for (i = 0; i < 2048; i++) {
		int match = 0;
		for (j = 0; j < 11; j++) {
			/* check if i matches barker code at given bit j */
			if (((i ^ barker_code) & (0x400 >> j)) == 0)
				match++;
		}
		barker_decode[i] = match;
	}

	/* convert string to block code words */
	for (i = 0; i < 128; i++) {
		int word = 0;
		for (j = 0; j < 15; j++)
			word = (word << 1) + (blockcode[i][14 - j] - '0');
		if ((word & 0x7f) != i) {
			printf("Databits are wrong, expecting %d, but got %d\n", i, word & 0x7f);
			return -1;
		}
		block_code[i] = word;
	}

	/* check if redunancy of a single bit matches the combined redundancy */
	for (i = 0; i < 128; i++) {
		int r = 0;
		for (j = 0; j < 7; j++) {
			if ((i & (1 << j)))
				r ^= block_code[1 << j] >> 7;
		}
		if (r != block_code[i] >> 7) {
			printf("Redundancy bits are wrong\n");
			return -1;
		}
	}

	/* create table to decode one block code and return value + error */
	/* set all combinations invalid */
	for (i = 0; i < 32768; i++)
		block_decode[i] = 0xffff;
	for (i = 0; i < 128; i++) {
		int word;
		/* set all error free combinations valid */
		word = block_code[i];
		if (block_decode[word] != 0xffff) {
			printf("Overlap, please fix!\n");
			return -1;
		}
		block_decode[word] = i;
		/* set all one bit error combinations valid with flag */
		for (j = 0; j < 15; j++) {
			word = block_code[i];
			word ^= (1 << j);
			if (block_decode[word] != 0xffff) {
				printf("Overlap, please fix!\n");
				return -1;
			}
			block_decode[word] = i | 0x100; /* indicate 1 error */
			/* set all two bit error combinations valid with flag */
			for (k = j + 1; k < 15; k++) {
				word = block_code[i];
				word ^= (1 << j) | (1 << k);
				if (block_decode[word] != 0xffff) {
					printf("Overlap, please fix!\n");
					return -1;
				}
				block_decode[word] = i | 0x200; /* indicate 2 errors */
			}
		}
	}

#if 0
	int count = 0;
	for (i = 0; i < 32768; i++) {
		printf("%d,", (int16_t)block_decode[i]);
		if (block_decode[i] == 0xffff)
		count++;
	}
	printf("bad blocks = %d\n", count);
#endif

	return 0;
}

/* check for sync (3 * barker code) + 1 bit */
int detect_sync(uint64_t bitstream)
{
	int match;

	/* hack: ignore first 3 bits of first barker code */
	bitstream |= 0x380000000;

	/* metch 33 bits, not as specified by FTZ */
	match = barker_decode[(bitstream >> 23) & 0x7ff];
	if (match < 11)
		return 0;
	match += barker_decode[(bitstream >> 12) & 0x7ff];
	if (match < 22)
		return 0;
	match += barker_decode[(bitstream >> 1) & 0x7ff];
	if (match < 33)
		return 0;

	return 1;
}

/* encode data block
 * input: 70 data bits MSB first
 * output: 10*15 code words (LSB first)
 * FTZ 171 TR 60 / 5.1.1.3 */
static char *encode(const char *input)
{
	static char output[150];
	int16_t word;
	int i, j;

#ifdef DEBUG_CODER
	printf("Encoding block to transmit:\n");
	printf("0123456.01234567\n");
#endif
	for (i = 0; i < 10; i++) {
		word = 0;
		for (j = 0; j < 7; j++)
			word = (word << 1) | (input[(9 - i) * 7 + j] == '1');
		word = block_code[word];
		for (j = 0; j < 15; j++) {
			output[i * 15 + j] = ((word >> j) & 1) + '0';
#ifdef DEBUG_CODER
			printf("%c", output[i * 15 + j]);
			if (j == 6)
				printf(".");
#endif
		}
#ifdef DEBUG_CODER
		printf("\n");
#endif
	}

	return output;
}

/* decode data block
 * input: 10*15 code words (LSB first)
 * output: 70 data bits MSB first
 * FTZ 171 TR 60 / 5.1.1.3 */
static const char *decode(const char *input, int *_bit_errors)
{
	int failed = 0, warn = 0;
	char fail_str[11];
	static char output[70];
	uint16_t word;
	int i, j;

#ifdef DEBUG_CODER
	printf("Decoding received block:\n");
	printf("0123456.01234567 Without errors:  Error bits:\n");
#endif
	for (i = 0; i < 10; i++) {
		word = 0;
		for (j = 0; j < 15; j++) {
			word = (word << 1) | (input[i * 15 + 14 - j] == '1');
#ifdef DEBUG_CODER
			printf("%c", input[i * 15 + j]);
			if (j == 6)
				printf(".");
#endif
		}
		word = block_decode[word];
		for (j = 0; j < 7; j++) {
			output[(9 - i) * 7 + j] = ((word >> (6 - j)) & 1) + '0';
		}
		if (word > 0x2ff) {
			failed = 1;
			fail_str[i] = 'X';
		} else
		if (word > 0x1ff) {
			warn += 2;
			fail_str[i] = '2';
		}else
		if (word > 0xff) {
			warn += 1;
			fail_str[i] = '1';
		} else
			fail_str[i] = '.';
#ifdef DEBUG_CODER
		if (word > 0x2ff)
			printf("decode failed");
		else {
			printf(" ");
			for (j = 0; j < 15; j++) {
				printf("%c", blockcode[word & 0x7f][j]);
				if (j == 6)
					printf(".");
			}
			printf(" ");
			for (j = 0; j < 15; j++) {
				if (blockcode[word & 0x7f][j] != input[i * 15 + j])
					printf("*");
				else
					printf("-");
				if (j == 6)
					printf(".");
			}
		}
		printf("\n");
#endif
	}
	fail_str[10] = '\0';
	
	if (failed)
		PDEBUG(DFRAME, DEBUG_DEBUG, "Received Telegram with these block errors: '%s' (X = uncorrectable)\n", fail_str);
	else if (warn)
		PDEBUG(DFRAME, DEBUG_DEBUG, "Received Telegram with these block errors: '%s' (1 / 2 = correctable)\n", fail_str);
	else
		PDEBUG(DFRAME, DEBUG_DEBUG, "Received Telegram with no block errors.\n");

	if (failed)
		return NULL;
	*_bit_errors = warn;
	return output;
}

/* interleving of code words
 * input: 10*15 code words (LSB first)
 * output: stream of 33 sync + 1 + 150 interleaved bits
 * FTZ 171 TR 60 / 5.1.1.2 and 5.1.1.2 */
static char *interleave(const char *input)
{
	static char output[185]; /* + termination char for debug */
	int i, j;

	strcpy(output, barker_string);
	strcpy(output + 11, barker_string);
	strcpy(output + 22, barker_string);

	output[33] = '1';

#ifdef DEBUG_BLOCK
	printf("Interleaving block to transmit:\n");
#endif
	for (i = 0; i < 10; i++) {
		for (j = 0; j < 15; j++) {
			output[i + j * 10 + 34] = input[j + i * 15];
#ifdef DEBUG_BLOCK
			printf("%c", input[j + i * 15]);
#endif
		}
#ifdef DEBUG_BLOCK
		printf("\n");
#endif
	}

#ifdef DEBUG_RAW
	output[184] = '\0';
	printf("Raw TX: %s\n", output + 34);
#endif

	return output;
}

/* deinterleave of code words
 * input: stream of 150 interleaved bits
 * output: 10*15 code words (LSB first)
 * FTZ 171 TR 60 / 5.1.1.4 */
static const char *deinterleave(const char *input)
{
	static char output[150];
	int i, j;

#ifdef DEBUG_RAW
	char debug_bits[151];

	memcpy(debug_bits, input, 150);
	debug_bits[151] = '\0';
	printf("Raw RX: %s\n", debug_bits);
#endif

#ifdef DEBUG_BLOCK
	printf("Deinterleaving received block:\n");
#endif
	for (i = 0; i < 10; i++) {
		for (j = 0; j < 15; j++) {
			output[j + i * 15] = input[i + j * 10];
#ifdef DEBUG_BLOCK
			printf("%c", output[j + i * 15]);
#endif
		}
#ifdef DEBUG_BLOCK
		printf("\n");
#endif
	}

	return output;
}

void cnetz_decode_telegramm(cnetz_t *cnetz, const char *bits, double level, double sync_time, double stddev)
{
	telegramm_t telegramm;
	uint8_t opcode;
	int i;
	int block;
	int bit_errors;

	bits = deinterleave(bits);
	bits = decode(bits, &bit_errors);
	if (!bits)
		return;

	/* filter out mysterious zero-telegramm */
	for (i = 0; i < 70; i++) {
		if (bits[i] != bits[0])
			break;
	}
	if (i == 70) {
		PDEBUG(DFRAME, DEBUG_INFO, "Ignoring mysterious unmodulated telegramm (noise from phone's transmitter)\n");
		return;
	}

	disassemble_telegramm(&telegramm, bits, si.authentifikationsbit);
	opcode = telegramm.opcode;
	telegramm.level = level;
	telegramm.sync_time = sync_time;

	if (bit_errors)
		PDEBUG_CHAN(DDSP, DEBUG_INFO, "RX Level: %.0f%% Standard deviation: %.0f%% Sync Time: %.2f (TS %.2f) Bit errors: %d %s\n", fabs(level) / cnetz->fsk_deviation * 100.0, stddev / fabs(level) * 100.0, sync_time, sync_time / 396.0, bit_errors, (level < 0) ? "NEGATIVE (phone's mode)" : "POSITIVE (base station's mode)");
	else
		PDEBUG_CHAN(DDSP, DEBUG_INFO, "RX Level: %.0f%% Standard deviation: %.0f%% Sync Time: %.2f (TS %.2f) %s\n", fabs(level) / cnetz->fsk_deviation * 100.0, stddev / fabs(level) * 100.0, sync_time, sync_time / 396.0, (level < 0) ? "NEGATIVE (phone's mode)" : "POSITIVE (base station's mode)");

	if (cnetz->sender.loopback) {
		PDEBUG(DFRAME, DEBUG_NOTICE, "Received Telegramm in loopback test mode (opcode %d = %s)\n", opcode, definition_opcode[opcode].message_name);
		cnetz_sync_frame(cnetz, sync_time, -1);
		return;
	}

	if (opcode >= 32) {
		PDEBUG(DFRAME, DEBUG_NOTICE, "Received Telegramm that is not used by mobile station, ignoring! (opcode %d = %s)\n", opcode, definition_opcode[opcode].message_name);
		return;
	}

	if (definition_opcode[opcode].block == BLOCK_I) {
		PDEBUG(DFRAME, DEBUG_NOTICE, "Received Telegramm that is an illegal opcode, ignoring! (opcode %d = %s)\n", opcode, definition_opcode[opcode].message_name);
		return;
	}

	/* auto select cell */
	if (cnetz->auto_polarity && match_fuz(&telegramm)) {
		sender_t *sender;
		cnetz_t *c;

		printf("***********************************************\n");
		printf("*** Autoselecting %stive FSK TX polarity! ***\n", (cnetz->negative_polarity) ? "nega" : "posi");
		printf("***********************************************\n");
		/* select on all transceivers */
		for (sender = sender_head; sender; sender = sender->next) {
			c = (cnetz_t *) sender;
			c->auto_polarity = 0;
			c->negative_polarity = cnetz->negative_polarity;
		}
	}

	switch (cnetz->dsp_mode) {
	case DSP_MODE_OGK:
		if (definition_opcode[opcode].block != BLOCK_R && definition_opcode[opcode].block != BLOCK_M) {
			PDEBUG(DFRAME, DEBUG_NOTICE, "Received Telegramm that is not used OgK channel signaling, ignoring! (opcode %d = %s)\n", opcode, definition_opcode[opcode].message_name);
			return;
		}
		/* determine block by last timeslot sent and by message type
		 * this is needed to sync the time of the receiver
		 */
		block = cnetz->sched_last_ts * 2;
		if (definition_opcode[opcode].block == BLOCK_M)
			block++;
		cnetz_receive_telegramm_ogk(cnetz, &telegramm, block);
		break;
	case DSP_MODE_SPK_K:
		if (definition_opcode[opcode].block != BLOCK_K) {
			PDEBUG(DFRAME, DEBUG_NOTICE, "Received Telegramm that is not used for concentrated signaling, ignoring! (opcode %d = %s)\n", opcode, definition_opcode[opcode].message_name);
			return;
		}
		cnetz_receive_telegramm_spk_k(cnetz, &telegramm);
		break;
	case DSP_MODE_SPK_V:
		if (definition_opcode[opcode].block != BLOCK_V) {
			PDEBUG(DFRAME, DEBUG_NOTICE, "Received Telegramm that is not used for distributed signaling, ignoring! (opcode %d = %s)\n", opcode, definition_opcode[opcode].message_name);
			return;
		}
		cnetz_receive_telegramm_spk_v(cnetz, &telegramm);
		break;
	default:
		;
	}
}

const char *cnetz_encode_telegramm(cnetz_t *cnetz)
{
	const telegramm_t *telegramm = NULL;
	uint8_t opcode;
	char *bits;

	switch (cnetz->dsp_mode) {
	case DSP_MODE_OGK:
		if (!cnetz->sched_r_m)
			telegramm = cnetz_transmit_telegramm_rufblock(cnetz);
		else
			telegramm = cnetz_transmit_telegramm_meldeblock(cnetz);
		break;
	case DSP_MODE_SPK_K:
		telegramm = cnetz_transmit_telegramm_spk_k(cnetz);
		break;
	case DSP_MODE_SPK_V:
		telegramm = cnetz_transmit_telegramm_spk_v(cnetz);
		break;
	default:
		;
	}

	opcode = telegramm->opcode;
	bits = assemble_telegramm(telegramm, (opcode != OPCODE_LR_R) && (opcode != OPCODE_MLR_M));
	bits = encode(bits);
	bits = interleave(bits);

	/* invert, if polarity of the cell is negative */
	if (cnetz->negative_polarity) {
		int i;

		for (i = 0; i < 184; i++)
			bits[i] ^= 1;
	}

	return bits;
}

