/* built-in console to talk to a phone
 *
 * (C) 2017 by Andreas Eversberg <jolly@eversberg.eu>
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
 G* along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include "../libsample/sample.h"
#include "../libsamplerate/samplerate.h"
#include "../libjitter/jitter.h"
#include "../libdebug/debug.h"
#include "../libtimer/timer.h"
#include "../libosmocc/endpoint.h"
#include "../libosmocc/helper.h"
#include "testton.h"
#include "console.h"
#include "cause.h"
#include "../libmobile/call.h"
#ifdef HAVE_ALSA
#include "../libsound/sound.h"
#endif

enum console_state {
	CONSOLE_IDLE = 0,	/* IDLE */
	CONSOLE_SETUP_RO,	/* call from radio to console */
	CONSOLE_SETUP_RT,	/* call from console to radio */
	CONSOLE_ALERTING_RO,	/* call from radio to console */
	CONSOLE_ALERTING_RT,	/* call from console to radio */
	CONSOLE_CONNECT,
	CONSOLE_DISCONNECT_RO,
};

static const char *console_state_name[] = {
	"IDLE",
	"SETUP_RO",
	"SETUP_RT",
	"ALERTING_RO",
	"ALERTING_RT",
	"CONNECT",
	"DISCONNECT_RO",
};

/* console call instance */
typedef struct console {
	osmo_cc_session_t *session;
	osmo_cc_session_codec_t *codec;
	uint32_t callref;
	enum console_state state;
	int disc_cause;		/* cause that has been sent by transceiver instance for release */
	char station_id[33];
	char dialing[33];
	char audiodev[64];	/* headphone interface, if used */
	int samplerate;		/* sample rate of headphone interface */
	void *sound;		/* headphone interface */
	int buffer_size;	/* sample buffer size at headphone interface */
	samplerate_t srstate;	/* patterns/announcement upsampling */
	jitter_t dejitter;	/* headphone audio dejittering */
	int test_audio_pos;	/* position for test tone toward mobile */
	sample_t tx_buffer[160];/* transmit audio buffer */
	int tx_buffer_pos;	/* current position in transmit audio buffer */
	int num_digits;		/* number of digits to be dialed */
	int loopback;		/* loopback test for echo */
	int echo_test;		/* send echo back to mobile phone */
	const char *digits;	/* list of dialable digits */
} console_t;

static console_t console;

extern osmo_cc_endpoint_t *ep;

void encode_l16(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len);
void decode_l16(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len);

static struct osmo_cc_helper_audio_codecs codecs[] = {
	{ "L16", 8000, 1, encode_l16, decode_l16 },
	{ NULL, 0, 0, NULL, NULL},
};

/* stream test music */
int16_t *test_spl = NULL;
int test_size = 0;
int test_max = 0;

static void get_test_patterns(int16_t *samples, int length)
{
	const int16_t *spl;
	int size, max, pos;

	spl = test_spl;
	size = test_size;
	max = test_max;

	/* stream sample */
	pos = console.test_audio_pos;
	while(length--) {
		if (pos >= size)
			*samples++ = 0;
		else
			*samples++ = spl[pos] >> 2;
		if (++pos == max)
			pos = 0;
	}
	console.test_audio_pos = pos;
}

static void console_new_state(enum console_state state)
{
	PDEBUG(DCC, DEBUG_DEBUG, "Call state '%s' -> '%s'\n", console_state_name[console.state], console_state_name[state]);
	console.state = state;
	console.test_audio_pos = 0;
}

static void free_console(void)
{
	if (console.session) {
		osmo_cc_free_session(console.session);
		console.session = NULL;
	}
	console.codec = NULL;
	console.callref = 0;
}

void up_audio(struct osmo_cc_session_codec *codec, uint16_t __attribute__((unused)) sequence_number, uint32_t __attribute__((unused)) timestamp, uint8_t *data, int len)
{
	int count = len / 2;
	sample_t samples[count];

	/* save audio from transceiver to jitter buffer */
	if (console.sound) {
		sample_t up[(int)((double)count * console.srstate.factor + 0.5) + 10];
		int16_to_samples(samples, (int16_t *)data, count);
		count = samplerate_upsample(&console.srstate, samples, count, up);
		jitter_save(&console.dejitter, up, count);
		return;
	}
	/* if echo test is used, send echo back to mobile */
	if (console.echo_test) {
		osmo_cc_rtp_send(codec, (uint8_t *)data, count * 2, 1, count);
		return;
	}
	/* if no sound is used, send test tone to mobile */
	if (console.state == CONSOLE_CONNECT) {
		get_test_patterns((int16_t *)data, count);
		osmo_cc_rtp_send(codec, (uint8_t *)data, count * 2, 1, count);
		return;
	}
}

