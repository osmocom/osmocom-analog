/* A-Netz protocol handling
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

#define CHAN anetz->sender.kanal

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../libsample/sample.h"
#include "../liblogging/logging.h"
#include <osmocom/core/timer.h>
#include "../libmobile/call.h"
#include "../libmobile/cause.h"
#include <osmocom/cc/message.h>
#include "anetz.h"
#include "dsp.h"

/* Timers */
#define PAGING_TO	30,0	/* Nach dieser Zeit ist der Operator genervt... */
#define RELEASE_TO	3,0	/* Release time, so station keeps blocked for a while */

const char *anetz_state_name(enum anetz_state state)
{
	static char invalid[16];

	switch (state) {
	case ANETZ_NULL:
		return "(NULL)";
	case ANETZ_FREI:
		return "FREI";
	case ANETZ_GESPRAECH:
		return "GESPRAECH";
	case ANETZ_ANRUF:
		return "ANRUF";
	case ANETZ_AUSLOESEN:
		return "AUSLOESEN";
	}

	sprintf(invalid, "invalid(%d)", state);
	return invalid;
}

void anetz_display_status(void)
{
	sender_t *sender;
	anetz_t *anetz;

	display_status_start();
	for (sender = sender_head; sender; sender = sender->next) {
		anetz = (anetz_t *) sender;
		display_status_channel(anetz->sender.kanal, NULL, anetz_state_name(anetz->state));
		if (anetz->station_id[0])
			display_status_subscriber(anetz->station_id, NULL);
	}
	display_status_end();
}

static void anetz_new_state(anetz_t *anetz, enum anetz_state new_state)
{
	if (anetz->state == new_state)
		return;
	LOGP_CHAN(DANETZ, LOGL_DEBUG, "State change: %s -> %s\n", anetz_state_name(anetz->state), anetz_state_name(new_state));
	anetz->state = new_state;
	anetz_display_status();
}

/* Convert channel number to frequency number of base station.
   Set 'unterband' to 1 to get frequency of mobile station. */
double anetz_kanal2freq(int kanal, int unterband)
{
	double freq = 162.050;

	if (unterband == 2)
		return -4.500 * 1e6;

	freq += (kanal - 30) * 0.050;
	if (kanal >= 45)
		freq += 6.800;
	if (unterband)
		freq -= 4.500;

	return freq * 1e6;
}

/* Convert paging frequency number to to frequency. */
static double anetz_dauerruf_frq(int n)
{
	if (n < 1 || n > 30)
		abort();

	return 337.5 + (double)n * 15.0;
}

/* Table with frequency sets to use for paging. */
static struct anetz_dekaden {
	int dekade[4];
} anetz_gruppenkennziffer[10] = {
	{ { 2, 2, 3, 3 } }, /* 0 */
	{ { 1, 1, 2, 2 } }, /* 1 */
	{ { 1, 1, 3, 3 } }, /* 2 */
	{ { 1, 1, 2, 3 } }, /* 3 */
	{ { 1, 2, 2, 3 } }, /* 4 */
	{ { 1, 2, 3, 3 } }, /* 5 */
	{ { 1, 1, 1, 2 } }, /* 6 */
	{ { 1, 1, 1, 3 } }, /* 7 */
	{ { 2, 2, 2, 3 } }, /* 8 */
	{ { 1, 2, 2, 2 } }, /* 9 */
};

/* Takes the 5 digits of a number and returns 4 paging tones.
   If number is invalid, NULL is returned.  */
static char anetz_nummer2freq_error[256];
static double *anetz_nummer2freq(const char *nummer)
{
	int f[4];
	static double freq[4];
	int *dekade;
	int i, digit;

	/* skip prefix */
	if (strlen(nummer) == 7)
		nummer += 2;

	/* get decade */
	dekade =  anetz_gruppenkennziffer[*nummer - '0'].dekade;
	LOGP(DANETZ, LOGL_DEBUG, "Dekaden: %d %d %d %d\n", dekade[0], dekade[1], dekade[2], dekade[3]);
	nummer++;

	/* get 4 frequencies out of decades */
	for (i = 0; i < 4; i++) {
		digit = nummer[i] - '0';
		if (digit == 0)
			digit = 10;
		f[i] = (dekade[i] - 1) * 10 + digit;
		freq[i] = anetz_dauerruf_frq(f[i]);
	}

	/* check if any frequency is used twice */
	for (i = 0; i < 3; i++) {
		if (dekade[i] == dekade[i + 1] && nummer[i] == nummer[i + 1]) {
			sprintf(anetz_nummer2freq_error, "Digit #%d and #%d of '%s' use same frequency F%d=%.1f of same decade %d.", i + 1, i + 2, nummer, f[i], freq[i], dekade[i]);
			return NULL;
		}
	}

	LOGP(DANETZ, LOGL_DEBUG, "Frequencies: F%d=%.1f F%d=%.1f F%d=%.1f F%d=%.1f\n", f[0], freq[0], f[1], freq[1], f[2], freq[2], f[3], freq[3]);

	return freq;
}

