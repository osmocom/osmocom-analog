/* Zeitansage processing
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
#include <errno.h>
#include <math.h>
#include <time.h>
#include "../libsample/sample.h"
#include "../liblogging/logging.h"
#include "../libmobile/call.h"
#include "../libmobile/cause.h"
#include "../libmobile/get_time.h"
#include "zeitansage.h"

#define db2level(db)	pow(10, (double)(db) / 20.0)

/* list of calls */
zeit_call_t *zeit_call_list = NULL;

double audio_gain;
double early_audio;

#define	BEEP_TIME	16000	/* adjust distance from beep in samples */

int16_t *bntie_spl;
int bntie_size;
int bntie_time; /* sample index when intro is over */
int16_t *urrr_spl[24];
int urrr_size[24];
int urrr_time; /* sample index when hour is over */
int16_t *minuten_spl[60];
int minuten_size[60];
int minuten_time; /* sample index when minute is over */
int16_t *sekunden_spl[60];
int sekunden_size[60];
int sekunden_time; /* sample index when second is over */
int16_t *tut_spl;
int tut_size;
int tut_time; /* sample index when beep is over */

static const char *call_state_name(enum zeit_call_state state)
{
	static char invalid[16];

	switch (state) {
	case ZEIT_CALL_NULL:
		return "(NULL)";
	case ZEIT_CALL_BEEP:
		return "BEEP";
	case ZEIT_CALL_INTRO:
		return "INTRO";
	case ZEIT_CALL_HOUR:
		return "HOUR";
	case ZEIT_CALL_MINUTE:
		return "MINUTE";
	case ZEIT_CALL_SECOND:
		return "SECOND";
	case ZEIT_CALL_PAUSE:
		return "PAUSE";
	}

	sprintf(invalid, "invalid(%d)", state);
	return invalid;
}

static void zeit_display_status(void)
{
	zeit_call_t *call;

	display_status_start();
	for (call = zeit_call_list; call; call = call->next)
		display_status_subscriber(call->caller_id, call_state_name(call->state));
	display_status_end();
}


static void call_new_state(zeit_call_t *call, enum zeit_call_state new_state)
{
	if (call->state == new_state)
		return;
	LOGP(DZEIT, LOGL_DEBUG, "State change: %s -> %s\n", call_state_name(call->state), call_state_name(new_state));
	call->state = new_state;
	zeit_display_status();
}

/* global init */
int zeit_init(double audio_level_dBm, int alerting)
{
	int i;

	/* the recordings are speech level, so we apply gain as is */
	audio_gain = db2level(audio_level_dBm);

	early_audio = alerting;

	/* get maximum length for each speech segment */
	tut_time = BEEP_TIME;
	bntie_time = bntie_size + tut_time;
	urrr_time = 0;
	for (i = 0; i < 24; i++) {
		if (urrr_size[i] > urrr_time)
			urrr_time = urrr_size[i];
	}
	urrr_time += bntie_time;
	minuten_time = 0;
	for (i = 0; i < 60; i++) {
		if (minuten_size[i] > minuten_time)
			minuten_time = minuten_size[i];
	}
	minuten_time += urrr_time;
	sekunden_time = 0;
	for (i = 0; i < 60; i += 10) {
		if (sekunden_size[i] > sekunden_time)
			sekunden_time = sekunden_size[i];
	}
	sekunden_time += minuten_time;

	LOGP(DZEIT, LOGL_DEBUG, "Total time to play announcement, starting with beep: %.2f seconds\n", (double)sekunden_time / 8000.0);

	return 0;
}

/* global exit */
void zeit_exit(void)
{
}

/* calculate what time to speak */
static void zeit_calc_time(zeit_call_t *call, time_t time_sec)
{
	struct tm *tm;

	/* we speak 10 seconds in advance */
	time_sec += 10;

	tm = localtime(&time_sec);
	call->h = tm->tm_hour;
	call->m = tm->tm_min;
	call->s = tm->tm_sec;

	LOGP(DZEIT, LOGL_INFO, "The time at the next beep is: %d:%02d:%02d\n", call->h, call->m, call->s);
}