static void request_setup(int callref, const char *dialing)
{
	osmo_cc_msg_t *msg;

	msg = osmo_cc_new_msg(OSMO_CC_MSG_SETUP_REQ);
	/* called number */
	if (dialing)
		osmo_cc_add_ie_called(msg, OSMO_CC_TYPE_UNKNOWN, OSMO_CC_PLAN_TELEPHONY, dialing);
	/* bearer capability */
	osmo_cc_add_ie_bearer(msg, OSMO_CC_CODING_ITU_T, OSMO_CC_CAPABILITY_AUDIO, OSMO_CC_MODE_CIRCUIT);
	/* sdp offer */
	console.session = osmo_cc_helper_audio_offer(&ep->session_config, NULL, codecs, up_audio, msg, 1);

	osmo_cc_ul_msg(ep, callref, msg);
}

static void request_answer(int callref, const char *connectid, const char *sdp)
{
	osmo_cc_msg_t *msg;

	msg = osmo_cc_new_msg(OSMO_CC_MSG_SETUP_RSP);
	/* calling number */
	if (connectid)
		osmo_cc_add_ie_calling(msg, OSMO_CC_TYPE_SUBSCRIBER, OSMO_CC_PLAN_TELEPHONY, OSMO_CC_PRESENT_ALLOWED, OSMO_CC_SCREEN_NETWORK, connectid);
	/* SDP */
	if (sdp)
		osmo_cc_add_ie_sdp(msg, sdp);

	osmo_cc_ul_msg(ep, callref, msg);
}

static void request_answer_ack(int callref)
{
	osmo_cc_msg_t *msg;

	msg = osmo_cc_new_msg(OSMO_CC_MSG_SETUP_COMP_REQ);

	osmo_cc_ul_msg(ep, callref, msg);
}

static void request_disconnect_release_reject(int callref, int cause, uint8_t msg_type)
{
	osmo_cc_msg_t *msg;

	msg = osmo_cc_new_msg(msg_type);
	osmo_cc_add_ie_cause(msg, OSMO_CC_LOCATION_USER, cause, 0, 0);

	osmo_cc_ul_msg(ep, callref, msg);
}

