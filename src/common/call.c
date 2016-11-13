/* built-in call control
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
#include "debug.h"
#include "sender.h"
#include "cause.h"
#include "call.h"
#include "timer.h"
#include "mncc_sock.h"
#include "testton.h"

#define DISC_TIMEOUT	30

extern int use_mncc_sock;
extern int send_patterns;
extern int release_on_disconnect;

/* stream patterns/announcements */
int16_t *test_spl = NULL;
int16_t *ringback_spl = NULL;
int16_t *hangup_spl = NULL;
int16_t *busy_spl = NULL;
int16_t *noanswer_spl = NULL;
int16_t *outoforder_spl = NULL;
int16_t *invalidnumber_spl = NULL;
int16_t *congestion_spl = NULL;
int test_size = 0;
int ringback_size = 0;
int hangup_size = 0;
int busy_size = 0;
int noanswer_size = 0;
int outoforder_size = 0;
int invalidnumber_size = 0;
int congestion_size = 0;
int test_max = 0;
int ringback_max = 0;
int hangup_max = 0;
int busy_max = 0;
int noanswer_max = 0;
int outoforder_max = 0;
int invalidnumber_max = 0;
int congestion_max = 0;

enum call_state {
	CALL_IDLE = 0,
	CALL_SETUP_MO,
	CALL_SETUP_MT,
	CALL_ALERTING,
	CALL_CONNECT,
	CALL_DISCONNECTED,
};

static const char *call_state_name[] = {
	"IDLE",
	"SETUP_MO",
	"SETUP_MT",
	"ALERTING",
	"CONNECT",
	"DISCONNECTED",
};

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
};

void get_pattern(const int16_t **spl, int *size, int *max, enum audio_pattern pattern)
{
	*spl = NULL;
	*size = 0;
	*max = 0;

	switch (pattern) {
	case PATTERN_TEST:
		*spl = test_spl;
		*size = test_size;
		*max = test_max;
		break;
	case PATTERN_RINGBACK:
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
	default:
		;
	}
}

static int new_callref = 0; /* toward mobile */

/* built in call instance */
typedef struct call {
	int callref;
	enum call_state state;
	int disc_cause;		/* cause that has been sent by transceiver instance for release */
	char station_id[16];
	char dialing[16];
	void *sound;		/* headphone interface */
	int latspl;		/* sample latency at sound interface */
	samplerate_t srstate;	/* patterns/announcement upsampling */
	jitter_t audio;		/* headphone audio dejittering */
	int audio_pos;		/* position when playing patterns */
	int test_audio_pos;	/* position for test tone toward mobile */
	int dial_digits;	/* number of digits to be dialed */
	int loopback;		/* loopback test for echo */
} call_t;

static call_t call;

static void call_new_state(enum call_state state)
{
	PDEBUG(DCALL, DEBUG_DEBUG, "Call state '%s' -> '%s'\n", call_state_name[call.state], call_state_name[state]);
	call.state = state;
	call.audio_pos = 0;
	call.test_audio_pos = 0;
}

static void get_call_patterns(int16_t *samples, int length, enum audio_pattern pattern)
{
	const int16_t *spl;
	int size, max, pos;

	get_pattern(&spl, &size, &max, pattern);

	/* stream sample */
	pos = call.audio_pos;
	while(length--) {
		if (pos >= size)
			*samples++ = 0;
		else
			*samples++ = spl[pos] >> 1;
		if (++pos == max)
			pos = 0;
	}
	call.audio_pos = pos;
}

