/* A-Netz signal processing
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "../common/debug.h"
#include "../common/timer.h"
#include "../common/call.h"
#include "../common/goertzel.h"
#include "anetz.h"
#include "dsp.h"

#define PI		3.1415927

/* signalling */
/* NOTE: The peak deviation is similar for paging tone and signalling tone,
 * so both tones should be equal after pre-emphasis. This is why the paging
 * tones is so much louder.*/
#define TX_PEAK_TONE	8192.0	/* peak amplitude for all tones */
#define TX_PEAK_PAGE	32767.0	/* peak amplitude paging tone */
// FIXME: what is the allowed deviation of tone?
#define CHUNK_DURATION	0.010	/* 10 ms */

// FIXME: how long until we detect a tone?
#define TONE_DETECT_TH	8	/* chunk intervals to detect continous tone */

/* carrier loss detection */
#define LOSS_INTERVAL	100	/* filter steps (chunk durations) for one second interval */
#define LOSS_TIME	12	/* duration of signal loss before release */

extern int page_sequence;

/* two signalling tones */
static double fsk_tones[2] = {
	2280.0,
	1750.0,
};

/* table for fast sine generation */
int dsp_sine_tone[256];
int dsp_sine_page[256];

/* global init for audio processing */
void dsp_init(void)
{
	int i;
	double s;

	PDEBUG(DDSP, DEBUG_DEBUG, "Generating sine tables.\n");
	for (i = 0; i < 256; i++) {
		s = sin((double)i / 256.0 * 2.0 * PI);
		dsp_sine_tone[i] = (int)(s * TX_PEAK_TONE);
		dsp_sine_page[i] = (int)(s * TX_PEAK_PAGE);
	}

	if (TX_PEAK_TONE > 32767.0) {
		fprintf(stderr, "TX_PEAK_TONE definition too high, please fix!\n");
		abort();
	}
	if (TX_PEAK_PAGE > 32767.0) {
		fprintf(stderr, "TX_PEAK_PAGE definition too high, please fix!\n");
		abort();
	}
}

/* Init transceiver instance. */
int dsp_init_sender(anetz_t *anetz)
{
	int16_t *spl;
	double coeff;
	int i;
	double tone;

	PDEBUG(DDSP, DEBUG_DEBUG, "Init DSP for 'Sender'.\n");

	audio_init_loss(&anetz->sender.loss, LOSS_INTERVAL, anetz->sender.loss_volume, LOSS_TIME);

	anetz->samples_per_chunk = anetz->sender.samplerate * CHUNK_DURATION;
	PDEBUG(DDSP, DEBUG_DEBUG, "Using %d samples per chunk duration.\n", anetz->samples_per_chunk);
	spl = calloc(1, anetz->samples_per_chunk << 1);
	if (!spl) {
		PDEBUG(DDSP, DEBUG_ERROR, "No memory!\n");
		return -ENOMEM;
	}
	anetz->fsk_filter_spl = spl;

	anetz->tone_detected = -1;

	for (i = 0; i < 2; i++) {
		coeff = 2.0 * cos(2.0 * PI * fsk_tones[i] / (double)anetz->sender.samplerate);
		anetz->fsk_tone_coeff[i] = coeff * 32768.0;
		PDEBUG(DDSP, DEBUG_DEBUG, "RX %.0f Hz coeff = %d\n", fsk_tones[i], (int)anetz->fsk_tone_coeff[i]);
	}
	tone = fsk_tones[(anetz->sender.loopback == 0) ? 0 : 1];
	anetz->tone_phaseshift256 = 256.0 / ((double)anetz->sender.samplerate / tone);
	PDEBUG(DDSP, DEBUG_DEBUG, "TX %.0f Hz phaseshift = %.4f\n", tone, anetz->tone_phaseshift256);

	return 0;
}

/* Cleanup transceiver instance. */
void dsp_cleanup_sender(anetz_t *anetz)
{
	PDEBUG(DDSP, DEBUG_DEBUG, "Cleanup DSP for 'Sender'.\n");

	if (anetz->fsk_filter_spl) {
		free(anetz->fsk_filter_spl);
		anetz->fsk_filter_spl = NULL;
	}
}

