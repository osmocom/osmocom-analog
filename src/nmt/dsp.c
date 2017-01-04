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

#define CHAN nmt->sender.kanal

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "../common/debug.h"
#include "../common/timer.h"
#include "../common/goertzel.h"
#include "nmt.h"
#include "transaction.h"
#include "dsp.h"

#define PI			M_PI

/* Notes on frequency deviation of supervidory signal:
 *
 * The FSK deviation at 1500 Hz is 3.5 KHz. If we use a level of 10000
 * The supervisory deviation shall be 0.3 KHz: 10000 / 3.5 * 0.3 = 857
 * Supervisory is raised by pre-emphasis by factor 2.68 (4015 / 1500),
 * so we need to lower it: 857 / 2.68 = 320
 */

/* signaling */
#define COMPANDOR_0DB		32767	/* works quite well */
#define TX_PEAK_FSK		10000.0	/* peak amplitude of signaling FSK +-3.5 KHz @ 1500 Hz */
#define TX_PEAK_SUPER		(TX_PEAK_FSK / 3.5 * 0.3 / 2.68) /* peak amplitude of supervisory signal +-0.3 KHz @ 4015 Hz */
#define BIT_RATE		1200	/* baud rate */
#define STEPS_PER_BIT		10	/* step every 1/12000 sec */
#define DIALTONE_HZ		425.0	/* dial tone frequency */
#define TX_PEAK_DIALTONE	16000	/* dial tone peak */
#define SUPER_DURATION		0.25	/* duration of supervisory signal measurement */
#define SUPER_DETECT_COUNT	4	/* number of measures to detect supervisory signal */
#define MUTE_DURATION		0.280	/* a tiny bit more than two frames */