static void get_test_patterns(int16_t *samples, int length)
{
	const int16_t *spl;
	int size, max, pos;

	get_pattern(&spl, &size, &max, PATTERN_TEST);

	/* stream sample */
	pos = call.test_audio_pos;
	while(length--) {
		if (pos >= size)
			*samples++ = 0;
		else
			*samples++ = spl[pos] >> 1;
		if (++pos == max)
			pos = 0;
	}
	call.test_audio_pos = pos;
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

/* MNCC call instance */
typedef struct process {
	struct process *next;
	int callref;
	enum call_state state;
	int audio_disconnected; /* if not associated with transceiver anymore */
	enum audio_pattern pattern;
	int audio_pos;
	uint8_t cause;
	struct timer timer;
} process_t;

static process_t *process_head = NULL;

static void process_timeout(struct timer *timer);

static void create_process(int callref, int state)
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

static int is_process(int callref)
{
	process_t *process = process_head;

	while (process) {
		if (process->callref == callref)
			return 1;
		process = process->next;
	}
	return 0;
}

static enum call_state is_process_state(int callref)
{
	process_t *process = process_head;

	while (process) {
		if (process->callref == callref)
			return process->state;
		process = process->next;
	}
	return CALL_IDLE;
}

static void set_state_process(int callref, enum call_state state)
{
	process_t *process = process_head;

	while (process) {
		if (process->callref == callref) {
			process->state = state;
			return;
		}
		process = process->next;
	}
	PDEBUG(DCALL, DEBUG_ERROR, "Process with callref 0x%x not found!\n", callref);
}

static void set_pattern_process(int callref, enum audio_pattern pattern)
{
	process_t *process = process_head;

	while (process) {
		if (process->callref == callref) {
			process->pattern = pattern;
			process->audio_pos = 0;
			return;
		}
		process = process->next;
	}
	PDEBUG(DCALL, DEBUG_ERROR, "Process with callref 0x%x not found!\n", callref);
}

/* disconnect audio, now send audio directly from pattern/announcement, not from transceiver */
static void disconnect_process(int callref, int cause)
{
	process_t *process = process_head;

	while (process) {
		if (process->callref == callref) {
			process->pattern = cause2pattern(cause);
			process->audio_disconnected = 1;
			process->audio_pos = 0;
			process->cause = cause;
			timer_start(&process->timer, DISC_TIMEOUT);
			return;
		}
		process = process->next;
	}
	PDEBUG(DCALL, DEBUG_ERROR, "Process with callref 0x%x not found!\n", callref);
}

/* check if audio is disconnected */
static int is_process_disconnected(int callref)
{
	process_t *process = process_head;

	while (process) {
		if (process->callref == callref) {
			return process->audio_disconnected;
		}
		process = process->next;
	}
	PDEBUG(DCALL, DEBUG_DEBUG, "Process with callref 0x%x not found, this is ok!\n", callref);

	return 0;
}

/* check if pattern is set, so we send patterns and announcements */
static int is_process_pattern(int callref)
{
	process_t *process = process_head;

	while (process) {
		if (process->callref == callref) {
			return (process->pattern != PATTERN_NONE);
		}
		process = process->next;
	}
	PDEBUG(DCALL, DEBUG_DEBUG, "Process with callref 0x%x not found, this is ok!\n", callref);

	return 0;
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
		mncc->cause.location = 1; /* private local */
		mncc->cause.value = process->cause;

		destroy_process(process->callref);
		PDEBUG(DMNCC, DEBUG_INFO, "Releasing MNCC call towards Network (after timeout)\n");
		mncc_write(buf, sizeof(struct gsm_mncc));
	}
}

int call_init(const char *station_id, const char *sounddev, int samplerate, int latency, int dial_digits, int loopback)
{
	int rc = 0;

	init_testton();

	if (use_mncc_sock)
		return 0;

	memset(&call, 0, sizeof(call));
	strncpy(call.station_id, station_id, sizeof(call.station_id) - 1);
	call.latspl = latency * samplerate / 1000;
	call.dial_digits = dial_digits;
	call.loopback = loopback;

	if (!sounddev[0])
		return 0;

	/* open sound device for call control */
	call.sound = sound_open(sounddev, samplerate);
	if (!call.sound) {
		PDEBUG(DSENDER, DEBUG_ERROR, "No sound device!\n");

		rc = -EIO;
		goto error;
	}

	rc = init_samplerate(&call.srstate, samplerate);
	if (rc < 0) {
		PDEBUG(DSENDER, DEBUG_ERROR, "Failed to init sample rate conversion!\n");
		goto error;
	}

	rc = jitter_create(&call.audio, samplerate / 5);
	if (rc < 0) {
		PDEBUG(DSENDER, DEBUG_ERROR, "Failed to create and init audio buffer!\n");
		goto error;
	}

	return 0;

error:
	call_cleanup();
	return rc;
}

void call_cleanup(void)
{
	if (use_mncc_sock)
		return;

	/* close sound devoice */
	if (call.sound)
		sound_close(call.sound);

	jitter_destroy(&call.audio);

	if (process_head) {
		PDEBUG(DMNCC, DEBUG_ERROR, "Not all MNCC instances have been released!\n");
	}
}

static char console_text[256];
static char console_clear[256];
static int console_len = 0;

