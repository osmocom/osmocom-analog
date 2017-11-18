/* interface between mobile network/phone implementation and MNCC
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
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include "../libsample/sample.h"
#include "debug.h"
#include "sender.h"
#include "call.h"
#include "../libtimer/timer.h"
#include "../libmncc/mncc.h"
#include "../libmncc/cause.h"

#define DISC_TIMEOUT	30

//#define DEBUG_LEVEL

#ifdef DEBUG_LEVEL
static double level_of(double *samples, int count)
{
	double level = 0;
	int i;

	for (i = 0; i < count; i++) {
		if (samples[i] > level)
			level = samples[i];
	}

	return level;
}
#endif

static int send_patterns;		/* send patterns towards fixed network */
static int release_on_disconnect;	/* release towards mobile phone, if MNCC call disconnects, don't send disconnect tone */

/* stream patterns/announcements */
int16_t *ringback_spl = NULL;
int ringback_size = 0;
int ringback_max = 0;
int16_t *hangup_spl = NULL;
int hangup_size = 0;
int hangup_max = 0;
int16_t *busy_spl = NULL;
int busy_size = 0;
int busy_max = 0;
int16_t *noanswer_spl = NULL;
int noanswer_size = 0;
int noanswer_max = 0;
int16_t *outoforder_spl = NULL;
int outoforder_size = 0;
int outoforder_max = 0;
int16_t *invalidnumber_spl = NULL;
int invalidnumber_size = 0;
int invalidnumber_max = 0;
int16_t *congestion_spl = NULL;
int congestion_size = 0;
int congestion_max = 0;
int16_t *recall_spl = NULL;
int recall_size = 0;
int recall_max = 0;

enum audio_pattern {
	PATTERN_NONE = 0,
	PATTERN_TEST,
	PATTERN_RINGBACK,
	PATTERN_HANGUP,
	PATTERN_BUSY,
	PATTERN_NOANSWER,
	PATTERN_OUTOFORDER,
	PATTERN_INVALIDNUMBER,
	PATTERN_CONGESTION,
	PATTERN_RECALL,
};

static void get_pattern(const int16_t **spl, int *size, int *max, enum audio_pattern pattern)
{
	*spl = NULL;
	*size = 0;
	*max = 0;

	switch (pattern) {
	case PATTERN_RINGBACK:
no_recall:
		*spl = ringback_spl;
		*size = ringback_size;
		*max = ringback_max;
		break;
	case PATTERN_HANGUP:
		if (!hangup_spl)
			goto no_hangup;
		*spl = hangup_spl;
		*size = hangup_size;
		*max = hangup_max;
		break;
	case PATTERN_BUSY:
no_hangup:
no_noanswer:
		*spl = busy_spl;
		*size = busy_size;
		*max = busy_max;
		break;
	case PATTERN_NOANSWER:
		if (!noanswer_spl)
			goto no_noanswer;
		*spl = noanswer_spl;
		*size = noanswer_size;
		*max = noanswer_max;
		break;
	case PATTERN_OUTOFORDER:
		if (!outoforder_spl)
			goto no_outoforder;
		*spl = outoforder_spl;
		*size = outoforder_size;
		*max = outoforder_max;
		break;
	case PATTERN_INVALIDNUMBER:
		if (!invalidnumber_spl)
			goto no_invalidnumber;
		*spl = invalidnumber_spl;
		*size = invalidnumber_size;
		*max = invalidnumber_max;
		break;
	case PATTERN_CONGESTION:
no_outoforder:
no_invalidnumber:
		*spl = congestion_spl;
		*size = congestion_size;
		*max = congestion_max;
		break;
	case PATTERN_RECALL:
		if (!recall_spl)
			goto no_recall;
		*spl = recall_spl;
		*size = recall_size;
		*max = recall_max;
		break;
	default:
		;
	}
}

static enum audio_pattern cause2pattern(int cause)
{
	int pattern;

	switch (cause) {
	case CAUSE_NORMAL:
		pattern = PATTERN_HANGUP;
		break;
	case CAUSE_BUSY:
		pattern = PATTERN_BUSY;
		break;
	case CAUSE_NOANSWER:
		pattern = PATTERN_NOANSWER;
		break;
	case CAUSE_OUTOFORDER:
		pattern = PATTERN_OUTOFORDER;
		break;
	case CAUSE_INVALNUMBER:
		pattern = PATTERN_INVALIDNUMBER;
		break;
	case CAUSE_NOCHANNEL:
		pattern = PATTERN_CONGESTION;
		break;
	default:
		pattern = PATTERN_HANGUP;
	}

