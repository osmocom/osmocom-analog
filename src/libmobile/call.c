/* interface between mobile network/phone implementation and OsmoCC
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
#include <arpa/inet.h>
#include "../libsample/sample.h"
#include "../libdebug/debug.h"
#include "../libtimer/timer.h"
#include "../libosmocc/endpoint.h"
#include "../libosmocc/helper.h"
#include "../libg711/g711.h"
#include "cause.h"
#include "sender.h"
#include "call.h"
#include "console.h"

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

static int connect_on_setup;		/* send patterns towards fixed network */
static int release_on_disconnect;	/* release towards mobile phone, if OSMO-CC call disconnects, don't send disconnect tone */

osmo_cc_endpoint_t endpoint, *ep;

void encode_l16(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len)
{
	uint16_t *src = (uint16_t *)src_data, *dst;
	int len = src_len / 2, i;

	dst = malloc(len * 2);
	if (!dst)
		return;
	for (i = 0; i < len; i++)
		dst[i] = htons(src[i]);
	*dst_data = (uint8_t *)dst;
	*dst_len = len * 2;
}

void decode_l16(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len)
{
	uint16_t *src = (uint16_t *)src_data, *dst;
	int len = src_len / 2, i;

	dst = malloc(len * 2);
	if (!dst)
		return;
	for (i = 0; i < len; i++)
		dst[i] = ntohs(src[i]);
	*dst_data = (uint8_t *)dst;
	*dst_len = len * 2;
}

static struct osmo_cc_helper_audio_codecs codecs[] = {
	{ "L16", 8000, 1, encode_l16, decode_l16 },
	{ "PCMA", 8000, 1, g711_encode_alaw, g711_decode_alaw },
	{ "PCMU", 8000, 1, g711_encode_ulaw, g711_decode_ulaw },
	{ NULL, 0, 0, NULL, NULL},
};

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
	PROCESS_SETUP_RO,	/* call from radio to OSMO-CC */
	PROCESS_SETUP_RT,	/* call from OSMO-CC to radio */
	PROCESS_ALERTING_RO,	/* call from radio to OSMO-CC */
	PROCESS_ALERTING_RT,	/* call from OSMO-CC to radio */
	PROCESS_CONNECT,
	PROCESS_DISCONNECT,
};

/* call process */
typedef struct process {
	struct process *next;
	int callref;
	enum process_state state;
	int audio_disconnected; /* if not associated with transceiver anymore */
	enum audio_pattern pattern;
	int audio_pos;
	uint8_t cause;
	struct timer timer;
	osmo_cc_session_t *session;
	osmo_cc_session_codec_t *codec; /* codec to send */
} process_t;

static process_t *process_head = NULL;

static void process_timeout(struct timer *timer);
static void indicate_disconnect_release(int callref, int cause, uint8_t msg_type);

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
			if (process->session)
				osmo_cc_free_session(process->session);
			free(process);
			return;
		}
		process_p = &process->next;
		process = process->next;
	}
	PDEBUG(DCALL, DEBUG_ERROR, "Process with callref %d not found!\n", callref);
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
		PDEBUG(DCALL, DEBUG_ERROR, "Process with callref %d not found!\n", callref);
		return;
	}
	PDEBUG(DCALL, DEBUG_DEBUG, "Changing state for callref %d  %d->%d\n", callref, process->state, state);
	process->state = state;
}

static void set_pattern_process(int callref, enum audio_pattern pattern)
{
	process_t *process = get_process(callref);

	if (!process) {
		PDEBUG(DCALL, DEBUG_ERROR, "Process with callref %d not found!\n", callref);
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
		PDEBUG(DCALL, DEBUG_ERROR, "Process with callref %d not found!\n", callref);
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
			*samples++ = spl[pos] >> 2;
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
		if (process->state == PROCESS_DISCONNECT) {
			PDEBUG(DCALL, DEBUG_INFO, "Call released toward mobile network (after timeout)\n");
			call_down_release(process->callref, process->cause);
		}
		indicate_disconnect_release(process->callref, process->cause, OSMO_CC_MSG_REL_IND);
		destroy_process(process->callref);
	}
}