static void process_ui(int c)
{
	switch (call.state) {
	case CALL_IDLE:
		if (c > 0) {
			if (c >= '0' && c <= '9' && (int)strlen(call.station_id) < call.dial_digits) {
				call.station_id[strlen(call.station_id) + 1] = '\0';
				call.station_id[strlen(call.station_id)] = c;
			}
			if ((c == 8 || c == 127) && strlen(call.station_id))
				call.station_id[strlen(call.station_id) - 1] = '\0';
dial_after_hangup:
			if (c == 'd' && (int)strlen(call.station_id) == call.dial_digits) {
				int rc;
				int callref = ++new_callref;

				PDEBUG(DCALL, DEBUG_INFO, "Outgoing call to %s\n", call.station_id);
				call.dialing[0] = '\0';
				call_new_state(CALL_SETUP_MT);
				call.callref = callref;
				rc = call_out_setup(callref, "", TYPE_NOTAVAIL, call.station_id);
				if (rc < 0) {
					PDEBUG(DCALL, DEBUG_NOTICE, "Call rejected, cause %d\n", -rc);
					call_new_state(CALL_DISCONNECTED);
					call.callref = 0;
					call.disc_cause = -rc;
				}
			}
		}
		sprintf(console_text, "on-hook: %s%s (enter 0..9 or d=dial)\r", call.station_id, "..............." + 15 - call.dial_digits + strlen(call.station_id));
		break;
	case CALL_SETUP_MO:
	case CALL_SETUP_MT:
	case CALL_ALERTING:
	case CALL_CONNECT:
	case CALL_DISCONNECTED:
		if (c > 0) {
			if (c == 'h' || c == 'd') {
				PDEBUG(DCALL, DEBUG_INFO, "Call hangup\n");
				call_new_state(CALL_IDLE);
				if (call.callref) {
					call_out_release(call.callref, CAUSE_NORMAL);
					call.callref = 0;
				}
				if (c == 'd')
					goto dial_after_hangup;
			}
		}
		if (call.state == CALL_SETUP_MT)
			sprintf(console_text, "call setup: %s (enter h=hangup)\r", call.station_id);
		if (call.state == CALL_ALERTING)
			sprintf(console_text, "call ringing: %s (enter h=hangup)\r", call.station_id);
		if (call.state == CALL_CONNECT) {
			if (call.dialing[0])
				sprintf(console_text, "call active: %s->%s (enter h=hangup)\r", call.station_id, call.dialing);
			else
				sprintf(console_text, "call active: %s (enter h=hangup)\r", call.station_id);
		}
		if (call.state == CALL_DISCONNECTED)
			sprintf(console_text, "call disconnected: %s (enter h=hangup)\r", cause_name(call.disc_cause));
		break;
	}
	console_len = strlen(console_text);
	memset(console_clear, ' ', console_len - 1);
	console_clear[console_len - 1] = '\r';
	fwrite(console_text, console_len, 1, stdout);
	fflush(stdout);
}

void clear_console_text(void)
{
	if (!console_len)
		return;

	fwrite(console_clear, console_len, 1, stdout);
	// note: fflused by user of this function
}


/* get keys from keyboad to control call via console
 * returns 1 on exit (ctrl+c) */
void process_call(int c)
{
	if (use_mncc_sock) {
		mncc_handle();
		return;
	}

	if (!call.loopback)
		process_ui(c);

	if (!call.sound)
		return;

	/* handle audio, if sound device is used */

	int16_t samples[call.latspl];
	int count;
	int rc;

	count = sound_get_inbuffer(call.sound);
	if (count < 0) {
		PDEBUG(DSENDER, DEBUG_ERROR, "Failed to get samples in buffer (rc = %d)!\n", count);
		if (count == -EPIPE)
			PDEBUG(DSENDER, DEBUG_ERROR, "Trying to recover.\n");
		return;
	}
	if (count < call.latspl) {
		int16_t up[count + 10];
		count = call.latspl - count;
		switch(call.state) {
		case CALL_ALERTING:
			count = (int)((double)count / call.srstate.factor + 0.5);
			get_call_patterns(samples, count, PATTERN_RINGBACK);
			count = samplerate_upsample(&call.srstate, samples, count, up);
			/* prevent click after hangup */
			jitter_clear(&call.audio);
			break;
		case CALL_DISCONNECTED:
			count = (int)((double)count / call.srstate.factor + 0.5);
			get_call_patterns(samples, count, cause2pattern(call.disc_cause));
			count = samplerate_upsample(&call.srstate, samples, count, up);
			/* prevent click after hangup */
			jitter_clear(&call.audio);
			break;
		default:
			jitter_load(&call.audio, up, count);
		}
		rc = sound_write(call.sound, up, up, count);
		if (rc < 0) {
			PDEBUG(DSENDER, DEBUG_ERROR, "Failed to write TX data to sound device (rc = %d)\n", rc);
			if (rc == -EPIPE)
				PDEBUG(DSENDER, DEBUG_ERROR, "Trying to recover.\n");
			return;
		}
	}
	count = sound_read(call.sound, samples, samples, call.latspl);
	if (count < 0) {
		PDEBUG(DSENDER, DEBUG_ERROR, "Failed to read from sound device (rc = %d)!\n", count);
		if (count == -EPIPE)
			PDEBUG(DSENDER, DEBUG_ERROR, "Trying to recover.\n");
		return;
	}
	if (count) {
		int16_t down[count]; /* more than enough */

		if (call.loopback == 3)
			jitter_save(&call.audio, samples, count);
		count = samplerate_downsample(&call.srstate, samples, count, down);
		call_rx_audio(call.callref, down, count);
	}
}

