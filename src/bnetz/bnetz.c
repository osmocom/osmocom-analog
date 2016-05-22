/* B-Netz protocol handling
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../common/debug.h"
#include "../common/timer.h"
#include "../common/call.h"
#include "../common/cause.h"
#include "bnetz.h"
#include "dsp.h"

/* Call reference for calls from mobile station to network
   This offset of 0x400000000 is required for MNCC interface. */
static int new_callref = 0x40000000;

/* mobile originating call */
#define CARRIER_TO	0.08	/* 80 ms search for carrier */
#define GRUPPE_TO	0.4	/* 400 ms search for "Gruppensignal" */
#define DIALING_TO	1.00	/* FIXME: get real value */

/* radio loss condition */
#define LOSS_OF_SIGNAL	9.6	/* duration of carrier loss until release: 9.6 s */

/* mobile terminating call */
#define ALERTING_TO	60	/* timeout after 60 seconds alerting the MS */
#define PAGING_TO	4	/* timeout 4 seconds after "Selektivruf" */
#define PAGE_TRIES	2	/* two tries */
#define SWITCH19_TIME	1.0	/* time to switch channel (radio should be tansmitting after that) */
#define SWITCHBACK_TIME	0.1	/* time to wait until switching back (latency of sound device shall be lower) */

/* Convert channel number to frequency number of base station.
   Set 'unterband' to 1 to get frequency of mobile station. */
double bnetz_kanal2freq(int kanal, int unterband)
{
	double freq = 153.010;

	if (kanal >= 50)
		freq += 9.200 - 0.020 * 49;
	freq += (kanal - 1) * 0.020;
	if (unterband)
		freq -= 4.600;

	return freq;
}

