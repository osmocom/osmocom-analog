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
#include "../common/debug.h"
#include "../common/timer.h"
#include "../common/call.h"
#include "../common/cause.h"
#include "../common/freiton.h"
#include "../common/besetztton.h"
#include "anetz.h"
#include "dsp.h"

/* Call reference for calls from mobile station to network
   This offset of 0x400000000 is required for MNCC interface. */
static int new_callref = 0x40000000;

/* Timers */
#define PAGING_TO	30	/* Nach dieser Zeit ist der Operator genervt... */
#define RELEASE_TO	3	/* Release time, so station keeps blocked for a while */

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

static void anetz_new_state(anetz_t *anetz, enum anetz_state new_state)
{
	if (anetz->state == new_state)
		return;
	PDEBUG_CHAN(DANETZ, DEBUG_DEBUG, "State change: %s -> %s\n", anetz_state_name(anetz->state), anetz_state_name(new_state));
	anetz->state = new_state;
}

/* Convert channel number to frequency number of base station.
   Set 'unterband' to 1 to get frequency of mobile station. */
double anetz_kanal2freq(int kanal, int unterband)
{
	double freq = 162.050;

	freq += (kanal - 30) * 0.050;
	if (kanal >= 45)
		freq += 6.800;
	if (unterband)
		freq -= 4.500;

	return freq;
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

/* Takes the last 5 digits of a number and returns 4 paging tones.
   If number is invalid, NULL is returned.  */
static double *anetz_nummer2freq(const char *nummer)
{
	int f[4];
	static double freq[4];
	int *dekade;
	int i, j, digit;

	/* get last 5 digits */
	if (strlen(nummer) < 5) {
		PDEBUG(DANETZ, DEBUG_ERROR, "Number must have at least 5 digits!\n");
		return NULL;
	}
	nummer = nummer + strlen(nummer) - 5;

	/* check for digits */
	for (i = 0; i < 4; i++) {
		if (nummer[i] < '0' || nummer[i] > '9') {
			PDEBUG(DANETZ, DEBUG_ERROR, "Number must have digits 0..9!\n");
			return NULL;
		}
	}

	/* get decade */
	dekade =  anetz_gruppenkennziffer[*nummer - '0'].dekade;
	PDEBUG(DANETZ, DEBUG_DEBUG, "Dekaden: %d %d %d %d\n", dekade[0], dekade[1], dekade[2], dekade[3]);
	nummer++;

	/* get 4 frequencies out of decades */
	for (i = 0; i < 4; i++) {
		digit = nummer[i] - '0';
		if (digit == 0)
			digit = 10;
		f[i] = (dekade[i] - 1) * 10 + digit;
		freq[i] = anetz_dauerruf_frq(f[i]);
		for (j = 0; j < i; j++) {
			if (dekade[i] == dekade[j] && nummer[i] == nummer[j]) {
				PDEBUG(DANETZ, DEBUG_NOTICE, "Number invalid, digit #%d and #%d of '%s' use same frequency F%d=%.1f of same decade %d!\n", i+1, j+1, nummer, f[i], freq[i], dekade[i]);
				return NULL;
			}
		}
	}
	PDEBUG(DANETZ, DEBUG_DEBUG, "Frequencies: F%d=%.1f F%d=%.1f F%d=%.1f F%d=%.1f\n", f[0], freq[0], f[1], freq[1], f[2], freq[2], f[3], freq[3]);

	return freq;
}

/* global init */
int anetz_init(void)
{
	return 0;
}

static void anetz_timeout(struct timer *timer);
static void anetz_go_idle(anetz_t *anetz);

/* Create transceiver instance and link to a list. */
int anetz_create(int kanal, const char *sounddev, int samplerate, int cross_channels, double rx_gain, int page_sequence, int pre_emphasis, int de_emphasis, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, int loopback, double loss_volume)
{
	anetz_t *anetz;
	int rc;

	if (kanal < 30 || kanal > 63) {
		PDEBUG(DANETZ, DEBUG_ERROR, "Channel ('Kanal') number %d invalid.\n", kanal);
		return -EINVAL;
	}

	anetz = calloc(1, sizeof(anetz_t));
	if (!anetz) {
		PDEBUG(DANETZ, DEBUG_ERROR, "No memory!\n");
		return -EIO;
	}

	PDEBUG(DANETZ, DEBUG_DEBUG, "Creating 'A-Netz' instance for 'Kanal' = %d (sample rate %d).\n", kanal, samplerate);

	/* init general part of transceiver */
	rc = sender_create(&anetz->sender, kanal, sounddev, samplerate, cross_channels, rx_gain, pre_emphasis, de_emphasis, write_rx_wave, write_tx_wave, read_rx_wave, loopback, loss_volume, PILOT_SIGNAL_NONE);
	if (rc < 0) {
		PDEBUG(DANETZ, DEBUG_ERROR, "Failed to init 'Sender' processing!\n");
		goto error;
	}

	/* init audio processing */
	rc = dsp_init_sender(anetz, page_sequence);
	if (rc < 0) {
		PDEBUG(DANETZ, DEBUG_ERROR, "Failed to init signal processing!\n");
		goto error;
	}

	timer_init(&anetz->timer, anetz_timeout, anetz);

	/* go into idle state */
	anetz_go_idle(anetz);

	return 0;

error:
	anetz_destroy(&anetz->sender);

	return rc;
}

/* Destroy transceiver instance and unlink from list. */
void anetz_destroy(sender_t *sender)
{
	anetz_t *anetz = (anetz_t *) sender;

	PDEBUG(DANETZ, DEBUG_DEBUG, "Destroying 'A-Netz' instance for 'Kanal' = %d.\n", sender->kanal);

	timer_exit(&anetz->timer);
	dsp_cleanup_sender(anetz);
	sender_destroy(&anetz->sender);
	free(sender);
}

/* Abort connection towards mobile station by sending idle tone. */
static void anetz_go_idle(anetz_t *anetz)
{
	timer_stop(&anetz->timer);

	PDEBUG(DANETZ, DEBUG_INFO, "Entering IDLE state on channel %d, sending 2280 Hz tone.\n", anetz->sender.kanal);
	anetz_new_state(anetz, ANETZ_FREI);
	anetz_set_dsp_mode(anetz, DSP_MODE_TONE);
	anetz->station_id[0] = '\0';
}

/* Release connection towards mobile station by sending idle tone for a while. */
static void anetz_release(anetz_t *anetz)
{
	timer_stop(&anetz->timer);

	PDEBUG_CHAN(DANETZ, DEBUG_INFO, "Sending 2280 Hz release tone.\n");
	anetz_new_state(anetz, ANETZ_AUSLOESEN);
	anetz_set_dsp_mode(anetz, DSP_MODE_TONE);
	anetz->station_id[0] = '\0';
	timer_start(&anetz->timer, RELEASE_TO);
}

/* Enter paging state and transmit 4 paging tones. */
static void anetz_page(anetz_t *anetz, const char *dial_string, double *freq)
{
	PDEBUG_CHAN(DANETZ, DEBUG_INFO, "Entering paging state, sending 'Selektivruf' to '%s'.\n", dial_string);
	anetz_new_state(anetz, ANETZ_ANRUF);
	anetz_set_dsp_mode(anetz, DSP_MODE_PAGING);
	dsp_set_paging(anetz, freq);
	strcpy(anetz->station_id, dial_string);
	timer_start(&anetz->timer, PAGING_TO);
}

/* Loss of signal was detected, release active call. */
void anetz_loss_indication(anetz_t *anetz)
{
	if (anetz->state == ANETZ_GESPRAECH) {
		PDEBUG_CHAN(DANETZ, DEBUG_NOTICE, "Detected loss of signal, releasing.\n");
		anetz_release(anetz);
		call_in_release(anetz->callref, CAUSE_TEMPFAIL);
		anetz->callref = 0;
	}
}

/* A continuous tone was detected or is gone. */
void anetz_receive_tone(anetz_t *anetz, int tone)
{
	if (tone >= 0)
		PDEBUG_CHAN(DANETZ, DEBUG_DEBUG, "Received contiuous %d Hz tone.\n", (tone) ? 1750 : 2280);
	else
		PDEBUG_CHAN(DANETZ, DEBUG_DEBUG, "Continuous tone is gone.\n");

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
			PDEBUG_CHAN(DANETZ, DEBUG_INFO, "Received 1750 Hz calling signal from mobile station, removing idle signal.\n");
			anetz_new_state(anetz, ANETZ_GESPRAECH);
			anetz_set_dsp_mode(anetz, DSP_MODE_SILENCE);
			break;
		}
		break;
	case ANETZ_GESPRAECH:
		/* throughconnect speech when calling/answer tone is gone */
		if (tone != 1) {
			if (!anetz->callref) {
				int callref = ++new_callref;
				int rc;

				PDEBUG_CHAN(DANETZ, DEBUG_INFO, "1750 Hz signal from mobile station is gone, setup call.\n");
				rc = call_in_setup(callref, NULL, "010");
				if (rc < 0) {
					PDEBUG_CHAN(DANETZ, DEBUG_NOTICE, "Call rejected (cause %d), sending release tone.\n", -rc);
					anetz_release(anetz);
					break;
				}
				anetz->callref = callref;
			} else {
				PDEBUG_CHAN(DANETZ, DEBUG_INFO, "1750 Hz signal from mobile station is gone, answer call.\n");
				call_in_answer(anetz->callref, anetz->station_id);
			}
			anetz_set_dsp_mode(anetz, DSP_MODE_AUDIO);
		}
		/* release call */
		if (tone == 1) {
			PDEBUG_CHAN(DANETZ, DEBUG_INFO, "Received 1750 Hz release signal from mobile station, sending release tone.\n");
			anetz_release(anetz);
			call_in_release(anetz->callref, CAUSE_NORMAL);
			anetz->callref = 0;
			break;
		}
		break;
	case ANETZ_ANRUF:
		/* answer call on answer tone */
		if (tone == 1) {
			PDEBUG_CHAN(DANETZ, DEBUG_INFO, "Received 1750 Hz answer signal from mobile station, removing paging tones.\n");
			timer_stop(&anetz->timer);
			anetz_new_state(anetz, ANETZ_GESPRAECH);
			anetz_set_dsp_mode(anetz, DSP_MODE_SILENCE);
			break;
		}
	default:
		break;
	}
}

