/* NMT audio processing
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
#include "nmt.h"
#include "dsp.h"

#define PI			M_PI

/* signalling */
#define TX_PEAK_FSK		16384	/* peak amplitude of signalling FSK */
#define TX_PEAK_SUPER		1638	/* peak amplitude of supervisory signal */
#define BIT_RATE		1200	/* baud rate */
#define STEPS_PER_BIT		10	/* step every 1/12000 sec */
#define DIALTONE_HZ		425.0	/* dial tone frequency */
#define TX_PEAK_DIALTONE	16000	/* dial tone peak */
#define SUPER_DURATION		0.25	/* duration of supervisory signal measurement */
#define SUPER_DETECT_COUNT	4	/* number of measures to detect supervisory signal */
#define MUTE_DURATION		0.280	/* a tiny bit more than two frames */

/* two signalling tones */
static double fsk_bits[2] = {
	1800.0,
	1200.0,
};

/* two supervisory tones */
static double super_freq[5] = {
	3955.0, /* 0-Signal 1 */
	3985.0, /* 0-Signal 2 */
	4015.0, /* 0-Signal 3 */
	4045.0, /* 0-Signal 4 */
	3900.0, /* noise level to check against */
};

/* table for fast sine generation */
int dsp_sine_super[256];
int dsp_sine_dialtone[256];

/* global init for FSK */
void dsp_init(void)
{
	int i;
	double s;

	PDEBUG(DDSP, DEBUG_DEBUG, "Generating sine table for supervisory signal.\n");
	for (i = 0; i < 256; i++) {
		s = sin((double)i / 256.0 * 2.0 * PI);
		dsp_sine_super[i] = (int)(s * TX_PEAK_SUPER);
		dsp_sine_dialtone[i] = (int)(s * TX_PEAK_DIALTONE);
	}
}

