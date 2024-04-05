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
#include <stdbool.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/param.h>
#include "../libsample/sample.h"
#include "../libsamplerate/samplerate.h"
#include "../libjitter/jitter.h"
#include "../liblogging/logging.h"
#include <osmocom/core/timer.h>
#include <osmocom/core/select.h>
#include <osmocom/cc/endpoint.h>
#include <osmocom/cc/helper.h>
#include <osmocom/cc/rtp.h>
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
	const struct number_lengths *number_lengths;/* number of digits to be dialed */
	int number_max_length;	/* number of digits of the longest number to be dialed */
	int loopback;		/* loopback test for echo */
	int echo_test;		/* send echo back to mobile phone */
	const char *digits;	/* list of dialable digits */
} console_t;

static console_t console;

extern osmo_cc_endpoint_t *ep;

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
	LOGP(DCALL, LOGL_DEBUG, "Call state '%s' -> '%s'\n", console_state_name[console.state], console_state_name[state]);
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

static void up_audio(struct osmo_cc_session_codec *codec, uint8_t marker, uint16_t sequence, uint32_t timestamp, uint32_t ssrc, uint8_t *payload, int payload_len)
{
	/* save audio from transceiver to jitter buffer */
	if (console.sound) {
		jitter_frame_t *jf;
		jf = jitter_frame_alloc(codec->decoder, &console, payload, payload_len, marker, sequence, timestamp, ssrc);
		if (!jf)
			return;
		jitter_save(&console.dejitter, jf);
		return;
	}
	/* if echo test is used, send echo back to mobile */
	if (console.echo_test) {
		osmo_cc_rtp_send_ts(codec, payload, payload_len, marker, sequence, timestamp);
		return;
	}
	/* if no sound is used, send test tone to mobile */
	if (console.state == CONSOLE_CONNECT) {
		int16_t spl[160];
		uint8_t *payload;
		int payload_len;
		get_test_patterns(spl, 160);
		codec->encoder((uint8_t *)spl, 160 * 2, &payload, &payload_len, &console);
		osmo_cc_rtp_send(codec, payload, payload_len, 0, 1, 160);
		free(payload);
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
		LOGP(DCALL, LOGL_ERROR, "invalid call ref %u (msg=0x%02x).\n", call->callref, msg->type);
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
		LOGP(DCALL, LOGL_INFO, "Incoming call from '%s'\n", caller_id);
		/* setup is also allowed on disconnected call */
		if (console.state == CONSOLE_DISCONNECT_RO) {
			LOGP(DCALL, LOGL_INFO, "Releasing pending disconnected call\n");
			if (console.callref) {
				request_disconnect_release_reject(console.callref, CAUSE_NORMAL, OSMO_CC_MSG_REL_REQ);
				free_console();
			}
			console_new_state(CONSOLE_IDLE);
		}
		if (console.state != CONSOLE_IDLE) {
			LOGP(DCALL, LOGL_NOTICE, "We are busy, rejecting.\n");
			request_disconnect_release_reject(console.callref, CAUSE_NORMAL, OSMO_CC_MSG_REJ_REQ);
			osmo_cc_free_msg(msg);
			return;
		}
		console.callref = call->callref;
		/* sdp accept */
		sdp = osmo_cc_helper_audio_accept(&ep->session_config, NULL, codecs, up_audio, msg, &console.session, &console.codec, 0);
		if (!sdp) {
			LOGP(DCALL, LOGL_NOTICE, "Cannot accept codec, rejecting.\n");
			request_disconnect_release_reject(console.callref, CAUSE_RESOURCE_UNAVAIL, OSMO_CC_MSG_REJ_REQ);
			osmo_cc_free_msg(msg);
			return;
		}
		if (caller_id[0]) {
			strncpy(console.station_id, caller_id, sizeof(console.station_id));
			console.station_id[sizeof(console.station_id) - 1] = '\0';
		}
		strncpy(console.dialing, number, sizeof(console.dialing) - 1);
		console.dialing[sizeof(console.dialing) - 1] = '\0';
		console_new_state(CONSOLE_CONNECT);
		LOGP(DCALL, LOGL_INFO, "Call automatically answered\n");
		request_answer(console.callref, number, sdp);
		break;
	    }
	case OSMO_CC_MSG_SETUP_ACK_IND:
	case OSMO_CC_MSG_PROC_IND:
		osmo_cc_helper_audio_negotiate(msg, &console.session, &console.codec);
		break;
	case OSMO_CC_MSG_ALERT_IND:
		LOGP(DCALL, LOGL_INFO, "Call alerting\n");
		osmo_cc_helper_audio_negotiate(msg, &console.session, &console.codec);
		console_new_state(CONSOLE_ALERTING_RT);
		break;
	case OSMO_CC_MSG_SETUP_CNF:
	    {
		/* connected id */
		rc = osmo_cc_get_ie_calling(msg, 0, &type, &plan, &present, &screen, caller_id, sizeof(caller_id));
		if (rc < 0)
			caller_id[0] = '\0';
		LOGP(DCALL, LOGL_INFO, "Call connected to '%s'\n", caller_id);
		osmo_cc_helper_audio_negotiate(msg, &console.session, &console.codec);
		console_new_state(CONSOLE_CONNECT);
		if (caller_id[0]) {
			strncpy(console.station_id, caller_id, sizeof(console.station_id));
			console.station_id[sizeof(console.station_id) - 1] = '\0';
		}
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
			LOGP(DCALL, LOGL_INFO, "Call disconnected with audio (%s)\n", cause_name(isdn_cause));
			console_new_state(CONSOLE_DISCONNECT_RO);
			console.disc_cause = isdn_cause;
		} else {
			LOGP(DCALL, LOGL_INFO, "Call disconnected without audio (%s)\n", cause_name(isdn_cause));
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
		LOGP(DCALL, LOGL_INFO, "Call released (%s)\n", cause_name(isdn_cause));
		console_new_state(CONSOLE_IDLE);
		free_console();
		break;
	}
	osmo_cc_free_msg(msg);
}