void console_msg(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	uint8_t location, isdn_cause, socket_cause;
	uint16_t sip_cause;
	uint8_t type, plan, present, screen;
	uint8_t progress, coding;
	char caller_id[33], number[33];
	const char *sdp;
	int rc;

	if (msg->type != OSMO_CC_MSG_SETUP_IND && console.callref != call->callref) {
		PDEBUG(DCC, DEBUG_ERROR, "invalid call ref %u (msg=0x%02x).\n", call->callref, msg->type);
		request_disconnect_release_reject(call->callref, CAUSE_INVALCALLREF, OSMO_CC_MSG_REL_REQ);
		osmo_cc_free_msg(msg);
		return;
	}

	switch(msg->type) {
	case OSMO_CC_MSG_SETUP_IND:
	    {
		/* caller id */
		rc = osmo_cc_get_ie_calling(msg, 0, &type, &plan, &present, &screen, caller_id, sizeof(caller_id));
		if (rc < 0)
			caller_id[0] = '\0';
		/* dialing */
		rc = osmo_cc_get_ie_called(msg, 0, &type, &plan, number, sizeof(number));
		if (rc < 0)
			number[0] = '\0';
		PDEBUG(DCC, DEBUG_INFO, "Incoming call from '%s'\n", caller_id);
		/* setup is also allowed on disconnected call */
		if (console.state == CONSOLE_DISCONNECT_RO) {
			PDEBUG(DCC, DEBUG_INFO, "Releasing pending disconnected call\n");
			if (console.callref) {
				request_disconnect_release_reject(console.callref, CAUSE_NORMAL, OSMO_CC_MSG_REL_REQ);
				free_console();
			}
			console_new_state(CONSOLE_IDLE);
		}
		if (console.state != CONSOLE_IDLE) {
			PDEBUG(DCC, DEBUG_NOTICE, "We are busy, rejecting.\n");
			request_disconnect_release_reject(console.callref, CAUSE_NORMAL, OSMO_CC_MSG_REJ_REQ);
			osmo_cc_free_msg(msg);
			return;
		}
		console.callref = call->callref;
		/* sdp accept */
		sdp = osmo_cc_helper_audio_accept(&ep->session_config, NULL, codecs, up_audio, msg, &console.session, &console.codec, 0);
		if (!sdp) {
			PDEBUG(DCC, DEBUG_NOTICE, "Cannot accept codec, rejecting.\n");
			request_disconnect_release_reject(console.callref, CAUSE_RESOURCE_UNAVAIL, OSMO_CC_MSG_REJ_REQ);
			osmo_cc_free_msg(msg);
			return;
		}
		if (caller_id[0]) {
			strncpy(console.station_id, caller_id, console.num_digits);
			console.station_id[console.num_digits] = '\0';
		}
		strncpy(console.dialing, number, sizeof(console.dialing) - 1);
		console.dialing[sizeof(console.dialing) - 1] = '\0';
		console_new_state(CONSOLE_CONNECT);
		PDEBUG(DCC, DEBUG_INFO, "Call automatically answered\n");
		request_answer(console.callref, number, sdp);
		break;
	    }
	case OSMO_CC_MSG_SETUP_ACK_IND:
	case OSMO_CC_MSG_PROC_IND:
		osmo_cc_helper_audio_negotiate(msg, &console.session, &console.codec);
		break;
	case OSMO_CC_MSG_ALERT_IND:
		PDEBUG(DCC, DEBUG_INFO, "Call alerting\n");
		osmo_cc_helper_audio_negotiate(msg, &console.session, &console.codec);
		console_new_state(CONSOLE_ALERTING_RT);
		break;
	case OSMO_CC_MSG_SETUP_CNF:
	    {
		/* connected id */
		rc = osmo_cc_get_ie_calling(msg, 0, &type, &plan, &present, &screen, caller_id, sizeof(caller_id));
		if (rc < 0)
			caller_id[0] = '\0';
		PDEBUG(DCC, DEBUG_INFO, "Call connected to '%s'\n", caller_id);
		osmo_cc_helper_audio_negotiate(msg, &console.session, &console.codec);
		console_new_state(CONSOLE_CONNECT);
		strncpy(console.station_id, caller_id, console.num_digits);
		console.station_id[console.num_digits] = '\0';
		request_answer_ack(console.callref);
		break;
	    }
	case OSMO_CC_MSG_SETUP_COMP_IND:
		break;
	case OSMO_CC_MSG_DISC_IND:
		rc = osmo_cc_get_ie_cause(msg, 0, &location, &isdn_cause, &sip_cause, &socket_cause);
		if (rc < 0)
			isdn_cause = OSMO_CC_ISDN_CAUSE_NORM_CALL_CLEAR;
		rc = osmo_cc_get_ie_progress(msg, 0, &coding, &location, &progress);
		osmo_cc_helper_audio_negotiate(msg, &console.session, &console.codec);
		if (rc >= 0 && (progress == 1 || progress == 8)) {
			PDEBUG(DCC, DEBUG_INFO, "Call disconnected with audio (%s)\n", cause_name(isdn_cause));
			console_new_state(CONSOLE_DISCONNECT_RO);
			console.disc_cause = isdn_cause;
		} else {
			PDEBUG(DCC, DEBUG_INFO, "Call disconnected without audio (%s)\n", cause_name(isdn_cause));
			request_disconnect_release_reject(console.callref, isdn_cause, OSMO_CC_MSG_REL_REQ);
			console_new_state(CONSOLE_IDLE);
			free_console();
		}
		break;
	case OSMO_CC_MSG_REL_IND:
	case OSMO_CC_MSG_REJ_IND:
		rc = osmo_cc_get_ie_cause(msg, 0, &location, &isdn_cause, &sip_cause, &socket_cause);
		if (rc < 0)
			isdn_cause = OSMO_CC_ISDN_CAUSE_NORM_CALL_CLEAR;
		PDEBUG(DCC, DEBUG_INFO, "Call released (%s)\n", cause_name(isdn_cause));
		console_new_state(CONSOLE_IDLE);
		free_console();
		break;
	}
	osmo_cc_free_msg(msg);
}

static char console_text[256];
static char console_clear[256];
static int console_len = 0;

static void _clear_console_text(void)
{
	if (!console_len)
		return;

	fwrite(console_clear, console_len, 1, stdout);
	// note: fflused by user of this function
	console_len = 0;
}