/* Setup is received from transceiver. */
int call_in_setup(int callref, const char *callerid, const char *dialing)
{
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

	if (use_mncc_sock) {
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

		PDEBUG(DMNCC, DEBUG_INFO, "Sending MNCC call towards Network\n");

		create_process(callref, CALL_SETUP_MO);

		rc = mncc_write(buf, sizeof(struct gsm_mncc));
		if (rc < 0) {
			PDEBUG(DCALL, DEBUG_NOTICE, "We have no MNCC connection, rejecting.\n");
			destroy_process(callref);
			return -CAUSE_TEMPFAIL;
		}
		return 0;
	}

	/* setup is also allowed on disconnected call */
	if (call.state == CALL_DISCONNECTED)
		call_new_state(CALL_IDLE);
	if (call.state != CALL_IDLE) {
		PDEBUG(DCALL, DEBUG_NOTICE, "We are busy, rejecting.\n");
		return -CAUSE_BUSY;
	}
	call.callref = callref;
	call_new_state(CALL_CONNECT);
	if (callerid) {
		strncpy(call.station_id, callerid, call.dial_digits);
		call.station_id[call.dial_digits] = '\0';
	}
	strncpy(call.dialing, dialing, sizeof(call.dialing) - 1);
	call.dialing[sizeof(call.dialing) - 1] = '\0';

	return 0;
}

/* Transceiver indicates alerting. */
void call_in_alerting(int callref)
{
	if (!callref) {
		PDEBUG(DCALL, DEBUG_DEBUG, "Ignoring alerting, because callref not set. (not for us)\n");
		return;
	}

	PDEBUG(DCALL, DEBUG_INFO, "Call is alerting\n");

	if (use_mncc_sock) {
		uint8_t buf[sizeof(struct gsm_mncc)];
		struct gsm_mncc *mncc = (struct gsm_mncc *)buf;

		if (!send_patterns) {
			memset(buf, 0, sizeof(buf));
			mncc->msg_type = MNCC_ALERT_IND;
			mncc->callref = callref;
			PDEBUG(DMNCC, DEBUG_INFO, "Indicate MNCC alerting towards Network\n");
			mncc_write(buf, sizeof(struct gsm_mncc));
		} else
			set_pattern_process(callref, PATTERN_RINGBACK);
		return;
	}

	if (call.callref != callref) {
		PDEBUG(DCALL, DEBUG_ERROR, "invalid call ref.\n");
		call_out_release(callref, CAUSE_INVALCALLREF);
		return;
	}
	call_new_state(CALL_ALERTING);
}

/* Transceiver indicates answer. */
static void _indicate_answer(int callref, const char *connect_id)
{
	uint8_t buf[sizeof(struct gsm_mncc)];
	struct gsm_mncc *mncc = (struct gsm_mncc *)buf;

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


	PDEBUG(DMNCC, DEBUG_INFO, "Indicate MNCC answer towards Network\n");
	mncc_write(buf, sizeof(struct gsm_mncc));
}
void call_in_answer(int callref, const char *connect_id)
{
	if (!callref) {
		PDEBUG(DCALL, DEBUG_DEBUG, "Ignoring answer, because callref not set. (not for us)\n");
		return;
	}

	PDEBUG(DCALL, DEBUG_INFO, "Call has been answered by '%s'\n", connect_id);

	if (use_mncc_sock) {
		_indicate_answer(callref, connect_id);
		set_pattern_process(callref, PATTERN_NONE);
		set_state_process(callref, CALL_CONNECT);
		return;
	}

	if (call.callref != callref) {
		PDEBUG(DCALL, DEBUG_ERROR, "invalid call ref.\n");
		call_out_release(callref, CAUSE_INVALCALLREF);
		return;
	}
	call_new_state(CALL_CONNECT);
	strncpy(call.station_id, connect_id, call.dial_digits);
	call.station_id[call.dial_digits] = '\0';
}