static char console_text[256];
static int console_len = 0;

int console_init(const char *audiodev, int samplerate, int buffer, int loopback, int echo_test, const char *digits, const struct number_lengths *lengths, const char *station_id)
{
	int rc = 0;
	int i;

	init_testton();

	/* Put scrolling window one line above bottom. */
	logging_limit_scroll_bottom(1);

	memset(&console, 0, sizeof(console));
	strncpy(console.audiodev, audiodev, sizeof(console.audiodev) - 1);
	console.samplerate = samplerate;
	console.buffer_size = buffer * samplerate / 1000;
	console.loopback = loopback;
	console.echo_test = echo_test;
	console.digits = digits;
	console.number_lengths = lengths;
	if (lengths) {
		for (i = 0; lengths[i].usage; i++) {
			if (lengths[i].digits > console.number_max_length)
				console.number_max_length = lengths[i].digits;
		}
	}
	if (station_id)
		strncpy(console.station_id, station_id, sizeof(console.station_id) - 1);

	if (!audiodev[0])
		return 0;

	rc = init_samplerate(&console.srstate, 8000.0, (double)samplerate, 3300.0);
	if (rc < 0) {
		LOGP(DSENDER, LOGL_ERROR, "Failed to init sample rate conversion!\n");
		goto error;
	}

	rc = jitter_create(&console.dejitter, "console", 8000, 0.040, 0.200, JITTER_FLAG_NONE);
	if (rc < 0) {
		LOGP(DSENDER, LOGL_ERROR, "Failed to create and init dejitter buffer!\n");
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
	console.sound = sound_open(SOUND_DIR_DUPLEX, console.audiodev, NULL, NULL, NULL, 1, 0.0, console.samplerate, buffer_size, interval, 1.4, 4000.0, 2.0);
	if (!console.sound) {
		LOGP(DSENDER, LOGL_ERROR, "No sound device!\n");
		return -EIO;
	}
#else
	LOGP(DSENDER, LOGL_ERROR, "No sound card support compiled in!\n");
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

/* process input from console
 * it is not called at loopback mode
 * calling this implies that the console.number_lengths is set
 */
static void process_ui(int c)
{
	char text[256] = "";
	int len, w, h;
	int i;

	switch (console.state) {
	case CONSOLE_IDLE:
		if (c > 0) {
			if ((int)strlen(console.station_id) < console.number_max_length) {
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
			len = strlen(console.station_id);
			for (i = 0; console.number_lengths[i].usage; i++) {
				if (len == console.number_lengths[i].digits)
					break;
			}
			if (c == 'd' && console.number_lengths[i].usage) {
				LOGP(DCALL, LOGL_INFO, "Outgoing call to '%s'\n", console.station_id);
				console.dialing[0] = '\0';
				console_new_state(CONSOLE_SETUP_RT);
				console.callref = osmo_cc_new_callref();
				request_setup(console.callref, console.station_id);
			}
		}
		sprintf(text, "on-hook: %s%s ", console.station_id, "................................" + 32 - console.number_max_length + strlen(console.station_id));
		len = strlen(console.station_id);
		for (i = 0; console.number_lengths[i].usage; i++) {
			if (len == console.number_lengths[i].digits)
				break;
		}
		if (console.number_lengths[i].usage) {
			if (console.number_lengths[i + 1].usage)
				sprintf(strchr(text, '\0'), "(enter digits %s or press d=dial)", console.digits);
			else
				sprintf(strchr(text, '\0'), "(press d=dial)");
		} else
			sprintf(strchr(text, '\0'), "(enter digits %s)", console.digits);
		break;
	case CONSOLE_SETUP_RO:
	case CONSOLE_SETUP_RT:
	case CONSOLE_ALERTING_RO:
	case CONSOLE_ALERTING_RT:
	case CONSOLE_CONNECT:
	case CONSOLE_DISCONNECT_RO:
		if (c > 0) {
			if (c == 'h' || (c == 'd' && console.state == CONSOLE_DISCONNECT_RO)) {
				LOGP(DCALL, LOGL_INFO, "Call hangup\n");
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
			sprintf(text, "call setup: %s (press h=hangup)", console.station_id);
		if (console.state == CONSOLE_ALERTING_RT)
			sprintf(text, "call ringing: %s (press h=hangup)", console.station_id);
		if (console.state == CONSOLE_CONNECT) {
			if (console.dialing[0])
				sprintf(text, "call active: %s->%s (press h=hangup)", console.station_id, console.dialing);
			else
				sprintf(text, "call active: %s (press h=hangup)", console.station_id);
		}
		if (console.state == CONSOLE_DISCONNECT_RO)
			sprintf(text, "call disconnected: %s (press h=hangup d=redial)", cause_name(console.disc_cause));
		break;
	}
	/* skip if nothing has changed */
	len = strlen(text);
	if (console_len == len && !memcmp(console_text, text, len))
		return;
	/* lock logging */
	lock_logging();
	/* disable window */
	enable_limit_scroll(false);
	/* geht height */
	get_win_size(&w, &h);
	/* save cursor go to bottom, use white color */
	printf("\0337\033[%d;1H\033[1;37m", h);
	/* copy text and pad with spaces */
	console_len = len;
	memcpy(console_text, text, console_len);
	if (console_len < (int)MIN(sizeof(console_text), w))
		memset(console_text + console_len, ' ', MIN(sizeof(console_text), w) - console_len);
	/* write text */
	fwrite(console_text, MIN(sizeof(console_text), w), 1, stdout);
	/* reset color, go back to previous line, flush */
	printf("\033[0;39m\0338");
	/* flush output */
	fflush(stdout);
	/* enable window */
	enable_limit_scroll(true);
	/* unlock logging */
	unlock_logging();
}

/* get keys from keyboard to control call via console
 * returns 1 on exit (ctrl+c) */
void process_console(int c)
{
	if (!console.loopback && console.number_max_length)
		process_ui(c);

	if (!console.sound)
		return;

#ifdef HAVE_ALSA
	/* handle audio, if sound device is used */
	sample_t samples[console.buffer_size + 10], *samples_list[1];
	uint8_t *power_list[1];
	int count, input_num;
	int rc;

	count = sound_get_tosend(console.sound, console.buffer_size);
	if (count < 0) {
		LOGP(DSENDER, LOGL_ERROR, "Failed to get samples in buffer (rc = %d)!\n", count);
		if (count == -EPIPE)
			LOGP(DSENDER, LOGL_ERROR, "Trying to recover.\n");
		return;
	}
	if (count > 0) {
		/* load and upsample */
		input_num = samplerate_upsample_input_num(&console.srstate, count);
		{
			int16_t spl[input_num];
			jitter_load_samples(&console.dejitter, (uint8_t *)spl, input_num, sizeof(*spl), jitter_conceal_s16, NULL);
			int16_to_samples_speech(samples, spl, input_num);
		}
		samplerate_upsample(&console.srstate, samples, input_num, samples, count);
		/* write to sound device */
		samples_list[0] = samples;
		power_list[0] = NULL;
		rc = sound_write(console.sound, samples_list, power_list, count, NULL, NULL, 1);
		if (rc < 0) {
			LOGP(DSENDER, LOGL_ERROR, "Failed to write TX data to sound device (rc = %d)\n", rc);
			if (rc == -EPIPE)
				LOGP(DSENDER, LOGL_ERROR, "Trying to recover.\n");
			return;
		}
	}
	samples_list[0] = samples;
	count = sound_read(console.sound, samples_list, console.buffer_size, 1, NULL);
	if (count < 0) {
		LOGP(DSENDER, LOGL_ERROR, "Failed to read from sound device (rc = %d)!\n", count);
		if (count == -EPIPE)
			LOGP(DSENDER, LOGL_ERROR, "Trying to recover.\n");
		return;
	}
	if (count) {
		int i;

		count = samplerate_downsample(&console.srstate, samples, count);
		/* put samples into ring buffer */
		for (i = 0; i < count; i++) {
			console.tx_buffer[console.tx_buffer_pos] = samples[i];
			/* if ring buffer wraps, deliver data down to call process */
			if (++console.tx_buffer_pos == 160) {
				console.tx_buffer_pos = 0;
				/* only if we have a call */
				if (console.callref && console.codec) {
					int16_t spl[160];
					uint8_t *payload;
					int payload_len;
					samples_to_int16_speech(spl, console.tx_buffer, 160);
					console.codec->encoder((uint8_t *)spl, 160 * 2, &payload, &payload_len, &console);
					osmo_cc_rtp_send(console.codec, payload, payload_len, 0, 1, 160);
				}
			}
		}
	}
#endif
}

/* Call this for every inscription. If the console's dial string is empty, it is set to the number that has been inscribed. */
int console_inscription(const char *station_id)
{
	if (console.loopback || !console.number_max_length)
		return -EINVAL;

	if (console.station_id[0])
		return 1;

	strncpy(console.station_id, station_id, sizeof(console.station_id) - 1);
	process_ui(-1);
	return 0;
}