void down_audio(struct osmo_cc_session_codec *codec, uint16_t __attribute__((unused)) sequence_number, uint32_t __attribute__((unused)) timestamp, uint8_t *data, int len)
{
	process_t *process = codec->media->session->priv;
	sample_t samples[len / 2];

	/* if we are disconnected, ignore audio */
	if (!process || process->pattern != PATTERN_NONE)
		return;
	int16_to_samples(samples, (int16_t *)data, len / 2);
#ifdef DEBUG_LEVEL
	double lev = level_of(samples, len / 2);
	printf("festnetz-level: %s                  %.4f\n", debug_db(lev), (20 * log10(lev)));
#endif
	call_down_audio(process->callref, samples, len / 2);
}

static void indicate_setup(process_t *process, const char *callerid, const char *dialing, uint8_t network_type, const char *network_id)
{
	osmo_cc_msg_t *msg;

	msg = osmo_cc_new_msg(OSMO_CC_MSG_SETUP_IND);
	/* network type */
	if (network_type)
		osmo_cc_add_ie_calling_network(msg, network_type, network_id);
	/* calling number */
	if (callerid && callerid[0])
		osmo_cc_add_ie_calling(msg, OSMO_CC_TYPE_SUBSCRIBER, OSMO_CC_PLAN_TELEPHONY, OSMO_CC_PRESENT_ALLOWED, OSMO_CC_SCREEN_NETWORK, callerid);
	/* called number */
	if (dialing && dialing[0])
		osmo_cc_add_ie_called(msg, OSMO_CC_TYPE_UNKNOWN, OSMO_CC_PLAN_TELEPHONY, dialing);
	/* bearer capability */
	osmo_cc_add_ie_bearer(msg, OSMO_CC_CODING_ITU_T, OSMO_CC_CAPABILITY_AUDIO, OSMO_CC_MODE_CIRCUIT);
	/* sdp offer */
	process->session = osmo_cc_helper_audio_offer(process, codecs, down_audio, msg, 1);

	PDEBUG(DCALL, DEBUG_INFO, "Indicate OSMO-CC setup towards fixed network\n");
	osmo_cc_ll_msg(ep, process->callref, msg);
}

static void indicate_proceeding(int callref, const char *sdp)
{
	osmo_cc_msg_t *msg;

	msg = osmo_cc_new_msg(OSMO_CC_MSG_PROC_IND);

	/* sdp */
	osmo_cc_add_ie_sdp(msg, sdp);

	/* progress information */
	osmo_cc_add_ie_progress(msg, OSMO_CC_CODING_ITU_T, OSMO_CC_LOCATION_BEYOND_INTERWORKING, OSMO_CC_PROGRESS_INBAND_INFO_AVAILABLE);

	PDEBUG(DCALL, DEBUG_INFO, "Indicate OSMO-CC call confirm towards fixed network\n");
	osmo_cc_ll_msg(ep, callref, msg);
}

static void indicate_alerting(int callref)
{
	osmo_cc_msg_t *msg;

	msg = osmo_cc_new_msg(OSMO_CC_MSG_ALERT_IND);

	PDEBUG(DCALL, DEBUG_INFO, "Indicate OSMO-CC alerting towards fixed network\n");
	osmo_cc_ll_msg(ep, callref, msg);
}

static void indicate_answer(int callref, const char *connectid)
{
	osmo_cc_msg_t *msg;

	msg = osmo_cc_new_msg(OSMO_CC_MSG_SETUP_CNF);
	/* calling number */
	if (connectid && connectid[0])
		osmo_cc_add_ie_calling(msg, OSMO_CC_TYPE_SUBSCRIBER, OSMO_CC_PLAN_TELEPHONY, OSMO_CC_PRESENT_ALLOWED, OSMO_CC_SCREEN_NETWORK, connectid);

	PDEBUG(DCALL, DEBUG_INFO, "Indicate OSMO-CC answer towards fixed network\n");
	osmo_cc_ll_msg(ep, callref, msg);
}

static void indicate_answer_ack(int callref)
{
	osmo_cc_msg_t *msg;

	msg = osmo_cc_new_msg(OSMO_CC_MSG_SETUP_COMP_IND);

	PDEBUG(DCALL, DEBUG_INFO, "Indicate OSMO-CC setup complete towards fixed network\n");
	osmo_cc_ll_msg(ep, callref, msg);
}

