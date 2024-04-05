/* C-Netz Mobile User Part message coding
 *
 * (C) 2020 by Andreas Eversberg <jolly@eversberg.eu>
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include <osmocom/core/utils.h>
#include "../liblogging/logging.h"
#include "mup.h"
#include "systemmeldungen.h"

const char *einrichtrungstyp_string(uint8_t T)
{
	static char unknown[4];
	switch (T) {
	case 1:
		return "OGK";
	case 2:
		return "SPK";
	case 3:
		return "PHE";
	case 4:
		return "PFG";
	case 5:
		return "PBR";
	case 6:
		return "FME";
	case 8:
		return "DKV";
	case 9:
		return "SAE";
	case 10:
		return "FDS";
	case 11:
		return "BS ";
	case 12:
		return "MSC";
	case 13:
		return "OSK";
	default:
		sprintf(unknown, "%03d", T);
		return unknown;
	}
}

static const char *version_string(uint8_t V)
{
	switch (V) {
	case 0:
		return "undefined";
	case 1:
		return "LM3.6 BS";
	case 2:
		return "LM4 BS";
	case 3:
		return "LM5 BS";
	case 4:
		return "LM6 BS";
	case 5:
		return "LM7 BS";
	case 6:
		return "LM8 BS";
	}

	return "unknown";
}

static const char *chip_string(uint8_t C)
{
	switch (C) {
	case 0:
		return "chipfunction off";
	case 1:
		return "chipfunction on";
	}

	return "unknown";
}

static const char *beacon_string(uint8_t B)
{
	switch (B) {
	case 0:
		return "normal BS";
	case 1:
		return "stand alone beacon";
	case 2:
		return "low power beacon CCH pair 2 exclusively";
	case 3:
		return "low power beacon CCH pair 3 exclusively";
	case 4:
		return "low power beacon";
	}

	return "unknown";
}

static const char *qualitaet_string(uint8_t Q)
{
	switch (Q) {
	case 0:
		return "unsichere Angabe";
	case 1:
		return "sichere Angabe";
	}

	return "unknown";
}

static const char *wiederholung_string(uint8_t N)
{
	switch (N) {
	case 0:
		return "keine Wiederholung";
	case 1:
		return "Wiederholung";
	}

	return "unknown";
}

static const char *woche_string(uint8_t Q)
{
	switch (Q) {
	case 1:
		return "Sonntag";
	case 2:
		return "Montag";
	case 3:
		return "Dienstag";
	case 4:
		return "Mittwoch";
	case 5:
		return "Donnerstag";
	case 6:
		return "Freitag";
	case 7:
		return "Samstag";
	}

	return "unknown";
}

static const char *aktivdatei_string(uint8_t A)
{
	switch (A) {
	case 0:
		return "wurde gesendet";
	case 1:
		return "wurde nicht gesendet";
	}

	return "unknown";
}

static const char *prio_string(uint8_t P)
{
	switch (P) {
	case 0:
		return "Verbindung ohne Prioritaet";
	case 1:
		return "Verbindung mit Prioritaet";
	}

	return "unknown";
}

static const char *rufzeit_string(uint8_t F)
{
	switch (F) {
	case 0:
		return "Begrenzung durch SPK";
	case 1:
		return "Begrenzung durch MSC";
	}

	return "unknown";
}

static const char *typ_string(uint8_t V)
{
	switch (V) {
	case 0:
		return "gehender Verbindungs-Aufbau";
	case 1:
		return "kommender Verbindungs-Aufbau";
	}

	return "unknown";
}

static const char *fufst_cause(uint8_t X)
{
	switch (X) {
	case 0:
		return "undefiniert";
	case 1:
		return "Einh.A (A-BS)";
	case 2:
		return "Einh.B (B-BS)";
	case 3:
		return "Tln-besetzt (A-BS)";
	case 4:
		return "Gassenbesetzt";
	case 5:
		return "Time-Out: kein Sprechkanal";
	case 6:
		return "Time-Out: kein Melden B (A-BS)";
	case 7:
		return "Funk-Tln nicht verbindungsberechtigt";
	case 8:
		return "WS ist blockiert (Time out)";
	case 9:
		return "Funk-Tln nicht aktiv";
	case 10:
		return "Funk-Tln nicht erreichbar";
	case 11:
		return "Time-Out: keine Reaktion von der MSC";
	case 12:
		return "Umbuchung in WS oder gegenlaeufige Einbuchung";
	case 13:
		return "ungueltige Wahl";
	case 17:
		return "Time out: Gespraechszeitbegrenzung (BS)";
	case 18:
		return "Qualitaet Sprechkanal (Funkverbindung MS->BS)";
	case 19:
		return "Qualitaet Fu-Tln-Geraet (Funkverbindung BS->MS)";
	case 20:
		return "Signalisierungsverlust Sprechkanal (MS->BS)";
	case 21:
		return "Signalisierungsverlust Fu-Tln-Geraet (BS-MS)";
	case 22:
		return "Sprechkanal-Funktionsstoerung (HW-/SW Fehler)";
	case 23:
		return "Umschalten";
	case 24:
		return "Authentifikation negativ";
	case 25:
		return "Stoerung Randomzahluebertragung";
	case 27:
		return "Kein Melden B (B-BS), Infobox aktiviert";
	case 26:
		return "Stoerung Autorisierungsparameteruebertragung";
	}

	return "unknown";
}

static const char *futln_cause(uint8_t Y)
{
	switch (Y) {
	case 0:
		return "Gassenbesetzt";
	case 1:
		return "Teilnehmer-besetzt";
	case 2:
		return "Funktechnisch";
	case 3:
		return "Timeout";
	case 4:
		return "ungueltige Wahl";
	}

	return "unknown";
}

/*
 * message coding and decoding
 */