/* List of message digits */
static struct impulstelegramme {
	int digit;
	const char *sequence;
	uint16_t telegramm;
	const char *description;
} impulstelegramme[] = {
	/* Ziffern */
	{ '0',	"0111011000000011", 0x0000, "Ziffer 0" },
	{ '1',	"0111010100000101", 0x0000, "Ziffer 1" },
	{ '2',	"0111010010001001", 0x0000, "Ziffer 2" },
	{ '3',	"0111010001010001", 0x0000, "Ziffer 3" },
	{ '4',	"0111001100000110", 0x0000, "Ziffer 4" },
	{ '5',	"0111001010001010", 0x0000, "Ziffer 5" },
	{ '6',	"0111001001010010", 0x0000, "Ziffer 6" },
	{ '7',	"0111000110001100", 0x0000, "Ziffer 7" },
	{ '8',	"0111000101010100", 0x0000, "Ziffer 8" },
	{ '9',	"0111000011011000", 0x0000, "Ziffer 9" },
	/* Signale */
	{ 's',	"0111001000100010", 0x0000, "Funkwahl ohne Gebuehrenuebermittlung" },
	{ 'S',	"0111000100100100", 0x0000, "Funkwahl mit Gebuehrenuebermittlung" },
	{ 'e',	"0111010000100001", 0x0000, "Funkwahlende" },
	{ 't',	"0111010101010101", 0x0000, "Trennsignal/Schlusssignal" },
	/* Kanalbefehl B1 */
	{ 1001,	"0111011000000101", 0x0000, "Kanalbefehl 1" },
	{ 1002,	"0111011000001001", 0x0000, "Kanalbefehl 2" },
	{ 1003,	"0111011000010001", 0x0000, "Kanalbefehl 3" },
	{ 1004,	"0111011000000110", 0x0000, "Kanalbefehl 4" },
	{ 1005,	"0111011000001010", 0x0000, "Kanalbefehl 5" },
	{ 1006,	"0111011000010010", 0x0000, "Kanalbefehl 6" },
	{ 1007,	"0111011000001100", 0x0000, "Kanalbefehl 7" },
	{ 1008,	"0111011000010100", 0x0000, "Kanalbefehl 8" },
	{ 1009,	"0111011000011000", 0x0000, "Kanalbefehl 9" },
	{ 1010,	"0111010100000011", 0x0000, "Kanalbefehl 10" },
	{ 1011,	"0111001100000101", 0x0000, "Kanalbefehl 11" }, /* 41 */
	{ 1012,	"0111010100001001", 0x0000, "Kanalbefehl 12" },
	{ 1013,	"0111010100010001", 0x0000, "Kanalbefehl 13" },
	{ 1014,	"0111010100000110", 0x0000, "Kanalbefehl 14" },
	{ 1015,	"0111010100001010", 0x0000, "Kanalbefehl 15" },
	{ 1016,	"0111010100010010", 0x0000, "Kanalbefehl 16" },
	{ 1017,	"0111010100001100", 0x0000, "Kanalbefehl 17" },
	{ 1018,	"0111010100010100", 0x0000, "Kanalbefehl 18" },
	{ 1019,	"0111010100011000", 0x0000, "Kanalbefehl 19 (illegal)" },
	{ 1020,	"0111010010000011", 0x0000, "Kanalbefehl 20" },
	{ 1021,	"0111010010000101", 0x0000, "Kanalbefehl 21" },
	{ 1022,	"0111001100001001", 0x0000, "Kanalbefehl 22" }, /* 42 */
	{ 1023,	"0111010010010001", 0x0000, "Kanalbefehl 23" },
	{ 1024,	"0111010010000110", 0x0000, "Kanalbefehl 24" },
	{ 1025,	"0111010010001010", 0x0000, "Kanalbefehl 25" },
	{ 1026,	"0111010010010010", 0x0000, "Kanalbefehl 26" },
	{ 1027,	"0111010010001100", 0x0000, "Kanalbefehl 27" },
	{ 1028,	"0111010010010100", 0x0000, "Kanalbefehl 28" },
	{ 1029,	"0111010010011000", 0x0000, "Kanalbefehl 29" },
	{ 1030,	"0111010001000011", 0x0000, "Kanalbefehl 30" },
	{ 1031,	"0111010001000101", 0x0000, "Kanalbefehl 31" },
	{ 1032,	"0111010001001001", 0x0000, "Kanalbefehl 32" },
	{ 1033,	"0111001100010001", 0x0000, "Kanalbefehl 33" }, /* 43 */
	{ 1034,	"0111010001000110", 0x0000, "Kanalbefehl 34" },
	{ 1035,	"0111010001001010", 0x0000, "Kanalbefehl 35" },
	{ 1036,	"0111010001010010", 0x0000, "Kanalbefehl 36" },
	{ 1037,	"0111010001001100", 0x0000, "Kanalbefehl 37" },
	{ 1038,	"0111010001010100", 0x0000, "Kanalbefehl 38" },
	{ 1039,	"0111010001011000", 0x0000, "Kanalbefehl 39" },
	/* Kanalbefehl B2 */
	{ 1050, "0111001010000011", 0x0000, "Kanalbefehl 50" },
	{ 1051, "0111001010000101", 0x0000, "Kanalbefehl 51" },
	{ 1052, "0111001010001001", 0x0000, "Kanalbefehl 52" },
	{ 1053, "0111001010010001", 0x0000, "Kanalbefehl 53" },
	{ 1054, "0111001010000110", 0x0000, "Kanalbefehl 54" },
	{ 1055, "0111001100001010", 0x0000, "Kanalbefehl 55" }, /* 45 */
	{ 1056, "0111001010010010", 0x0000, "Kanalbefehl 56" },
	{ 1057, "0111001010001100", 0x0000, "Kanalbefehl 57" },
	{ 1058, "0111001010010100", 0x0000, "Kanalbefehl 58" },
	{ 1059, "0111001010011000", 0x0000, "Kanalbefehl 59" },
	{ 1060, "0111001001000011", 0x0000, "Kanalbefehl 60" },
	{ 1061, "0111001001000101", 0x0000, "Kanalbefehl 61" },
	{ 1062, "0111001001001001", 0x0000, "Kanalbefehl 62" },
	{ 1063, "0111001001010001", 0x0000, "Kanalbefehl 63" },
	{ 1064, "0111001001000110", 0x0000, "Kanalbefehl 64" },
	{ 1065, "0111001001001010", 0x0000, "Kanalbefehl 65" },
	{ 1066, "0111001100010010", 0x0000, "Kanalbefehl 66" }, /* 46 */
	{ 1067, "0111001001001100", 0x0000, "Kanalbefehl 67" },
	{ 1068, "0111001001010100", 0x0000, "Kanalbefehl 68" },
	{ 1069, "0111001001011000", 0x0000, "Kanalbefehl 69" },
	{ 1070, "0111000110000011", 0x0000, "Kanalbefehl 70" },
	{ 1071, "0111000110000101", 0x0000, "Kanalbefehl 71" },
	{ 1072, "0111000110001001", 0x0000, "Kanalbefehl 72" },
	{ 1073, "0111000110010001", 0x0000, "Kanalbefehl 73" },
	{ 1074, "0111000110000110", 0x0000, "Kanalbefehl 74" },
	{ 1075, "0111000110001010", 0x0000, "Kanalbefehl 75" },
	{ 1076, "0111000110010010", 0x0000, "Kanalbefehl 76" },
	{ 1077, "0111001100001100", 0x0000, "Kanalbefehl 77" }, /* 47 */
	{ 1078, "0111000110010100", 0x0000, "Kanalbefehl 78" },
	{ 1079, "0111000110011000", 0x0000, "Kanalbefehl 79" },
	{ 1080, "0111000101000011", 0x0000, "Kanalbefehl 80" },
	{ 1081, "0111000101000101", 0x0000, "Kanalbefehl 81" },
	{ 1082, "0111000101001001", 0x0000, "Kanalbefehl 82" },
	{ 1083, "0111000101010001", 0x0000, "Kanalbefehl 83" },
	{ 1084, "0111000101000110", 0x0000, "Kanalbefehl 84" },
	{ 1085, "0111000101001010", 0x0000, "Kanalbefehl 85" },
	{ 1086, "0111000101010010", 0x0000, "Kanalbefehl 86" },
	/* Gruppenfreisignale */
	{ 2001,	"0111000011000101", 0x0000, "Gruppenfreisignal 1" }, /* 91 */
	{ 2002,	"0111000011001001", 0x0000, "Gruppenfreisignal 2" }, /* 92 */
	{ 2003,	"0111000011010001", 0x0000, "Gruppenfreisignal 3" }, /* 93 */
	{ 2004,	"0111000011000110", 0x0000, "Gruppenfreisignal 4" }, /* 94 */
	{ 2005,	"0111000011001010", 0x0000, "Gruppenfreisignal 5" }, /* 95 */
	{ 2006,	"0111000011010010", 0x0000, "Gruppenfreisignal 6" }, /* 96 */
	{ 2007,	"0111000011001100", 0x0000, "Gruppenfreisignal 7" }, /* 97 */
	{ 2008,	"0111000011010100", 0x0000, "Gruppenfreisignal 8" }, /* 98 */
	{ 2009,	"0111000011000011", 0x0000, "Gruppenfreisignal 9" }, /* 90 */
	{ 2010,	"0111000101000011", 0x0000, "Gruppenfreisignal 10" }, /* 80 */
	{ 2011,	"0111000101000101", 0x0000, "Gruppenfreisignal 11" }, /* 81 */
	{ 2012,	"0111000101001001", 0x0000, "Gruppenfreisignal 12" }, /* 82 */
	{ 2013,	"0111000101010001", 0x0000, "Gruppenfreisignal 13" }, /* 83 */
	{ 2014,	"0111000101000110", 0x0000, "Gruppenfreisignal 14" }, /* 84 */
	{ 2015,	"0111000101001010", 0x0000, "Gruppenfreisignal 15" }, /* 85 */
	{ 2016,	"0111000101010010", 0x0000, "Gruppenfreisignal 16" }, /* 86 */
	{ 2017,	"0111000101001100", 0x0000, "Gruppenfreisignal 17" }, /* 87 */
	{ 2018,	"0111000101010100", 0x0000, "Gruppenfreisignal 18" }, /* 88 */
	{ 2019,	"0111000101011000", 0x0000, "Gruppenfreisignal 19 (Kanaele kleiner Leistung)" }, /* 89 */
	{ 0, NULL, 0, NULL }
};

