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

#define CHAN bnetz->sender.kanal

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../common/sample.h"
#include "../common/debug.h"
#include "../common/call.h"
#include "../libmncc/cause.h"
#include "bnetz.h"
#include "telegramm.h"
#include "dsp.h"

// Uncomment this to generate a dial sequnce (for testing)
//#define GEN_DIALSEQUENCE

/* Call reference for calls from mobile station to network
   This offset of 0x400000000 is required for MNCC interface. */
static int new_callref = 0x40000000;

/* mobile originating call */
#define CARRIER_TO		0.08	/* 80 ms search for carrier */
#define DIALING_TO		3.8	/* timeout after channel allocation "Kanalbelegung" */
#define DIALING_TO2		0.5	/* timeout while receiving digits */

/* mobile terminating call */
#define ALERTING_TO		60	/* timeout after 60 seconds alerting the MS */
#define PAGING_TO		2.1	/* 700..2100 ms timeout after paging "Selektivruf" */
#define PAGE_TRIES		2	/* two tries */
#define SWITCH19_TIME		1.0	/* time to switch channel (radio should be tansmitting after that) */
#define SWITCHBACK_TIME		0.1	/* time to wait until switching back (latency of sound device shall be lower) */

#define TRENN_COUNT		4	/* min. 350 ms disconnect "Trennsignal" */
#define TRENN_COUNT_NA		96	/* 12 s disconnect "Trennsignal" if no answer */

#define METERING_DURATION	0.140	/* duration of metering pulse (according to FTZ 1727 Pfl 32 Clause 3.2.6.6.1) */
#define METERING_START		1.0	/* start metering 1 second after call start */

const char *bnetz_state_name(enum bnetz_state state)
{
	static char invalid[16];

	switch (state) {
	case BNETZ_NULL:
		return "(NULL)";
	case BNETZ_FREI:
		return "FREI";
	case BNETZ_WAHLABRUF:
		return "WAHLABRUF";
	case BNETZ_SELEKTIVRUF_EIN:
		return "SELEKTIVRUF_EIN";
	case BNETZ_SELEKTIVRUF_AUS:
		return "SELEKTIVRUF_AUS";
	case BNETZ_RUFBESTAETIGUNG:
		return "RUFBESTAETIGUNG";
	case BNETZ_RUFHALTUNG:
		return "RUFHALTUNG";
	case BNETZ_GESPRAECH:
		return "GESPRAECH";
	case BNETZ_TRENNEN:
		return "TRENNEN";
	}

	sprintf(invalid, "invalid(%d)", state);
	return invalid;
}

void bnetz_display_status(void)
{
	sender_t *sender;
	bnetz_t *bnetz;

	display_status_start();
	for (sender = sender_head; sender; sender = sender->next) {
		bnetz = (bnetz_t *) sender;
		display_status_channel(bnetz->sender.kanal, NULL, bnetz_state_name(bnetz->state));
		if (bnetz->station_id[0])
			display_status_subscriber(bnetz->station_id, NULL);
	}
	display_status_end();
}


static void bnetz_new_state(bnetz_t *bnetz, enum bnetz_state new_state)
{
	if (bnetz->state == new_state)
		return;
	PDEBUG_CHAN(DBNETZ, DEBUG_DEBUG, "State change: %s -> %s\n", bnetz_state_name(bnetz->state), bnetz_state_name(new_state));
	bnetz->state = new_state;
	bnetz_display_status();
}

/* Convert channel number to frequency number of base station.
   Set 'unterband' to 1 to get frequency of mobile station. */
double bnetz_kanal2freq(int kanal, int unterband)
{
	double freq = 153.010;

	if (unterband == 2)
		return -4.600 * 1e6;

	if (kanal >= 50)
		freq += 9.200 - 0.020 * 49;
	freq += (kanal - 1) * 0.020;
	if (unterband)
		freq -= 4.600;

	return freq * 1e6;
}