/* base station boots */
void decode_swaf(uint8_t *data, int len, uint8_t *V, uint8_t *N, uint8_t *U, uint8_t *F, uint8_t *C, uint8_t *B)
{
	if (len < 6) {
		LOGP(DMUP, LOGL_NOTICE, "Message too short!\n");
		return;
	}

	*V = data[0];
	*N = data[1];
	*U = data[2];
	*F = data[3];
	*C = data[4];
	*B = data[5];

	LOGP(DMUP, LOGL_INFO, "(BS SWAF) Wiederanlauf der BS: version=%d (%s) FuFSt=%d,%d,%d chip=%d (%s) beacon=%d (%s)\n", *V, version_string(*V), *N, *U, *F, *C, chip_string(*C), *B, beacon_string(*B));
}

/* ack to base station boot */
int encode_swqu(uint8_t *opcode, uint8_t **data, uint8_t A)
{
	static uint8_t buffer[1];

	LOGP(DMUP, LOGL_INFO, "(MSC SWQU) Wiederanlaufquittung des MSC: aktivdatei=%d (%s)\n", A, aktivdatei_string(A));

	*opcode = OPCODE_SWQU;
	buffer[0] = A;

	*data = buffer;
	return sizeof(buffer);
}

/* base station requests time */
void decode_suaf(uint8_t *data, int len, uint8_t *V, uint8_t *N, uint8_t *U, uint8_t *F, uint8_t *C, uint8_t *B)
{
	if (len < 6) {
		LOGP(DMUP, LOGL_NOTICE, "Message too short!\n");
		return;
	}

	*V = data[0];
	*N = data[1];
	*U = data[2];
	*F = data[3];
	*C = data[4];
	*B = data[5];

	LOGP(DMUP, LOGL_INFO, "(BS SUAF) Datum-Uhrzeit-Anforderung der BS: version=%d (%s) FuFSt=%d,%d,%d chip=%d (%s) beacon=%d (%s)\n", *V, version_string(*V), *N, *U, *F, *C, chip_string(*C), *B, beacon_string(*B));
}

/* ack to time request */
int encode_suqu(uint8_t *opcode, uint8_t **data, uint8_t Q, uint8_t N, time_t now)
{
	static uint8_t buffer[8];
	struct tm *tm;
	uint8_t R, W, D, M, J, h, m, s;

	tm = localtime(&now);

	R = 0x00;
	W = tm->tm_wday + 1; /* 1 = Sonntag */
	D = tm->tm_mday;
	M = tm->tm_mon + 1;
	J = tm->tm_year % 100; /* Erlaubt: 0--99 */
	h = tm->tm_hour;
	m = tm->tm_min;
	s = tm->tm_sec;

	LOGP(DMUP, LOGL_INFO, "(MSC SUQU) Datum-Uhrzeit-Quittung des MSC: Q=%d (%s) Widerholung=%d (%s) Wochentag=%d (%s) Datum: %d.%d.%d %d:%02d:%02d\n", Q, qualitaet_string(Q), N, wiederholung_string(N), W, woche_string(W), D, M, J, h, m, s);

	*opcode = OPCODE_SUQU;
	buffer[0] = Q | (N << 1) | (R << 2);
	buffer[1] = W;
	buffer[2] = D;
	buffer[3] = M;
	buffer[4] = J;
	buffer[5] = h;
	buffer[6] = m;
	buffer[7] = s;

	*data = buffer;
	return sizeof(buffer);
}