/* Return bit sequence string by given digit. */
static struct impulstelegramme *bnetz_telegramm(int digit)
{
	int i;

	for (i = 0; impulstelegramme[i].digit; i++) {
		if (impulstelegramme[i].digit == digit)
			return &impulstelegramme[i];
	}

	return NULL;
}

/* switch pilot signal (tone or file) */
static void switch_channel_19(bnetz_t *bnetz, int on)
{
	if (bnetz->sender.use_pilot_signal >= 0) {
		bnetz->sender.pilot_on = on;
		return;
	}

	if (bnetz->pilot_file && bnetz->pilot_is_on != on) {
		FILE *fp;

		fp = fopen(bnetz->pilot_file, "w");
		if (!fp) {
			PDEBUG(DBNETZ, DEBUG_ERROR, "Failed to open file '%s' to switch channel 19!\n");
			return;
		}
		fprintf(fp, "%s\n", (on) ? bnetz->pilot_on : bnetz->pilot_off);
		fclose(fp);
		bnetz->pilot_is_on = on;
	}
}

/* global init */
int bnetz_init(void)
{
	int i, j;

	for (i = 0; impulstelegramme[i].digit; i++) {
		uint16_t telegramm = 0;
		for (j = 0; j < strlen(impulstelegramme[i].sequence); j++) {
			telegramm <<= 1;
			telegramm |= (impulstelegramme[i].sequence[j] == '1');
		}
		impulstelegramme[i].telegramm = telegramm;
	}

	return 0;
}

static void bnetz_timeout(struct timer *timer);
static void bnetz_go_idle(bnetz_t *bnetz);