/* two signaling tones */
static double fsk_freq[2] = {
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
uint16_t dsp_tone_bit[2][2][256]; /* polarity, bit, phase */
uint16_t dsp_sine_super[256];
uint16_t dsp_sine_dialtone[256];

/* global init for FSK */
void dsp_init(void)
{
	int i;
	double s;

	PDEBUG(DDSP, DEBUG_DEBUG, "Generating sine table for supervisory signal.\n");
	for (i = 0; i < 256; i++) {
		s = sin((double)i / 256.0 * 2.0 * PI);
		/* supervisor sine */
		dsp_sine_super[i] = (int)(s * TX_PEAK_SUPER);
		/* dialtone sine */
		dsp_sine_dialtone[i] = (int)(s * TX_PEAK_DIALTONE);
		/* bit(1) 1 cycle */
		dsp_tone_bit[0][1][i] = (int)(s * TX_PEAK_FSK);
		dsp_tone_bit[1][1][i] = (int)(-s * TX_PEAK_FSK);
		/* bit(0) 1.5 cycles */
		s = sin((double)i / 256.0 * 3.0 * PI);
		dsp_tone_bit[0][0][i] = (int)(s * TX_PEAK_FSK);
		dsp_tone_bit[1][0][i] = (int)(-s * TX_PEAK_FSK);
	}
}

/* Init FSK of transceiver */
int dsp_init_sender(nmt_t *nmt)
{
	double coeff;
	int16_t *spl;
	int i;

	/* attack (3ms) and recovery time (13.5ms) according to NMT specs */
	init_compandor(&nmt->cstate, 8000, 3.0, 13.5, COMPANDOR_0DB);

	/* this should not happen. it is implied by previous check */
	if (nmt->supervisory && nmt->sender.samplerate < 12000) {
		PDEBUG(DDSP, DEBUG_ERROR, "Sample rate must be at least 12000 Hz to process FSK+supervisory signal.\n");
		return -EINVAL;
	}

	PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Init DSP for Transceiver.\n");

	PDEBUG(DDSP, DEBUG_DEBUG, "Using FSK level of %.0f (3.5 KHz deviation @ 1500 Hz)\n", TX_PEAK_FSK);
	PDEBUG(DDSP, DEBUG_DEBUG, "Using Supervisory level of %.0f (0.3 KHz deviation @ 4015 Hz)\n", TX_PEAK_SUPER);

	nmt->fsk_samples_per_bit = (double)nmt->sender.samplerate / (double)BIT_RATE;
	nmt->fsk_bits_per_sample = 1.0 / nmt->fsk_samples_per_bit;
	PDEBUG(DDSP, DEBUG_DEBUG, "Use %.4f samples for full bit duration @ %d.\n", nmt->fsk_samples_per_bit, nmt->sender.samplerate);

	/* allocate ring buffers, one bit duration */
	nmt->fsk_filter_size = floor(nmt->fsk_samples_per_bit); /* buffer holds one bit (rounded down) */
	spl = calloc(1, nmt->fsk_filter_size * sizeof(*spl));
	if (!spl) {
		PDEBUG(DDSP, DEBUG_ERROR, "No memory!\n");
		return -ENOMEM;
	}
	nmt->fsk_filter_spl = spl;
	nmt->fsk_filter_bit = -1;

	/* allocate transmit buffer for a complete frame, add 10 to be safe */
	nmt->frame_size = 166.0 * (double)nmt->fsk_samples_per_bit + 10;
	spl = calloc(nmt->frame_size, sizeof(*spl));
	if (!spl) {
		PDEBUG(DDSP, DEBUG_ERROR, "No memory!\n");
		return -ENOMEM;
	}
	nmt->frame_spl = spl;

	/* allocate DMS transmit buffer for a complete frame, add 10 to be safe */
	nmt->dms.frame_size = 127.0 * (double)nmt->fsk_samples_per_bit + 10;
	spl = calloc(nmt->dms.frame_size, sizeof(*spl));
	if (!spl) {
		PDEBUG(DDSP, DEBUG_ERROR, "No memory!\n");
		return -ENOMEM;
	}
	nmt->dms.frame_spl = spl;

	/* allocate ring buffer for supervisory signal detection */
	nmt->super_samples = (int)((double)nmt->sender.samplerate * SUPER_DURATION + 0.5);
	spl = calloc(1, nmt->super_samples * sizeof(*spl));
	if (!spl) {
		PDEBUG(DDSP, DEBUG_ERROR, "No memory!\n");
		return -ENOMEM;
	}
	nmt->super_filter_spl = spl;

	/* count symbols */
	for (i = 0; i < 2; i++) {
		coeff = 2.0 * cos(2.0 * PI * fsk_freq[i] / (double)nmt->sender.samplerate);
		nmt->fsk_coeff[i] = coeff * 32768.0;
		PDEBUG(DDSP, DEBUG_DEBUG, "fsk_coeff[%d] = %d\n", i, (int)nmt->fsk_coeff[i]);
	}
	nmt->fsk_phaseshift256 = 256.0 / nmt->fsk_samples_per_bit;
	PDEBUG(DDSP, DEBUG_DEBUG, "fsk_phaseshift = %.4f\n", nmt->fsk_phaseshift256);

	/* count supervidory tones */
	for (i = 0; i < 5; i++) {
		coeff = 2.0 * cos(2.0 * PI * super_freq[i] / (double)nmt->sender.samplerate);
		nmt->super_coeff[i] = coeff * 32768.0;
		PDEBUG(DDSP, DEBUG_DEBUG, "supervisory coeff[%d] = %d\n", i, (int)nmt->super_coeff[i]);

		if (i < 4) {
			nmt->super_phaseshift256[i] = 256.0 / ((double)nmt->sender.samplerate / super_freq[i]);
			PDEBUG(DDSP, DEBUG_DEBUG, "super_phaseshift[%d] = %.4f\n", i, nmt->super_phaseshift256[i]);
		}
	}
	super_reset(nmt);

	/* dial tone */
	nmt->dial_phaseshift256 = 256.0 / ((double)nmt->sender.samplerate / DIALTONE_HZ);
	PDEBUG(DDSP, DEBUG_DEBUG, "dial_phaseshift = %.4f\n", nmt->dial_phaseshift256);

	/* dtmf */
	dtmf_init(&nmt->dtmf, 8000);

	return 0;
}

/* Cleanup transceiver instance. */
void dsp_cleanup_sender(nmt_t *nmt)
{
	PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Cleanup DSP for Transceiver.\n");

	if (nmt->frame_spl) {
		free(nmt->frame_spl);
		nmt->frame_spl = NULL;
	}
	if (nmt->dms.frame_spl) {
		free(nmt->dms.frame_spl);
		nmt->dms.frame_spl = NULL;
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
	int i;

//	printf("bit=%d quality=%.4f\n", bit, quality);
	if (!nmt->fsk_filter_in_sync) {
		nmt->fsk_filter_sync = (nmt->fsk_filter_sync << 1) | bit;

		/* level and quality */
		nmt->fsk_filter_level[nmt->fsk_filter_count & 0xff] = level;
		nmt->fsk_filter_quality[nmt->fsk_filter_count & 0xff] = quality;
		nmt->fsk_filter_count++;

		/* check if pattern 1010111100010010 matches */
		if (nmt->fsk_filter_sync != 0xaf12)
			return;

		/* average level and quality */
		level = quality = 0;
		for (i = 0; i < 16; i++) {
			level += nmt->fsk_filter_level[(nmt->fsk_filter_count - 1 - i) & 0xff];
			quality += nmt->fsk_filter_quality[(nmt->fsk_filter_count - 1 - i) & 0xff];
		}
		level /= 16.0; quality /= 16.0;
//		printf("sync (level = %.2f, quality = %.2f\n", level, quality);

		/* do not accept garbage */
		if (quality < 0.65)
			return;

		/* sync time */
		nmt->rx_bits_count_last = nmt->rx_bits_count_current;
		nmt->rx_bits_count_current = nmt->rx_bits_count - 26.0;

		/* rest sync register */
		nmt->fsk_filter_sync = 0;
		nmt->fsk_filter_in_sync = 1;
		nmt->fsk_filter_count = 0;

		/* set muting of receive path */
		nmt->fsk_filter_mute = (int)((double)nmt->sender.samplerate * MUTE_DURATION);
		return;
	}

	/* read bits */
	nmt->fsk_filter_frame[nmt->fsk_filter_count] = bit + '0';
	nmt->fsk_filter_level[nmt->fsk_filter_count] = level;
	nmt->fsk_filter_quality[nmt->fsk_filter_count] = quality;
	if (++nmt->fsk_filter_count != 140)
		return;

	/* end of frame */
	nmt->fsk_filter_frame[140] = '\0';
	nmt->fsk_filter_in_sync = 0;

	/* average level and quality */
	level = quality = 0;
	for (i = 0; i < 140; i++) {
		level += nmt->fsk_filter_level[i];
		quality += nmt->fsk_filter_quality[i];
	}
	level /= 140.0; quality /= 140.0;

	/* send telegramm */
	frames_elapsed = (nmt->rx_bits_count_current - nmt->rx_bits_count_last) / 166.0;
	/* convert level so that received level at TX_PEAK_FSK results in 1.0 (100%) */
	nmt_receive_frame(nmt, nmt->fsk_filter_frame, quality, level, frames_elapsed);
}

//#define DEBUG_MODULATOR
//#define DEBUG_FILTER
//#define DEBUG_QUALITY

/* Filter one chunk of audio an detect tone, quality and loss of signal.
 * The chunk is a window of 1/1200s. This window slides over audio stream
 * and is processed every 1/12000s. (one step) */
static inline void fsk_decode_step(nmt_t *nmt, int pos)
{
	double level, result[2], softbit, quality;
	int max;
	int16_t *spl;
	int bit;
	
	max = nmt->fsk_filter_size;
	spl = nmt->fsk_filter_spl;

	/* count time in bits */
	nmt->rx_bits_count += 0.1;

	level = audio_level(spl, max);
	/* limit level to prevent division by zero */
	if (level < 0.01)
		level = 0.01;
//	level = 0.63662 / 2.0;

	audio_goertzel(spl, max, pos, nmt->fsk_coeff, result, 2);

	/* calculate soft bit from both frequencies */
	softbit = (result[1] / level - result[0] / level + 1.0) / 2.0;
//printf("%.3f: %.3f\n", level, softbit);
	/* scale it, since both filters overlap by some percent */
#define MIN_QUALITY 0.33
	softbit = (softbit - MIN_QUALITY) / (1.0 - MIN_QUALITY - MIN_QUALITY);
#ifdef DEBUG_FILTER
//	printf("|%s", debug_amplitude(result[0]/level));
//	printf("|%s| low=%.3f high=%.3f level=%d\n", debug_amplitude(result[1]/level), result[0]/level, result[1]/level, (int)level);
	printf("|%s| softbit=%.3f\n", debug_amplitude(softbit), softbit);
#endif
	if (softbit > 1)
		softbit = 1;
	if (softbit < 0)
		softbit = 0;
	if (softbit > 0.5)
		bit = 1;
	else
		bit = 0;

	if (nmt->fsk_filter_bit != bit) {
		/* if we have a bit change, reset sample counter to one half bit duration */
#ifdef DEBUG_FILTER
		puts("bit change");
#endif
		nmt->fsk_filter_bit = bit;
		nmt->fsk_filter_sample = 5;
	} else if (--nmt->fsk_filter_sample == 0) {
		/* if sample counter bit reaches 0, we reset sample counter to one bit duration */
#ifdef DEBUG_FILTER
		puts("sample");
#endif
//		quality = result[bit] / level;
		if (softbit > 0.5)
			quality = softbit * 2.0 - 1.0;
		else
			quality = 1.0 - softbit * 2.0;
#ifdef DEBUG_QUALITY
		printf("|%s| quality=%.2f ", debug_amplitude(softbit), quality);
		printf("|%s|\n", debug_amplitude(quality));
#endif
		/* adjust level, so a peak level becomes 100% */
		fsk_receive_bit(nmt, bit, quality, level / 0.63662 * 32768.0 / TX_PEAK_FSK);
		if (nmt->dms_call)
			fsk_receive_bit_dms(nmt, bit, quality, level / 0.63662 * 32768.0 / TX_PEAK_FSK);
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
	result[0] *= 32768.0 / TX_PEAK_SUPER / 0.63662;
	result[1] *= 32768.0 / TX_PEAK_SUPER / 0.63662;
	printf("signal=%.4f noise=%.4f\n", result[0], result[1]);
#endif

	quality = (result[0] - result[1]) / result[0];
	if (quality < 0)
		quality = 0;

	if (nmt->state == STATE_ACTIVE)
		PDEBUG_CHAN(DDSP, DEBUG_NOTICE, "Supervisory level %.0f%% quality %.0f%%\n", result[0] / 0.63662 * 32768.0 / TX_PEAK_SUPER * 100.0, quality * 100.0);
	if (quality > 0.5) {
		if (nmt->super_detected == 0) {
			nmt->super_detect_count++;
			if (nmt->super_detect_count == SUPER_DETECT_COUNT) {
				nmt->super_detected = 1;
				nmt->super_detect_count = 0;
				PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Supervisory signal detected with level=%.0f%%, quality=%.0f%%.\n", result[0] / 0.63662 * 32768.0 / TX_PEAK_SUPER * 100.0, quality * 100.0);
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
				PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Supervisory signal lost.\n");
				nmt_rx_super(nmt, 0, 0.0);
			}
		} else
			nmt->super_detect_count = 0;
	}
}

/* Reset supervisory detection states, so ongoing tone will be detected again. */
void super_reset(nmt_t *nmt)
{
	PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Supervisory detector reset.\n");
	nmt->super_detected = 0;
	nmt->super_detect_count = 0;
}

/* Process received audio stream from radio unit. */
void sender_receive(sender_t *sender, int16_t *samples, int length)
{
	nmt_t *nmt = (nmt_t *) sender;
	int16_t *spl;
	int max, pos;
	double step, bps;
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
	max = nmt->fsk_filter_size;
	pos = nmt->fsk_filter_pos;
	step = nmt->fsk_filter_step;
	bps = nmt->fsk_bits_per_sample;
	spl = nmt->fsk_filter_spl;
	for (i = 0; i < length; i++) {
#ifdef DEBUG_MODULATOR
		printf("|%s|\n", debug_amplitude((double)samples[i] / TX_PEAK_FSK / 2.0));
#endif
		/* write into ring buffer */
		spl[pos++] = samples[i];
		if (pos == max)
			pos = 0;
		/* muting audio while receiving frame */
		if (nmt->fsk_filter_mute && !nmt->sender.loopback) {
			samples[i] = 0;
			nmt->fsk_filter_mute--;
		}
		/* if 1/10th of a bit duration is reached, decode buffer */
		step += bps;
		if (step >= 0.1) {
			step -= 0.1;
			fsk_decode_step(nmt, pos);
		}
	}
	nmt->fsk_filter_step = step;
	nmt->fsk_filter_pos = pos;

	if ((nmt->dsp_mode == DSP_MODE_AUDIO || nmt->dsp_mode == DSP_MODE_DTMF)
	 && nmt->trans && nmt->trans->callref) {
		int16_t down[length]; /* more than enough */
		int count;

		count = samplerate_downsample(&nmt->sender.srstate, samples, length, down);
		if (nmt->compandor)
			expand_audio(&nmt->cstate, down, count);
		if (nmt->dsp_mode == DSP_MODE_DTMF)
			dtmf_tone(&nmt->dtmf, down, count);
		spl = nmt->sender.rxbuf;
		pos = nmt->sender.rxbuf_pos;
		for (i = 0; i < count; i++) {
			spl[pos++] = down[i];
			if (pos == 160) {
				call_tx_audio(nmt->trans->callref, spl, 160);
				pos = 0;
			}
		}
		nmt->sender.rxbuf_pos = pos;
	} else
		nmt->sender.rxbuf_pos = 0;
}

/* render frame */
int fsk_render_frame(nmt_t *nmt, const char *frame, int length, int16_t *sample)
{
	int bit, polarity;
        double phaseshift, phase;
	int count = 0, i;

	polarity = nmt->fsk_polarity;
	phaseshift = nmt->fsk_phaseshift256;
	phase = nmt->fsk_phase256;
	for (i = 0; i < length; i++) {
		bit = (frame[i] == '1');
		do {
			*sample++ = dsp_tone_bit[polarity][bit][((uint8_t)phase) & 0xff];
			count++;
			phase += phaseshift;
		} while (phase < 256.0);
		phase -= 256.0;
		/* flip polarity when we have 1.5 sine waves */
		if (bit == 0)
			polarity = 1 - polarity;
	}
	nmt->fsk_phase256 = phase;
	nmt->fsk_polarity = polarity;

	/* return number of samples created for frame */
	return count;
}

static int fsk_frame(nmt_t *nmt, int16_t *samples, int length)
{
	const char *frame;
	int16_t *spl;
	int i;
	int count, max;

next_frame:
	if (!nmt->frame_length) {
		/* request frame */
		frame = nmt_get_frame(nmt);
		if (!frame) {
			PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Stop sending frames.\n");
			return length;
		}
		/* render frame */
		nmt->frame_length = fsk_render_frame(nmt, frame, 166, nmt->frame_spl);
		nmt->frame_pos = 0;
		if (nmt->frame_length > nmt->frame_size) {
			PDEBUG_CHAN(DDSP, DEBUG_ERROR, "Frame exceeds buffer, please fix!\n");
			abort();
		}
	}

	/* send audio from frame */
	max = nmt->frame_length;
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
		nmt->frame_length = 0;
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
		jitter_load(&nmt->sender.dejitter, samples, length);
		/* send after dejitter, so audio is flushed */
		if (nmt->dms.frame_valid) {
			fsk_dms_frame(nmt, samples, length);
			break;
		}
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

const char *nmt_dsp_mode_name(enum dsp_mode mode)
{
        static char invalid[16];

	switch (mode) {
	case DSP_MODE_SILENCE:
		return "SILENCE";
	case DSP_MODE_DIALTONE:
		return "DIALTONE";
	case DSP_MODE_AUDIO:
		return "AUDIO";
	case DSP_MODE_FRAME:
		return "FRAME";
	case DSP_MODE_DTMF:
		return "DTMF";
	}

	sprintf(invalid, "invalid(%d)", mode);
	return invalid;
}

void nmt_set_dsp_mode(nmt_t *nmt, enum dsp_mode mode)
{
	/* reset telegramm */
	if (mode == DSP_MODE_FRAME && nmt->dsp_mode != mode)
		nmt->frame_length = 0;

	PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "DSP mode %s -> %s\n", nmt_dsp_mode_name(nmt->dsp_mode), nmt_dsp_mode_name(mode));
	nmt->dsp_mode = mode;
}