/* base station lists voice channels */
void decode_sssaf(uint8_t *data, int len)
{
	uint8_t E_;
	uint16_t A, E;
	uint8_t S = 0xff, last_S = 0xff;
	int i, start_i = 0, stop_i = 0;

	if (len < 11) {
		LOGP(DMUP, LOGL_NOTICE, "Message too short!\n");
		return;
	}

	E_ = data[0] >> 7;
	A = data[1] | (data[2] << 8);
	E = data[3] | (data[4] << 8);

	LOGP(DMUP, LOGL_INFO, "(BS SSSAF) Sprechkanal-Sammel-Sperrauftrag der BS: Liste-Ende=%d Anfang=%d Ende=%d\n", E_, A, E);
	if (E - A + 1 > 6 * 8) {
		LOGP(DMUP, LOGL_INFO, " -> Bereich zu gross für Nachricht!\n");
		return;
	}
	if ((int)E - (int)A < 0)
		return;
	/* count number of channels + 1, so that we can output the last range at the end */
	for (i = 0; i <= E - A + 1; i++) {
		/* after all loop turns (A..E), output last range */
		if (i == E - A + 1)
			goto end;
		S = (data[5 + (i >> 3)] >> (i & 7)) & 1;
		/* set start of range if first channel */
		if (i == 0)
			start_i = i;
		/* set stop of range, if first channel OR same state as last channel OR for last channel */
		if (i == 0 || S == last_S)
			stop_i = i;
		/* output range if we have a change in channel or is we reached the last channels */
		if (i > 0 && S != last_S) {
end:
			if (start_i == stop_i)
				LOGP(DMUP, LOGL_INFO, " -> SpK #%d=%d (%s)\n", start_i + A, last_S, (last_S) ? "gesperrt" : "frei");
			else
				LOGP(DMUP, LOGL_INFO, " -> SpK #%d..%d=%d (%s)\n", start_i + A, stop_i + A, last_S, (last_S) ? "gesperrt" : "frei");
			/* new start */
			start_i = stop_i = i;
		}
		last_S = S;
	}
}

/* ack voice channel list */
void encode_sssqu(uint8_t *opcode)
{
	*opcode = OPCODE_SSSQU;
	LOGP(DMUP, LOGL_INFO, "(MSC SSSQU) Sprechkanal-Sammel-Sperrquittung des MSC\n");
}

/* base station locks a voice channel */
void decode_ssaf(uint8_t *data, int len, uint8_t *S)
{
	if (len < 1) {
		LOGP(DMUP, LOGL_NOTICE, "Message too short!\n");
		return;
	}

	*S = data[0];

	LOGP(DMUP, LOGL_INFO, "(BS SSAF) Sprechkanal-Sperr-Auftrag der BS: SPK=%d\n", *S);
}

/* ack to lockeed voice channel */
int encode_ssqu(uint8_t *opcode, uint8_t **data, uint8_t S)
{
	static uint8_t buffer[1];

	LOGP(DMUP, LOGL_INFO, "(MSC SSQU) Sprechkanal-Sperr-Quittung von der MSC: SPK=%d\n", S);

	*opcode = OPCODE_SSQU;
	buffer[0] = S;

	*data = buffer;
	return sizeof(buffer);
}

/* base station unlocks a voice channel */
void decode_sfaf(uint8_t *data, int len, uint8_t *S)
{
	if (len < 1) {
		LOGP(DMUP, LOGL_NOTICE, "Message too short!\n");
		return;
	}

	*S = data[0];

	LOGP(DMUP, LOGL_INFO, "(BS SFAF) Sprechkanal-Freigabe-Auftrag der BS: SPK=%d\n", *S);
}

/* ack to unlockeed voice channel */
int encode_sfqu(uint8_t *opcode, uint8_t **data, uint8_t S)
{
	static uint8_t buffer[1];

	LOGP(DMUP, LOGL_INFO, "(MSC SFQU) Sprechkanal-Freigabe-Quittung von der MSC: SPK=%d\n", S);

	*opcode = OPCODE_SFQU;
	buffer[0] = S;

	*data = buffer;
	return sizeof(buffer);
}

/* base station ready */
void decode_svaf(uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len)
{
	LOGP(DMUP, LOGL_INFO, "(BS SVAF) Vermittlungsfaehig-Auftrag der BS\n");
}

/* ack to base station ready */
int encode_svqu(uint8_t *opcode, uint8_t **data)
{
	LOGP(DMUP, LOGL_INFO, "(MSC SVQU) Vermittlungsfaehig-Quittung des MSC\n");

	*opcode = OPCODE_SVQU;
	*data = NULL;
	return 0;
}

/* base station requests alarm messages */
void decode_ylsaf(uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len)
{
	LOGP(DMUP, LOGL_INFO, "(BS YLSAF) Systemmeldungsanforderung an MSC\n");
}

/* ack to base stations alarm request */
int encode_ylsmu(uint8_t *opcode, uint8_t **data)
{
	LOGP(DMUP, LOGL_INFO, "(MSC YLSMU) Systemmeldungsbestaetigung vom MSC\n");

	*opcode = OPCODE_YLSMU;
	*data = NULL;
	return 0;
}