/* Create transceiver instance and link to a list. */
int bnetz_create(int kanal, const char *sounddev, int samplerate, int cross_channels, double rx_gain, int gfs, int pre_emphasis, int de_emphasis, const char *write_wave, const char *read_wave, int loopback, double loss_factor, const char *pilot)
{
	bnetz_t *bnetz;
	int use_pilot_tone = -1;
	char pilot_file[256] = "", pilot_on[256] = "", pilot_off[256] = "";
	int rc;

	if (!(kanal >= 1 && kanal <= 39) && !(kanal >= 50 && kanal <= 86)) {
		PDEBUG(DBNETZ, DEBUG_ERROR, "Channel ('Kanal') number %d invalid.\n", kanal);
		return -EINVAL;
	}

	if (kanal == 19) {
		PDEBUG(DBNETZ, DEBUG_ERROR, "Selected calling channel ('Rufkanal') number %d can't be used as traffic channel.\n", kanal);
		return -EINVAL;
	}

	if ((gfs < 1 || gfs > 19)) {
		PDEBUG(DBNETZ, DEBUG_ERROR, "Given 'Gruppenfreisignal' %d invalid.\n", gfs);
		return -EINVAL;
	}
	
	if (!strcmp(pilot, "tone"))
		use_pilot_tone = 2;
	else
	if (!strcmp(pilot, "positive"))
		use_pilot_tone = 1;
	else
	if (!strcmp(pilot, "negative"))
		use_pilot_tone = 0;
	else {
		char *p;

		strncpy(pilot_file, pilot, sizeof(pilot_file) - 1);
		p = strchr(pilot_file, '=');
		if (!p) {
error_pilot:
			PDEBUG(DBNETZ, DEBUG_ERROR, "Given pilot file (to switch to channel 19) is missing parameters. Use <file>=<on>:<off> format!\n");
			return -EINVAL;
		}
		*p++ = '\0';
		strncpy(pilot_on, p, sizeof(pilot_on) - 1);
		p = strchr(pilot_file, ':');
		if (!p)
			goto error_pilot;
		*p++ = '\0';
		strncpy(pilot_off, p, sizeof(pilot_off) - 1);
	}

	bnetz = calloc(1, sizeof(bnetz_t));
	if (!bnetz) {
		PDEBUG(DBNETZ, DEBUG_ERROR, "No memory!\n");
		return -ENOMEM;
	}

	PDEBUG(DBNETZ, DEBUG_DEBUG, "Creating 'B-Netz' instance for 'Kanal' = %d 'Gruppenfreisignal' = %d (sample rate %d).\n", kanal, gfs, samplerate);

	/* init general part of transceiver */
	rc = sender_create(&bnetz->sender, kanal, sounddev, samplerate, cross_channels, rx_gain, pre_emphasis, de_emphasis, write_wave, read_wave, loopback, loss_factor, use_pilot_tone);
	if (rc < 0) {
		PDEBUG(DBNETZ, DEBUG_ERROR, "Failed to init transceiver process!\n");
		goto error;
	}

	/* init audio processing */
	rc = dsp_init_sender(bnetz);
	if (rc < 0) {
		PDEBUG(DBNETZ, DEBUG_ERROR, "Failed to init audio processing!\n");
		goto error;
	}

	bnetz->gfs = gfs;
	strncpy(bnetz->pilot_file, pilot_file, sizeof(bnetz->pilot_file) - 1);
	strncpy(bnetz->pilot_on, pilot_on, sizeof(bnetz->pilot_on) - 1);
	strncpy(bnetz->pilot_off, pilot_off, sizeof(bnetz->pilot_off) - 1);
	timer_init(&bnetz->timer, bnetz_timeout, bnetz);

	/* go into idle state */
	bnetz_go_idle(bnetz);

	return 0;

error:
	bnetz_destroy(&bnetz->sender);

	return rc;
}

/* Destroy transceiver instance and unlink from list. */
void bnetz_destroy(sender_t *sender)
{
	bnetz_t *bnetz = (bnetz_t *) sender;

	PDEBUG(DBNETZ, DEBUG_DEBUG, "Destroying 'B-Netz' instance for 'Kanal' = %d.\n", sender->kanal);
	switch_channel_19(bnetz, 0);
	dsp_cleanup_sender(bnetz);
	timer_exit(&bnetz->timer);
	sender_destroy(&bnetz->sender);
	free(bnetz);
}

/* Abort connection towards mobile station by sending idle digits. */
static void bnetz_go_idle(bnetz_t *bnetz)
{
	timer_stop(&bnetz->timer);

	PDEBUG(DBNETZ, DEBUG_INFO, "Entering IDLE state, sending 'Gruppenfreisignal' %d.\n", bnetz->gfs);
	bnetz->state = BNETZ_FREI;
	bnetz_set_dsp_mode(bnetz, DSP_MODE_TELEGRAMM);
	switch_channel_19(bnetz, 0);
	bnetz->station_id[0] = '\0';
}

/* Release connection towards mobile station by sending release digits. */
static void bnetz_release(bnetz_t *bnetz)
{
	timer_stop(&bnetz->timer);

	PDEBUG(DBNETZ, DEBUG_INFO, "Entering release state, sending 'Trennsignal'.\n");
	bnetz->state = BNETZ_TRENNEN;
	bnetz_set_dsp_mode(bnetz, DSP_MODE_TELEGRAMM);
	switch_channel_19(bnetz, 0);
	bnetz->trenn_count = 0;
	bnetz->station_id[0] = '\0';
}

/* Enter paging state and transmit station ID. */
static void bnetz_page(bnetz_t *bnetz, const char *dial_string, int try)
{
	PDEBUG(DBNETZ, DEBUG_INFO, "Entering paging state (try %d), sending 'Selektivruf' to '%s'.\n", try, dial_string);
	bnetz->state = BNETZ_SELEKTIVRUF_EIN;
	bnetz_set_dsp_mode(bnetz, DSP_MODE_0);
	bnetz->page_mode = PAGE_MODE_NUMBER;
	bnetz->page_try = try;
	strcpy(bnetz->station_id, dial_string);
	bnetz->station_id_pos = 0;
	timer_start(&bnetz->timer, SWITCH19_TIME);
	switch_channel_19(bnetz, 1);
}

/* FSK processing requests next digit after transmission of previous
   digit has been finished. */