static void indicate_disconnect_release(int callref, int cause, uint8_t msg_type)
{
	osmo_cc_msg_t *msg;

	msg = osmo_cc_new_msg(msg_type);

	/* cause */
	osmo_cc_add_ie_cause(msg, OSMO_CC_LOCATION_PRIV_SERV_LOC_USER, cause, 0, 0);

	/* progress information */
	if (msg_type == OSMO_CC_MSG_DISC_IND)
		osmo_cc_add_ie_progress(msg, OSMO_CC_CODING_ITU_T, OSMO_CC_LOCATION_BEYOND_INTERWORKING, OSMO_CC_PROGRESS_INBAND_INFO_AVAILABLE);

	PDEBUG(DCALL, DEBUG_INFO, "%s OSMO-CC %s towards fixed network\n", (msg_type == OSMO_CC_MSG_REL_CNF) ? "Confirm" : "Indicated", (msg_type == OSMO_CC_MSG_DISC_IND) ? "disconnect" : "release");
	osmo_cc_ll_msg(ep, callref, msg);
}

/* Setup is received from transceiver. */
int call_up_setup(const char *callerid, const char *dialing, uint8_t network, const char *network_id)
{
	osmo_cc_call_t *call;
	process_t *process;

	PDEBUG(DCALL, DEBUG_INFO, "Incoming call from '%s' to '%s'\n", callerid ? : "unknown", dialing);
	if (!strcmp(dialing, "010"))
		PDEBUG(DCALL, DEBUG_INFO, " -> Call to Operator '%s'\n", dialing);

	call = osmo_cc_call_new(ep);

	process = create_process(call->callref, PROCESS_SETUP_RO);

	indicate_setup(process, callerid, dialing, network, network_id);

	return call->callref;
}

/* Transceiver indicates alerting. */
void call_up_alerting(int callref)
{
	if (!callref) {
		PDEBUG(DCALL, DEBUG_DEBUG, "Ignoring alerting, because callref not set. (not for us)\n");
		return;
	}

	PDEBUG(DCALL, DEBUG_INFO, "Call is alerting\n");

	if (!connect_on_setup)
		indicate_alerting(callref);
	set_pattern_process(callref, PATTERN_RINGBACK);
	new_state_process(callref, PROCESS_ALERTING_RT);
}

/* Transceiver indicates early audio */
void call_up_early(int callref)
{
	set_pattern_process(callref, PATTERN_NONE);
}

/* Transceiver indicates answer. */
void call_up_answer(int callref, const char *connect_id)
{
	if (!callref) {
		PDEBUG(DCALL, DEBUG_DEBUG, "Ignoring answer, because callref not set. (not for us)\n");
		return;
	}

	PDEBUG(DCALL, DEBUG_INFO, "Call has been answered by '%s'\n", connect_id);

	if (!connect_on_setup)
		indicate_answer(callref, connect_id);
	set_pattern_process(callref, PATTERN_NONE);
	new_state_process(callref, PROCESS_CONNECT);
}

/* Transceiver indicates release. */
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
		/* just keep OSMO-CC connection if tones shall be sent.
		 * no tones while setting up / alerting the call. */
		if (connect_on_setup
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
			indicate_disconnect_release(callref, cause, OSMO_CC_MSG_REL_IND);
		/* if no tones shall be sent, disconnect on all other states */
		} else {
			disconnect_process(callref, cause);
			indicate_disconnect_release(callref, cause, OSMO_CC_MSG_DISC_IND);
		}
	} else {
		/* we don't know about the process, just send release to upper layer anyway */
		indicate_disconnect_release(callref, cause, OSMO_CC_MSG_REL_IND);
	}
}

/* turn recall tone on or off */
void call_tone_recall(int callref, int on)
{
	set_pattern_process(callref, (on) ? PATTERN_RECALL : PATTERN_NONE);
}