/* base station sends alarm message */
void decode_ylsmf(uint8_t *data, int len, uint8_t *N, uint8_t *C, struct SysMeld *SM)
{
	if (len < 9) {
		LOGP(DMUP, LOGL_NOTICE, "Message too short!\n");
		return;
	}

	*N = data[0];
	*C = data[8];

	switch (*N) {
	case 1:
		memset(SM, 0, sizeof(*SM));
		SM->FUKO = data[1] << 8;
		SM->FUKO |= data[2];
		SM->Monat = (data[3] >> 4) * 10 + (data[3] & 0xf);
		SM->Tag = (data[4] >> 4) * 10 + (data[4] & 0xf);
		SM->Stunde = (data[5] >> 4) * 10 + (data[5] & 0xf);
		SM->Minute = (data[6] >> 4) * 10 + (data[6] & 0xf);
		SM->Kennzeichen_posthum = data[7];
		break;
	case 2:
		SM->Systemmeldungsnr = data[1] << 8;
		SM->Systemmeldungsnr |= data[2];
		SM->Indizienlaenge = data[3];
		SM->Indizien[0] = data[4];
		SM->Indizien[1] = data[5];
		SM->Indizien[2] = data[6];
		SM->Indizien[3] = data[7];
		break;
	case 3:
		SM->Indizien[4] = data[1];
		SM->Indizien[5] = data[2];
		SM->Indizien[6] = data[3];
		SM->Indizien[7] = data[4];
		SM->Indizien[8] = data[5];
		SM->Indizien[9] = data[6];
		SM->ASCII_Typ = data[7];
		break;
	case 4:
		SM->Einrichtungstyp = data[1];
		SM->Einrichtungsnr = data[2];
		SM->Zusatzinfo[0] = data[3];
		SM->Zusatzinfo[1] = data[4];
		SM->Zusatzinfo[2] = data[5];
		SM->Zusatzinfo[3] = data[6];
		break;
	}

	if (*N != 4)
		return;

	char indizien[24] = "                       ";
	int i, ii;

	ii = SM->Indizienlaenge;
	if (ii > 10)
		ii = 10;
	for (i = 0; i < ii; i++) {
		sprintf(indizien + (i * 2) + (i > 3) + (i > 7), "%02X", SM->Indizien[i]);
		indizien[strlen(indizien)] = ' ';
	}

	LOGP(DMUP, LOGL_INFO, "SM: %03d %02d.%02d %02d:%02d %s%02d  %c  H\"%04X  %02d H\"%sH\"%02X%02X%02X%02X\n", *C,
		SM->Monat, SM->Tag, SM->Stunde, SM->Minute,
		einrichtrungstyp_string(SM->Einrichtungstyp), SM->Einrichtungsnr,
		SM->ASCII_Typ ? : '0', SM->Systemmeldungsnr,
		SM->Indizienlaenge, indizien,
		SM->Zusatzinfo[0], SM->Zusatzinfo[1], SM->Zusatzinfo[2], SM->Zusatzinfo[3]);
	print_systemmeldung(SM->Systemmeldungsnr, SM->Indizienlaenge, SM->Indizien);
}

/* base station ends list of alarm messages */
void decode_ylsef(uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len)
{
	LOGP(DMUP, LOGL_INFO, "(BS YLSEF) Systemmeldungsuebertragungsende an MSC\n");
}

/* base station requests billing info */
void decode_stdaf(uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len)
{
	LOGP(DMUP, LOGL_INFO, "(BS STDAF) Tarifdatenauftrag der BS\n");
}

/* reply to billing info */
int encode_xgtau(uint8_t *opcode, uint8_t **data, uint8_t Z, uint32_t T, uint8_t S, uint8_t K, uint16_t CS)
{
	static uint8_t buffer[9];
	// Example from UeLE-ROM = { 0xff, 0x00, 0x01, 0xec, 0x3f, 0x01, 0x31, 0x1c, 0x03 };
	//                         { 0xff, 0x00, 0x01, 0xec, 0x3f, 0x01, 0x41, 0x1c, 0x03 };

	LOGP(DMUP, LOGL_INFO, "(MSC XGTAU) Tarifdatensignalisierung vom MSC\n");

	*opcode = OPCODE_XGTAU;
	buffer[0] = 0xff;
	buffer[1] = 0x00;
	buffer[2] = Z;
	buffer[3] = T;
	buffer[4] = T >> 8;
	buffer[5] = T >> 16;
	buffer[6] = S | (K << 4);
	buffer[7] = CS;
	buffer[8] = CS >> 8;

	*data = buffer;
	return sizeof(buffer);
}

/* inscription */
void decode_ebaf(uint8_t *data, int len, uint16_t *T, uint8_t *U, uint8_t *N, uint16_t *s, uint8_t *u, uint8_t *b, uint8_t *l)
{
	if (len < 6) {
		LOGP(DMUP, LOGL_NOTICE, "Message too short!\n");
		return;
	}

	*T = (data[1] << 8) | data[0];
	*U = data[2] & 0x1f;
	*N = data[2] >> 5;
	*s = (data[4] << 8) | data[3];
	*u = (data[5] >> 1) & 0x7;
	*b = (data[5] >> 6) & 0x1;
	*l = data[5] >> 7;

	LOGP(DMUP, LOGL_INFO, "(BS EBAF) Einbuchauftrag: FuTln=%d,%d,%d (0161-%d%d%05d) reader=%s\n", *N, *U, *T, *N, *U, *T, (b) ? "chip" : "magent");
}