/* check if number is a valid station ID */
const char *anetz_number_valid(const char *number)
{
	double *freq;

	/* assume that the number has valid length(s) and digits */

	freq = anetz_nummer2freq(number);
	if (!freq)
		return anetz_nummer2freq_error;

	return NULL;
}

/* global init */
int anetz_init(void)
{
	return 0;
}

static void anetz_timeout(void *data);
static void anetz_go_idle(anetz_t *anetz);

/* Create transceiver instance and link to a list. */
int anetz_create(const char *kanal, const char *device, int use_sdr, int samplerate, double rx_gain, double tx_gain, double page_gain, int page_sequence, int pre_emphasis, int de_emphasis, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, const char *read_tx_wave, int loopback, double squelch_db, const char *operator)
{
	anetz_t *anetz;
	int rc;

	if (atoi(kanal) < 30 || atoi(kanal) > 63) {
		LOGP(DANETZ, LOGL_ERROR, "Channel ('Kanal') number %s invalid.\n", kanal);
		return -EINVAL;
	}

	anetz = calloc(1, sizeof(anetz_t));
	if (!anetz) {
		LOGP(DANETZ, LOGL_ERROR, "No memory!\n");
		return -EIO;
	}

	anetz->operator = operator;

	LOGP(DANETZ, LOGL_DEBUG, "Creating 'A-Netz' instance for 'Kanal' = %s (sample rate %d).\n", kanal, samplerate);

	/* init general part of transceiver */
	rc = sender_create(&anetz->sender, kanal, anetz_kanal2freq(atoi(kanal), 0), anetz_kanal2freq(atoi(kanal), 1), device, use_sdr, samplerate, rx_gain, tx_gain, pre_emphasis, de_emphasis, write_rx_wave, write_tx_wave, read_rx_wave, read_tx_wave, loopback, PAGING_SIGNAL_NONE);
	if (rc < 0) {
		LOGP(DANETZ, LOGL_ERROR, "Failed to init 'Sender' processing!\n");
		goto error;
	}

	/* init audio processing */
	rc = dsp_init_sender(anetz, page_gain, page_sequence, squelch_db);
	if (rc < 0) {
		LOGP(DANETZ, LOGL_ERROR, "Failed to init signal processing!\n");
		goto error;
	}

	osmo_timer_setup(&anetz->timer, anetz_timeout, anetz);

	/* go into idle state */
	anetz_go_idle(anetz);

	LOGP(DANETZ, LOGL_NOTICE, "Created 'Kanal' #%s\n", kanal);

	return 0;

error:
	anetz_destroy(&anetz->sender);

	return rc;
}

/* Destroy transceiver instance and unlink from list. */
void anetz_destroy(sender_t *sender)
{
	anetz_t *anetz = (anetz_t *) sender;

	LOGP(DANETZ, LOGL_DEBUG, "Destroying 'A-Netz' instance for 'Kanal' = %s.\n", sender->kanal);

	osmo_timer_del(&anetz->timer);
	dsp_cleanup_sender(anetz);
	sender_destroy(&anetz->sender);
	free(sender);
}

/* Abort connection towards mobile station by sending idle tone. */
static void anetz_go_idle(anetz_t *anetz)
{
	osmo_timer_del(&anetz->timer);

	LOGP(DANETZ, LOGL_INFO, "Entering IDLE state on channel %s, sending 2280 Hz tone.\n", anetz->sender.kanal);
	anetz->station_id[0] = '\0'; /* remove station ID before state change, so status is shown correctly */
	anetz_new_state(anetz, ANETZ_FREI);
	/* also reset detector, so if there is a new call it is answered */
	anetz_set_dsp_mode(anetz, DSP_MODE_TONE, 1);
}

/* Release connection towards mobile station by sending idle tone for a while. */
static void anetz_release(anetz_t *anetz)
{
	osmo_timer_del(&anetz->timer);

	LOGP_CHAN(DANETZ, LOGL_INFO, "Sending 2280 Hz release tone.\n");
	anetz->station_id[0] = '\0'; /* remove station ID before state change, so status is shown correctly */
	anetz_new_state(anetz, ANETZ_AUSLOESEN);
	anetz_set_dsp_mode(anetz, DSP_MODE_TONE, 0);
	osmo_timer_schedule(&anetz->timer, RELEASE_TO);
}

