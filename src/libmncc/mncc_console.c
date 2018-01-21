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
#include "testton.h"
#include "mncc.h"
#include "mncc_console.h"
#include "cause.h"
#include "../libmobile/call.h"
#ifdef HAVE_ALSA
#include "../libsound/sound.h"
#endif

static int new_callref = 0; /* toward mobile */

enum console_state {
	CONSOLE_IDLE = 0,	/* IDLE */
	CONSOLE_SETUP_RO,	/* call from radio to console */
	CONSOLE_SETUP_RT,	/* call from console to radio */
	CONSOLE_ALERTING_RO,	/* call from radio to console */
	CONSOLE_ALERTING_RT,	/* call from console to radio */
	CONSOLE_CONNECT,
	CONSOLE_DISCONNECT,
};

static const char *console_state_name[] = {
	"IDLE",
	"SETUP_RO",
	"SETUP_RT",
	"ALERTING_RO",
	"ALERTING_RT",
	"CONNECT",
	"DISCONNECT",
};

/* console call instance */
typedef struct console {
	uint32_t callref;
	enum console_state state;
	int disc_cause;		/* cause that has been sent by transceiver instance for release */
	char station_id[16];
	char dialing[16];
	char audiodev[64];	/* headphone interface, if used */
	int samplerate;		/* sample rate of headphone interface */
	void *sound;		/* headphone interface */
	int latspl;		/* sample latency at headphone interface */
	samplerate_t srstate;	/* patterns/announcement upsampling */
	jitter_t dejitter;	/* headphone audio dejittering */
	int test_audio_pos;	/* position for test tone toward mobile */
	sample_t tx_buffer[160];/* transmit audio buffer */
	int tx_buffer_pos;	/* current position in transmit audio buffer */
	int dial_digits;	/* number of digits to be dialed */
	int loopback;		/* loopback test for echo */
	int echo_test;		/* send echo back to mobile phone */
} console_t;

static console_t console;

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
	PDEBUG(DMNCC, DEBUG_DEBUG, "Call state '%s' -> '%s'\n", console_state_name[console.state], console_state_name[state]);
	console.state = state;
	console.test_audio_pos = 0;
}