/* switch paging signal (tone or file) */
static void switch_channel_19(bnetz_t *bnetz, int on)
{
	/* affects only if paging signal is used */
	sender_paging(&bnetz->sender, on);

	if (bnetz->paging_file[0] && bnetz->paging_is_on != on) {
		FILE *fp;

		fp = fopen(bnetz->paging_file, "w");
		if (!fp) {
			PDEBUG(DBNETZ, DEBUG_ERROR, "Failed to open file '%s' to switch channel 19!\n", bnetz->paging_file);
			return;
		}
		fprintf(fp, "%s\n", (on) ? bnetz->paging_on : bnetz->paging_off);
		fclose(fp);
		bnetz->paging_is_on = on;
	}
}

/* global init */
int bnetz_init(void)
{
	bnetz_init_telegramm();

	return 0;
}

static void bnetz_timeout(struct timer *timer);
static void bnetz_go_idle(bnetz_t *bnetz);

/* Create transceiver instance and link to a list. */
int bnetz_create(int kanal, const char *audiodev, int use_sdr, int samplerate, double rx_gain, int gfs, int pre_emphasis, int de_emphasis, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, const char *read_tx_wave, int loopback, double squelch_db, const char *paging, int metering)
{
	bnetz_t *bnetz;
	enum paging_signal paging_signal = PAGING_SIGNAL_NONE;
	char paging_file[256] = "", paging_on[256] = "", paging_off[256] = "";
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
	
	if (!strcmp(paging, "notone"))
		paging_signal = PAGING_SIGNAL_NOTONE;
	else
	if (!strcmp(paging, "tone"))
		paging_signal = PAGING_SIGNAL_TONE;
	else
	if (!strcmp(paging, "positive"))
		paging_signal = PAGING_SIGNAL_POSITIVE;
	else
	if (!strcmp(paging, "negative"))
		paging_signal = PAGING_SIGNAL_NEGATIVE;
	else {
		char *p;

		strncpy(paging_file, paging, sizeof(paging_file) - 1);
		p = strchr(paging_file, '=');
		if (!p) {
error_paging:
			PDEBUG(DBNETZ, DEBUG_ERROR, "Given paging file (to switch to channel 19) is missing parameters. Use <file>=<on>:<off> format!\n");
			return -EINVAL;
		}
		*p++ = '\0';
		strncpy(paging_on, p, sizeof(paging_on) - 1);
		p = strchr(paging_on, ':');
		if (!p)
			goto error_paging;
		*p++ = '\0';
		strncpy(paging_off, p, sizeof(paging_off) - 1);
	}

	bnetz = calloc(1, sizeof(bnetz_t));
	if (!bnetz) {
		PDEBUG(DBNETZ, DEBUG_ERROR, "No memory!\n");
		return -ENOMEM;
	}

	PDEBUG(DBNETZ, DEBUG_DEBUG, "Creating 'B-Netz' instance for 'Kanal' = %d 'Gruppenfreisignal' = %d (sample rate %d).\n", kanal, gfs, samplerate);

	/* init general part of transceiver */
	rc = sender_create(&bnetz->sender, kanal, bnetz_kanal2freq(kanal, 0), bnetz_kanal2freq(kanal, 1), audiodev, use_sdr, samplerate, rx_gain, pre_emphasis, de_emphasis, write_rx_wave, write_tx_wave, read_rx_wave, read_tx_wave, loopback, paging_signal);
	if (rc < 0) {
		PDEBUG(DBNETZ, DEBUG_ERROR, "Failed to init transceiver process!\n");
		goto error;
	}
	bnetz->sender.ruffrequenz = bnetz_kanal2freq(19, 0);

	/* init audio processing */
	rc = dsp_init_sender(bnetz, squelch_db);
	if (rc < 0) {
		PDEBUG(DBNETZ, DEBUG_ERROR, "Failed to init audio processing!\n");
		goto error;
	}

	bnetz->gfs = gfs;
	bnetz->metering = metering;
	strncpy(bnetz->paging_file, paging_file, sizeof(bnetz->paging_file) - 1);
	strncpy(bnetz->paging_on, paging_on, sizeof(bnetz->paging_on) - 1);
	strncpy(bnetz->paging_off, paging_off, sizeof(bnetz->paging_off) - 1);
	timer_init(&bnetz->timer, bnetz_timeout, bnetz);

	/* go into idle state */
	bnetz_go_idle(bnetz);

	PDEBUG(DBNETZ, DEBUG_NOTICE, "Created 'Kanal' #%d\n", kanal);
	PDEBUG(DBNETZ, DEBUG_NOTICE, " -> Using station ID (Gruppenfreisignal) %d\n", gfs);

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

	PDEBUG(DBNETZ, DEBUG_INFO, "Entering IDLE state on channel %d, sending 'Gruppenfreisignal' %d.\n", bnetz->sender.kanal, bnetz->gfs);
	bnetz->station_id[0] = '\0'; /* remove station ID before state change, so status is shown correctly */
	bnetz_new_state(bnetz, BNETZ_FREI);
	bnetz_set_dsp_mode(bnetz, DSP_MODE_TELEGRAMM);
	switch_channel_19(bnetz, 0);
#ifdef GEN_DIALSEQUENCE
	if (bnetz->sender.loopback) {
		bnetz_set_dsp_mode(bnetz, DSP_MODE_0);
		timer_start(&bnetz->timer, 0.6);
	}
#endif
}