static void call_timeout(void *data);

#define FLOAT_TO_TIMEOUT(f) floor(f), ((f) - floor(f)) * 1000000

/* Create call instance */
static zeit_call_t *zeit_call_create(uint32_t callref, const char *id)
{
	zeit_call_t *call, **callp;
	double now, time_offset;
	time_t time_sec;

	LOGP(DZEIT, LOGL_INFO, "Creating call instance to play time for caller '%s'.\n", id);

	/* create */
	call = calloc(1, sizeof(*call));
	if (!call) {
		LOGP(DZEIT, LOGL_ERROR, "No mem!\n");
		abort();
	}

	/* init */
	call->callref = callref;
	strncpy(call->caller_id, id, sizeof(call->caller_id) - 1);
	osmo_timer_setup(&call->timer, call_timeout, call);
	now = get_time();
	time_offset = fmod(now, 10.0);
	time_sec = (int)floor(now / 10.0) * 10;
	call->spl_time = (int)(time_offset * 8000.0);
	zeit_calc_time(call, time_sec);
	osmo_timer_schedule(&call->timer, FLOAT_TO_TIMEOUT(10.0 - time_offset));

	/* link */
	callp = &zeit_call_list;
	while ((*callp))
		callp = &(*callp)->next;
	(*callp) = call;

	return call;
}

/* Destroy call instance */
static void zeit_call_destroy(zeit_call_t *call)
{
	zeit_call_t **callp;

	/* unlink */
	callp = &zeit_call_list;
	while ((*callp) != call)
		callp = &(*callp)->next;
	(*callp) = call->next;

	/* cleanup */
	osmo_timer_del(&call->timer);

	/* destroy */
	free(call);

	/* update display */
	zeit_display_status();
}

/* play samples for one call */
static void call_play(zeit_call_t *call)
{
	int i = 0;
	int16_t chunk[160];
	sample_t spl[160];
	int16_t *play_spl;	/* current sample */
	int play_size;		/* current size of sample*/
	int play_index;		/* current sample index */
	int play_max;		/* total length to plax */
	int spl_time;		/* sample offset from start of 10 minutes */

	spl_time = call->spl_time;

next_sample:
	/* select sample from current sample time stamp */
	if (spl_time < tut_time) {
		play_index = spl_time;
		play_max = tut_time;
		play_size = tut_size;
		play_spl = tut_spl;
		call_new_state(call, ZEIT_CALL_BEEP);
	} else
	if (spl_time < bntie_time) {
		play_index = spl_time - tut_time;
		play_max = bntie_time - tut_time;
		play_size = bntie_size;
		play_spl = bntie_spl;
		call_new_state(call, ZEIT_CALL_INTRO);
	} else
	if (spl_time < urrr_time) {
		play_index = spl_time - bntie_time;
		play_max = urrr_time - bntie_time;
		play_size = urrr_size[call->h];
		play_spl = urrr_spl[call->h];
		call_new_state(call, ZEIT_CALL_HOUR);
	} else
	if (spl_time < minuten_time) {
		play_index = spl_time - urrr_time;
		play_max = minuten_time - urrr_time;
		play_size = minuten_size[call->m];
		play_spl = minuten_spl[call->m];
		call_new_state(call, ZEIT_CALL_MINUTE);
	} else
	if (spl_time < sekunden_time) {
		play_index = spl_time - minuten_time;
		play_max = sekunden_time - minuten_time;
		play_size = sekunden_size[call->s];
		play_spl = sekunden_spl[call->s];
		call_new_state(call, ZEIT_CALL_SECOND);
	} else {
		play_index = 0;
		play_max = 0;
		play_size = 0;
		play_spl = NULL;
		call_new_state(call, ZEIT_CALL_PAUSE);
	}

	while (i < 160) {
		if (!play_size) {
			chunk[i++] = 0.0;
			continue;
		}
		/* go to next sample */
		if (play_index == play_max)
			goto next_sample;
		/* announcement or silence, if finished or not set */
		if (play_index < play_size)
			chunk[i++] = play_spl[play_index];
		else
			chunk[i++] = 0.0;
		play_index++;
		spl_time++;
	}

	call->spl_time = spl_time;

	/* convert to samples, apply gain and send toward fixed network */
	int16_to_samples_speech(spl, chunk, 160);
	for (i = 0; i < 160; i++)
		spl[i] *= audio_gain;
	call_up_audio(call->callref, spl, 160);
}