/* ack to inscription */
int encode_ebpqu(uint8_t *opcode, uint8_t **data)
{
	LOGP(DMUP, LOGL_INFO, "(MSC EBPQU) Einbuchungs-Positiv-Quittiung vom MSC\n");

	*opcode = OPCODE_EBPQU;
	*data = NULL;
	return 0;
}

/* leave BS */
void decode_abaf(uint8_t *data, int len, uint16_t *T, uint8_t *U, uint8_t *N)
{
	if (len < 3) {
		LOGP(DMUP, LOGL_NOTICE, "Message too short!\n");
		return;
	}

	*T = (data[1] << 8) | data[0];
	*U = data[2] & 0x1f;
	*N = data[2] >> 5;

	LOGP(DMUP, LOGL_INFO, "(BS ABAF) Ausbuchung-Auftrag der BS: FuTln=%d,%d,%d (0161-%d%d%05d)\n", *N, *U, *T, *N, *U, *T);
}

static char digit2char[16] = "0123456789a*#bcd";

/* MO call */
static void _decode_outgoing(uint8_t *data, int len, uint16_t *T, uint8_t *U, uint8_t *N, char *number)
{
	int i;

	if (len < 11) {
		LOGP(DMUP, LOGL_NOTICE, "Message too short!\n");
		return;
	}

	*T = (data[1] << 8) | data[0];
	*U = data[2] & 0x1f;
	*N = data[2] >> 5;

	len = data[3] & 0xf;
	if (len == 0) {
		/* 16 digits, starting with 0 */
		for (i = 0; i < 16; i++) {
			if ((i & 1) == 0)
				number[i] = digit2char[(data[3 + (i >> 1)] & 0xf)];
			else
				number[i] = digit2char[(data[3 + (i >> 1)] >> 4)];
		}
		number[16] = '\0';
	} else {
		/* up to 16 digits */
		for (i = 0; i < len; i++) {
			if ((i & 1) == 0)
				number[i] = digit2char[(data[3 + ((i + 1) >> 1)] >> 4)];
			else
				number[i] = digit2char[(data[3 + ((i + 1) >> 1)] & 0xf)];
		}
		number[len] = '\0';
	}
}

void decode_gvaf(uint8_t *data, int len, uint16_t *T, uint8_t *U, uint8_t *N, char *number)
{
	_decode_outgoing(data, len, T, U , N, number);

	LOGP(DMUP, LOGL_INFO, "(BS GVAF) Gehender Verbindungs-Auftrag der BS: FuTln=%d,%d,%d (0161-%d%d%05d) number=%s\n", *N, *U, *T, *N, *U, *T, number);
}

void decode_gvwaf(uint8_t *data, int len, uint16_t *T, uint8_t *U, uint8_t *N, char *number)
{
	_decode_outgoing(data, len, T, U , N, number);

	LOGP(DMUP, LOGL_INFO, "(BS GVWAF) Gehender Verbindungs-Warteschlange-Auftrag der BS: FuTln=%d,%d,%d (0161-%d%d%05d) number=%s\n", *N, *U, *T, *N, *U, *T, number);
}

/* ack to MO call */
int encode_gvpqu(uint8_t *opcode, uint8_t **data, uint8_t P, uint8_t e)
{
	static uint8_t buffer[2];

	LOGP(DMUP, LOGL_INFO, "(MSC GVPQU) Verbindungs-Positiv-Quittiung vom MSC: Prio=%d (%s) AP-Pruefung=%d\n", P, prio_string(P), e);

	*opcode = OPCODE_GVNQU;
	buffer[0] = P;
	buffer[1] = e;

	*data = buffer;
	return sizeof(buffer);
}

/* nack to MO call */
int encode_gvnqu(uint8_t *opcode, uint8_t **data, uint8_t X, uint8_t Y)
{
	static uint8_t buffer[2];

	LOGP(DMUP, LOGL_INFO, "(MSC GVNQU) Verbindungs-Negativ-Quittiung vom MSC: Grund=%d (%s) Grund(FuTlg)=%d (%s)\n", X, fufst_cause(X), Y, futln_cause(Y));

	*opcode = OPCODE_GVNQU;
	buffer[0] = X;
	buffer[1] = Y;

	*data = buffer;
	return sizeof(buffer);
}

/* MT call */
int encode_kvau(uint8_t *opcode, uint8_t **data, uint16_t T, uint8_t U, uint8_t N, uint8_t F, uint8_t e)
{
	static uint8_t buffer[5];

	LOGP(DMUP, LOGL_INFO, "(MSC KVAU) Kommender Verbindungs-Auftrag vom MSC: FuTln=%d,%d,%d (0161-%d%d%05d) Rufzeitbegrenzung=%d (%s) AP-Pruefung=%d\n", N, U, T, N, U, T, F, rufzeit_string(F), e);

	*opcode = OPCODE_KVAU;
	buffer[0] = T;
	buffer[1] = T >> 8;
	buffer[2] = U | (N << 5);
	buffer[3] = F;
	buffer[4] = e;

	*data = buffer;
	return sizeof(buffer);
}