/* forward audio to OSMO-CC or call instance */
void call_up_audio(int callref, sample_t *samples, int count)
{
	process_t *process;
	int16_t data[count];

	if (count != 160) {
		fprintf(stderr, "Samples must be 160, please fix!\n");
		abort();
	}
	if (!callref)
		return;

	/* if we are disconnected, ignore audio */
	process = get_process(callref);
	if (!process || process->pattern != PATTERN_NONE)
		return;

	/* forward audio */
#ifdef DEBUG_LEVEL
	double lev = level_of(samples, count);
	printf("   mobil-level: %s%.4f\n", debug_db(lev), (20 * log10(lev)));
#endif
	samples_to_int16(data, samples, count);
	osmo_cc_rtp_send(process->codec, (uint8_t *)data, count * 2, 1, count);
	/* don't destroy process here in case of an error */
}

/* clock that is used to transmit patterns */
void call_clock(void)
{
	process_t *process = process_head;

	call_down_clock();

	while(process) {
		if (process->pattern != PATTERN_NONE) {
			int16_t data[160];
			/* try to get patterns, else copy the samples we got */
			get_process_patterns(process, data, 160);
#ifdef DEBUG_LEVEL
			sample_t samples[160];
			int16_to_samples(samples, (int16_t *)data->data, 160);
			double lev = level_of(samples, 160);
			printf("   mobil-level: %s%.4f\n", debug_db(lev), (20 * log10(lev)));
			samples_to_int16(data, samples, 160);
#endif
			osmo_cc_rtp_send(process->codec, (uint8_t *)data, 160 * 2, 1, 160);
			/* don't destroy process here in case of an error */
		}
		process = process->next;
	}
}