	return pattern;
}

enum process_state {
	PROCESS_IDLE = 0,	/* IDLE */
	PROCESS_SETUP_RO,	/* call from radio to MNCC */
	PROCESS_SETUP_RT,	/* call from MNCC to radio */
	PROCESS_ALERTING_RO,	/* call from radio to MNCC */
	PROCESS_ALERTING_RT,	/* call from MNCC to radio */
	PROCESS_CONNECT,
	PROCESS_DISCONNECT,
};

/* MNCC call instance */
typedef struct process {
	struct process *next;
	int callref;
	enum process_state state;
	int audio_disconnected; /* if not associated with transceiver anymore */
	enum audio_pattern pattern;
	int audio_pos;
	uint8_t cause;
	struct timer timer;
} process_t;

static process_t *process_head = NULL;

static void process_timeout(struct timer *timer);

static process_t *create_process(int callref, enum process_state state)
{
	process_t *process;

	process = calloc(sizeof(*process), 1);
	if (!process) {
		PDEBUG(DCALL, DEBUG_ERROR, "No memory!\n");
		abort();
	}
	timer_init(&process->timer, process_timeout, process);
	process->next = process_head;
	process_head = process;

	process->callref = callref;
	process->state = state;

	return process;
}

static void destroy_process(int callref)
{
	process_t *process = process_head;
	process_t **process_p = &process_head;

	while (process) {
		if (process->callref == callref) {
			*process_p = process->next;
			timer_exit(&process->timer);
			free(process);
			return;
		}
		process_p = &process->next;
		process = process->next;
	}
	PDEBUG(DCALL, DEBUG_ERROR, "Process with callref 0x%x not found!\n", callref);
}

static process_t *get_process(int callref)
{
	process_t *process = process_head;

	while (process) {
		if (process->callref == callref)
			return process;
		process = process->next;
	}
	return NULL;
}

static void new_state_process(int callref, enum process_state state)
{
	process_t *process = get_process(callref);

	if (!process) {
		PDEBUG(DCALL, DEBUG_ERROR, "Process with callref 0x%x not found!\n", callref);
		return;
	}
	process->state = state;
}

static void set_pattern_process(int callref, enum audio_pattern pattern)
{
	process_t *process = get_process(callref);

	if (!process) {
		PDEBUG(DCALL, DEBUG_ERROR, "Process with callref 0x%x not found!\n", callref);
		return;
	}
	process->pattern = pattern;
	process->audio_pos = 0;
}

/* disconnect audio, now send audio directly from pattern/announcement, not from transceiver */
static void disconnect_process(int callref, int cause)
{
	process_t *process = get_process(callref);

	if (!process) {
		PDEBUG(DCALL, DEBUG_ERROR, "Process with callref 0x%x not found!\n", callref);
		return;
	}
	process->pattern = cause2pattern(cause);
	process->audio_disconnected = 1;
	process->audio_pos = 0;
	process->cause = cause;
	timer_start(&process->timer, DISC_TIMEOUT);
}

static void get_process_patterns(process_t *process, int16_t *samples, int length)
{
	const int16_t *spl;
	int size, max, pos;

	get_pattern(&spl, &size, &max, process->pattern);

	/* stream sample */
	pos = process->audio_pos;
	while(length--) {
		if (pos >= size)
			*samples++ = 0;
		else
			*samples++ = spl[pos] >> 1;
		if (++pos == max)
			pos = 0;
	}
	process->audio_pos = pos;
}

static void process_timeout(struct timer *timer)
{
	process_t *process = (process_t *)timer->priv;

	{
		/* announcement timeout */
		uint8_t buf[sizeof(struct gsm_mncc)];
		struct gsm_mncc *mncc = (struct gsm_mncc *)buf;

		memset(buf, 0, sizeof(buf));
		mncc->msg_type = MNCC_REL_IND;
		mncc->callref = process->callref;
		mncc->fields |= MNCC_F_CAUSE;
		mncc->cause.location = LOCATION_PRIVATE_LOCAL;
		mncc->cause.value = process->cause;

		destroy_process(process->callref);
		PDEBUG(DCALL, DEBUG_INFO, "Releasing MNCC call towards fixed network (after timeout)\n");
		mncc_up(buf, sizeof(struct gsm_mncc));
	}
}