static void _print_console_text(void)
{
	if (!console_len)
		return;

	printf("\033[1;37m");
	fwrite(console_text, console_len, 1, stdout);
	printf("\033[0;39m");
}

int console_init(const char *station_id, const char *audiodev, int samplerate, int buffer, int num_digits, int loopback, int echo_test, const char *digits)
{
	int rc = 0;

	init_testton();

	clear_console_text = _clear_console_text;
	print_console_text = _print_console_text;

	memset(&console, 0, sizeof(console));
	if (station_id)
		strncpy(console.station_id, station_id, sizeof(console.station_id) - 1);
	strncpy(console.audiodev, audiodev, sizeof(console.audiodev) - 1);
	console.samplerate = samplerate;
	console.buffer_size = buffer * samplerate / 1000;
	console.num_digits = num_digits;
	console.loopback = loopback;
	console.echo_test = echo_test;
	console.digits = digits;

	if (!audiodev[0])
		return 0;

	rc = init_samplerate(&console.srstate, 8000.0, (double)samplerate, 3300.0);
	if (rc < 0) {
		PDEBUG(DSENDER, DEBUG_ERROR, "Failed to init sample rate conversion!\n");
		goto error;
	}

	rc = jitter_create(&console.dejitter, samplerate / 5);
	if (rc < 0) {
		PDEBUG(DSENDER, DEBUG_ERROR, "Failed to create and init dejitter buffer!\n");
		goto error;
	}

	return 0;

error:
	console_cleanup();
	return rc;
}

int console_open_audio(int __attribute__((unused)) buffer_size, double __attribute__((unused)) interval)
{
	if (!console.audiodev[0])
		return 0;

#ifdef HAVE_ALSA
	/* open sound device for call control */
	/* use factor 1.4 of speech level for complete range of sound card */
	console.sound = sound_open(console.audiodev, NULL, NULL, NULL, 1, 0.0, console.samplerate, buffer_size, interval, 1.4, 4000.0, 2.0);
	if (!console.sound) {
		PDEBUG(DSENDER, DEBUG_ERROR, "No sound device!\n");
		return -EIO;
	}
#else
	PDEBUG(DSENDER, DEBUG_ERROR, "No sound card support compiled in!\n");
	return -ENOTSUP;
#endif

	return 0;
}

int console_start_audio(void)
{
	if (!console.audiodev[0])
		return 0;

#ifdef HAVE_ALSA
	return sound_start(console.sound);
#else
	return -EINVAL;
#endif
}

void console_cleanup(void)
{
#ifdef HAVE_ALSA
	/* close sound devoice */
	if (console.sound) {
		sound_close(console.sound);
		console.sound = NULL;
	}
#endif

	jitter_destroy(&console.dejitter);

	if (console.session) {
		osmo_cc_free_session(console.session);
		console.session = NULL;
	}
}