/* Enter paging state and transmit 4 paging tones. */
static void anetz_page(anetz_t *anetz, const char *dial_string, double *freq)
{
	LOGP_CHAN(DANETZ, LOGL_INFO, "Entering paging state, sending 'Selektivruf' to '%s'.\n", dial_string);
	strcpy(anetz->station_id, dial_string); /* set station ID before state change, so status is shown correctly */
	anetz_new_state(anetz, ANETZ_ANRUF);
	anetz_set_dsp_mode(anetz, DSP_MODE_PAGING, 0);
	dsp_set_paging(anetz, freq);
	osmo_timer_schedule(&anetz->timer, PAGING_TO);
}

/* Loss of signal was detected, release active call. */
void anetz_loss_indication(anetz_t *anetz, double loss_time)
{
	if (anetz->state == ANETZ_GESPRAECH) {
		LOGP_CHAN(DANETZ, LOGL_NOTICE, "Detected loss of signal after %.1f seconds, releasing.\n", loss_time);
		anetz_release(anetz);
		call_up_release(anetz->callref, CAUSE_TEMPFAIL);
		anetz->callref = 0;
	}
}

/* A continuous tone was detected or is gone. */
void anetz_receive_tone(anetz_t *anetz, int tone)
{
	if (tone >= 0)
		LOGP_CHAN(DANETZ, LOGL_DEBUG, "Received contiuous %d Hz tone.\n", (tone) ? 1750 : 2280);
	else
		LOGP_CHAN(DANETZ, LOGL_DEBUG, "Continuous tone is gone.\n");

	/* skip any handling in loopback mode */
	if (anetz->sender.loopback)
		return;

	/* skip tone 2280 Hz, because it is not relevant for base station */
	if (tone == 0)
		return;

	switch (anetz->state) {
	case ANETZ_FREI:
		/* initiate call on calling tone */
		if (tone == 1) {
			LOGP_CHAN(DANETZ, LOGL_INFO, "Received 1750 Hz calling signal from mobile station, removing idle signal.\n");
			strcpy(anetz->station_id, "unknown"); /* set station ID before state change, so status is shown correctly */

			anetz_new_state(anetz, ANETZ_GESPRAECH);
			anetz_set_dsp_mode(anetz, DSP_MODE_SILENCE, 0);
			break;
		}
		break;
	case ANETZ_GESPRAECH:
		/* throughconnect speech when calling/answer tone is gone */
		if (tone != 1) {
			if (!anetz->callref) {
				LOGP_CHAN(DANETZ, LOGL_INFO, "1750 Hz signal from mobile station is gone, setup call.\n");
				anetz->callref = call_up_setup(NULL, anetz->operator, OSMO_CC_NETWORK_ANETZ_NONE, "");
			} else {
				LOGP_CHAN(DANETZ, LOGL_INFO, "1750 Hz signal from mobile station is gone, answer call.\n");
				call_up_answer(anetz->callref, anetz->station_id);
			}
			anetz_set_dsp_mode(anetz, DSP_MODE_AUDIO, 0);
		}
		/* release call */
		if (tone == 1) {
			LOGP_CHAN(DANETZ, LOGL_INFO, "Received 1750 Hz release signal from mobile station, sending release tone.\n");
			anetz_release(anetz);
			call_up_release(anetz->callref, CAUSE_NORMAL);
			anetz->callref = 0;
			break;
		}
		break;
	case ANETZ_ANRUF:
		/* answer call on answer tone */
		if (tone == 1) {
			LOGP_CHAN(DANETZ, LOGL_INFO, "Received 1750 Hz answer signal from mobile station, removing paging tones.\n");
			osmo_timer_del(&anetz->timer);
			anetz_new_state(anetz, ANETZ_GESPRAECH);
			anetz_set_dsp_mode(anetz, DSP_MODE_SILENCE, 0);
			break;
		}
	default:
		break;
	}
}

/* Timeout handling */
static void anetz_timeout(void *data)
{
	anetz_t *anetz = data;

	switch (anetz->state) {
	case ANETZ_ANRUF:
		LOGP_CHAN(DANETZ, LOGL_NOTICE, "Timeout while waiting for answer, releasing.\n");
	 	anetz_go_idle(anetz);
		call_up_release(anetz->callref, CAUSE_NOANSWER);
		anetz->callref = 0;
		break;
	case ANETZ_AUSLOESEN:
	 	anetz_go_idle(anetz);
		break;
	default:
		break;
	}
}

