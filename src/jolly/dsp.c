/* digital signal processing for jollycom
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define CHAN jolly->sender.kanal

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include "../libsample/sample.h"
#include <osmocom/core/timer.h>
#include "../liblogging/logging.h"
#include "../libmobile/call.h"
#include "jolly.h"
#include "dsp.h"
#include "voice.h"

#define db2level(db)	pow(10, (double)db / 20.0)

/* transceiver parameters */
#define MAX_DEVIATION		5000.0	/* deviation of signal */
#define MAX_MODULATION		4000.0	/* frequency spectrum of signal */
#define SPEECH_DEVIATION	3000.0	/* deviation of speech at 1 kHz (generally used with 25 kHz channel spacing) */
#define	MAX_DISPLAY		1.0	/* maximum level to display */
#define TX_INFO_TONE		1.0	/* Level of tone relative to speech level (each component) */
#define TX_ACK_TONE		0.1	/* Level of tone relative to speech level */
#define INFO_TONE_F1		640.0
#define INFO_TONE_F2		670.0
#define ACK_TONE		1000.0

/* Squelch */
#define MUTE_TIME	0.1	/* Time until muting */
#define DELAY_TIME	0.15	/* delay, so we don't hear the noise before squelch mutes */
#define ACK_TIME	0.15	/* Time to play the ack tone */
#define REPEATER_TIME	5.0	/* Time to transmit in repeater mode */

/* table for fast sine generation */
static sample_t dsp_info_tone[65536];
static sample_t dsp_ack_tone[65536];

/* global init for audio processing */
void dsp_init(void)
{
	int i;
	double s;

	LOGP(DDSP, LOGL_DEBUG, "Generating sine tables.\n");
	for (i = 0; i < 65536; i++) {
		s = sin((double)i / 65536.0 * 2.0 * M_PI);
		dsp_info_tone[i] = s * TX_INFO_TONE;
		dsp_ack_tone[i] = s * TX_ACK_TONE;
	}
}

/* Init transceiver instance. */
int dsp_init_sender(jolly_t *jolly, int nbfm, double squelch_db, int repeater)
{
	int rc;

	/* init squelch */
	squelch_init(&jolly->squelch, jolly->sender.kanal, squelch_db, MUTE_TIME, MUTE_TIME);
	if (!isinf(squelch_db))
		jolly->is_mute = 1;

	/* set modulation parameters (NBFM uses half channel spacing, so we use half deviation) */
	if (nbfm)
		sender_set_fm(&jolly->sender, MAX_DEVIATION / 2.0, MAX_MODULATION, SPEECH_DEVIATION / 2.0, MAX_DISPLAY);
	else
		sender_set_fm(&jolly->sender, MAX_DEVIATION, MAX_MODULATION, SPEECH_DEVIATION, MAX_DISPLAY);

	/* init dtmf audio processing.
	 * each frequency may be +6 dB deviation, which means a total deviation of +12 dB is allowed for detection.
	 * also we allow a minimum of -30 dB for each tone. */
	rc = dtmf_decode_init(&jolly->dtmf, jolly, jolly_receive_dtmf, 8000, db2level(6.0), db2level(-30.0));
	if (rc < 0) {
		LOGP(DDSP, LOGL_ERROR, "Failed to init DTMF decoder!\n");
		goto error;
	}

	/* tones */
	jolly->dt_phaseshift65536[0] = 65536.0 / ((double)jolly->sender.samplerate / INFO_TONE_F1);
	jolly->dt_phaseshift65536[1] = 65536.0 / ((double)jolly->sender.samplerate / INFO_TONE_F2);
	jolly->ack_phaseshift65536 = 65536.0 / ((double)jolly->sender.samplerate / ACK_TONE);
	jolly->ack_max = (int)((double)jolly->sender.samplerate * ACK_TIME);

	/* delay buffer */
	jolly->delay_max = (int)((double)jolly->sender.samplerate * DELAY_TIME);
	jolly->delay_spl = calloc(jolly->delay_max, sizeof(*jolly->delay_spl));
	if (!jolly->delay_spl) {
		LOGP(DDSP, LOGL_ERROR, "No mem for delay buffer!\n");
		goto error;
	}

	/* repeater */
	jolly->repeater = repeater;
	jolly->repeater_max = (int)((double)jolly->sender.samplerate * REPEATER_TIME);
	rc = jitter_create(&jolly->repeater_dejitter, "repeater", jolly->sender.samplerate, sizeof(sample_t), 0.050, 0.500, JITTER_FLAG_NONE);
	if (rc < 0) {
		LOGP(DDSP, LOGL_ERROR, "Failed to create and init repeater buffer!\n");
		goto error;
	}


	jolly->dmp_dtmf_low = display_measurements_add(&jolly->sender.dispmeas, "DTMF Low", "%.1f dB (last)", DISPLAY_MEAS_LAST, DISPLAY_MEAS_LEFT, -30.0, 6.0, 0.0);
	jolly->dmp_dtmf_high = display_measurements_add(&jolly->sender.dispmeas, "DTMF High", "%.1f dB (last)", DISPLAY_MEAS_LAST, DISPLAY_MEAS_LEFT, -30.0, 6.0, 0.0);

	return 0;

error:
	dsp_cleanup_sender(jolly);
	return -EINVAL;
}