const char *bnetz_get_telegramm(bnetz_t *bnetz)
{
	struct impulstelegramme *it = NULL;

	if (bnetz->sender.loopback) {
		bnetz->loopback_time[bnetz->loopback_count] = get_time();
		it = bnetz_telegramm(bnetz->loopback_count + '0');
		if (++bnetz->loopback_count > 9)
			bnetz->loopback_count = 0;
	} else
	switch(bnetz->state) {
	case BNETZ_FREI:
		it = bnetz_telegramm(2000 + bnetz->gfs);
		break;
	case BNETZ_WAHLABRUF:
		if (bnetz->station_id_pos == 5) {
			bnetz_set_dsp_mode(bnetz, DSP_MODE_1);
			return NULL;
		}
		it = bnetz_telegramm(bnetz->station_id[bnetz->station_id_pos++]);
		break;
	case BNETZ_SELEKTIVRUF_EIN:
		if (bnetz->page_mode == PAGE_MODE_KANALBEFEHL) {
			PDEBUG(DBNETZ, DEBUG_INFO, "Paging mobile station %s complete, waiting for answer.\n", bnetz->station_id);
			bnetz->state = BNETZ_SELEKTIVRUF_AUS;
			bnetz_set_dsp_mode(bnetz, DSP_MODE_SILENCE);
			timer_start(&bnetz->timer, SWITCHBACK_TIME);
			return NULL;
		}
		if (bnetz->station_id_pos == 5) {
			it = bnetz_telegramm(bnetz->sender.kanal + 1000);
			bnetz->page_mode = PAGE_MODE_KANALBEFEHL;
			break;
		}
		it = bnetz_telegramm(bnetz->station_id[bnetz->station_id_pos++]);
		break;
	case BNETZ_TRENNEN:
		if (bnetz->trenn_count++ == 75) {
			PDEBUG(DBNETZ, DEBUG_DEBUG, "Maximum number of release digits sent, going idle.\n");
			bnetz_go_idle(bnetz);
			return NULL;
		}
		it = bnetz_telegramm('t');
		break;
	default:
		break;
	}

	if (!it)
		abort();

	PDEBUG(DBNETZ, DEBUG_DEBUG, "Sending telegramm '%s'.\n", it->description);
	return it->sequence;
}

/* Loss of signal was detected, release active call. */
void bnetz_loss_indication(bnetz_t *bnetz)
{
	if (bnetz->state == BNETZ_GESPRAECH
	 || bnetz->state == BNETZ_RUFHALTUNG) {
		PDEBUG(DBNETZ, DEBUG_NOTICE, "Detected loss of signal, releasing.\n");
		bnetz_release(bnetz);
		call_in_release(bnetz->sender.callref, CAUSE_TEMPFAIL);
		bnetz->sender.callref = 0;
	}
}

/* A continuous tone was detected or is gone. */
void bnetz_receive_tone(bnetz_t *bnetz, int bit)
{
	if (bit >= 0)
		PDEBUG(DBNETZ, DEBUG_DEBUG, "Received contiuous %d Hz tone.\n", (bit)?1950:2070);
	else
		PDEBUG(DBNETZ, DEBUG_DEBUG, "Continuous tone is gone.\n");

	if (bnetz->sender.loopback) {
		return;
	}

	switch (bnetz->state) {
	case BNETZ_FREI:
		if (bit == 0) {
			PDEBUG(DBNETZ, DEBUG_INFO, "Received signal 'Kanalbelegung' from mobile station, sending signal 'Wahlabruf'.\n");
			bnetz->state = BNETZ_WAHLABRUF;
			bnetz->dial_mode = DIAL_MODE_START;
			bnetz_set_dsp_mode(bnetz, DSP_MODE_1);
			timer_start(&bnetz->timer, DIALING_TO);
			break;
		}
		break;
	case BNETZ_RUFBESTAETIGUNG:
		if (bit == 1) {
			PDEBUG(DBNETZ, DEBUG_INFO, "Received signal 'Rufbestaetigung' from mobile station, sending signal 'Rufhaltung'. (call is ringing)\n");
			timer_stop(&bnetz->timer);
			bnetz->state = BNETZ_RUFHALTUNG;
			bnetz_set_dsp_mode(bnetz, DSP_MODE_1);
			call_in_alerting(bnetz->sender.callref);
			timer_start(&bnetz->timer, ALERTING_TO);
			break;
		}
		break;
	case BNETZ_RUFHALTUNG:
		if (bit == 0) {
			PDEBUG(DBNETZ, DEBUG_INFO, "Received signal 'Beginnsignal' from mobile station, call establised.\n");
			timer_stop(&bnetz->timer);
			bnetz->state = BNETZ_GESPRAECH;
			bnetz_set_dsp_mode(bnetz, DSP_MODE_AUDIO);
			call_in_answer(bnetz->sender.callref, bnetz->station_id);
			break;
		}
	default:
		break;
	}
}