static void process_ui(int c)
{
	char text[256] = "";
	int len;
	int i;

	switch (console.state) {
	case CONSOLE_IDLE:
		if (c > 0) {
			if ((int)strlen(console.station_id) < console.num_digits) {
				for (i = 0; i < (int)strlen(console.digits); i++) {
					if (c == console.digits[i]) {
						console.station_id[strlen(console.station_id) + 1] = '\0';
						console.station_id[strlen(console.station_id)] = c;
					}
				}
			}
			if ((c == 8 || c == 127) && strlen(console.station_id))
				console.station_id[strlen(console.station_id) - 1] = '\0';
dial_after_hangup:
			if (c == 'd' && (int)strlen(console.station_id) == console.num_digits) {
				PDEBUG(DCC, DEBUG_INFO, "Outgoing call to '%s'\n", console.station_id);
				console.dialing[0] = '\0';
				console_new_state(CONSOLE_SETUP_RT);
				console.callref = osmo_cc_new_callref();
				request_setup(console.callref, console.station_id);
			}
		}
		if (console.num_digits != (int)strlen(console.station_id))
			sprintf(text, "on-hook: %s%s (enter digits 0..9)\r", console.station_id, "..............." + 15 - console.num_digits + strlen(console.station_id));
		else
			sprintf(text, "on-hook: %s (press d=dial)\r", console.station_id);
		break;
	case CONSOLE_SETUP_RO:
	case CONSOLE_SETUP_RT:
	case CONSOLE_ALERTING_RO:
	case CONSOLE_ALERTING_RT:
	case CONSOLE_CONNECT:
	case CONSOLE_DISCONNECT_RO:
		if (c > 0) {
			if (c == 'h' || (c == 'd' && console.state == CONSOLE_DISCONNECT_RO)) {
				PDEBUG(DCC, DEBUG_INFO, "Call hangup\n");
				if (console.callref) {
					if (console.state == CONSOLE_SETUP_RO)
						request_disconnect_release_reject(console.callref, CAUSE_NORMAL, OSMO_CC_MSG_REJ_REQ);
					else
						request_disconnect_release_reject(console.callref, CAUSE_NORMAL, OSMO_CC_MSG_REL_REQ);
					free_console();
				}
				console_new_state(CONSOLE_IDLE);
				if (c == 'd')
					goto dial_after_hangup;
			}
		}
		if (console.state == CONSOLE_SETUP_RT)
			sprintf(text, "call setup: %s (press h=hangup)\r", console.station_id);
		if (console.state == CONSOLE_ALERTING_RT)
			sprintf(text, "call ringing: %s (press h=hangup)\r", console.station_id);
		if (console.state == CONSOLE_CONNECT) {
			if (console.dialing[0])
				sprintf(text, "call active: %s->%s (press h=hangup)\r", console.station_id, console.dialing);
			else
				sprintf(text, "call active: %s (press h=hangup)\r", console.station_id);
		}
		if (console.state == CONSOLE_DISCONNECT_RO)
			sprintf(text, "call disconnected: %s (press h=hangup d=redial)\r", cause_name(console.disc_cause));
		break;
	}
	/* skip if nothing has changed */
	len = strlen(text);
	if (console_len == len && !memcmp(console_text, text, len))
		return;
	clear_console_text();
	console_len = len;
	memcpy(console_text, text, len);
	memset(console_clear, ' ', len - 1);
	console_clear[len - 1] = '\r';
	print_console_text();
	fflush(stdout);
}

/* get keys from keyboard to control call via console
 * returns 1 on exit (ctrl+c) */
void process_console(int c)
{
	if (!console.loopback && console.num_digits)
		process_ui(c);

	if (console.session)
		osmo_cc_session_handle(console.session);

	if (!console.sound)
		return;

#ifdef HAVE_ALSA
	/* handle audio, if sound device is used */
	sample_t samples[console.buffer_size + 10], *samples_list[1];
	uint8_t *power_list[1];
	int count;
	int rc;

	count = sound_get_tosend(console.sound, console.buffer_size);
	if (count < 0) {
		PDEBUG(DSENDER, DEBUG_ERROR, "Failed to get samples in buffer (rc = %d)!\n", count);
		if (count == -EPIPE)
			PDEBUG(DSENDER, DEBUG_ERROR, "Trying to recover.\n");
		return;
	}
	if (count > 0) {
		jitter_load(&console.dejitter, samples, count);
		samples_list[0] = samples;
		power_list[0] = NULL;
		rc = sound_write(console.sound, samples_list, power_list, count, NULL, NULL, 1);
		if (rc < 0) {
			PDEBUG(DSENDER, DEBUG_ERROR, "Failed to write TX data to sound device (rc = %d)\n", rc);
			if (rc == -EPIPE)
				PDEBUG(DSENDER, DEBUG_ERROR, "Trying to recover.\n");
			return;
		}
	}
	samples_list[0] = samples;
	count = sound_read(console.sound, samples_list, console.buffer_size, 1, NULL);
	if (count < 0) {
		PDEBUG(DSENDER, DEBUG_ERROR, "Failed to read from sound device (rc = %d)!\n", count);
		if (count == -EPIPE)
			PDEBUG(DSENDER, DEBUG_ERROR, "Trying to recover.\n");
		return;
	}
	if (count) {
		int i;

		if (console.loopback == 3)
			jitter_save(&console.dejitter, samples, count);
		count = samplerate_downsample(&console.srstate, samples, count);
		/* put samples into ring buffer */
		for (i = 0; i < count; i++) {
			console.tx_buffer[console.tx_buffer_pos] = samples[i];
			/* if ring buffer wraps, deliver data down to call process */
			if (++console.tx_buffer_pos == 160) {
				console.tx_buffer_pos = 0;
				/* only if we have a call */
				if (console.callref && console.codec) {
					int16_t data[160];
					samples_to_int16(data, console.tx_buffer, 160);
					osmo_cc_rtp_send(console.codec, (uint8_t *)data, 160 * 2, 1, 160);
				}
			}
		}
	}
#endif
}