void dsp_cleanup_sender(jolly_t *jolly)
{
	jitter_destroy(&jolly->repeater_dejitter);
	dtmf_decode_exit(&jolly->dtmf);
	if (jolly->delay_spl) {
		free(jolly->delay_spl);
		jolly->delay_spl = NULL;
	}
}

void set_speech_string(jolly_t *jolly, char announcement, const char *number)
{
	jolly->speech_string[0] = announcement;
	jolly->speech_string[1] = '\0';
	strncat(jolly->speech_string, number, sizeof(jolly->speech_string) - strlen(jolly->speech_string) - 1);
	jolly->speech_digit = 0;
	jolly->speech_pos = 0;
}

void reset_speech_string(jolly_t *jolly)
{
	jolly->speech_string[0] = '\0';
	jolly->speech_digit = 0;
}

/* Generate audio stream from voice samples. */
static int speak_voice(jolly_t *jolly, sample_t *samples, int length)
{
	sample_t *spl;
	int size;
	int i;
	int count = 0;

again:
	/* no speech */
	if (!jolly->speech_string[jolly->speech_digit])
		return count;

	/* select sample */
	switch (jolly->speech_string[jolly->speech_digit]) {
	case 'i':
		spl = jolly_voice.spl[10];
		size = jolly_voice.size[10];
		if (!jolly->speech_pos)
			LOGP(DDSP, LOGL_DEBUG, "speaking 'incoming'.\n");
		break;
	case 'o':
		spl = jolly_voice.spl[11];
		size = jolly_voice.size[11];
		if (!jolly->speech_pos)
			LOGP(DDSP, LOGL_DEBUG, "speaking 'outgoing'.\n");
		break;
	case 'r':
		spl = jolly_voice.spl[12];
		size = jolly_voice.size[12];
		if (!jolly->speech_pos)
			LOGP(DDSP, LOGL_DEBUG, "speaking 'released'.\n");
		break;
	case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
		spl = jolly_voice.spl[jolly->speech_string[jolly->speech_digit] - '0'];
		size = jolly_voice.size[jolly->speech_string[jolly->speech_digit] - '0'];
		if (!jolly->speech_pos)
			LOGP(DDSP, LOGL_DEBUG, "speaking digit '%c'.\n", jolly->speech_string[jolly->speech_digit]);
		break;
	default:
		jolly->speech_digit++;
		goto again;
	}

	/* copy sample */
	for (; length && jolly->speech_pos < size; i++) {
		*samples++ = spl[jolly->speech_pos++];
		length--;
		count++;
	}

	if (jolly->speech_pos == size) {
		jolly->speech_pos = 0;
		jolly->speech_digit++;
		if (!jolly->speech_string[jolly->speech_digit])
			speech_finished(jolly);
		goto again;
	}

	return count;
}

static void delay_audio(jolly_t *jolly, sample_t *samples, int count)
{
	sample_t *spl, s;
	int pos, max;
	int i;

	spl = jolly->delay_spl;
	pos = jolly->delay_pos;
	max = jolly->delay_max;

	/* feed audio though delay buffer */
	for (i = 0; i < count; i++) {
		s = samples[i];
		samples[i] = spl[pos];
		spl[pos] = s;
		if (++pos == max)
			pos = 0;
	}

	jolly->delay_pos = pos;
}

/* Generate audio stream from tone. Keep phase for next call of function. */
static void dial_tone(jolly_t *jolly, sample_t *samples, int length)
{
	double *phaseshift, *phase;
	int i;

	phaseshift = jolly->dt_phaseshift65536;
	phase = jolly->dt_phase65536;
	
	for (i = 0; i < length; i++) {
		*samples = dsp_info_tone[(uint16_t)(phase[0])];
		*samples++ += dsp_info_tone[(uint16_t)(phase[1])];
		phase[0] += phaseshift[0];
		if (phase[0] >= 65536)
			phase[0] -= 65536;
		phase[1] += phaseshift[1];
		if (phase[1] >= 65536)
			phase[1] -= 65536;
	}
}