/* Call control starts call towards mobile station. */
int call_down_setup(int callref, const char __attribute__((unused)) *caller_id, enum number_type __attribute__((unused)) caller_type, const char *dialing)
{
	sender_t *sender;
	anetz_t *anetz;
	double *freq;

	/* 1. determine paging frequencies */
	freq = anetz_nummer2freq(dialing);
	if (!freq) {
		LOGP(DANETZ, LOGL_NOTICE, "Number invalid: %s\n", anetz_nummer2freq_error);
		LOGP(DANETZ, LOGL_NOTICE, "Outgoing call to invalid number '%s', rejecting!\n", dialing);
		return -CAUSE_INVALNUMBER;
	}

	/* 2. check if given number is already in a call, return BUSY */
	for (sender = sender_head; sender; sender = sender->next) {
		anetz = (anetz_t *) sender;
		if (strlen(anetz->station_id) < 5)
			continue;
		if (!strcmp(anetz->station_id + strlen(anetz->station_id) - 5, dialing + strlen(dialing) - 5))
			break;
	}
	if (sender) {
		LOGP(DANETZ, LOGL_NOTICE, "Outgoing call to busy number, rejecting!\n");
		return -CAUSE_BUSY;
	}

	/* 3. check if all senders are busy, return NOCHANNEL */
	for (sender = sender_head; sender; sender = sender->next) {
		anetz = (anetz_t *) sender;
		if (anetz->state == ANETZ_FREI)
			break;
	}
	if (!sender) {
		LOGP(DANETZ, LOGL_NOTICE, "Outgoing call, but no free channel, rejecting!\n");
		return -CAUSE_NOCHANNEL;
	}

	LOGP_CHAN(DANETZ, LOGL_INFO, "Call to mobile station, paging with tones: %.1f %.1f %.1f %.1f\n", freq[0], freq[1], freq[2], freq[3]);
	if (anetz->page_sequence)
		LOGP(DANETZ, LOGL_NOTICE, "Sending paging tones in sequence.\n");

	/* 4. trying to page mobile station */
	anetz->callref = callref;
	anetz_page(anetz, dialing, freq);

	call_up_alerting(callref);

	return 0;
}

void call_down_answer(int __attribute__((unused)) callref, struct timeval __attribute__((unused)) *tv_meter)
{
}

/* Call control sends disconnect (with tones).
 * An active call stays active, so tones and annoucements can be received
 * by mobile station.
 */
void call_down_disconnect(int callref, int cause)
{
	sender_t *sender;
	anetz_t *anetz;

	LOGP(DANETZ, LOGL_INFO, "Call has been disconnected by network.\n");

	for (sender = sender_head; sender; sender = sender->next) {
		anetz = (anetz_t *) sender;
		if (anetz->callref == callref)
			break;
	}
	if (!sender) {
		LOGP(DANETZ, LOGL_NOTICE, "Outgoing disconnect, but no callref!\n");
		call_up_release(callref, CAUSE_INVALCALLREF);
		return;
	}

	/* Release when not active */
	if (anetz->state == ANETZ_GESPRAECH)
		return;
	switch (anetz->state) {
	case ANETZ_ANRUF:
		LOGP_CHAN(DANETZ, LOGL_NOTICE, "Outgoing disconnect, during alerting, going idle!\n");
	 	anetz_go_idle(anetz);
		break;
	default:
		break;
	}

	call_up_release(callref, cause);

	anetz->callref = 0;

}

/* Call control releases call toward mobile station. */
void call_down_release(int callref, __attribute__((unused)) int cause)
{
	sender_t *sender;
	anetz_t *anetz;

	LOGP(DANETZ, LOGL_INFO, "Call has been released by network, releasing call.\n");

	for (sender = sender_head; sender; sender = sender->next) {
		anetz = (anetz_t *) sender;
		if (anetz->callref == callref)
			break;
	}
	if (!sender) {
		LOGP(DANETZ, LOGL_NOTICE, "Outgoing release, but no callref!\n");
		/* don't send release, because caller already released */
		return;
	}

	anetz->callref = 0;

	switch (anetz->state) {
	case ANETZ_GESPRAECH:
		LOGP_CHAN(DANETZ, LOGL_NOTICE, "Outgoing release, during call, sending release tone!\n");
	 	anetz_release(anetz);
		break;
	case ANETZ_ANRUF:
		LOGP_CHAN(DANETZ, LOGL_NOTICE, "Outgoing release, during alerting, going idle!\n");
	 	anetz_go_idle(anetz);
		break;
	default:
		break;
	}
}

void dump_info(void) {}