/* ack to MT call on queue */
void decode_kvwqf(uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len)
{
	LOGP(DMUP, LOGL_INFO, "(BS KVWQF) Kommende Verbindungs-Warteschalngen-Quittung der BS\n");
}

/* answer of MT call */
void decode_kvbaf(uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len)
{
	LOGP(DMUP, LOGL_INFO, "(BS KVBAF) Kommende Verbindungs-Beginn-Auftrag der BS\n");
}

/* loop test request */
void decode_staf(uint8_t *data, int len, uint8_t *Q, uint8_t *V, uint8_t *e, uint64_t *n)
{
	if (len < 10) {
		LOGP(DMUP, LOGL_NOTICE, "Message too short!\n");
		return;
	}

	*Q = data[0];
	*V = data[1] & 1;
	*e = (data[1] >> 1) & 1;
	*n = (uint64_t)data[2];
	*n |= (uint64_t)data[3] << 8;
	*n |= (uint64_t)data[4] << 16;
	*n |= (uint64_t)data[5] << 24;
	*n |= (uint64_t)data[6] << 32;
	*n |= (uint64_t)data[7] << 40;
	*n |= (uint64_t)data[8] << 48;
	*n |= (uint64_t)data[9] << 56;

	LOGP(DMUP, LOGL_INFO, "(BS STAF) Schleifentest-Auftrag der BS: SPK=%d Typ=%d (%s) AP-Pruefung=%d Random=0x%016" PRIx64 "\n", *Q, *V, typ_string(*V), *e, *n);
}

/* loop test positive */
int encode_stpqu(uint8_t *opcode, uint8_t **data, uint8_t Q, uint8_t A, uint8_t K, uint16_t G, uint8_t U, uint8_t X, uint8_t Y, uint8_t mystery)
{
	static uint8_t buffer[7];

	LOGP(DMUP, LOGL_INFO, "(MSC STPQU) Schleifentest-Positiv-Quittiung vom MSC: SPK=%d\n", Q);

	*opcode = OPCODE_STPQU;
	buffer[0] = Q;
	buffer[1] = A;
	buffer[2] = K;
	buffer[3] = G;
	buffer[4] = G >> 8;
	buffer[5] = (X << 7) | U;
	buffer[6] = (Y << 7) | (mystery << 1);

	*data = buffer;
	return sizeof(buffer);
}

/* loop test negative */
int encode_stnqu(uint8_t *opcode, uint8_t **data, uint8_t Q)
{
	static uint8_t buffer[1];

	LOGP(DMUP, LOGL_INFO, "(MSC STNQU) Schleifentest-Negativ-Quittiung vom MSC: SPK=%d\n", Q);

	*opcode = OPCODE_STNQU;
	buffer[0] = Q;

	*data = buffer;
	return sizeof(buffer);
}

/* authentication response */
void decode_apf(uint8_t *data, int len, uint8_t *Q, uint64_t *a)
{
	if (len < 9) {
		LOGP(DMUP, LOGL_NOTICE, "Message too short!\n");
		return;
	}

	*Q = data[0];
	*a = (uint64_t)data[1];
	*a |= (uint64_t)data[2] << 8;
	*a |= (uint64_t)data[3] << 16;
	*a |= (uint64_t)data[4] << 24;
	*a |= (uint64_t)data[5] << 32;
	*a |= (uint64_t)data[6] << 40;
	*a |= (uint64_t)data[7] << 48;
	*a |= (uint64_t)data[8] << 56;

	LOGP(DMUP, LOGL_INFO, "(BS APF) Autorisierunsparameter FUKO: SPK=%d AP=0x%016" PRIx64 "\n", *Q, *a);
}

/* start metering pulses (answer to call) */
int encode_gstau(uint8_t *opcode, uint8_t **data, uint8_t Q, uint16_t G, uint8_t U, uint8_t Y, uint8_t A, uint8_t K)
{
	static uint8_t buffer[6];

	LOGP(DMUP, LOGL_INFO, "(MSC GSTAU) Gebuehren-Start-Auftrag vom MSC: SPK=%d\n", Q);

	*opcode = OPCODE_GSTAU;
	buffer[0] = Q;
	buffer[1] = G;
	buffer[2] = G >> 8;
	buffer[3] = (Y << 7) | U;
	buffer[4] = A;
	buffer[5] = K;

	*data = buffer;
	return sizeof(buffer);
}

/* MCID */
void decode_faf(uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len)
{
	LOGP(DMUP, LOGL_INFO, "(BS FAF) Fang-Auftrag der BS\n");
}