/* Transceiver indicates release. */
void call_in_release(int callref, int cause)
{
	if (!callref) {
		PDEBUG(DCALL, DEBUG_DEBUG, "Ignoring release, because callref not set. (not for us)\n");
		return;
	}

	PDEBUG(DCALL, DEBUG_INFO, "Call has been released with cause=%d\n", cause);

	if (use_mncc_sock) {
		uint8_t buf[sizeof(struct gsm_mncc)];
		struct gsm_mncc *mncc = (struct gsm_mncc *)buf;

		memset(buf, 0, sizeof(buf));
		mncc->msg_type = MNCC_REL_IND;
		mncc->callref = callref;
		mncc->fields |= MNCC_F_CAUSE;
		mncc->cause.location = 1; /* private local */
		mncc->cause.value = cause;

		if (is_process(callref)) {
			if (!send_patterns
			 || is_process_state(callref) == CALL_DISCONNECTED
			 || is_process_state(callref) == CALL_SETUP_MO) {
				PDEBUG(DMNCC, DEBUG_INFO, "Releasing MNCC call towards Network\n");
				destroy_process(callref);
				mncc_write(buf, sizeof(struct gsm_mncc));
			} else {
				disconnect_process(callref, cause);
			}
		} else {
			PDEBUG(DMNCC, DEBUG_INFO, "Releasing MNCC call towards Network\n");
			mncc_write(buf, sizeof(struct gsm_mncc));
		}
		return;
	}

	if (call.callref != callref) {
		PDEBUG(DCALL, DEBUG_ERROR, "invalid call ref.\n");
		/* don't send release, because caller already released */
		return;
	}
	call_new_state(CALL_DISCONNECTED);
	call.callref = 0;
	call.disc_cause = cause;
}

/* forward audio to MNCC or call instance */
void call_tx_audio(int callref, int16_t *samples, int count)
{
	if (!callref)
		return;

	if (use_mncc_sock) {
		uint8_t buf[sizeof(struct gsm_data_frame) + count * sizeof(int16_t)];
		struct gsm_data_frame *data = (struct gsm_data_frame *)buf;

		/* if we are disconnected, ignore audio */
		if (is_process_pattern(callref))
			return;

		/* forward audio */
		data->msg_type = ANALOG_8000HZ;
		data->callref = callref;
		memcpy(data->data, samples, count * sizeof(int16_t));

		mncc_write(buf, sizeof(buf));
		return;
	}

	/* save audio from transceiver to jitter buffer */
	if (call.sound) {
		int16_t up[(int)((double)count * call.srstate.factor + 0.5) + 10];
		count = samplerate_upsample(&call.srstate, samples, count, up);
		jitter_save(&call.audio, up, count);
	} else
	/* else, if no sound is used, send test tone to mobile */
	if (call.state == CALL_CONNECT) {
		get_test_patterns(samples, count);
		call_rx_audio(callref, samples, count);
	}
}

/* clock that is used to transmit patterns */
void call_mncc_clock(void)
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
			mncc_write(buf, sizeof(buf));
		}
		process = process->next;
	}
}