/* Init FSK of transceiver */
int dsp_init_sender(nmt_t *nmt)
{
	double coeff;
	int16_t *spl;
	int i;

	init_compander(&nmt->cstate, 8000, 3.0, 13.5);

	if ((nmt->sender.samplerate % (BIT_RATE * STEPS_PER_BIT))) {
		PDEBUG(DDSP, DEBUG_ERROR, "Sample rate must be a multiple of %d bits per second.\n", BIT_RATE * STEPS_PER_BIT);
		return -EINVAL;
	}

	/* this should not happen. it is implied by previous check */
	if (nmt->supervisory && nmt->sender.samplerate < 12000) {
		PDEBUG(DDSP, DEBUG_ERROR, "Sample rate must be at least 12000 Hz to process supervisory signal.\n");
		return -EINVAL;
	}

	PDEBUG(DDSP, DEBUG_DEBUG, "Init DSP for Transceiver.\n");

	/* allocate sample for 2 bits with 2 polarities */
	nmt->samples_per_bit = nmt->sender.samplerate / BIT_RATE;
	PDEBUG(DDSP, DEBUG_DEBUG, "Using %d samples per bit duration.\n", nmt->samples_per_bit);
	nmt->fsk_filter_step = nmt->samples_per_bit / STEPS_PER_BIT;
	PDEBUG(DDSP, DEBUG_DEBUG, "Using %d samples per filter step.\n", nmt->fsk_filter_step);
	nmt->fsk_sine[0][0] = calloc(4, nmt->samples_per_bit * sizeof(int16_t));
	nmt->fsk_sine[0][1] = nmt->fsk_sine[0][0] + nmt->samples_per_bit;
	nmt->fsk_sine[1][0] = nmt->fsk_sine[0][1] + nmt->samples_per_bit;
	nmt->fsk_sine[1][1] = nmt->fsk_sine[1][0] + nmt->samples_per_bit;
	if (!nmt->fsk_sine[0][0]) {
		PDEBUG(DDSP, DEBUG_ERROR, "No memory!\n");
		return -ENOMEM;
	}

	/* generate sines */
	for (i = 0; i < nmt->samples_per_bit; i++) {
		nmt->fsk_sine[0][0][i] = TX_PEAK_FSK * sin(3.0 * PI * (double)i / (double)nmt->samples_per_bit); /* 1.5 waves */
		nmt->fsk_sine[0][1][i] = TX_PEAK_FSK * sin(2.0 * PI * (double)i / (double)nmt->samples_per_bit); /* 1 wave */
		nmt->fsk_sine[1][0][i] = -nmt->fsk_sine[0][0][i];
		nmt->fsk_sine[1][1][i] = -nmt->fsk_sine[0][1][i];
	}

	/* allocate ring buffers, one bit duration */
	spl = calloc(1, nmt->samples_per_bit * sizeof(*spl));
	if (!spl) {
		PDEBUG(DDSP, DEBUG_ERROR, "No memory!\n");
		return -ENOMEM;
	}
	nmt->fsk_filter_spl = spl;
	nmt->fsk_filter_bit = -1;

	/* allocate transmit buffer for a complete frame */
	spl = calloc(166, nmt->samples_per_bit * sizeof(*spl));
	if (!spl) {
		PDEBUG(DDSP, DEBUG_ERROR, "No memory!\n");
		return -ENOMEM;
	}
	nmt->frame_spl = spl;

	/* allocate ring buffer for supervisory signal detection */
	nmt->super_samples = (int)((double)nmt->sender.samplerate * SUPER_DURATION + 0.5);
	spl = calloc(166, nmt->super_samples * sizeof(*spl));
	if (!spl) {
		PDEBUG(DDSP, DEBUG_ERROR, "No memory!\n");
		return -ENOMEM;
	}
	nmt->super_filter_spl = spl;

	/* count symbols */
	for (i = 0; i < 2; i++) {
		coeff = 2.0 * cos(2.0 * PI * fsk_bits[i] / (double)nmt->sender.samplerate);
		nmt->fsk_coeff[i] = coeff * 32768.0;
		PDEBUG(DDSP, DEBUG_DEBUG, "coeff[%d] = %d\n", i, (int)nmt->fsk_coeff[i]);
	}

	/* count supervidory tones */
	for (i = 0; i < 5; i++) {
		coeff = 2.0 * cos(2.0 * PI * super_freq[i] / (double)nmt->sender.samplerate);
		nmt->super_coeff[i] = coeff * 32768.0;
		PDEBUG(DDSP, DEBUG_DEBUG, "supervisory coeff[%d] = %d\n", i, (int)nmt->super_coeff[i]);

		if (i < 4) {
			nmt->super_phaseshift256[i] = 256.0 / ((double)nmt->sender.samplerate / super_freq[i]);
			PDEBUG(DDSP, DEBUG_DEBUG, "phaseshift_super[%d] = %.4f\n", i, nmt->super_phaseshift256[i]);
		}
	}
	super_reset(nmt);

	/* dial tone */
	nmt->dial_phaseshift256 = 256.0 / ((double)nmt->sender.samplerate / DIALTONE_HZ);

	/* dtmf */
	dtmf_init(&nmt->dtmf, 8000);

	return 0;
}

/* Cleanup transceiver instance. */
void dsp_cleanup_sender(nmt_t *nmt)
{
	PDEBUG(DDSP, DEBUG_DEBUG, "Cleanup DSP for 'Sender'.\n");

	if (nmt->frame_spl) {
		free(nmt->frame_spl);
		nmt->frame_spl = NULL;
	}
	if (nmt->fsk_filter_spl) {
		free(nmt->fsk_filter_spl);
		nmt->fsk_filter_spl = NULL;
	}
	if (nmt->super_filter_spl) {
		free(nmt->super_filter_spl);
		nmt->super_filter_spl = NULL;
	}
}

/* Check for SYNC bits, then collect data bits */
static void fsk_receive_bit(nmt_t *nmt, int bit, double quality, double level)
{
	double frames_elapsed;

//	printf("bit=%d quality=%.4f\n", bit, quality);
	if (!nmt->fsk_filter_in_sync) {
		nmt->fsk_filter_sync = (nmt->fsk_filter_sync << 1) | bit;

		/* check if pattern 1010111100010010 matches */
		if (nmt->fsk_filter_sync != 0xaf12)
			return;
//		printf("sync\n");

		/* sync time */
		nmt->rx_sample_count_last = nmt->rx_sample_count_current;
		nmt->rx_sample_count_current = nmt->rx_sample_count - nmt->samples_per_bit * 26;

		/* rest sync register */
		nmt->fsk_filter_sync = 0;
		nmt->fsk_filter_in_sync = 1;
		nmt->fsk_filter_count = 0;
		nmt->fsk_filter_levelsum = 0;
		nmt->fsk_filter_qualitysum = 0;

		/* set muting of receive path */
		nmt->fsk_filter_mute = (int)((double)nmt->sender.samplerate * MUTE_DURATION);
		return;
	}

	/* read bits */
	nmt->fsk_filter_frame[nmt->fsk_filter_count++] = bit + '0';
	nmt->fsk_filter_levelsum += level;
	nmt->fsk_filter_qualitysum += quality;
	if (nmt->fsk_filter_count != 140)
		return;

	/* end of frame */
	nmt->fsk_filter_frame[140] = '\0';
	nmt->fsk_filter_in_sync = 0;

	/* send telegramm */
	frames_elapsed = (double)(nmt->rx_sample_count_current - nmt->rx_sample_count_last) / (double)(nmt->samples_per_bit * 166);
	nmt_receive_frame(nmt, nmt->fsk_filter_frame, nmt->fsk_filter_qualitysum / 140.0, nmt->fsk_filter_levelsum / 140.0, frames_elapsed);
}