/* release by base station (before SPK assignment) */
void decode_naf(uint8_t *data, int len, uint8_t *X)
{
	if (len < 1) {
		LOGP(DMUP, LOGL_NOTICE, "Message too short!\n");
		return;
	}

	*X = data[0];

	LOGP(DMUP, LOGL_INFO, "(BS AAF) Negativ-Auftrag der BS: Grund=%d (%s)\n", *X, fufst_cause(*X));
}

/* release by base station ack (before SPK assignment) */
int encode_equ(uint8_t *opcode, uint8_t **data)
{
	LOGP(DMUP, LOGL_INFO, "(MSC AQU) Ende-Quittung vom MSC\n");

	*opcode = OPCODE_EQU;
	*data = NULL;
	return 0;
}

/* release by base station (after SPK assignment) */
void decode_aaf(uint8_t *data, int len, uint8_t *Q, uint8_t *X)
{
	if (len < 2) {
		LOGP(DMUP, LOGL_NOTICE, "Message too short!\n");
		return;
	}

	*Q = data[0];
	*X = data[1];

	LOGP(DMUP, LOGL_INFO, "(BS AAF) Ausloese-Auftrag der BS: SPK=%d, Grund=%d (%s)\n", *Q, *X, fufst_cause(*X));
}

/* release by base station ack (after SPK assignment) */
int encode_aqu(uint8_t *opcode, uint8_t **data, uint8_t Q)
{
	static uint8_t buffer[1];

	LOGP(DMUP, LOGL_INFO, "(MSC AQU) Ausloese-Quittung vom MSC: SPK=%d\n", Q);

	*opcode = OPCODE_AQU;
	buffer[0] = Q;

	*data = buffer;
	return sizeof(buffer);
}

/* release by network (before SPK assignment) */
int encode_nau(uint8_t *opcode, uint8_t **data, uint8_t X, uint8_t Y)
{
	static uint8_t buffer[2];

	LOGP(DMUP, LOGL_INFO, "(MSC NAU) Negativ-Auftrag vom MSC: Grund=%d (%s) Grund(FuTlg)=%d (%s)\n", X, fufst_cause(X), Y, futln_cause(Y));

	*opcode = OPCODE_NAU;
	buffer[0] = X;
	buffer[1] = Y;

	*data = buffer;
	return sizeof(buffer);
}

/* release by network ack (before SPK assignment) */
void decode_eqf(uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len)
{
	LOGP(DMUP, LOGL_INFO, "(BS EQF) Ende-Quittung der BS\n");
}

/* release by network (after SPK assignment) */
int encode_aau(uint8_t *opcode, uint8_t **data, uint8_t Q, uint8_t X, uint8_t Y)
{
	static uint8_t buffer[3];

	LOGP(DMUP, LOGL_INFO, "(MSC AAU) Ausloese-Auftrag vom MSC: SPK=%d, Grund=%d (%s) Grund(FuTlg)=%d (%s)\n", Q, X, fufst_cause(X), Y, futln_cause(Y));

	*opcode = OPCODE_AAU;
	buffer[0] = Q;
	buffer[1] = X;
	buffer[2] = Y;

	*data = buffer;
	return sizeof(buffer);
}

/* release by network ack (after SPK assignment) */
void decode_aqf(uint8_t *data, int len, uint8_t *Q)
{
	if (len < 1) {
		LOGP(DMUP, LOGL_NOTICE, "Message too short!\n");
		return;
	}

	*Q = data[0];

	LOGP(DMUP, LOGL_INFO, "(BS AQF) Ausloese-Quittung der BS: SPK=%d\n", *Q);
}

/* request data base block */
void decode_xadbf(uint8_t *data, int len, uint8_t *PJ, uint16_t *D, uint16_t *L)
{
	if (len < 6) {
		LOGP(DMUP, LOGL_NOTICE, "Message too short!\n");
		return;
	}

	*PJ = data[1];
	*D = data[2] | (data[3] << 8);
	*L = data[4] | (data[5] << 8);

	LOGP(DMUP, LOGL_INFO, "(BS XADBF) Auftragssign. Anfordern BS-DB-Datenblock am MSC: job=%d, offset=0x%02x length=0x%02x\n", *PJ, *D, *L);
}