/* Count duration of tone and indicate detection/loss to protocol handler. */
static void fsk_receive_tone(anetz_t *anetz, int tone, int goodtone, double level)
{
	/* lost tone because it is not good anymore or has changed */
	if (!goodtone || tone != anetz->tone_detected) {
		if (anetz->tone_count >= TONE_DETECT_TH) {
			PDEBUG(DDSP, DEBUG_INFO, "Lost %.0f Hz tone after %d ms.\n", fsk_tones[anetz->tone_detected], 1000.0 * CHUNK_DURATION * anetz->tone_count);
			anetz_receive_tone(anetz, -1);
		}
		if (goodtone)
			anetz->tone_detected = tone;
		else
			anetz->tone_detected = -1;
		anetz->tone_count = 0;

		return;
	}

	anetz->tone_count++;

	if (anetz->tone_count >= TONE_DETECT_TH)
		audio_reset_loss(&anetz->sender.loss);
	if (anetz->tone_count == TONE_DETECT_TH) {
		PDEBUG(DDSP, DEBUG_INFO, "Detecting continous %.0f Hz tone. (level = %d%%)\n", fsk_tones[anetz->tone_detected], (int)(level * 100.0 + 0.5));
		anetz_receive_tone(anetz, anetz->tone_detected);
	}
}

/* Filter one chunk of audio an detect tone, quality and loss of signal. */
static void fsk_decode_chunk(anetz_t *anetz, int16_t *spl, int max)
{
	double level, result[2];

	level = audio_level(spl, max);

	if (audio_detect_loss(&anetz->sender.loss, level))
		anetz_loss_indication(anetz);

	audio_goertzel(spl, max, 0, anetz->fsk_tone_coeff, result, 2);

	/* show quality of tone */
	if (anetz->sender.loopback) {
		/* adjust level, so we get peak of sine curve */
		PDEBUG(DDSP, DEBUG_NOTICE, "Tone %.0f: Level=%3.0f%% Quality=%3.0f%%\n", fsk_tones[1], level / 0.63662 * 100.0 * 32768.0 / TX_PEAK_TONE, result[1] / level * 100.0);
	}
	if (level / 0.63 > 0.05 && result[0] / level > 0.5)
		PDEBUG(DDSP, DEBUG_INFO, "Tone %.0f: Level=%3.0f%% Quality=%3.0f%%\n", fsk_tones[0], level / 0.63662 * 100.0 * 32768.0 / TX_PEAK_TONE, result[0] / level * 100.0);

	/* adjust level, so we get peak of sine curve */
	/* indicate detected tone */
	if (level / 0.63 > 0.05 && result[0] / level > 0.5)
		fsk_receive_tone(anetz, 0, 1, level / 0.63662 * 32768.0 / TX_PEAK_TONE);
	else if (level / 0.63 > 0.05 && result[1] / level > 0.5)
		fsk_receive_tone(anetz, 1, 1, level / 0.63662 * 32768.0 / TX_PEAK_TONE);
	else
		fsk_receive_tone(anetz, -1, 0, level / 0.63662 * 32768.0 / TX_PEAK_TONE);
}

/* Process received audio stream from radio unit. */
void sender_receive(sender_t *sender, int16_t *samples, int length)
{
	anetz_t *anetz = (anetz_t *) sender;
	int16_t *spl;
	int max, pos;
	int i;

	/* write received samples to decode buffer */
	max = anetz->samples_per_chunk;
	pos = anetz->fsk_filter_pos;
	spl = anetz->fsk_filter_spl;
	for (i = 0; i < length; i++) {
		spl[pos++] = samples[i];
		if (pos == max) {
			pos = 0;
			fsk_decode_chunk(anetz, spl, max);
		}
	}
	anetz->fsk_filter_pos = pos;

	/* Forward audio to network (call process). */
	if (anetz->dsp_mode == DSP_MODE_AUDIO && anetz->sender.callref) {
		int16_t down[length]; /* more than enough */
		int count;

		count = samplerate_downsample(&anetz->sender.srstate, samples, length, down);
		spl = anetz->sender.rxbuf;
		pos = anetz->sender.rxbuf_pos;
		for (i = 0; i < count; i++) {
			spl[pos++] = down[i];
			if (pos == 160) {
				call_tx_audio(anetz->sender.callref, spl, 160);
				pos = 0;
			}
		}
		anetz->sender.rxbuf_pos = pos;
	} else
		anetz->sender.rxbuf_pos = 0;
}