int call_init(int _send_patterns, int _release_on_disconnect)
{
	send_patterns = _send_patterns;
	release_on_disconnect = _release_on_disconnect;

	return 0;
}

/* Setup is received from transceiver. */
static int _indicate_setup(int callref, const char *callerid, const char *dialing)
{
	uint8_t buf[sizeof(struct gsm_mncc)];
	struct gsm_mncc *mncc = (struct gsm_mncc *)buf;
	int rc;

	memset(buf, 0, sizeof(buf));
	mncc->msg_type = MNCC_SETUP_IND;
	mncc->callref = callref;
	mncc->fields |= MNCC_F_CALLING;
	if (callerid) {
		strncpy(mncc->calling.number, callerid, sizeof(mncc->calling.number) - 1);
		mncc->calling.type = 4; /* caller ID is of type 'subscriber' */
	} // otherwise unknown and no number
	mncc->fields |= MNCC_F_CALLED;
	strncpy(mncc->called.number, dialing, sizeof(mncc->called.number) - 1);
	mncc->called.type = 0; /* dialing is of type 'unknown' */
	mncc->lchan_type = GSM_LCHAN_TCH_F;
	mncc->fields |= MNCC_F_BEARER_CAP;
	mncc->bearer_cap.speech_ver[0] = BCAP_ANALOG_8000HZ;
	mncc->bearer_cap.speech_ver[1] = -1;

	PDEBUG(DCALL, DEBUG_INFO, "Indicate MNCC setup towards fixed network\n");
	rc = mncc_up(buf, sizeof(struct gsm_mncc));
	if (rc < 0)
		destroy_process(callref);
	return rc;
}
int call_up_setup(int callref, const char *callerid, const char *dialing)
{
	int rc;

	if (!callref) {
		PDEBUG(DCALL, DEBUG_DEBUG, "Ignoring setup, because callref not set. (not for us)\n");
		return -CAUSE_INVALCALLREF;
	}

	if (callref < 0x4000000) {
		PDEBUG(DCALL, DEBUG_ERROR, "Invalid callref from mobile station, please fix!\n");
		abort();
	}

	PDEBUG(DCALL, DEBUG_INFO, "Incoming call from '%s' to '%s'\n", callerid ? : "unknown", dialing);
	if (!strcmp(dialing, "010"))
		PDEBUG(DCALL, DEBUG_INFO, " -> Call to Operator '%s'\n", dialing);


	create_process(callref, PROCESS_SETUP_RO);

	rc = _indicate_setup(callref, callerid, dialing);

	return rc;
}

/* Transceiver indicates alerting. */
static void _indicate_alerting(int callref)
{
	uint8_t buf[sizeof(struct gsm_mncc)];
	struct gsm_mncc *mncc = (struct gsm_mncc *)buf;
	int rc;

	memset(buf, 0, sizeof(buf));
	mncc->msg_type = MNCC_ALERT_IND;
	mncc->callref = callref;

	PDEBUG(DCALL, DEBUG_INFO, "Indicate MNCC alerting towards fixed network\n");
	rc = mncc_up(buf, sizeof(struct gsm_mncc));
	if (rc < 0)
		destroy_process(callref);
}
void call_up_alerting(int callref)
{
	if (!callref) {
		PDEBUG(DCALL, DEBUG_DEBUG, "Ignoring alerting, because callref not set. (not for us)\n");
		return;
	}

	PDEBUG(DCALL, DEBUG_INFO, "Call is alerting\n");

	if (!send_patterns)
		_indicate_alerting(callref);
	set_pattern_process(callref, PATTERN_RINGBACK);
	new_state_process(callref, PROCESS_ALERTING_RT);
}