/* Timeout handling */
static void anetz_timeout(struct timer *timer)
{
	anetz_t *anetz = (anetz_t *)timer->priv;

	switch (anetz->state) {
	case ANETZ_ANRUF:
		PDEBUG_CHAN(DANETZ, DEBUG_NOTICE, "Timeout while waiting for answer, releasing.\n");
	 	anetz_go_idle(anetz);
		call_in_release(anetz->callref, CAUSE_NOANSWER);
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
int call_out_setup(int callref, const char __attribute__((unused)) *caller_id, enum number_type __attribute__((unused)) caller_type, const char *dialing)
{
	sender_t *sender;
	anetz_t *anetz;
	double *freq;

	/* 1. check if number is invalid, return INVALNUMBER */
	if (strlen(dialing) > 7) {
inval:
		PDEBUG(DANETZ, DEBUG_NOTICE, "Outgoing call to invalid number '%s', rejecting!\n", dialing);
		return -CAUSE_INVALNUMBER;
	}
	freq = anetz_nummer2freq(dialing);
	if (!freq)
		goto inval;

	/* 2. check if given number is already in a call, return BUSY */
	for (sender = sender_head; sender; sender = sender->next) {
		anetz = (anetz_t *) sender;
		if (strlen(anetz->station_id) < 5)
			continue;
		if (!strcmp(anetz->station_id + strlen(anetz->station_id) - 5, dialing + strlen(dialing) - 5))
			break;
	}
	if (sender) {
		PDEBUG(DANETZ, DEBUG_NOTICE, "Outgoing call to busy number, rejecting!\n");
		return -CAUSE_BUSY;
	}

	/* 3. check if all senders are busy, return NOCHANNEL */
	for (sender = sender_head; sender; sender = sender->next) {
		anetz = (anetz_t *) sender;
		if (anetz->state == ANETZ_FREI)
			break;
	}
	if (!sender) {
		PDEBUG(DANETZ, DEBUG_NOTICE, "Outgoing call, but no free channel, rejecting!\n");
		return -CAUSE_NOCHANNEL;
	}

	PDEBUG(DANETZ, DEBUG_INFO, "Call to mobile station, paging with tones: %.1f %.1f %.1f %.1f\n", freq[0], freq[1], freq[2], freq[3]);
	if (anetz->page_sequence)
		PDEBUG(DANETZ, DEBUG_NOTICE, "Sending paging tones in sequence.\n");

	/* 4. trying to page mobile station */
	anetz->callref = callref;
	anetz_page(anetz, dialing, freq);

	call_in_alerting(callref);

	return 0;
}

/* Call control sends disconnect (with tones).
 * An active call stays active, so tones and annoucements can be received
 * by mobile station.
 */
void call_out_disconnect(int callref, int cause)
{
	sender_t *sender;
	anetz_t *anetz;

	PDEBUG(DANETZ, DEBUG_INFO, "Call has been disconnected by network.\n");

	for (sender = sender_head; sender; sender = sender->next) {
		anetz = (anetz_t *) sender;
		if (anetz->callref == callref)
			break;
	}
	if (!sender) {
		PDEBUG(DANETZ, DEBUG_NOTICE, "Outgoing disconnect, but no callref!\n");
		call_in_release(callref, CAUSE_INVALCALLREF);
		return;
	}

	/* Release when not active */
	if (anetz->state == ANETZ_GESPRAECH)
		return;
	switch (anetz->state) {
	case ANETZ_ANRUF:
		PDEBUG(DANETZ, DEBUG_NOTICE, "Outgoing disconnect, during alerting, going idle!\n");
	 	anetz_go_idle(anetz);
		break;
	default:
		break;
	}

	call_in_release(callref, cause);

	anetz->callref = 0;

}

/* Call control releases call toward mobile station. */
void call_out_release(int callref, __attribute__((unused)) int cause)
{
	sender_t *sender;
	anetz_t *anetz;

	PDEBUG(DANETZ, DEBUG_INFO, "Call has been released by network, releasing call.\n");

	for (sender = sender_head; sender; sender = sender->next) {
		anetz = (anetz_t *) sender;
		if (anetz->callref == callref)
			break;
	}
	if (!sender) {
		PDEBUG(DANETZ, DEBUG_NOTICE, "Outgoing release, but no callref!\n");
		/* don't send release, because caller already released */
		return;
	}

	anetz->callref = 0;

	switch (anetz->state) {
	case ANETZ_GESPRAECH:
		PDEBUG(DANETZ, DEBUG_NOTICE, "Outgoing release, during call, sending release tone!\n");
	 	anetz_release(anetz);
		break;
	case ANETZ_ANRUF:
		PDEBUG(DANETZ, DEBUG_NOTICE, "Outgoing release, during alerting, going idle!\n");
	 	anetz_go_idle(anetz);
		break;
	default:
		break;
	}
}

/* Receive audio from call instance. */
void call_rx_audio(int callref, int16_t *samples, int count)
{
	sender_t *sender;
	anetz_t *anetz;

	for (sender = sender_head; sender; sender = sender->next) {
		anetz = (anetz_t *) sender;
		if (anetz->callref == callref)
			break;
	}
	if (!sender)
		return;

	if (anetz->dsp_mode == DSP_MODE_AUDIO) {
		int16_t up[(int)((double)count * anetz->sender.srstate.factor + 0.5) + 10];
		count = samplerate_upsample(&anetz->sender.srstate, samples, count, up);
		jitter_save(&anetz->sender.audio, up, count);
	}
}

void dump_info(void) {}