/* A digit was received. */
void bnetz_receive_telegramm(bnetz_t *bnetz, uint16_t telegramm, double level, double quality)
{
	int digit = 0;
	int i;

	PDEBUG(DFRAME, DEBUG_INFO, "Digit RX Level: %.0f%% Quality=%.0f\n", level * 100.0 + 0.5, quality * 100.0 + 0.5);

	/* drop any telegramm that is too bad */
	if (quality < 0.2)
		return;

	for (i = 0; impulstelegramme[i].digit; i++) {
		if (impulstelegramme[i].telegramm == telegramm) {
			digit = impulstelegramme[i].digit;
			break;
		}
	}
	if (digit == 0)
		PDEBUG(DBNETZ, DEBUG_DEBUG, "Received unknown telegramm digit '0x%04x'.\n", telegramm);
	else
		PDEBUG(DBNETZ, (bnetz->sender.loopback) ? DEBUG_NOTICE : DEBUG_INFO, "Received telegramm digit '%s'.\n", impulstelegramme[i].description);

	if (bnetz->sender.loopback) {
		if (digit >= '0' && digit <= '9') {
			PDEBUG(DBNETZ, DEBUG_NOTICE, "Round trip delay is %.3f seconds\n", get_time() - bnetz->loopback_time[digit - '0'] - 0.160);
		}
		return;
	}

	switch (bnetz->state) {
	case BNETZ_WAHLABRUF:
		timer_start(&bnetz->timer, DIALING_TO);
		switch (bnetz->dial_mode) {
		case DIAL_MODE_START:
			if (digit != 's' && digit != 'S') {
				PDEBUG(DBNETZ, DEBUG_NOTICE, "Received digit that is not a start digit ('Funkwahl'), aborting.\n");
				bnetz_go_idle(bnetz);
				return;
			}
			if (digit == 'S')
				bnetz->dial_metering = 1;
			else
				bnetz->dial_metering = 0;
			bnetz->dial_mode = DIAL_MODE_STATIONID;
			memset(bnetz->station_id, 0, sizeof(bnetz->station_id));
			bnetz->dial_pos = 0;
			break;
		case DIAL_MODE_STATIONID:
			if (digit < '0' || digit > '9') {
				PDEBUG(DBNETZ, DEBUG_NOTICE, "Received message that is not a valid station id digit, aborting.\n");
				bnetz_go_idle(bnetz);
				return;
			}
			bnetz->station_id[bnetz->dial_pos++] = digit;
			if (bnetz->dial_pos == 5) {
				PDEBUG(DBNETZ, DEBUG_INFO, "Received station id from mobile phone: %s\n", bnetz->station_id);
				bnetz->dial_mode = DIAL_MODE_NUMBER;
				memset(bnetz->dial_number, 0, sizeof(bnetz->dial_number));
				bnetz->dial_pos = 0;
			}
			break;
		case DIAL_MODE_NUMBER:
			if (digit == 'e') {
				PDEBUG(DBNETZ, DEBUG_INFO, "Received number from mobile phone: %s\n", bnetz->dial_number);
				bnetz->dial_mode = DIAL_MODE_START2;
				PDEBUG(DBNETZ, DEBUG_INFO, "Sending station id back to phone: %s.\n", bnetz->station_id);
				bnetz_set_dsp_mode(bnetz, DSP_MODE_TELEGRAMM);
				bnetz->station_id_pos = 0;
				break;
			}
			if (digit < '0' || digit > '9') {
				PDEBUG(DBNETZ, DEBUG_NOTICE, "Received message that is not a valid number digit, aborting.\n");
				bnetz_go_idle(bnetz);
				return;
			}
			if (bnetz->dial_pos == sizeof(bnetz->dial_number) - 1) {
				PDEBUG(DBNETZ, DEBUG_NOTICE, "Received too many number digits, aborting.\n");
				bnetz_go_idle(bnetz);
				return;
			}
			bnetz->dial_number[bnetz->dial_pos++] = digit;
			break;
		case DIAL_MODE_START2:
			if (digit != 's' && digit != 'S') {
				PDEBUG(DBNETZ, DEBUG_NOTICE, "Received message that is not a start message('Funkwahl'), aborting.\n");
				bnetz_go_idle(bnetz);
				return;
			}
			if ((digit == 'S' && bnetz->dial_metering != 1) || (digit == 's' && bnetz->dial_metering != 0)) {
				PDEBUG(DBNETZ, DEBUG_NOTICE, "Second received start message('Funkwahl') does not match first one, aborting.\n");
				bnetz_go_idle(bnetz);
				return;
			}
			bnetz->dial_mode = DIAL_MODE_STATIONID2;
			bnetz->dial_pos = 0;
			break;
		case DIAL_MODE_STATIONID2:
			if (digit < '0' || digit > '9') {
				PDEBUG(DBNETZ, DEBUG_NOTICE, "Received message that is not a valid station id digit, aborting.\n");
				bnetz_go_idle(bnetz);
				return;
			}
			if (bnetz->station_id[bnetz->dial_pos++] != digit) {
				PDEBUG(DBNETZ, DEBUG_NOTICE, "Second received station id does not match first one, aborting.\n");
				bnetz_go_idle(bnetz);
				return;
			}
			if (bnetz->dial_pos == 5) {
				bnetz->dial_mode = DIAL_MODE_NUMBER2;
				bnetz->dial_pos = 0;
			}
			break;
		case DIAL_MODE_NUMBER2:
			if (digit == 'e') {
				int callref = ++new_callref;
				int rc;
				/* add 0 in front of number */
				char dialing[sizeof(bnetz->dial_number) + 1] = "0";
				strcpy(dialing + 1, bnetz->dial_number);

				if (bnetz->dial_pos != strlen(bnetz->dial_number)) {
					PDEBUG(DBNETZ, DEBUG_NOTICE, "Received too few number digits the second time, aborting.\n");
					bnetz_go_idle(bnetz);
					return;
				}
				PDEBUG(DBNETZ, DEBUG_INFO, "Dialing complete %s->%s, call established.\n", bnetz->station_id, dialing);
				timer_stop(&bnetz->timer);
				bnetz_set_dsp_mode(bnetz, DSP_MODE_AUDIO);
				bnetz->state = BNETZ_GESPRAECH;

				/* setup call */
				PDEBUG(DBNETZ, DEBUG_INFO, "Setup call to network.\n");
				rc = call_in_setup(callref, bnetz->station_id, dialing);
				if (rc < 0) {
					PDEBUG(DBNETZ, DEBUG_NOTICE, "Call rejected (cause %d), releasing.\n", -rc);
					bnetz_release(bnetz);
					return;
				}
				bnetz->sender.callref = callref;
				break;
			}
			if (digit < '0' || digit > '9') {
				PDEBUG(DBNETZ, DEBUG_NOTICE, "Received message that is not a valid number digit, aborting.\n");
				bnetz_go_idle(bnetz);
				return;
			}
			if (bnetz->dial_pos == strlen(bnetz->dial_number)) {
				PDEBUG(DBNETZ, DEBUG_NOTICE, "Received too many number digits, aborting.\n");
				bnetz_go_idle(bnetz);
				return;
			}
			if (bnetz->dial_number[bnetz->dial_pos++] != digit) {
				PDEBUG(DBNETZ, DEBUG_NOTICE, "Second received number does not match first one, aborting.\n");
				bnetz_go_idle(bnetz);
				return;
			}
		}
		break;
	case BNETZ_GESPRAECH:
		/* only good telegramms shall pass */
		if (quality < 0.7)
			return;
		if (digit == 't') {
			PDEBUG(DBNETZ, DEBUG_NOTICE, "Received 'Schlusssignal' from mobile station\n");
			bnetz_go_idle(bnetz);
			call_in_release(bnetz->sender.callref, CAUSE_NORMAL);
			bnetz->sender.callref = 0;
			break;
		}
		break;
	default:
		break;
	}
}