/* transfer data base block */
int encode_xedbu_1(uint8_t *opcode, uint8_t **data, uint8_t R, uint8_t PJ, uint16_t A)
{
	static uint8_t buffer[4];

	LOGP(DMUP, LOGL_INFO, "(MSC XEDBU) Ergebnissignal. Transfer BS-DB-Datenblock (header): return=%d job=%d frames=%d\n", R, PJ, A);

	*opcode = OPCODE_XEDBU;
	buffer[0] = R;
	buffer[1] = PJ;
	buffer[2] = A;
	buffer[3] = A >> 8;

	*data = buffer;
	return sizeof(buffer);
}
int encode_xedbu_2(uint8_t *opcode, uint8_t **data, uint8_t S, uint8_t PJ, uint8_t *P)
{
	static uint8_t buffer[11];

	if (loglevel == LOGL_DEBUG)
		LOGP(DMUP, LOGL_INFO, "(MSC XEDBU) Ergebnissignal. Transfer BS-DB-Datenblock (data): count=%d job=%d data=%s\n", S, PJ, osmo_hexdump(P, 9));
	else if (S == 1)
		LOGP(DMUP, LOGL_INFO, "(MSC XEDBU) Ergebnissignal. Transfer BS-DB-Datenblock (data): Messages are not shown, due to heavy debug output!\n");

	*opcode = OPCODE_XEDBU;
	buffer[0] = S;
	buffer[1] = PJ;
	memcpy(buffer + 2, P, 9);

	*data = buffer;
	return sizeof(buffer);
}
int encode_xedbu_3(uint8_t *opcode, uint8_t **data, uint8_t S, uint8_t PJ, uint16_t D, uint16_t L, uint32_t CS)
{
	static uint8_t buffer[9];

	LOGP(DMUP, LOGL_INFO, "(MSC XEDBU) Ergebnissignal. Transfer BS-DB-Datenblock (footer): count=%d job=%d offset=0x%02x length=0x%02x checksum=0x%06x\n", S, PJ, D, L, CS);

	*opcode = OPCODE_XEDBU;
	buffer[0] = S;
	buffer[1] = PJ;
	buffer[2] = D;
	buffer[3] = D >> 8;
	buffer[4] = L;
	buffer[5] = L >> 8;
	buffer[6] = CS;
	buffer[7] = CS >> 8;
	buffer[8] = CS >> 16;

	*data = buffer;
	return sizeof(buffer);
}

/* BS reboot order */
int encode_yaaau(uint8_t *opcode, uint8_t **data, uint8_t J)
{
	static uint8_t buffer[2];

	LOGP(DMUP, LOGL_INFO, "(MSC YAAAU) Auftrag Initialisieren BS des MSC: job=%d\n", J);

	*opcode = OPCODE_YAAAU;
	buffer[0] = 0xff;
	buffer[1] = J;

	*data = buffer;
	return sizeof(buffer);
}

/* MSC boots */
int encode_swau(uint8_t *opcode, uint8_t **data, uint8_t V)
{
	static uint8_t buffer[1];

	LOGP(DMUP, LOGL_INFO, "(MSC SWAU) Wiederanlaufauftrag des MSC: version=%d (%s)\n", V, version_string(V));

	*opcode = OPCODE_SWAU;
	buffer[0] = V;

	*data = buffer;
	return sizeof(buffer);
}

/* ack to MSC boot */
void decode_swqf(uint8_t *data, int len, uint8_t *V, uint8_t *N, uint8_t *U, uint8_t *F, uint8_t *C, uint8_t *B)
{
	if (len < 6) {
		LOGP(DMUP, LOGL_NOTICE, "Message too short!\n");
		return;
	}

	*V = data[0];
	*N = data[1];
	*U = data[2];
	*F = data[3];
	*C = data[4];
	*B = data[5];

	LOGP(DMUP, LOGL_INFO, "(BS SWQF) Wiederanlauf-Quittung der BS: version=%d (%s) FuFSt=%d,%d,%d chip=%d (%s) beacon=%d (%s)\n", *V, version_string(*V), *N, *U, *F, *C, chip_string(*C), *B, beacon_string(*B));
}

/* request "Aktivdatei" (inscripted substribers) */
void encode_sadau(uint8_t *opcode)
{
	*opcode = OPCODE_SADAU;
	LOGP(DMUP, LOGL_INFO, "(MSC SADAU) Aktivdatei-Auftrag vom MSC\n");
}

/* ack "Aktivdatei" */
int decode_sadqf(uint8_t *data, int len, uint16_t *S, uint8_t *E, uint8_t *b, uint16_t *T, uint8_t *U, uint8_t *N)
{
	int i, n = 0;

	if (len < 11) {
		LOGP(DMUP, LOGL_NOTICE, "Message too short!\n");
		return 0;
	}

	*S = ((data[1] & 0xf) << 4) | data[0];
	*E = data[1] >> 7;
	for (i = 0; i < 3; i++) {
		b[n] = (data[1] >> (6 - i)) & 0x1;
		T[n] = (data[3 + (i * 3)] << 8) | data[2 + (i * 3)];
		U[n] = data[4 + (i * 3)] & 0x1f;
		N[n] = data[4 + (i * 3)] >> 5;
		if (T[n] != 0 || U[n] != 0 || N[n] != 0)
			n++;
	}

	LOGP(DMUP, LOGL_INFO, "(BS SADQF) Aktivdateiquittung der BS:\n");
	for (i = 0; i < n; i++)
		LOGP(DMUP, LOGL_INFO, " %d: FuTln=%d,%d,%d (0161-%d%d%05d) reader=%s\n", i + 1, N[i], U[i], T[i], N[i], U[i], T[i], (b[i]) ? "chip" : "magent");

	return n;
}