/* mncc messages received from network */
void call_mncc_recv(uint8_t *buf, int length)
{
	struct gsm_mncc *mncc = (struct gsm_mncc *)buf;
	char number[sizeof(mncc->called.number)];
	char caller_id[sizeof(mncc->calling.number)];
	enum number_type caller_type;
	int callref;
	int rc;

	if (mncc->msg_type == ANALOG_8000HZ) {
		struct gsm_data_frame *data = (struct gsm_data_frame *)buf;
		int count = (length - sizeof(struct gsm_data_frame)) / 2;
		/* if we are disconnected, ignore audio */
		if (is_process_pattern(data->callref))
			return;
		call_rx_audio(data->callref, (int16_t *)data->data, count);
		return;
	}

	callref = mncc->callref;

	if (is_process_disconnected(callref)) {
		switch(mncc->msg_type) {
		case MNCC_DISC_REQ:
			PDEBUG(DMNCC, DEBUG_INFO, "Received MNCC disconnect from Network with cause %d\n", mncc->cause.value);

			PDEBUG(DCALL, DEBUG_INFO, "Call disconnected, releasing!\n");

			destroy_process(callref);

			PDEBUG(DMNCC, DEBUG_INFO, "Releasing MNCC call towards Network\n");
			mncc->msg_type = MNCC_REL_IND;
			mncc_write(buf, sizeof(struct gsm_mncc));
		break;
		case MNCC_REL_REQ:
			PDEBUG(DMNCC, DEBUG_INFO, "Received MNCC release from Network with cause %d\n", mncc->cause.value);

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

		PDEBUG(DMNCC, DEBUG_INFO, "Received MNCC call from Network to '%s'\n", caller_id);

		if (mncc->callref >= 0x4000000) {
			fprintf(stderr, "Invalid callref from network, please fix!\n");
			abort();
		}

		PDEBUG(DMNCC, DEBUG_INFO, "Confirming MNCC call to Network\n");
		memset(buf, 0, length);
		mncc->msg_type = MNCC_CALL_CONF_IND;
		mncc->callref = callref;
		mncc->lchan_type = GSM_LCHAN_TCH_F;
		mncc->fields |= MNCC_F_BEARER_CAP;
		mncc->bearer_cap.speech_ver[0] = BCAP_ANALOG_8000HZ;
		mncc->bearer_cap.speech_ver[1] = -1;

		mncc_write(buf, sizeof(struct gsm_mncc));

		PDEBUG(DCALL, DEBUG_INFO, "Outgoing call from '%s' to '%s'\n", caller_id, number);

		create_process(callref, CALL_SETUP_MT);

		rc = call_out_setup(callref, caller_id, caller_type, number);
		if (rc < 0) {
			PDEBUG(DCALL, DEBUG_NOTICE, "Call rejected, cause %d\n", -rc);
			if (send_patterns) {
				PDEBUG(DCALL, DEBUG_DEBUG, "Early connecting after setup\n");
				_indicate_answer(callref, number);
				disconnect_process(callref, -rc);
				break;
			}
			PDEBUG(DMNCC, DEBUG_INFO, "Rejecting MNCC call towards Network (cause=%d)\n", -rc);
			memset(buf, 0, length);
			mncc->msg_type = MNCC_REL_IND;
			mncc->callref = callref;
			mncc->fields |= MNCC_F_CAUSE;
			mncc->cause.location = 1; /* private local */
			mncc->cause.value = -rc;
			mncc_write(buf, sizeof(struct gsm_mncc));
			destroy_process(callref);
			break;
		}

		if (send_patterns) {
			PDEBUG(DCALL, DEBUG_DEBUG, "Early connecting after setup\n");
			_indicate_answer(callref, number);
			break;
		}
		break;
	case MNCC_SETUP_RSP:
		PDEBUG(DMNCC, DEBUG_INFO, "Received MNCC answer from Network\n");
		set_state_process(callref, CALL_CONNECT);
		break;
	case MNCC_DISC_REQ:
		PDEBUG(DMNCC, DEBUG_INFO, "Received MNCC disconnect from Network with cause %d\n", mncc->cause.value);

		if (is_process_state(callref) == CALL_CONNECT && release_on_disconnect) {
			PDEBUG(DCALL, DEBUG_INFO, "Releaseing, because we don't send disconnect tones to mobile phone\n");

			PDEBUG(DMNCC, DEBUG_INFO, "Releasing MNCC call towards Network\n");
			mncc->msg_type = MNCC_REL_IND;
			mncc_write(buf, sizeof(struct gsm_mncc));
			goto release;
		}
		set_state_process(callref, CALL_DISCONNECTED);
		PDEBUG(DCALL, DEBUG_INFO, "Call disconnected\n");
		call_out_disconnect(callref, mncc->cause.value);
		break;
	case MNCC_REL_REQ:
		PDEBUG(DMNCC, DEBUG_INFO, "Received MNCC release from Network with cause %d\n", mncc->cause.value);

release:
		destroy_process(callref);
		PDEBUG(DCALL, DEBUG_INFO, "Call released\n");
		call_out_release(callref, mncc->cause.value);
		break;
	}
}

/* break down of MNCC socket */
void call_mncc_flush(void)
{
	while(process_head) {
		PDEBUG(DMNCC, DEBUG_NOTICE, "MNCC socket closed, releasing call\n");
		call_out_release(process_head->callref, CAUSE_TEMPFAIL);
		destroy_process(process_head->callref);
		/* note: callref is released by sender's instance */
	}
}