/* Transceiver indicates answer. */
static void _indicate_answer(int callref, const char *connect_id)
{
	uint8_t buf[sizeof(struct gsm_mncc)];
	struct gsm_mncc *mncc = (struct gsm_mncc *)buf;
	int rc;

	memset(buf, 0, sizeof(buf));
	mncc->msg_type = MNCC_SETUP_CNF;
	mncc->callref = callref;
	mncc->fields |= MNCC_F_CONNECTED;
	/* copy connected number as subscriber number */
	strncpy(mncc->connected.number, connect_id, sizeof(mncc->connected.number));
	mncc->connected.type = 4;
	mncc->connected.plan = 1;
	mncc->connected.present = 0;
	mncc->connected.screen = 3;

	PDEBUG(DCALL, DEBUG_INFO, "Indicate MNCC answer towards fixed network\n");
	rc = mncc_up(buf, sizeof(struct gsm_mncc));
	if (rc < 0)
		destroy_process(callref);
}
void call_up_answer(int callref, const char *connect_id)
{
	if (!callref) {
		PDEBUG(DCALL, DEBUG_DEBUG, "Ignoring answer, because callref not set. (not for us)\n");
		return;
	}

	PDEBUG(DCALL, DEBUG_INFO, "Call has been answered by '%s'\n", connect_id);

	if (!send_patterns)
		_indicate_answer(callref, connect_id);
	set_pattern_process(callref, PATTERN_NONE);
	new_state_process(callref, PROCESS_CONNECT);
}

/* Transceiver indicates release. */
static void _indicate_disconnect_release(int callref, int cause, int disc)
{
	uint8_t buf[sizeof(struct gsm_mncc)];
	struct gsm_mncc *mncc = (struct gsm_mncc *)buf;
	int rc;

	memset(buf, 0, sizeof(buf));
	mncc->msg_type = (disc) ? MNCC_DISC_IND : MNCC_REL_IND;
	mncc->callref = callref;
	mncc->fields |= MNCC_F_CAUSE;
	mncc->cause.location = LOCATION_PRIVATE_LOCAL;
	mncc->cause.value = cause;

	PDEBUG(DCALL, DEBUG_INFO, "Indicate MNCC %s towards fixed network\n", (disc) ? "disconnect" : "release");
	rc = mncc_up(buf, sizeof(struct gsm_mncc));
	if (rc < 0)
		destroy_process(callref);
}
void call_up_release(int callref, int cause)
{
	process_t *process;

	if (!callref) {
		PDEBUG(DCALL, DEBUG_DEBUG, "Ignoring release, because callref not set. (not for us)\n");
		return;
	}

	PDEBUG(DCALL, DEBUG_INFO, "Call has been released with cause=%d\n", cause);

	process = get_process(callref);
	if (process) {
		/* just keep MNCC connection if tones shall be sent.
		 * no tones while setting up / alerting the call. */
		if (send_patterns
		 && process->state != PROCESS_SETUP_RO
		 && process->state != PROCESS_ALERTING_RO)
			disconnect_process(callref, cause);
		else
		/* if no tones shall be sent, release on disconnect
		 * or RO setup states */
		if (process->state == PROCESS_DISCONNECT
		 || process->state == PROCESS_SETUP_RO
		 || process->state == PROCESS_ALERTING_RO) {
			destroy_process(callref);
			_indicate_disconnect_release(callref, cause, 0);
		/* if no tones shall be sent, disconnect on all other states */
		} else {
			disconnect_process(callref, cause);
			_indicate_disconnect_release(callref, cause, 1);
		}
	} else {
		/* we don't know about the process, just send release to upper layer anyway */
		_indicate_disconnect_release(callref, cause, 0);
	}
}

/* turn recall tone on or off */
void call_tone_recall(int callref, int on)
{
	set_pattern_process(callref, (on) ? PATTERN_RECALL : PATTERN_NONE);
}

/* forward audio to MNCC or call instance */
void call_up_audio(int callref, sample_t *samples, int count)
{
	if (count != 160) {
		fprintf(stderr, "Samples must be 160, please fix!\n");
		abort();
	}
	/* is MNCC us used, forward audio */
	uint8_t buf[sizeof(struct gsm_data_frame) + 160 * sizeof(int16_t)];
	struct gsm_data_frame *data = (struct gsm_data_frame *)buf;
	process_t *process;

	if (!callref)
		return;

	/* if we are disconnected, ignore audio */
	process = get_process(callref);
	if (!process || process->pattern != PATTERN_NONE)
		return;

	/* forward audio */
	data->msg_type = ANALOG_8000HZ;
	data->callref = callref;
#ifdef DEBUG_LEVEL
	double lev = level_of(samples, count);
	printf("   mobil-level: %s%.4f\n", debug_db(lev), (20 * log10(lev)));
#endif
	samples_to_int16((int16_t *)data->data, samples, count);

	mncc_up(buf, sizeof(buf));
	/* don't destroy process here in case of an error */
}