/* Release connection towards mobile station by sending release digits. */
static void bnetz_release(bnetz_t *bnetz, int trenn_count)
{
	timer_stop(&bnetz->timer);

	PDEBUG_CHAN(DBNETZ, DEBUG_INFO, "Entering release state, sending 'Trennsignal' (%d times).\n", trenn_count);
	bnetz->station_id[0] = '\0'; /* remove station ID before state change, so status is shown correctly */
	bnetz_new_state(bnetz, BNETZ_TRENNEN);
	bnetz_set_dsp_mode(bnetz, DSP_MODE_TELEGRAMM);
	switch_channel_19(bnetz, 0);
	bnetz->trenn_count = trenn_count;
}

/* Enter paging state and transmit station ID. */
static void bnetz_page(bnetz_t *bnetz, const char *dial_string, int try)
{
	PDEBUG_CHAN(DBNETZ, DEBUG_INFO, "Entering paging state (try %d), sending 'Selektivruf' to '%s'.\n", try, dial_string);
	strcpy(bnetz->station_id, dial_string); /* set station ID before state change, so status is shown correctly */
	bnetz->station_id_pos = 0;
	bnetz_new_state(bnetz, BNETZ_SELEKTIVRUF_EIN);
	bnetz_set_dsp_mode(bnetz, DSP_MODE_0);
	bnetz->page_mode = PAGE_MODE_NUMBER;
	bnetz->page_try = try;
	timer_start(&bnetz->timer, SWITCH19_TIME);
	switch_channel_19(bnetz, 1);
}

/* FSK processing requests next digit after transmission of previous
   digit has been finished. */
const char *bnetz_get_telegramm(bnetz_t *bnetz)
{
	struct impulstelegramm *it = NULL;

	if (bnetz->sender.loopback) {
#ifdef GEN_DIALSEQUENCE
		char *dialstring="s50993310es50993310e", c;
		c = dialstring[bnetz->loopback_count++];
		if (c) {
			it = bnetz_telegramm(c);
		} else {
			bnetz_set_dsp_mode(bnetz, DSP_MODE_SILENCE);
			return NULL;
		}
#else
		bnetz->loopback_time[bnetz->loopback_count] = get_time();
		it = bnetz_digit2telegramm(bnetz->loopback_count + '0');
		if (++bnetz->loopback_count > 9)
			bnetz->loopback_count = 0;
#endif
	} else
	switch(bnetz->state) {
	case BNETZ_FREI:
		it = bnetz_digit2telegramm(2000 + bnetz->gfs);
		break;
	case BNETZ_WAHLABRUF:
		if (bnetz->station_id_pos == 5) {
			bnetz_set_dsp_mode(bnetz, DSP_MODE_1);
			return NULL;
		}
		it = bnetz_digit2telegramm(bnetz->station_id[bnetz->station_id_pos++]);
		break;
	case BNETZ_SELEKTIVRUF_EIN:
		if (bnetz->page_mode == PAGE_MODE_KANALBEFEHL) {
			PDEBUG_CHAN(DBNETZ, DEBUG_INFO, "Paging mobile station %s complete, waiting for answer.\n", bnetz->station_id);
			bnetz_new_state(bnetz, BNETZ_SELEKTIVRUF_AUS);
			bnetz_set_dsp_mode(bnetz, DSP_MODE_SILENCE);
			timer_start(&bnetz->timer, SWITCHBACK_TIME);
			return NULL;
		}
		if (bnetz->station_id_pos == 5) {
			it = bnetz_digit2telegramm(bnetz->sender.kanal + 1000);
			bnetz->page_mode = PAGE_MODE_KANALBEFEHL;
			break;
		}
		it = bnetz_digit2telegramm(bnetz->station_id[bnetz->station_id_pos++]);
		break;
	case BNETZ_TRENNEN:
		if (bnetz->trenn_count-- == 0) {
			PDEBUG_CHAN(DBNETZ, DEBUG_DEBUG, "Maximum number of release digits sent, going idle.\n");
			bnetz_go_idle(bnetz);
			return NULL;
		}
		it = bnetz_digit2telegramm('t');
		break;
	default:
		break;
	}

	if (!it)
		abort();

	PDEBUG_CHAN(DBNETZ, DEBUG_DEBUG, "Sending telegramm '%s'.\n", it->description);
	return it->sequence;
}