/* messages received from fixed network */
void ll_msg_cb(osmo_cc_endpoint_t __attribute__((unused)) *ep, uint32_t callref, osmo_cc_msg_t *msg)
{
	process_t *process;
	uint8_t coding, location, progress, isdn_cause, socket_cause;
	uint16_t sip_cause;
	uint8_t type, plan, present, screen, caller_type;
	char caller_id[33], number[33];
	int rc;

	process = get_process(callref);
	if (!process) {
		if (msg->type == OSMO_CC_MSG_SETUP_REQ)
			process = create_process(callref, PROCESS_SETUP_RT);
		else {
			/* release collisions is not forbidden */
			if (msg->type != OSMO_CC_MSG_REL_REQ)
				PDEBUG(DCALL, DEBUG_ERROR, "No process!\n");
			osmo_cc_free_msg(msg);
			return;
		}
	}

	if (process->audio_disconnected) {
		switch(msg->type) {
		case OSMO_CC_MSG_DISC_REQ:
			rc = osmo_cc_get_ie_cause(msg, 0, &location, &isdn_cause, &sip_cause, &socket_cause);
			if (rc < 0)
				isdn_cause = OSMO_CC_ISDN_CAUSE_NORM_CALL_CLEAR;
			PDEBUG(DCALL, DEBUG_INFO, "Received OSMO-CC disconnect from fixed network with cause %d\n", isdn_cause);
			PDEBUG(DCALL, DEBUG_INFO, "Call disconnected, releasing!\n");
			destroy_process(callref);
			indicate_disconnect_release(callref, isdn_cause, OSMO_CC_MSG_REL_IND);
		break;
		case OSMO_CC_MSG_REL_REQ:
			rc = osmo_cc_get_ie_cause(msg, 0, &location, &isdn_cause, &sip_cause, &socket_cause);
			if (rc < 0)
				isdn_cause = OSMO_CC_ISDN_CAUSE_NORM_CALL_CLEAR;
			PDEBUG(DCALL, DEBUG_INFO, "Received OSMO-CC release from fixed network with cause %d\n", isdn_cause);
			PDEBUG(DCALL, DEBUG_INFO, "Call released\n");
			destroy_process(callref);
			indicate_disconnect_release(callref, isdn_cause, OSMO_CC_MSG_REL_CNF);
			break;
		}
		osmo_cc_free_msg(msg);
		return;
	}

	switch(msg->type) {
	case OSMO_CC_MSG_SETUP_REQ:
	    {
		const char *sdp;

		/* sdp accept */
		sdp = osmo_cc_helper_audio_accept(process, codecs, down_audio, msg, &process->session, &process->codec, 0);
		if (!sdp) {
			disconnect_process(callref, 47);
			break;
		}

		/* caller id */
		rc = osmo_cc_get_ie_calling(msg, 0, &type, &plan, &present, &screen, caller_id, sizeof(caller_id));
		if (rc < 0) {
			caller_type = TYPE_NOTAVAIL;
			caller_id[0] = '\0';
		} else {
			switch (type) {
			case OSMO_CC_TYPE_INTERNATIONAL:
				caller_type = TYPE_INTERNATIONAL;
				break;
			case OSMO_CC_TYPE_NATIONAL:
				caller_type = TYPE_NATIONAL;
				break;
			case OSMO_CC_TYPE_SUBSCRIBER:
				caller_type = TYPE_SUBSCRIBER;
				break;
			default:
				caller_type = TYPE_UNKNOWN;
			}
			if (present == OSMO_CC_PRESENT_RESTRICTED)
				caller_type = TYPE_ANONYMOUS;
		}
		/* dialing */
		rc = osmo_cc_get_ie_called(msg, 0, &type, &plan, number, sizeof(number));
		if (rc < 0)
			number[0] = '\0';
		PDEBUG(DCALL, DEBUG_INFO, "Received OSMO-CC call from fixed network '%s' to mobile '%s'\n", caller_id, number);
		indicate_proceeding(callref, sdp);
		PDEBUG(DCALL, DEBUG_INFO, "Outgoing call from '%s' to '%s'\n", caller_id, number);

		rc = call_down_setup(callref, caller_id, caller_type, number);
		if (rc < 0) {
			PDEBUG(DCALL, DEBUG_NOTICE, "Call rejected, cause %d\n", -rc);
			if (connect_on_setup) {
				PDEBUG(DCALL, DEBUG_DEBUG, "Early connecting after setup\n");
				indicate_answer(callref, number);
			} else {
				PDEBUG(DCALL, DEBUG_INFO, "Disconnecting OSMO-CC call towards fixed network (cause=%d)\n", -rc);
				indicate_disconnect_release(callref, -rc, OSMO_CC_MSG_DISC_IND);
			}
			disconnect_process(callref, -rc);
			break;
		}

		if (connect_on_setup) {
			PDEBUG(DCALL, DEBUG_DEBUG, "Early connecting after setup\n");
			indicate_answer(callref, number);
			break;
		}
		break;
	    }
	case OSMO_CC_MSG_SETUP_ACK_REQ:
		PDEBUG(DCALL, DEBUG_INFO, "Received OSMO-CC overlap from fixed network\n");
		rc = osmo_cc_helper_audio_negotiate(msg, &process->session, &process->codec);
		if (rc < 0) {
			nego_failed:
			PDEBUG(DCALL, DEBUG_INFO, "Releasing, because codec negotiation failed.\n");
			destroy_process(callref);
			indicate_disconnect_release(callref, 47, OSMO_CC_MSG_REL_IND);
			PDEBUG(DCALL, DEBUG_INFO, "Call released toward mobile network\n");
			call_down_release(callref, 47);
			break;
		}
		break;
	case OSMO_CC_MSG_PROC_REQ:
		PDEBUG(DCALL, DEBUG_INFO, "Received OSMO-CC proceeding from fixed network\n");
		rc = osmo_cc_helper_audio_negotiate(msg, &process->session, &process->codec);
		if (rc < 0)
			goto nego_failed;
		break;
	case OSMO_CC_MSG_PROGRESS_REQ:
		PDEBUG(DCALL, DEBUG_INFO, "Received OSMO-CC progress from fixed network\n");
		rc = osmo_cc_helper_audio_negotiate(msg, &process->session, &process->codec);
		if (rc < 0)
			goto nego_failed;
		break;
	case OSMO_CC_MSG_ALERT_REQ:
		PDEBUG(DCALL, DEBUG_INFO, "Received OSMO-CC alerting from fixed network\n");
		rc = osmo_cc_helper_audio_negotiate(msg, &process->session, &process->codec);
		if (rc < 0)
			goto nego_failed;
		new_state_process(callref, PROCESS_ALERTING_RO);
		break;
	case OSMO_CC_MSG_SETUP_RSP:
		PDEBUG(DCALL, DEBUG_INFO, "Received OSMO-CC answer from fixed network\n");
		rc = osmo_cc_helper_audio_negotiate(msg, &process->session, &process->codec);
		if (rc < 0)
			goto nego_failed;
		new_state_process(callref, PROCESS_CONNECT);
		PDEBUG(DCALL, DEBUG_INFO, "Call answered\n");
		call_down_answer(callref);
		indicate_answer_ack(callref);
		break;
	case OSMO_CC_MSG_DISC_REQ:
		rc = osmo_cc_helper_audio_negotiate(msg, &process->session, &process->codec);
		if (rc < 0)
			goto nego_failed;
		rc = osmo_cc_get_ie_cause(msg, 0, &location, &isdn_cause, &sip_cause, &socket_cause);
		if (rc < 0)
			isdn_cause = OSMO_CC_ISDN_CAUSE_NORM_CALL_CLEAR;
		rc = osmo_cc_get_ie_progress(msg, 0, &coding, &location, &progress);
		if (rc < 0)
			progress = 0;
		PDEBUG(DCALL, DEBUG_INFO, "Received OSMO-CC disconnect from fixed network with cause %d\n", isdn_cause);
		if (release_on_disconnect || (progress != 1 && progress != 8)) {
			PDEBUG(DCALL, DEBUG_INFO, "Releasing, because we don't send disconnect tones to mobile phone\n");
			destroy_process(callref);
			indicate_disconnect_release(callref, isdn_cause, OSMO_CC_MSG_REL_IND);
			PDEBUG(DCALL, DEBUG_INFO, "Call released toward mobile network\n");
			call_down_release(callref, isdn_cause);
			break;
		}
		new_state_process(callref, PROCESS_DISCONNECT);
		PDEBUG(DCALL, DEBUG_INFO, "Call disconnected\n");
		call_down_disconnect(callref, isdn_cause);
		/* we might get released during disconnect handling!!! */
		process = get_process(callref);
		if (process && process->state == PROCESS_DISCONNECT)
			timer_start(&process->timer, DISC_TIMEOUT);
		break;
	case OSMO_CC_MSG_REJ_REQ:
	case OSMO_CC_MSG_REL_REQ:
		rc = osmo_cc_get_ie_cause(msg, 0, &location, &isdn_cause, &sip_cause, &socket_cause);
		if (rc < 0)
			isdn_cause = OSMO_CC_ISDN_CAUSE_NORM_CALL_CLEAR;
		destroy_process(callref);
		if (msg->type == OSMO_CC_MSG_REL_REQ) {
			PDEBUG(DCALL, DEBUG_INFO, "Received OSMO-CC release from fixed network with cause %d\n", isdn_cause);
			indicate_disconnect_release(callref, isdn_cause, OSMO_CC_MSG_REL_CNF);
		} else
			PDEBUG(DCALL, DEBUG_INFO, "Received OSMO-CC reject from fixed network with cause %d\n", isdn_cause);
		PDEBUG(DCALL, DEBUG_INFO, "Call released toward mobile network\n");
		call_down_release(callref, isdn_cause);
		break;
	}
	osmo_cc_free_msg(msg);
}

int call_init(const char *name, int _send_patterns, int _release_on_disconnect, int use_socket, int argc, const char *argv[])
{
	int rc;

	connect_on_setup = _send_patterns;
	release_on_disconnect = _release_on_disconnect;

	g711_init();

	ep = &endpoint;
	rc = osmo_cc_new(ep, OSMO_CC_VERSION, name, OSMO_CC_LOCATION_PRIV_SERV_LOC_USER, ll_msg_cb, (use_socket) ? NULL : console_msg, NULL, argc, argv);
	if (rc > 0)
		return -EINVAL;
	if (rc < 0)
		return rc;

	return 0;
}

void call_exit(void)
{
	if (ep) {
		osmo_cc_delete(ep);
		ep = NULL;
	}
}

int call_handle(void)
{
	return osmo_cc_handle();
}

void call_media_handle(void)
{
	process_t *process = process_head;

	while(process) {
		if (process->session)
			osmo_cc_session_handle(process->session);
		process = process->next;
	}
}