char *show_level(int value)
{
	static char text[22];

	value /= 5;
	if (value < 0)
		value = 0;
	if (value > 20)
		value = 20;
	strcpy(text, "                     ");
	text[value] = '*';

	return text;
}

//#define DEBUG_MODULATOR
//#define DEBUG_FILTER
//#define DEBUG_QUALITY

/* Filter one chunk of audio an detect tone, quality and loss of signal.
 * The chunk is a window of 10ms. This window slides over audio stream
 * and is processed every 1ms. (one step) */
static inline void fsk_decode_step(nmt_t *nmt, int pos)
{
	double level, result[2], softbit, quality;
	int max;
	int16_t *spl;
	int bit;
	
	max = nmt->samples_per_bit;
	spl = nmt->fsk_filter_spl;

	/* count time in samples*/
	nmt->rx_sample_count += nmt->fsk_filter_step;

	level = audio_level(spl, max);
//	level = 0.63662 / 2.0;

	audio_goertzel(spl, max, pos, nmt->fsk_coeff, result, 2);

	/* calculate soft bit from both frequencies */
	softbit = (result[1] / level - result[0] / level + 1.0) / 2.0;
	/* scale it, since both filters overlap by some percent */
#define MIN_QUALITY 0.33
	softbit = (softbit - MIN_QUALITY) / (1.0 - MIN_QUALITY - MIN_QUALITY);
	if (softbit > 1)
		softbit = 1;
	if (softbit < 0)
		softbit = 0;
#ifdef DEBUG_FILTER
//	printf("|%s", show_level(result[0]/level*100));
//	printf("|%s| low=%.3f high=%.3f level=%d\n", show_level(result[1]/level*100), result[0]/level, result[1]/level, (int)level);
	printf("|%s| softbit=%.3f\n", show_level(softbit * 100), softbit);
#endif
	if (softbit > 0.5)
		bit = 1;
	else
		bit = 0;

	if (nmt->fsk_filter_bit != bit) {
#ifdef DEBUG_FILTER
		puts("bit change");
#endif
		/* if we have a bit change, reset sample counter to one half bit duration */
		nmt->fsk_filter_bit = bit;
		nmt->fsk_filter_sample = 5;
	} else if (--nmt->fsk_filter_sample == 0) {
#ifdef DEBUG_FILTER
		puts("sample");
#endif
		/* if sample counter bit reaches 0, we reset sample counter to one bit duration */
//		quality = result[bit] / level;
		if (softbit > 0.5)
			quality = softbit * 2.0 - 1.0;
		else
			quality = 1.0 - softbit * 2.0;
#ifdef DEBUG_QUALITY
		printf("|%s| quality=%.2f ", show_level(softbit * 100), quality);
		printf("|%s|\n", show_level(quality * 100));
#endif
		/* adjust level, so a peak level becomes 100% */
		fsk_receive_bit(nmt, bit, quality, level / 0.63662);
		nmt->fsk_filter_sample = 10;
	}
}