/* Timeout handling */
static void bnetz_timeout(struct timer *timer)
{
	bnetz_t *bnetz = (bnetz_t *)timer->priv;

	switch (bnetz->state) {
	case BNETZ_WAHLABRUF:
		PDEBUG(DBNETZ, DEBUG_NOTICE, "Timeout while receiving call setup from mobile station, aborting.\n");
		bnetz_go_idle(bnetz);
		break;
	case BNETZ_SELEKTIVRUF_EIN:
		PDEBUG(DBNETZ, DEBUG_DEBUG, "Transmitter switched to channel 19, starting paging telegramms.\n");
		bnetz_set_dsp_mode(bnetz, DSP_MODE_TELEGRAMM);
		break;
	case BNETZ_SELEKTIVRUF_AUS:
		PDEBUG(DBNETZ, DEBUG_DEBUG, "Transmitter switched back to channel %d, waiting for paging response.\n", bnetz->sender.kanal);
		bnetz->state = BNETZ_RUFBESTAETIGUNG;
		switch_channel_19(bnetz, 0);
		timer_start(&bnetz->timer, PAGING_TO);
		break;
	case BNETZ_RUFBESTAETIGUNG:
		if (bnetz->page_try == PAGE_TRIES) {
			PDEBUG(DBNETZ, DEBUG_NOTICE, "Timeout while waiting for call acknowledge from mobile station, going idle.\n");
			bnetz_go_idle(bnetz);
			call_in_release(bnetz->sender.callref, CAUSE_OUTOFORDER);
			bnetz->sender.callref = 0;
			break;
		}
		PDEBUG(DBNETZ, DEBUG_NOTICE, "Timeout while waiting for call acknowledge from mobile station, trying again.\n");
		bnetz_page(bnetz, bnetz->station_id, bnetz->page_try + 1);
		break;
	case BNETZ_RUFHALTUNG:
		PDEBUG(DBNETZ, DEBUG_NOTICE, "Timeout while waiting for answer of mobile station, releasing.\n");
	 	bnetz_release(bnetz);
		call_in_release(bnetz->sender.callref, CAUSE_NOANSWER);
		bnetz->sender.callref = 0;
		break;
	default:
		break;
	}
}