/* Loss of signal was detected, release active call. */
void bnetz_loss_indication(bnetz_t *bnetz, double loss_time)
{
	if (bnetz->state == BNETZ_GESPRAECH
	 || bnetz->state == BNETZ_RUFHALTUNG) {
		PDEBUG_CHAN(DBNETZ, DEBUG_NOTICE, "Detected loss of signal after %.1f seconds, releasing.\n", loss_time);
		bnetz_release(bnetz, TRENN_COUNT);
		call_up_release(bnetz->callref, CAUSE_TEMPFAIL);
		bnetz->callref = 0;
	}
}

/* A continuous tone was detected or is gone. */
void bnetz_receive_tone(bnetz_t *bnetz, int bit)
{
	if (bit >= 0)
		PDEBUG_CHAN(DBNETZ, DEBUG_DEBUG, "Received contiuous %d Hz tone.\n", (bit)?1950:2070);
	else
		PDEBUG_CHAN(DBNETZ, DEBUG_DEBUG, "Continuous tone is gone.\n");

	if (bnetz->sender.loopback) {
		return;
	}

	switch (bnetz->state) {
	case BNETZ_FREI:
		if (bit == 0) {
			PDEBUG_CHAN(DBNETZ, DEBUG_INFO, "Received signal 'Kanalbelegung' from mobile station, sending signal 'Wahlabruf'.\n");
			bnetz_new_state(bnetz, BNETZ_WAHLABRUF);
			bnetz->dial_mode = DIAL_MODE_START;
			bnetz_set_dsp_mode(bnetz, DSP_MODE_1);
			timer_start(&bnetz->timer, DIALING_TO);
			break;
		}
		break;
	case BNETZ_RUFBESTAETIGUNG:
		if (bit == 1) {
			PDEBUG_CHAN(DBNETZ, DEBUG_INFO, "Received signal 'Rufbestaetigung' from mobile station, sending signal 'Rufhaltung'. (call is ringing)\n");
			timer_stop(&bnetz->timer);
			bnetz_new_state(bnetz, BNETZ_RUFHALTUNG);
			bnetz_set_dsp_mode(bnetz, DSP_MODE_1);
			call_up_alerting(bnetz->callref);
			timer_start(&bnetz->timer, ALERTING_TO);
			break;
		}
		break;
	case BNETZ_RUFHALTUNG:
		if (bit == 0) {
			PDEBUG_CHAN(DBNETZ, DEBUG_INFO, "Received signal 'Beginnsignal' from mobile station, call establised.\n");
			timer_stop(&bnetz->timer);
			bnetz_new_state(bnetz, BNETZ_GESPRAECH);
			bnetz_set_dsp_mode(bnetz, DSP_MODE_AUDIO);
			/* start metering pulses if forced */
			if (bnetz->metering < 0)
				timer_start(&bnetz->timer, METERING_START);
			call_up_answer(bnetz->callref, bnetz->station_id);
			break;
		}
	default:
		break;
	}
}