/* compare supervisory signal against noise floor on 3900 Hz */
static void super_decode(nmt_t *nmt, int16_t *samples, int length)
{
	int coeff[2];
	double result[2], quality;

	coeff[0] = nmt->super_coeff[nmt->supervisory - 1];
	coeff[1] = nmt->super_coeff[4]; /* noise floor detection */
	audio_goertzel(samples, length, 0, coeff, result, 2);

#if 0
	/* normalize levels */
	result[0] *= 32768.0 / (double)TX_PEAK_SUPER / 0.63662;
	result[1] *= 32768.0 / (double)TX_PEAK_SUPER / 0.63662;
	printf("signal=%.4f noise=%.4f\n", result[0], result[1]);
#endif

	quality = (result[0] - result[1]) / result[0];
	if (quality < 0)
		quality = 0;

	if (nmt->sender.loopback)
		PDEBUG(DDSP, DEBUG_NOTICE, "Supervisory level %.2f%% quality %.0f%%\n", result[0] / 0.63662 * 100.0, quality * 100.0);
	if (quality > 0.5) {
		if (nmt->super_detected == 0) {
			nmt->super_detect_count++;
			if (nmt->super_detect_count == SUPER_DETECT_COUNT) {
				nmt->super_detected = 1;
				nmt->super_detect_count = 0;
				PDEBUG(DDSP, DEBUG_DEBUG, "Supervisory signal detected with level=%.0f%%, quality=%.0f%%.\n", result[0] / 0.63662 * 100.0, quality * 100.0);
				nmt_rx_super(nmt, 1, quality);
			}
		} else
			nmt->super_detect_count = 0;
	} else {
		if (nmt->super_detected == 1) {
			nmt->super_detect_count++;
			if (nmt->super_detect_count == SUPER_DETECT_COUNT) {
				nmt->super_detected = 0;
				nmt->super_detect_count = 0;
				PDEBUG(DDSP, DEBUG_DEBUG, "Supervisory signal lost.\n");
				nmt_rx_super(nmt, 0, 0.0);
			}
		} else
			nmt->super_detect_count = 0;
	}
}

/* Reset supervisory detection states, so ongoing tone will be detected again. */
void super_reset(nmt_t *nmt)
{
	PDEBUG(DDSP, DEBUG_DEBUG, "Supervisory detector reset.\n");
	nmt->super_detected = 0;
	nmt->super_detect_count = 0;
}

/* Process received audio stream from radio unit. */
void sender_receive(sender_t *sender, int16_t *samples, int length)
{
	nmt_t *nmt = (nmt_t *) sender;
	int16_t *spl;
	int max, pos, step;
	int i;

	/* write received samples to decode buffer */
	max = nmt->super_samples;
	spl = nmt->super_filter_spl;
	pos = nmt->super_filter_pos;
	for (i = 0; i < length; i++) {
		spl[pos++] = samples[i];
		if (pos == max) {
			pos = 0;
			if (nmt->supervisory)
				super_decode(nmt, spl, max);
		}
	}
	nmt->super_filter_pos = pos;

	/* write received samples to decode buffer */
	max = nmt->samples_per_bit;
	pos = nmt->fsk_filter_pos;
	step = nmt->fsk_filter_step;
	spl = nmt->fsk_filter_spl;
	for (i = 0; i < length; i++) {
#ifdef DEBUG_MODULATOR
		printf("|%s|\n", show_level((int)((samples[i] / (double)TX_PEAK_FSK) * 50)+50));
#endif
		spl[pos++] = samples[i];
		if (nmt->fsk_filter_mute) {
			samples[i] = 0;
			nmt->fsk_filter_mute--;
		}
		if (pos == max)
			pos = 0;
		/* if filter step has been reched */
		if (!(pos % step)) {
			fsk_decode_step(nmt, pos);
		}
	}
	nmt->fsk_filter_pos = pos;

	if ((nmt->dsp_mode == DSP_MODE_AUDIO || nmt->dsp_mode == DSP_MODE_DTMF)
	 && nmt->sender.callref) {
		int16_t down[length]; /* more than enough */
		int count;

		count = samplerate_downsample(&nmt->sender.srstate, samples, length, down);
		if (nmt->compander)
			expand_audio(&nmt->cstate, down, count);
		if (nmt->dsp_mode == DSP_MODE_DTMF)
			dtmf_tone(&nmt->dtmf, down, count);
		spl = nmt->sender.rxbuf;
		pos = nmt->sender.rxbuf_pos;
		for (i = 0; i < count; i++) {
			spl[pos++] = down[i];
			if (pos == 160) {
				call_tx_audio(nmt->sender.callref, spl, 160);
				pos = 0;
			}
		}
		nmt->sender.rxbuf_pos = pos;
	} else
		nmt->sender.rxbuf_pos = 0;
}