static void ack_tone(jolly_t *jolly, sample_t *samples, int length)
{
	double phaseshift, phase;
	int i;

	phaseshift = jolly->ack_phaseshift65536;
	phase = jolly->ack_phase65536;
	
	for (i = 0; i < length; i++) {
		*samples++ = dsp_ack_tone[(uint16_t)phase];
		phase += phaseshift;
		if (phase >= 65536)
			phase -= 65536;
	}

	jolly->ack_phase65536 = phase;
}

/* Process received audio stream from radio unit. */
void sender_receive(sender_t *sender, sample_t *samples, int length, double rf_level_db)
{
	jolly_t *jolly = (jolly_t *) sender;
	sample_t *spl;
	int count;
	int pos;
	int i;

	/* process signal mute/loss, also for DTMF tones */
	switch (squelch(&jolly->squelch, rf_level_db, (double)length / (double)jolly->sender.samplerate)) {
	case SQUELCH_LOSS:
	case SQUELCH_MUTE:
		if (!jolly->is_mute) {
			LOGP_CHAN(DDSP, LOGL_INFO, "Low RF level, muting.\n");
			jolly->ack_count = jolly->ack_max;
			jolly->repeater_count = jolly->repeater_max;
		}
		jolly->is_mute = 1;
		memset(samples, 0, sizeof(*samples) * length);
		break;
	default:
		if (jolly->is_mute)
			LOGP_CHAN(DDSP, LOGL_INFO, "High RF level, unmuting; turning transmitter on.\n");
		jolly->is_mute = 0;
		break;
	}

	/* delay audio to prevent noise before squelch mutes */
	delay_audio(jolly, samples, length);

	/* play ack tone */
	if (jolly->ack_count) {
		ack_tone(jolly, samples, length);
		jolly->ack_count -= length;
		if (jolly->ack_count < 0)
			jolly->ack_count = 0;
	}

	/* if repeater mode, store sample in jitter buffer */
	if (jolly->repeater)
		jitter_save(&jolly->repeater_dejitter, samples, length, 0, 0, 0, 0);

	/* downsample, decode DTMF */
	count = samplerate_downsample(&jolly->sender.srstate, samples, length);
	dtmf_decode(&jolly->dtmf, samples, count);

	/* Forward audio to network (call process) and feed DTMF decoder. */
	if (jolly->callref) {
		spl = jolly->sender.rxbuf;
		pos = jolly->sender.rxbuf_pos;
		for (i = 0; i < count; i++) {
			spl[pos++] = samples[i];
			if (pos == 160) {
				call_up_audio(jolly->callref, spl, 160);
				pos = 0;
			}
		}
		jolly->sender.rxbuf_pos = pos;
	} else
		jolly->sender.rxbuf_pos = 0;

}

/* Provide stream of audio toward radio unit */
void sender_send(sender_t *sender, sample_t *samples, uint8_t *power, int length)
{
	jolly_t *jolly = (jolly_t *) sender;
	int count, input_num;

	switch (jolly->state) {
	case STATE_IDLE:
		if (jolly->repeater && (!jolly->is_mute || jolly->ack_count || jolly->repeater_count)) {
			memset(power, 1, length);
			if (jolly->repeater_count) {
				jolly->repeater_count -= length;
				if (jolly->repeater_count < 0) {
					LOGP_CHAN(DDSP, LOGL_INFO, "turning transmitter off.\n");
					jolly->repeater_count = 0;
				}
			}
		} else {
			/* pwr off */
			memset(power, 0, length);
		}
		memset(samples, 0, length * sizeof(*samples));
		break;
	case STATE_CALL:
	case STATE_CALL_DIALING:
		memset(power, 1, length);
		input_num = samplerate_upsample_input_num(&sender->srstate, length);
		jitter_load(&sender->dejitter, samples, input_num);
		samplerate_upsample(&sender->srstate, samples, input_num, samples, length);
		break;
	case STATE_OUT_VERIFY:
	case STATE_IN_PAGING:
	case STATE_RELEASED:
		memset(power, 1, length);
		count = speak_voice(jolly, samples, length);
		if (count) {
			/* if voice ends, fill silence */
			if (count < length)
				memset(samples + count, 0, sizeof(*samples) * (length - count));
			break;
		}
		/* in case of no voice: */
		/* FALLTHRU */
	default:
		memset(power, 1, length);
		dial_tone(jolly, samples, length);
	}

	/* if repeater mode, sum samples from jitter buffer to samples */
	if (jolly->repeater) {
		sample_t uplink[length];
		int i;
		jitter_load(&jolly->repeater_dejitter, uplink, length);
		for (i = 0; i < length; i++)
			samples[i] += uplink[i];
	}
}