/* A digit was received. */
void bnetz_receive_telegramm(bnetz_t *bnetz, uint16_t telegramm, double level_avg, double level_stddev, double quality_avg)
{
	struct impulstelegramm *it;
	int digit = 0;

	/* drop any telegramm that is too bad */
	if (level_stddev / level_avg > 0.2)
		return;

	PDEBUG_CHAN(DDSP, DEBUG_INFO, "RX Level: average=%.0f%% standard deviation=%.0f%% Quality: %.0f%%\n", level_avg * 100.0, level_stddev / level_avg * 100.0, quality_avg * 100.0);

	it = bnetz_telegramm2digit(telegramm);
	if (it) {
		digit = it->digit;
		PDEBUG(DBNETZ, (bnetz->sender.loopback) ? DEBUG_NOTICE : DEBUG_INFO, "Received telegramm '%s'\n", it->description);
	} else {
		PDEBUG(DBNETZ, DEBUG_DEBUG, "Received unknown telegramm digit '0x%04x' (might be radio noise)\n", telegramm);
		return;
	}

	if (bnetz->sender.loopback) {
		if (digit >= '0' && digit <= '9') {
			PDEBUG(DBNETZ, DEBUG_NOTICE, "Round trip delay is %.3f seconds\n", get_time() - bnetz->loopback_time[digit - '0'] - 0.160);
		}
		return;
	}

	switch (bnetz->state) {
	case BNETZ_WAHLABRUF:
		timer_start(&bnetz->timer, DIALING_TO2);
		switch (bnetz->dial_mode) {
		case DIAL_MODE_START:
			switch (digit) {
			case 's':
				bnetz->dial_type = DIAL_TYPE_NOMETER;
				break;
			case 'S':
				bnetz->dial_type = DIAL_TYPE_METER;
				break;
			case 'U':
				bnetz->dial_type = DIAL_TYPE_METER_MUENZ;
				break;
			default:
				PDEBUG(DBNETZ, DEBUG_NOTICE, "Received digit that is not a start digit ('Funkwahl'), aborting.\n");
				bnetz_release(bnetz, TRENN_COUNT);
				return;
			}
			bnetz->dial_mode = DIAL_MODE_STATIONID;
			memset(bnetz->station_id, 0, sizeof(bnetz->station_id));
			bnetz->dial_pos = 0;
			break;
		case DIAL_MODE_STATIONID:
			if (digit < '0' || digit > '9') {
				PDEBUG(DBNETZ, DEBUG_NOTICE, "Received message that is not a valid station id digit, aborting.\n");
				bnetz_release(bnetz, TRENN_COUNT);
				return;
			}
			bnetz->station_id[bnetz->dial_pos++] = digit;
			/* update status while receiving station ID */
			bnetz_display_status();
			if (bnetz->dial_pos == 5) {
				PDEBUG(DBNETZ, DEBUG_INFO, "Received station id from mobile phone: %s\n", bnetz->station_id);
				bnetz->dial_mode = DIAL_MODE_NUMBER;
				memset(bnetz->dial_number, 0, sizeof(bnetz->dial_number));
				bnetz->dial_pos = 0;
				/* reply station ID */
				PDEBUG(DBNETZ, DEBUG_INFO, "Sending station id back to phone: %s.\n", bnetz->station_id);
				bnetz_set_dsp_mode(bnetz, DSP_MODE_TELEGRAMM);
				bnetz->station_id_pos = 0;
			}
			break;
		case DIAL_MODE_NUMBER:
			if (digit == 'e') {
				PDEBUG(DBNETZ, DEBUG_INFO, "Received number from mobile phone: %s\n", bnetz->dial_number);
				bnetz->dial_mode = DIAL_MODE_START2;
				break;
			}
			if (digit < '0' || digit > '9') {
				PDEBUG(DBNETZ, DEBUG_NOTICE, "Received message that is not a valid number digit, aborting.\n");
				bnetz_release(bnetz, TRENN_COUNT);
				return;
			}
			if (bnetz->dial_pos == sizeof(bnetz->dial_number) - 1) {
				PDEBUG(DBNETZ, DEBUG_NOTICE, "Received too many number digits, aborting.\n");
				bnetz_release(bnetz, TRENN_COUNT);
				return;
			}
			bnetz->dial_number[bnetz->dial_pos++] = digit;
			break;
		case DIAL_MODE_START2:
			switch (digit) {
			case 's':
				if (bnetz->dial_type != DIAL_TYPE_NOMETER) {
					PDEBUG(DBNETZ, DEBUG_NOTICE, "Second received start message('Funkwahl') does not match first one (no metering support), aborting.\n");
					bnetz_release(bnetz, TRENN_COUNT);
					return;
				}
				break;
			case 'S':
				if (bnetz->dial_type != DIAL_TYPE_METER) {
					PDEBUG(DBNETZ, DEBUG_NOTICE, "Second received start message('Funkwahl') does not match first one (metering support), aborting.\n");
					bnetz_release(bnetz, TRENN_COUNT);
					return;
				}
				break;
			case 'U':
				if (bnetz->dial_type != DIAL_TYPE_METER_MUENZ) {
					PDEBUG(DBNETZ, DEBUG_NOTICE, "Second received start message('Funkwahl') does not match first one (metering support, payphone), aborting.\n");
					bnetz_release(bnetz, TRENN_COUNT);
					return;
				}
				break;
			default:
				PDEBUG(DBNETZ, DEBUG_NOTICE, "Received digit that is not a start digit ('Funkwahl'), aborting.\n");
				bnetz_release(bnetz, TRENN_COUNT);
				return;
			}
			bnetz->dial_mode = DIAL_MODE_STATIONID2;
			bnetz->dial_pos = 0;
			break;
		case DIAL_MODE_STATIONID2:
			if (digit < '0' || digit > '9') {
				PDEBUG(DBNETZ, DEBUG_NOTICE, "Received message that is not a valid station id digit, aborting.\n");
				bnetz_release(bnetz, TRENN_COUNT);
				return;
			}
			if (bnetz->station_id[bnetz->dial_pos++] != digit) {
				PDEBUG(DBNETZ, DEBUG_NOTICE, "Second received station id does not match first one, aborting.\n");
				bnetz_release(bnetz, TRENN_COUNT);
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

				if (bnetz->dial_pos != (int)strlen(bnetz->dial_number)) {
					PDEBUG(DBNETZ, DEBUG_NOTICE, "Received too few number digits the second time, aborting.\n");
					bnetz_release(bnetz, TRENN_COUNT);
					return;
				}
				if (!strncmp(dialing, "0110", 4)) {
					PDEBUG(DBNETZ, DEBUG_INFO, "Translating emergency number to '110'.\n");
					strcpy(dialing, "110");
				}
				if (!strncmp(dialing, "0112", 4)) {
					PDEBUG(DBNETZ, DEBUG_INFO, "Translating emergency number to '112'.\n");
					strcpy(dialing, "112");
				}
				PDEBUG(DBNETZ, DEBUG_INFO, "Dialing complete %s->%s, call established.\n", bnetz->station_id, dialing);
				timer_stop(&bnetz->timer);
				bnetz_set_dsp_mode(bnetz, DSP_MODE_AUDIO);
				bnetz_new_state(bnetz, BNETZ_GESPRAECH);
				/* start metering pulses if enabled and supported by phone or if forced */
				if (bnetz->metering < 0 || (bnetz->metering > 0 && (bnetz->dial_type == DIAL_TYPE_METER || bnetz->dial_type == DIAL_TYPE_METER_MUENZ)))
					timer_start(&bnetz->timer, METERING_START);

				/* setup call */
				PDEBUG(DBNETZ, DEBUG_INFO, "Setup call to network.\n");
				rc = call_up_setup(callref, bnetz->station_id, dialing);
				if (rc < 0) {
					PDEBUG(DBNETZ, DEBUG_NOTICE, "Call rejected (cause %d), releasing.\n", -rc);
					bnetz_release(bnetz, TRENN_COUNT);
					return;
				}
				bnetz->callref = callref;
				break;
			}
			if (digit < '0' || digit > '9') {
				PDEBUG(DBNETZ, DEBUG_NOTICE, "Received message that is not a valid number digit, aborting.\n");
				bnetz_release(bnetz, TRENN_COUNT);
				return;
			}
			if (bnetz->dial_pos == (int)strlen(bnetz->dial_number)) {
				PDEBUG(DBNETZ, DEBUG_NOTICE, "Received too many number digits, aborting.\n");
				bnetz_release(bnetz, TRENN_COUNT);
				return;
			}
			if (bnetz->dial_number[bnetz->dial_pos++] != digit) {
				PDEBUG(DBNETZ, DEBUG_NOTICE, "Second received number does not match first one, aborting.\n");
				bnetz_release(bnetz, TRENN_COUNT);
				return;
			}
		}
		break;
	case BNETZ_GESPRAECH:
#if 0
disabled, because any quality shall release the call.
lets see, if noise will not generate a release signal....
		/* only good telegramms shall pass */
		if (quality < 0.7)
			return;
#endif
		if (digit == 't') {
			PDEBUG(DBNETZ, DEBUG_NOTICE, "Received 'Schlusssignal' from mobile station\n");
			bnetz_release(bnetz, TRENN_COUNT);
			call_up_release(bnetz->callref, CAUSE_NORMAL);
			bnetz->callref = 0;
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
#ifdef GEN_DIALSEQUENCE
	case BNETZ_FREI:
		bnetz_set_dsp_mode(bnetz, DSP_MODE_TELEGRAMM);
		break;
#endif
	case BNETZ_WAHLABRUF:
		PDEBUG_CHAN(DBNETZ, DEBUG_NOTICE, "Timeout while receiving call setup from mobile station, aborting.\n");
		bnetz_release(bnetz, TRENN_COUNT);
		break;
	case BNETZ_SELEKTIVRUF_EIN:
		PDEBUG_CHAN(DBNETZ, DEBUG_DEBUG, "Transmitter switched to channel 19, starting paging telegramms.\n");
		bnetz_set_dsp_mode(bnetz, DSP_MODE_TELEGRAMM);
		break;
	case BNETZ_SELEKTIVRUF_AUS:
		PDEBUG_CHAN(DBNETZ, DEBUG_DEBUG, "Transmitter switched back to channel %d, waiting for paging response.\n", bnetz->sender.kanal);
		bnetz_new_state(bnetz, BNETZ_RUFBESTAETIGUNG);
		switch_channel_19(bnetz, 0);
		timer_start(&bnetz->timer, PAGING_TO);
		break;
	case BNETZ_RUFBESTAETIGUNG:
		if (bnetz->page_try == PAGE_TRIES) {
			PDEBUG_CHAN(DBNETZ, DEBUG_NOTICE, "Timeout while waiting for call acknowledge from mobile station, going idle.\n");
			bnetz_release(bnetz, TRENN_COUNT);
			call_up_release(bnetz->callref, CAUSE_OUTOFORDER);
			bnetz->callref = 0;
			break;
		}
		PDEBUG_CHAN(DBNETZ, DEBUG_NOTICE, "Timeout while waiting for call acknowledge from mobile station, trying again.\n");
		bnetz_page(bnetz, bnetz->station_id, bnetz->page_try + 1);
		break;
	case BNETZ_RUFHALTUNG:
		PDEBUG_CHAN(DBNETZ, DEBUG_NOTICE, "Timeout while waiting for answer of mobile station, releasing.\n");
		bnetz_release(bnetz, TRENN_COUNT_NA);
		call_up_release(bnetz->callref, CAUSE_NOANSWER);
		bnetz->callref = 0;
		break;
	case BNETZ_GESPRAECH:
		switch (bnetz->dsp_mode) {
		case DSP_MODE_AUDIO:
			/* turn on merting pulse */
			bnetz_set_dsp_mode(bnetz, DSP_MODE_AUDIO_METER);
			timer_start(&bnetz->timer, METERING_DURATION);
			break;
		case DSP_MODE_AUDIO_METER:
			/* turn off and wait given seconds for next metering cycle */
			bnetz_set_dsp_mode(bnetz, DSP_MODE_AUDIO);
			timer_start(&bnetz->timer, (double)abs(bnetz->metering) - METERING_DURATION);
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
}

/* Call control starts call towards mobile station. */
int call_down_setup(int callref, const char __attribute__((unused)) *caller_id, enum number_type __attribute__((unused)) caller_type, const char *dialing)
{
	sender_t *sender;
	bnetz_t *bnetz;
	int i;

	/* 1. check if number is invalid, return INVALNUMBER */
	if (strlen(dialing) == 7 && dialing[0] == '0' && dialing[1] == '5')
		dialing += 2;
	else if (strlen(dialing) != 5) {
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

	PDEBUG_CHAN(DBNETZ, DEBUG_INFO, "Call to mobile station, paging station id '%s'\n", dialing);

	/* 4. trying to page mobile station */
	bnetz->callref = callref;
	bnetz_page(bnetz, dialing, 1);

	return 0;
}

void call_down_answer(int __attribute__((unused)) callref)
{
}

/* Call control sends disconnect (with tones).
 * An active call stays active, so tones and annoucements can be received
 * by mobile station.
 */
void call_down_disconnect(int callref, int cause)
{
	sender_t *sender;
	bnetz_t *bnetz;

	PDEBUG(DBNETZ, DEBUG_INFO, "Call has been disconnected by network.\n");

	for (sender = sender_head; sender; sender = sender->next) {
		bnetz = (bnetz_t *) sender;
		if (bnetz->callref == callref)
			break;
	}
	if (!sender) {
		PDEBUG(DBNETZ, DEBUG_NOTICE, "Outgoing disconnect, but no callref!\n");
		call_up_release(callref, CAUSE_INVALCALLREF);
		return;
	}

	/* Release when not active */
	if (bnetz->state == BNETZ_GESPRAECH)
		return;
	switch (bnetz->state) {
	case BNETZ_SELEKTIVRUF_EIN:
	case BNETZ_SELEKTIVRUF_AUS:
	case BNETZ_RUFBESTAETIGUNG:
		PDEBUG_CHAN(DBNETZ, DEBUG_NOTICE, "Outgoing disconnect, during paging, releasing!\n");
		bnetz_release(bnetz, TRENN_COUNT);
		break;
	case BNETZ_RUFHALTUNG:
		PDEBUG_CHAN(DBNETZ, DEBUG_NOTICE, "Outgoing disconnect, during alerting, releasing!\n");
		bnetz_release(bnetz, TRENN_COUNT);
		break;
	default:
		break;
	}

	call_up_release(callref, cause);

	bnetz->callref = 0;
}

/* Call control releases call toward mobile station. */
void call_down_release(int callref, int __attribute__((unused)) cause)
{
	sender_t *sender;
	bnetz_t *bnetz;

	PDEBUG(DBNETZ, DEBUG_INFO, "Call has been released by network, releasing call.\n");

	for (sender = sender_head; sender; sender = sender->next) {
		bnetz = (bnetz_t *) sender;
		if (bnetz->callref == callref)
			break;
	}
	if (!sender) {
		PDEBUG(DBNETZ, DEBUG_NOTICE, "Outgoing release, but no callref!\n");
		/* don't send release, because caller already released */
		return;
	}

	bnetz->callref = 0;

	switch (bnetz->state) {
	case BNETZ_GESPRAECH:
		PDEBUG_CHAN(DBNETZ, DEBUG_NOTICE, "Outgoing release, during call, releasing!\n");
		bnetz_release(bnetz, TRENN_COUNT);
		break;
	case BNETZ_SELEKTIVRUF_EIN:
	case BNETZ_SELEKTIVRUF_AUS:
	case BNETZ_RUFBESTAETIGUNG:
		PDEBUG_CHAN(DBNETZ, DEBUG_NOTICE, "Outgoing release, during paging, releasing!\n");
		bnetz_release(bnetz, TRENN_COUNT);
		break;
	case BNETZ_RUFHALTUNG:
		PDEBUG_CHAN(DBNETZ, DEBUG_NOTICE, "Outgoing release, during alerting, releasing!\n");
		bnetz_release(bnetz, TRENN_COUNT);
		break;
	default:
		break;
	}
}

/* Receive audio from call instance. */
void call_down_audio(int callref, sample_t *samples, int count)
{
	sender_t *sender;
	bnetz_t *bnetz;

	for (sender = sender_head; sender; sender = sender->next) {
		bnetz = (bnetz_t *) sender;
		if (bnetz->callref == callref)
			break;
	}
	if (!sender)
		return;

	if (bnetz->dsp_mode == DSP_MODE_AUDIO
	 || bnetz->dsp_mode == DSP_MODE_AUDIO_METER) {
		sample_t up[(int)((double)count * bnetz->sender.srstate.factor + 0.5) + 10];
		count = samplerate_upsample(&bnetz->sender.srstate, samples, count, up);
		jitter_save(&bnetz->sender.dejitter, up, count);
	}
}

void dump_info(void) {}