static int fsk_frame(nmt_t *nmt, int16_t *samples, int length)
{
	int16_t *spl;
	const char *frame;
	int i;
	int bit, polarity;
	int count, max;

next_frame:
	if (!nmt->frame) {
		/* request frame */
		frame = nmt_get_frame(nmt);
		if (!frame) {
			PDEBUG(DDSP, DEBUG_DEBUG, "Stop sending frames.\n");
			return length;
		}
		nmt->frame = 1;
		nmt->frame_pos = 0;
		spl = nmt->frame_spl;
		/* render frame */
		polarity = nmt->fsk_polarity;
		for (i = 0; i < 166; i++) {
			bit = (frame[i] == '1');
			memcpy(spl, nmt->fsk_sine[polarity][bit], nmt->samples_per_bit * sizeof(*spl));
			spl += nmt->samples_per_bit;
			/* flip polarity when we have 1.5 sine waves */
			if (bit == 0)
				polarity = 1 - polarity;
		}
		nmt->fsk_polarity = polarity;
	}

	/* send audio from frame */
	max = nmt->samples_per_bit * 166;
	count = max - nmt->frame_pos;
	if (count > length)
		count = length;
	spl = nmt->frame_spl + nmt->frame_pos;
	for (i = 0; i < count; i++) {
		*samples++ = *spl++;
	}
	length -= count;
	nmt->frame_pos += count;
	/* check for end of telegramm */
	if (nmt->frame_pos == max) {
		nmt->frame = 0;
		/* we need more ? */
		if (length)
			goto next_frame;
	}

	return length;
}

/* Generate audio stream with supervisory signal. Keep phase for next call of function. */
static void super_encode(nmt_t *nmt, int16_t *samples, int length)
{
        double phaseshift, phase;
	int32_t sample;
	int i;

	phaseshift = nmt->super_phaseshift256[nmt->supervisory - 1];
	phase = nmt->super_phase256;

	for (i = 0; i < length; i++) {
		sample = *samples;
		sample += dsp_sine_super[((uint8_t)phase) & 0xff];
		if (sample > 32767)
			sample = 32767;
		else if (sample < -32767)
			sample = -32767;
		*samples++ = sample;
		phase += phaseshift;
		if (phase >= 256)
			phase -= 256;
	}

	nmt->super_phase256 = phase;
}

/* Generate audio stream from dial tone. Keep phase for next call of function. */
static void dial_tone(nmt_t *nmt, int16_t *samples, int length)
{
        double phaseshift, phase;
	int i;

	phaseshift = nmt->dial_phaseshift256;
	phase = nmt->dial_phase256;

	for (i = 0; i < length; i++) {
		*samples++ = dsp_sine_dialtone[((uint8_t)phase) & 0xff];
		phase += phaseshift;
		if (phase >= 256)
			phase -= 256;
	}

	nmt->dial_phase256 = phase;
}

/* Provide stream of audio toward radio unit */
void sender_send(sender_t *sender, int16_t *samples, int length)
{
	nmt_t *nmt = (nmt_t *) sender;
	int len;

again:
	switch (nmt->dsp_mode) {
	case DSP_MODE_AUDIO:
	case DSP_MODE_DTMF:
		jitter_load(&nmt->sender.audio, samples, length);
		if (nmt->supervisory)
			super_encode(nmt, samples, length);
		break;
	case DSP_MODE_DIALTONE:
		dial_tone(nmt, samples, length);
		break;
	case DSP_MODE_SILENCE:
		memset(samples, 0, length * sizeof(*samples));
		break;
	case DSP_MODE_FRAME:
		/* Encode frame into audio stream. If frames have
		 * stopped, process again for rest of stream. */
		len = fsk_frame(nmt, samples, length);
		/* special case: add supervisory signal to frame at loop test */
		if (nmt->sender.loopback && nmt->supervisory)
			super_encode(nmt, samples, length);
		if (len) {
			samples += length - len;
			length = len;
			goto again;
		}
		break;
	}
}

void nmt_set_dsp_mode(nmt_t *nmt, enum dsp_mode mode)
{
	/* reset telegramm */
	if (mode == DSP_MODE_FRAME && nmt->dsp_mode != mode)
		nmt->frame = 0;
	nmt->dsp_mode = mode;
}