/* clock that is used to transmit patterns */
void call_clock(void)
{
	process_t *process = process_head;
	uint8_t buf[sizeof(struct gsm_data_frame) + 160 * sizeof(int16_t)];
	struct gsm_data_frame *data = (struct gsm_data_frame *)buf;

	while(process) {
		if (process->pattern != PATTERN_NONE) {
			data->msg_type = ANALOG_8000HZ;
			data->callref = process->callref;
			/* try to get patterns, else copy the samples we got */
			get_process_patterns(process, (int16_t *)data->data, 160);
#ifdef DEBUG_LEVEL
			sample_t samples[160];
			int16_to_samples(samples, (int16_t *)data->data, 160);
			double lev = level_of(samples, 160);
			printf("   mobil-level: %s%.4f\n", debug_db(lev), (20 * log10(lev)));
			samples_to_int16((int16_t *)data->data, samples, 160);
#endif
			mncc_up(buf, sizeof(buf));
			/* don't destroy process here in case of an error */
		}
		process = process->next;
	}
}

/* mncc messages received from fixed network */
void mncc_down(uint8_t *buf, int length)
{
	struct gsm_mncc *mncc = (struct gsm_mncc *)buf;
	char number[sizeof(mncc->called.number)];
	char caller_id[sizeof(mncc->calling.number)];
	enum number_type caller_type;
	int callref;
	int rc;
	process_t *process;

	callref = mncc->callref;
	process = get_process(callref);
	if (!process) {
		if (mncc->msg_type == MNCC_SETUP_REQ)
			process = create_process(callref, PROCESS_SETUP_RT);
		else {
			if (mncc->msg_type != MNCC_REL_REQ)
				PDEBUG(DCALL, DEBUG_ERROR, "No process!\n");
			return;
		}
	}

	if (mncc->msg_type == ANALOG_8000HZ) {
		struct gsm_data_frame *data = (struct gsm_data_frame *)buf;
		sample_t samples[160];

		/* if we are disconnected, ignore audio */
		if (process->pattern != PATTERN_NONE)
			return;
		int16_to_samples(samples, (int16_t *)data->data, 160);
#ifdef DEBUG_LEVEL
		double lev = level_of(samples, 160);
		printf("festnetz-level: %s                  %.4f\n", debug_db(lev), (20 * log10(lev)));
#endif
		call_down_audio(callref, samples, 160);
		return;
	}

	if (process->audio_disconnected) {
		switch(mncc->msg_type) {
		case MNCC_DISC_REQ:
			PDEBUG(DCALL, DEBUG_INFO, "Received MNCC disconnect from fixed network with cause %d\n", mncc->cause.value);

			PDEBUG(DCALL, DEBUG_INFO, "Call disconnected, releasing!\n");

			destroy_process(callref);

			PDEBUG(DCALL, DEBUG_INFO, "Indicate MNCC release towards fixed network\n");
			mncc->msg_type = MNCC_REL_IND;
			rc = mncc_up(buf, sizeof(struct gsm_mncc));
			if (rc < 0)
				destroy_process(callref);
		break;
		case MNCC_REL_REQ:
			PDEBUG(DCALL, DEBUG_INFO, "Received MNCC release from fixed network with cause %d\n", mncc->cause.value);

			PDEBUG(DCALL, DEBUG_INFO, "Call released\n");

			destroy_process(callref);
			break;
		}
		return;
	}

	switch(mncc->msg_type) {
	case MNCC_SETUP_REQ:
		strcpy(number, mncc->called.number);

		/* caller ID conversion */
		strcpy(caller_id, mncc->calling.number);
		switch(mncc->calling.type) {
		case 1:
			caller_type = TYPE_INTERNATIONAL;
			break;
		case 2:
			caller_type = TYPE_NATIONAL;
			break;
		case 4:
			caller_type = TYPE_SUBSCRIBER;
			break;
		default: /* or 0 */
			caller_type = TYPE_UNKNOWN;
			break;
		}
		if (!caller_id[0])
			caller_type = TYPE_NOTAVAIL;
		if (mncc->calling.present == 1)
			caller_type = TYPE_ANONYMOUS;

		PDEBUG(DCALL, DEBUG_INFO, "Received MNCC call from fixed network '%s' to mobile '%s'\n", caller_id, number);

		if (mncc->callref >= 0x4000000) {
			fprintf(stderr, "Invalid callref from fixed network, please fix!\n");
			abort();
		}

		PDEBUG(DCALL, DEBUG_INFO, "Indicate MNCC call confirm towards fixed network\n");
		memset(buf, 0, length);
		mncc->msg_type = MNCC_CALL_CONF_IND;
		mncc->callref = callref;
		mncc->lchan_type = GSM_LCHAN_TCH_F;
		mncc->fields |= MNCC_F_BEARER_CAP;
		mncc->bearer_cap.speech_ver[0] = BCAP_ANALOG_8000HZ;
		mncc->bearer_cap.speech_ver[1] = -1;

		mncc_up(buf, sizeof(struct gsm_mncc));

		PDEBUG(DCALL, DEBUG_INFO, "Outgoing call from '%s' to '%s'\n", caller_id, number);

		rc = call_down_setup(callref, caller_id, caller_type, number);
		if (rc < 0) {
			PDEBUG(DCALL, DEBUG_NOTICE, "Call rejected, cause %d\n", -rc);
			if (send_patterns) {
				PDEBUG(DCALL, DEBUG_DEBUG, "Early connecting after setup\n");
				_indicate_answer(callref, number);
			} else {
				PDEBUG(DCALL, DEBUG_INFO, "Disconnecting MNCC call towards fixed network (cause=%d)\n", -rc);
				_indicate_disconnect_release(callref, -rc, 1);
			}
			disconnect_process(callref, -rc);
			break;
		}

		if (send_patterns) {
			PDEBUG(DCALL, DEBUG_DEBUG, "Early connecting after setup\n");
			_indicate_answer(callref, number);
			break;
		}
		break;
	case MNCC_ALERT_REQ:
		PDEBUG(DCALL, DEBUG_INFO, "Received MNCC alerting from fixed network\n");
		new_state_process(callref, PROCESS_ALERTING_RO);
		break;
	case MNCC_SETUP_RSP:
		PDEBUG(DCALL, DEBUG_INFO, "Received MNCC answer from fixed network\n");
		new_state_process(callref, PROCESS_CONNECT);
		PDEBUG(DCALL, DEBUG_INFO, "Call answered\n");
		call_down_answer(callref);
		PDEBUG(DCALL, DEBUG_INFO, "Indicate MNCC setup complete towards fixed network\n");
		memset(buf, 0, length);
		mncc->msg_type = MNCC_SETUP_COMPL_IND;
		mncc->callref = callref;
		rc = mncc_up(buf, sizeof(struct gsm_mncc));
		if (rc < 0)
			destroy_process(callref);
		break;
	case MNCC_DISC_REQ:
		PDEBUG(DCALL, DEBUG_INFO, "Received MNCC disconnect from fixed network with cause %d\n", mncc->cause.value);

		process = get_process(callref);
		if (process && process->state == PROCESS_CONNECT && release_on_disconnect) {
			PDEBUG(DCALL, DEBUG_INFO, "Releasing, because we don't send disconnect tones to mobile phone\n");

			PDEBUG(DCALL, DEBUG_INFO, "Indicate MNCC release towards fixed network\n");
			mncc->msg_type = MNCC_REL_IND;
			mncc_up(buf, sizeof(struct gsm_mncc));
			goto release;
		}
		new_state_process(callref, PROCESS_DISCONNECT);
		PDEBUG(DCALL, DEBUG_INFO, "Call disconnected\n");
		call_down_disconnect(callref, mncc->cause.value);
		break;
	case MNCC_REL_REQ:
		PDEBUG(DCALL, DEBUG_INFO, "Received MNCC release from fixed network with cause %d\n", mncc->cause.value);

release:
		destroy_process(callref);
		PDEBUG(DCALL, DEBUG_INFO, "Call released\n");
		call_down_release(callref, mncc->cause.value);
		break;
	}
}

int (*mncc_up)(uint8_t *buf, int length) = NULL;

/* break down of MNCC socket */
void mncc_flush(void)
{
	while(process_head) {
		PDEBUG(DCALL, DEBUG_NOTICE, "MNCC socket closed, releasing call\n");
		call_down_release(process_head->callref, CAUSE_TEMPFAIL);
		destroy_process(process_head->callref);
		/* note: callref is released by sender's instance */
	}
}