static int console_mncc_up(uint8_t *buf, int length)
{
	struct gsm_mncc *mncc = (struct gsm_mncc *)buf;

	if (mncc->msg_type == ANALOG_8000HZ) {
		struct gsm_data_frame *data = (struct gsm_data_frame *)buf;
		int count = 160;
		sample_t samples[count];

		/* save audio from transceiver to jitter buffer */
		if (console.sound) {
			sample_t up[(int)((double)count * console.srstate.factor + 0.5) + 10];
			int16_to_samples(samples, (int16_t *)data->data, count);
			count = samplerate_upsample(&console.srstate, samples, count, up);
			jitter_save(&console.dejitter, up, count);
			return 0;
		}
		/* if echo test is used, send echo back to mobile */
		if (console.echo_test) {
			/* send down reused MNCC */
			mncc_down(buf, length);
			return 0;
		}
		/* if no sound is used, send test tone to mobile */
		if (console.state == CONSOLE_CONNECT) {
			/* send down reused MNCC */
			get_test_patterns((int16_t *)data->data, count);
			mncc_down(buf, length);
			return 0;
		}
		return 0;
	}

	if (mncc->msg_type != MNCC_SETUP_IND && console.callref != mncc->callref) {
		PDEBUG(DMNCC, DEBUG_ERROR, "invalid call ref.\n");
		/* send down reused MNCC */
		mncc->msg_type = MNCC_REL_REQ;
		mncc->fields |= MNCC_F_CAUSE;
		mncc->cause.location = LOCATION_USER;
		mncc->cause.value = CAUSE_INVALCALLREF;
		mncc_down(buf, length);
		return 0;
	}

	switch(mncc->msg_type) {
	case MNCC_SETUP_IND:
		PDEBUG(DMNCC, DEBUG_INFO, "Incoming call from '%s'\n", mncc->calling.number);
		/* setup is also allowed on disconnected call */
		if (console.state == CONSOLE_DISCONNECT) {
			PDEBUG(DMNCC, DEBUG_INFO, "Releasing pending disconnected call\n");
			if (console.callref) {
				uint8_t buf[sizeof(struct gsm_mncc)];
				struct gsm_mncc *mncc = (struct gsm_mncc *)buf;

				memset(buf, 0, sizeof(buf));
				mncc->msg_type = MNCC_REL_REQ;
				mncc->callref = console.callref;
				mncc->fields |= MNCC_F_CAUSE;
				mncc->cause.location = LOCATION_USER;
				mncc->cause.value = CAUSE_NORMAL;
				mncc_down(buf, sizeof(struct gsm_mncc));
				console.callref = 0;
			}
			console_new_state(CONSOLE_IDLE);
		}
		if (console.state != CONSOLE_IDLE) {
			PDEBUG(DMNCC, DEBUG_NOTICE, "We are busy, rejecting.\n");
			return -CAUSE_BUSY;
		}
		console.callref = mncc->callref;
		if (mncc->calling.number[0]) {
			strncpy(console.station_id, mncc->calling.number, console.dial_digits);
			console.station_id[console.dial_digits] = '\0';
		}
		strncpy(console.dialing, mncc->called.number, sizeof(console.dialing) - 1);
		console.dialing[sizeof(console.dialing) - 1] = '\0';
		console_new_state(CONSOLE_CONNECT);
		PDEBUG(DMNCC, DEBUG_INFO, "Call automatically answered\n");
		/* send down reused MNCC */
		mncc->msg_type = MNCC_SETUP_RSP;
		mncc_down(buf, length);
		break;
	case MNCC_ALERT_IND:
		PDEBUG(DMNCC, DEBUG_INFO, "Call alerting\n");
		console_new_state(CONSOLE_ALERTING_RT);
		break;
	case MNCC_SETUP_CNF:
		PDEBUG(DMNCC, DEBUG_INFO, "Call connected to '%s'\n", mncc->connected.number);
		console_new_state(CONSOLE_CONNECT);
		strncpy(console.station_id, mncc->connected.number, console.dial_digits);
		console.station_id[console.dial_digits] = '\0';
		/* send down reused MNCC */
		mncc->msg_type = MNCC_SETUP_COMPL_REQ;
		mncc_down(buf, length);
		break;
	case MNCC_DISC_IND:
		PDEBUG(DMNCC, DEBUG_INFO, "Call disconnected (%s)\n", cause_name(mncc->cause.value));
		console_new_state(CONSOLE_DISCONNECT);
		console.disc_cause = mncc->cause.value;
		break;
	case MNCC_REL_IND:
		PDEBUG(DMNCC, DEBUG_INFO, "Call released (%s)\n", cause_name(mncc->cause.value));
		console_new_state(CONSOLE_IDLE);
		console.callref = 0;
		break;
	}
	return 0;
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

int console_init(const char *station_id, const char *audiodev, int samplerate, int latency, int dial_digits, int loopback, int echo_test)
{
	int rc = 0;

	init_testton();

	clear_console_text = _clear_console_text;
	print_console_text = _print_console_text;

	memset(&console, 0, sizeof(console));
	strncpy(console.station_id, station_id, sizeof(console.station_id) - 1);
	strncpy(console.audiodev, audiodev, sizeof(console.audiodev) - 1);
	console.samplerate = samplerate;
	console.latspl = latency * samplerate / 1000;
	console.dial_digits = dial_digits;
	console.loopback = loopback;
	console.echo_test = echo_test;

	mncc_up = console_mncc_up;

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

int console_open_audio(int __attribute__((unused)) latspl)
{
	if (!console.audiodev[0])
		return 0;

#ifdef HAVE_ALSA
	/* open sound device for call control */
	/* use factor 1.4 of speech level for complete range of sound card */
	console.sound = sound_open(console.audiodev, NULL, NULL, 1, 0.0, console.samplerate, latspl, 1.4, 4000.0);
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
	if (console.sound)
		sound_close(console.sound);
#endif

	jitter_destroy(&console.dejitter);
}

static void process_ui(int c)
{
	char text[256];
	int len;

	switch (console.state) {
	case CONSOLE_IDLE:
		if (c > 0) {
			if (c >= '0' && c <= '9' && (int)strlen(console.station_id) < console.dial_digits) {
				console.station_id[strlen(console.station_id) + 1] = '\0';
				console.station_id[strlen(console.station_id)] = c;
			}
			if ((c == 8 || c == 127) && strlen(console.station_id))
				console.station_id[strlen(console.station_id) - 1] = '\0';
dial_after_hangup:
			if (c == 'd' && (int)strlen(console.station_id) == console.dial_digits) {
				int callref = ++new_callref;
				uint8_t buf[sizeof(struct gsm_mncc)];
				struct gsm_mncc *mncc = (struct gsm_mncc *)buf;

				PDEBUG(DMNCC, DEBUG_INFO, "Outgoing call to '%s'\n", console.station_id);
				console.dialing[0] = '\0';
				console_new_state(CONSOLE_SETUP_RT);
				console.callref = callref;
				memset(buf, 0, sizeof(buf));
				mncc->msg_type = MNCC_SETUP_REQ;
				mncc->callref = callref;
				mncc->fields |= MNCC_F_CALLED;
				strncpy(mncc->called.number, console.station_id, sizeof(mncc->called.number) - 1);
				mncc->called.type = 0; /* dialing is of type 'unknown' */
				mncc->lchan_type = GSM_LCHAN_TCH_F;
				mncc->fields |= MNCC_F_BEARER_CAP;
				mncc->bearer_cap.speech_ver[0] = BCAP_ANALOG_8000HZ;
				mncc->bearer_cap.speech_ver[1] = -1;
				mncc_down(buf, sizeof(struct gsm_mncc));
			}
		}
		if (console.dial_digits != (int)strlen(console.station_id))
			sprintf(text, "on-hook: %s%s (enter digits 0..9)\r", console.station_id, "..............." + 15 - console.dial_digits + strlen(console.station_id));
		else
			sprintf(text, "on-hook: %s (press d=dial)\r", console.station_id);
		break;
	case CONSOLE_SETUP_RO:
	case CONSOLE_SETUP_RT:
	case CONSOLE_ALERTING_RO:
	case CONSOLE_ALERTING_RT:
	case CONSOLE_CONNECT:
	case CONSOLE_DISCONNECT:
		if (c > 0) {
			if (c == 'h' || (c == 'd' && console.state == CONSOLE_DISCONNECT)) {
				PDEBUG(DMNCC, DEBUG_INFO, "Call hangup\n");
				console_new_state(CONSOLE_IDLE);
				if (console.callref) {
					uint8_t buf[sizeof(struct gsm_mncc)];
					struct gsm_mncc *mncc = (struct gsm_mncc *)buf;

					memset(buf, 0, sizeof(buf));
					mncc->msg_type = MNCC_REL_REQ;
					mncc->callref = console.callref;
					mncc->fields |= MNCC_F_CAUSE;
					mncc->cause.location = LOCATION_USER;
					mncc->cause.value = CAUSE_NORMAL;
					mncc_down(buf, sizeof(struct gsm_mncc));
					console.callref = 0;
				}
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
		if (console.state == CONSOLE_DISCONNECT)
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

/* get keys from keyboad to control call via console
 * returns 1 on exit (ctrl+c) */
void process_console(int c)
{
	if (!console.loopback)
		process_ui(c);

	if (!console.sound)
		return;

#ifdef HAVE_ALSA
	/* handle audio, if sound device is used */
	sample_t samples[console.latspl + 10], *samples_list[1];
	uint8_t *power_list[1];
	double rf_level_db[1];
	int count;
	int rc;

	count = sound_get_tosend(console.sound, console.latspl);
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
	count = sound_read(console.sound, samples_list, console.latspl, 1, rf_level_db);
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
			/* if ring buffer wrapps, deliver data down to call process */
			if (++console.tx_buffer_pos == 160) {
				console.tx_buffer_pos = 0;
				/* only if we have a call */
				if (console.callref) {
					uint8_t buf[sizeof(struct gsm_data_frame) + 160 * sizeof(int16_t)];
					struct gsm_data_frame *data = (struct gsm_data_frame *)buf;

					data->msg_type = ANALOG_8000HZ;
					data->callref = console.callref;
					samples_to_int16((int16_t *)data->data, console.tx_buffer, 160);
					mncc_down(buf, sizeof(struct gsm_mncc));
				}
			}
		}
	}
#endif
}