/* loop through all calls and play the announcement */
void call_down_clock(void)
{
	zeit_call_t *call;

	for (call = zeit_call_list; call; call = call->next) {
		/* no callref */
		if (!call->callref)
			continue;
		/* beep or announcement */
		call_play(call);
	}
}

/* Timeout handling */
static void call_timeout(void *data)
{
	zeit_call_t *call = data;
	double now, time_offset;
	time_t time_sec;

	LOGP(DZEIT, LOGL_INFO, "Beep!\n");

	now = get_time();

	time_offset = fmod(now, 10.0);
	time_sec = (int)floor(now / 10.0) * 10;
	/* if somehow the timer fires (a tiny bit) before the 10 seconds start */
	if (time_offset > 5.0) {
		time_offset -= 10.0;
		time_sec += 10;
	}
	call->spl_time = 0;
	zeit_calc_time(call, time_sec);
	osmo_timer_schedule(&call->timer, FLOAT_TO_TIMEOUT(10.0 - time_offset));
}

/* Call control starts call towards clock */
int call_down_setup(int callref, const char __attribute__((unused)) *caller_id, enum number_type __attribute__((unused)) caller_type, const char *dialing)
{
	zeit_call_t __attribute__((unused)) *call;

	/* create call process to page station or send out-of-order message */
	call = zeit_call_create(callref, caller_id);
	if (early_audio) {
		call_up_alerting(callref);
		call_up_early(callref);
	} else
		call_up_answer(callref, dialing);

	return 0;
}

void call_down_answer(int __attribute__((unused)) callref)
{
}

static void _release(int callref, int __attribute__((unused)) cause)
{
	zeit_call_t *call;

	LOGP(DZEIT, LOGL_INFO, "Call has been disconnected by network.\n");

	for (call = zeit_call_list; call; call = call->next) {
		if (call->callref == callref)
			break;
	}
	if (!call) {
		LOGP(DZEIT, LOGL_NOTICE, "Outgoing disconnect, but no callref!\n");
		call_up_release(callref, CAUSE_INVALCALLREF);
		return;
	}

	zeit_call_destroy(call);
}

/* Call control sends disconnect.
 * A queued ID will be kept until transmitted by mobile station.
 */
void call_down_disconnect(int callref, int cause)
{
	_release(callref, cause);

	call_up_release(callref, cause);
}

/* Call control releases call toward mobile station. */
void call_down_release(int callref, int cause)
{
	_release(callref, cause);
}

/* Receive audio from call instance. */
void call_down_audio(int __attribute__((unused)) callref, uint16_t __attribute__((unused)) sequence, uint32_t __attribute__((unused)) timestamp, uint32_t __attribute__((unused)) ssrc, sample_t __attribute__((unused)) *samples, int __attribute__((unused)) count)
{
}

void dump_info(void) {}

void sender_receive(sender_t __attribute__((unused)) *sender, sample_t __attribute__((unused)) *samples, int __attribute__((unused)) length, double __attribute__((unused)) rf_level_db) {}
void sender_send(sender_t __attribute__((unused)) *sender, sample_t __attribute__((unused)) *samples, uint8_t __attribute__((unused)) *power, int __attribute__((unused)) length) {}