/* Set 4 paging frequencies */
void dsp_set_paging(anetz_t *anetz, double *freq)
{
	int i;

	for (i = 0; i < 4; i++) {
		anetz->paging_phaseshift256[i] = 256.0 / ((double)anetz->sender.samplerate / freq[i]);
		anetz->paging_phase256[i] = 0;
	}
}

/* Generate audio stream of 4 simultanious paging tones. Keep phase for next call of function.
 * Use TX_PEAK_PAGE for all tones, which gives peak of (TX_PEAK_PAGE / 4) for each individual tone. */
static void fsk_paging_tone(anetz_t *anetz, int16_t *samples, int length)
{
	double phaseshift[5], phase[5];
	int i;
	int32_t sample;

	for (i = 0; i < 4; i++) {
		phaseshift[i] = anetz->paging_phaseshift256[i];
		phase[i] = anetz->paging_phase256[i];
	}

	for (i = 0; i < length; i++) {
		sample	= (int32_t)dsp_sine_page[((uint8_t)phase[0]) & 0xff]
			+ (int32_t)dsp_sine_page[((uint8_t)phase[1]) & 0xff]
			+ (int32_t)dsp_sine_page[((uint8_t)phase[2]) & 0xff]
			+ (int32_t)dsp_sine_page[((uint8_t)phase[3]) & 0xff];
		*samples++ = sample >> 2;
		phase[0] += phaseshift[0];
		phase[1] += phaseshift[1];
		phase[2] += phaseshift[2];
		phase[3] += phaseshift[3];
		if (phase[0] >= 256) phase[0] -= 256;
		if (phase[1] >= 256) phase[1] -= 256;
		if (phase[2] >= 256) phase[2] -= 256;
		if (phase[3] >= 256) phase[3] -= 256;
	}

	for (i = 0; i < 4; i++) {
		anetz->paging_phase256[i] = phase[i];
	}
}

/* Generate audio stream of 4 sequenced paging tones. Keep phase for next call of function.
 * Use TX_PEAK_PAGE / 2 for each tone, that is twice as much peak per tone.  */
static void fsk_paging_tone_sequence(anetz_t *anetz, int16_t *samples, int length, int numspl)
{
	double phaseshift, phase;
	int tone, count;

	phase = anetz->tone_phase256;
	tone = anetz->paging_tone;
	count = anetz->paging_count;

next_tone:
	phaseshift = anetz->paging_phaseshift256[tone];

	while (length) {
		*samples++ = dsp_sine_page[((uint8_t)phase) & 0xff] >> 1;
		phase += phaseshift;
		if (phase >= 256)
			phase -= 256;
		if (++count == numspl) {
			count = 0;
			if (++tone == 4)
				tone = 0;
			goto next_tone;
		}
		length--;
	}

	anetz->tone_phase256 = phase;
	anetz->paging_tone = tone;
	anetz->paging_count = count;
}

/* Generate audio stream from tone. Keep phase for next call of function. */
static void fsk_tone(anetz_t *anetz, int16_t *samples, int length)
{
	double phaseshift, phase;
	int i;

	phaseshift = anetz->tone_phaseshift256;
	phase = anetz->tone_phase256;

	for (i = 0; i < length; i++) {
		*samples++ = dsp_sine_tone[((uint8_t)phase) & 0xff];
		phase += phaseshift;
		if (phase >= 256)
			phase -= 256;
	}

	anetz->tone_phase256 = phase;
}

/* Provide stream of audio toward radio unit */
void sender_send(sender_t *sender, int16_t *samples, int length)
{
	anetz_t *anetz = (anetz_t *) sender;

	switch (anetz->dsp_mode) {
	case DSP_MODE_SILENCE:
		memset(samples, 0, length * sizeof(*samples));
		break;
	case DSP_MODE_AUDIO:
		jitter_load(&anetz->sender.audio, samples, length);
		break;
	case DSP_MODE_TONE:
		fsk_tone(anetz, samples, length);
		break;
	case DSP_MODE_PAGING:
		if (page_sequence)
			fsk_paging_tone_sequence(anetz, samples, length, page_sequence * anetz->sender.samplerate / 1000);
		else
			fsk_paging_tone(anetz, samples, length);
		break;
	}
}

void anetz_set_dsp_mode(anetz_t *anetz, enum dsp_mode mode)
{
	PDEBUG(DDSP, DEBUG_DEBUG, "DSP mode %d -> %d\n", anetz->dsp_mode, mode);
	anetz->dsp_mode = mode;
}