/* Call control starts call towards mobile station. */
int call_out_setup(int callref, char *dialing)
{
	sender_t *sender;
	bnetz_t *bnetz;
	int i;

	/* 1. check if number is invalid, return INVALNUMBER */
	if (strlen(dialing) == 7 && dialing[0] == '0' && dialing[1] == '5')
		dialing += 2;
	if (strlen(dialing) != 5) {
inval:
		PDEBUG(DBNETZ, DEBUG_NOTICE, "Outgoing call to invalid number '%s', rejecting!\n", dialing);
		return -CAUSE_INVALNUMBER;
	}
	for (i = 0; i < 5; i++) {
		if (dialing[i] < '0' || dialing[i] > '9')
			goto inval;
	}

	/* 2. check if given number is already in a call, return BUSY */
	for (sender = sender_head; sender; sender = sender->next) {
		bnetz = (bnetz_t *) sender;
		if (!strcmp(bnetz->station_id, dialing))
			break;
	}
	if (sender) {
		PDEBUG(DBNETZ, DEBUG_NOTICE, "Outgoing call to busy number, rejecting!\n");
		return -CAUSE_BUSY;
	}

	/* 3. check if all senders are busy, return NOCHANNEL */
	for (sender = sender_head; sender; sender = sender->next) {
		bnetz = (bnetz_t *) sender;
		if (bnetz->state == BNETZ_FREI)
			break;
	}
	if (!sender) {
		PDEBUG(DBNETZ, DEBUG_NOTICE, "Outgoing call, but no free channel, rejecting!\n");
		return -CAUSE_NOCHANNEL;
	}

	PDEBUG(DBNETZ, DEBUG_INFO, "Call to mobile station, paging station id '%s'\n", dialing);

	/* 4. trying to page mobile station */
	sender->callref = callref;
	bnetz_page(bnetz, dialing, 1);

	return 0;
}

/* Call control sends disconnect (with tones).
 * An active call stays active, so tones and annoucements can be received
 * by mobile station.
 */
void call_out_disconnect(int callref, int cause)
{
	sender_t *sender;
	bnetz_t *bnetz;

	PDEBUG(DBNETZ, DEBUG_INFO, "Call has been disconnected by network.\n");

	for (sender = sender_head; sender; sender = sender->next) {
		bnetz = (bnetz_t *) sender;
		if (sender->callref == callref)
			break;
	}
	if (!sender) {
		PDEBUG(DBNETZ, DEBUG_NOTICE, "Outgoing disconnect, but no callref!\n");
		call_in_release(callref, CAUSE_INVALCALLREF);
		return;
	}

	/* Release when not active */
	if (bnetz->state == BNETZ_GESPRAECH)
		return;
	switch (bnetz->state) {
	case BNETZ_SELEKTIVRUF_EIN:
	case BNETZ_SELEKTIVRUF_AUS:
	case BNETZ_RUFBESTAETIGUNG:
		PDEBUG(DBNETZ, DEBUG_NOTICE, "Outgoing disconnect, during paging, releasing!\n");
	 	bnetz_release(bnetz);
		break;
	case BNETZ_RUFHALTUNG:
		PDEBUG(DBNETZ, DEBUG_NOTICE, "Outgoing disconnect, during alerting, releasing!\n");
	 	bnetz_release(bnetz);
		break;
	default:
		break;
	}

	call_in_release(callref, cause);

	sender->callref = 0;
}

/* Call control releases call toward mobile station. */
void call_out_release(int callref, int cause)
{
	sender_t *sender;
	bnetz_t *bnetz;

	PDEBUG(DBNETZ, DEBUG_INFO, "Call has been released by network, releasing call.\n");

	for (sender = sender_head; sender; sender = sender->next) {
		bnetz = (bnetz_t *) sender;
		if (sender->callref == callref)
			break;
	}
	if (!sender) {
		PDEBUG(DBNETZ, DEBUG_NOTICE, "Outgoing release, but no callref!\n");
		/* don't send release, because caller already released */
		return;
	}

	sender->callref = 0;

	switch (bnetz->state) {
	case BNETZ_GESPRAECH:
		PDEBUG(DBNETZ, DEBUG_NOTICE, "Outgoing release, during call, releasing!\n");
	 	bnetz_release(bnetz);
		break;
	case BNETZ_SELEKTIVRUF_EIN:
	case BNETZ_SELEKTIVRUF_AUS:
	case BNETZ_RUFBESTAETIGUNG:
		PDEBUG(DBNETZ, DEBUG_NOTICE, "Outgoing release, during paging, releasing!\n");
	 	bnetz_release(bnetz);
		break;
	case BNETZ_RUFHALTUNG:
		PDEBUG(DBNETZ, DEBUG_NOTICE, "Outgoing release, during alerting, releasing!\n");
	 	bnetz_release(bnetz);
		break;
	default:
		break;
	}
}

/* Receive audio from call instance. */
void call_rx_audio(int callref, int16_t *samples, int count)
{
	sender_t *sender;
	bnetz_t *bnetz;

	for (sender = sender_head; sender; sender = sender->next) {
		bnetz = (bnetz_t *) sender;
		if (sender->callref == callref)
			break;
	}
	if (!sender)
		return;

	if (bnetz->dsp_mode == DSP_MODE_AUDIO) {
		int16_t up[(int)((double)count * bnetz->sender.srstate.factor + 0.5) + 10];
		count = samplerate_upsample(&bnetz->sender.srstate, samples, count, up);
		jitter_save(&bnetz->sender.audio, up, count);
	}
}

