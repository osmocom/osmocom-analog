/* Radiocom 2000 audio processing
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

#define CHAN r2000->sender.kanal

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "../common/sample.h"
#include "../common/debug.h"
#include "../common/timer.h"
#include "r2000.h"
#include "dsp.h"

#define PI			M_PI

/* Notes on TX_PEAK_FSK level:
 *
 * Applies similar to NMT, read it there!
 *
 * I assume that the deviation at 1800 Hz (Bit 0) is +-1700 Hz.
 *
 * Notes on TX_PEAK_SUPER level:
 *
 * No emphasis applies (done afterwards), so it is 300 Hz deviation.
 */

/* signaling */
#define MAX_DEVIATION		2500.0
#define MAX_MODULATION		2550.0
#define DBM0_DEVIATION		1500.0	/* deviation of dBm0 at 1 kHz */
#define COMPANDOR_0DB		1.0	/* A level of 0dBm (1.0) shall be unaccected */
#define TX_PEAK_FSK		(1700.0 / 1800.0 * 1000.0 / DBM0_DEVIATION) /* with emphasis */
#define TX_PEAK_SUPER		(300.0 / DBM0_DEVIATION) /* no emphasis */
#define BIT_RATE		1200.0
#define SUPER_RATE		50.0
#define FILTER_STEP		0.002	/* step every 2 ms */
#define MAX_DISPLAY		1.4	/* something above dBm0 */

/* two signaling tones */
static double super_bits[2] = {
	136.0,
	164.0,
};

/* table for fast sine generation */
static sample_t super_sine[65536];

/* global init for FFSK */
void dsp_init(void)
{
	int i;

	ffsk_global_init(TX_PEAK_FSK);

	PDEBUG(DDSP, DEBUG_DEBUG, "Generating sine table.\n");
	for (i = 0; i < 65536; i++) {
		super_sine[i] = sin((double)i / 65536.0 * 2.0 * PI) * TX_PEAK_SUPER;
	}
}

static void fsk_receive_bit(void *inst, int bit, double quality, double level);

/* Init FSK of transceiver */
int dsp_init_sender(r2000_t *r2000)
{
	sample_t *spl;
	double fsk_samples_per_bit;
	int i;

	/* attack (3ms) and recovery time (13.5ms) according to NMT specs */
	init_compandor(&r2000->cstate, 8000, 3.0, 13.5, COMPANDOR_0DB);

	PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Init DSP for Transceiver.\n");

	/* set modulation parameters */
	sender_set_fm(&r2000->sender, MAX_DEVIATION, MAX_MODULATION, DBM0_DEVIATION, MAX_DISPLAY);

	PDEBUG(DDSP, DEBUG_DEBUG, "Using FSK level of %.3f\n", TX_PEAK_FSK);

	/* init ffsk */
	if (ffsk_init(&r2000->ffsk, r2000, fsk_receive_bit, r2000->sender.kanal, r2000->sender.samplerate) < 0) {
		PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "FFSK init failed!\n");
		return -EINVAL;
	}
	if (r2000->sender.loopback)
		r2000->rx_max = 176;
	else
		r2000->rx_max = 144;

	/* allocate transmit buffer for a complete frame, add 10 to be safe */

	fsk_samples_per_bit = (double)r2000->sender.samplerate / BIT_RATE;
	r2000->frame_size = 208.0 * fsk_samples_per_bit + 10;
	spl = calloc(r2000->frame_size, sizeof(*spl));
	if (!spl) {
		PDEBUG(DDSP, DEBUG_ERROR, "No memory!\n");
		return -ENOMEM;
	}
	r2000->frame_spl = spl;

	/* strange: better quality with window size of two bits */
	r2000->super_samples_per_window = (double)r2000->sender.samplerate / SUPER_RATE * 2.0;
	r2000->super_filter_step = (double)r2000->sender.samplerate * FILTER_STEP;
	r2000->super_size = 20.0 * r2000->super_samples_per_window + 10;
	PDEBUG(DDSP, DEBUG_DEBUG, "Using %d samples per filter step for supervisory signal.\n", r2000->super_filter_step);
	spl = calloc(r2000->super_size, sizeof(*spl));
	if (!spl) {
		PDEBUG(DDSP, DEBUG_ERROR, "No memory!\n");
		return -ENOMEM;
	}
	r2000->super_spl = spl;
	spl = calloc(1, r2000->super_samples_per_window * sizeof(*spl));
	if (!spl) {
		PDEBUG(DDSP, DEBUG_ERROR, "No memory!\n");
		return -ENOMEM;
	}
	r2000->super_filter_spl = spl;
	r2000->super_filter_bit = -1;

	/* count supervisory symbols */
	for (i = 0; i < 2; i++) {
		audio_goertzel_init(&r2000->super_goertzel[i], super_bits[i], r2000->sender.samplerate);
		r2000->super_phaseshift65536[i] = 65536.0 / ((double)r2000->sender.samplerate / super_bits[i]);
		PDEBUG(DDSP, DEBUG_DEBUG, "phaseshift[%d] = %.4f\n", i, r2000->super_phaseshift65536[i]);
	}
	r2000->super_bittime = SUPER_RATE / (double)r2000->sender.samplerate;

	return 0;
}

/* Cleanup transceiver instance. */
void dsp_cleanup_sender(r2000_t *r2000)
{
	PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Cleanup DSP for Transceiver.\n");

	ffsk_cleanup(&r2000->ffsk);

	if (r2000->frame_spl) {
		free(r2000->frame_spl);
		r2000->frame_spl = NULL;
	}
	if (r2000->super_spl) {
		free(r2000->super_spl);
		r2000->super_spl = NULL;
	}
	if (r2000->super_filter_spl) {
		free(r2000->super_filter_spl);
		r2000->super_filter_spl = NULL;
	}
}

/* Check for SYNC bits, then collect data bits */
static void fsk_receive_bit(void *inst, int bit, double quality, double level)
{
	r2000_t *r2000 = (r2000_t *)inst;
//	uint64_t frames_elapsed;
	int i;

	/* normalize FSK level */
	level /= TX_PEAK_FSK;

	r2000->rx_bits_count++;

//	printf("bit=%d quality=%.4f\n", bit, quality);
	if (!r2000->rx_in_sync) {
		r2000->rx_sync = (r2000->rx_sync << 1) | bit;

		/* level and quality */
		r2000->rx_level[r2000->rx_count & 0xff] = level;
		r2000->rx_quality[r2000->rx_count & 0xff] = quality;
		r2000->rx_count++;

		/* check if pattern 1010111100010010 matches */
		if (r2000->rx_sync != 0xaf12)
			return;

		/* average level and quality */
		level = quality = 0;
		for (i = 0; i < 16; i++) {
			level += r2000->rx_level[(r2000->rx_count - 1 - i) & 0xff];
			quality += r2000->rx_quality[(r2000->rx_count - 1 - i) & 0xff];
		}
		level /= 16.0; quality /= 16.0;
//		printf("sync (level = %.2f, quality = %.2f\n", level, quality);

		/* do not accept garbage */
		if (quality < 0.65)
			return;

		/* sync time */
		r2000->rx_bits_count_last = r2000->rx_bits_count_current;
		r2000->rx_bits_count_current = r2000->rx_bits_count - 32.0;

		/* rest sync register */
		r2000->rx_sync = 0;
		r2000->rx_in_sync = 1;
		r2000->rx_count = 0;

		return;
	}

	/* read bits */
	r2000->rx_frame[r2000->rx_count] = bit + '0';
	r2000->rx_level[r2000->rx_count] = level;
	r2000->rx_quality[r2000->rx_count] = quality;
	if (++r2000->rx_count != r2000->rx_max)
		return;

	/* end of frame */
	r2000->rx_frame[r2000->rx_max] = '\0';
	r2000->rx_in_sync = 0;

	/* average level and quality */
	level = quality = 0;
	for (i = 0; i < r2000->rx_max; i++) {
		level += r2000->rx_level[i];
		quality += r2000->rx_quality[i];
	}
	level /= (double)r2000->rx_max; quality /= (double)r2000->rx_max;

	/* send frame to upper layer */
	r2000_receive_frame(r2000, r2000->rx_frame, quality, level);
}

static void super_receive_bit(r2000_t *r2000, int bit, double level, double quality)
{
	int i;

	/* normalize supervisory level */
	level /= TX_PEAK_SUPER;

	/* store bit */
	r2000->super_rx_word = (r2000->super_rx_word << 1) | bit;
	r2000->super_rx_level[r2000->super_rx_index] = level;
	r2000->super_rx_quality[r2000->super_rx_index] = quality;
	r2000->super_rx_index = (r2000->super_rx_index + 1) % 20;

//	printf("%d  ->  %05x\n", bit, r2000->super_rx_word & 0xfffff);
	/* check for sync 0100000000 01xxxxxxx1 */
	if ((r2000->super_rx_word & 0xfff01) != 0x40101)
		return;

	/* average level and quality */
	level = quality = 0;
	for (i = 0; i < 20; i++) {
		level += r2000->super_rx_level[i];
		quality += r2000->super_rx_quality[i];
	}
	level /= 20.0; quality /= 20.0;

	/* send received supervisory digit to call control */
	r2000_receive_super(r2000, (r2000->super_rx_word >> 1) & 0x7f, quality, level);
}

//#define DEBUG_FILTER
//#define DEBUG_QUALITY

/* demodulate supervisory signal
 * filter one chunk, that is 2ms long (1/10th of a bit) */
static inline void super_decode_step(r2000_t *r2000, int pos)
{
	double level, result[2], softbit, quality;
	int max;
	sample_t *spl;
	int bit;

	max = r2000->super_samples_per_window;
	spl = r2000->super_filter_spl;

	level = audio_level(spl, max);

	audio_goertzel(r2000->super_goertzel, spl, max, pos, result, 2);

	/* calculate soft bit from both frequencies */
	softbit = (result[1] / level - result[0] / level + 1.0) / 2.0;
//	/* scale it, since both filters overlap by some percent */
//#define MIN_QUALITY 0.08
//	softbit = (softbit - MIN_QUALITY) / (0.850 - MIN_QUALITY - MIN_QUALITY);
	if (softbit > 1)
		softbit = 1;
	if (softbit < 0)
		softbit = 0;
#ifdef DEBUG_FILTER
	printf("|%s", debug_amplitude(result[0]/level));
	printf("|%s| low=%.3f high=%.3f level=%d\n", debug_amplitude(result[1]/level), result[0]/level, result[1]/level, (int)level);
#endif
	if (softbit > 0.5)
		bit = 1;
	else
		bit = 0;

//	quality = result[bit] / level;
	if (softbit > 0.5)
		quality = softbit * 2.0 - 1.0;
	else
		quality = 1.0 - softbit * 2.0;

	/* scale quality, because filters overlap */
	quality /= 0.80;

	if (r2000->super_filter_bit != bit) {
#ifdef DEBUG_FILTER
		puts("bit change");
#endif
		r2000->super_filter_bit = bit;
#if 0
		/* If we have a bit change, move sample counter towards one half bit duration.
		 * We may have noise, so the bit change may be wrong or not at the correct place.
		 * This can cause bit slips.
		 * Therefore we change the sample counter only slightly, so bit slips may not
		 * happen so quickly.
		 */
		if (r2000->super_filter_sample < 5)
			r2000->super_filter_sample++;
		if (r2000->super_filter_sample > 5)
			r2000->super_filter_sample--;
#else
		/* directly center the sample position, because we don't have any sync sequence */ 
		r2000->super_filter_sample = 5;
#endif

	} else if (--r2000->super_filter_sample == 0) {
		/* if sample counter bit reaches 0, we reset sample counter to one bit duration */
#ifdef DEBUG_QUALITY
		printf("|%s| quality=%.2f ", debug_amplitude(softbit), quality);
		printf("|%s|\n", debug_amplitude(quality);
#endif
		/* adjust level, so we get peak of sine curve */
		super_receive_bit(r2000, bit, level / 0.63662, quality);
		r2000->super_filter_sample = 10;
	}
}

/* get audio chunk out of received stream */
void super_receive(r2000_t *r2000, sample_t *samples, int length)
{
	sample_t *spl;
	int max, pos, step;
	int i;
	/* write received samples to decode buffer */
	max = r2000->super_samples_per_window;
	pos = r2000->super_filter_pos;
	step = r2000->super_filter_step;
	spl = r2000->super_filter_spl;
	for (i = 0; i < length; i++) {
		spl[pos++] = samples[i];
		if (pos == max)
			pos = 0;
		/* if filter step has been reched */
		if (!(pos % step)) {
			super_decode_step(r2000, pos);
		}
	}
	r2000->super_filter_pos = pos;
}

/* Process received audio stream from radio unit. */
void sender_receive(sender_t *sender, sample_t *samples, int length)
{
	r2000_t *r2000 = (r2000_t *) sender;
	sample_t *spl;
	int pos;
	int i;

	/* do dc filter */
	if (r2000->de_emphasis)
		dc_filter(&r2000->estate, samples, length);

	/* supervisory signal */
	if (r2000->dsp_mode == DSP_MODE_AUDIO_TX
	 || r2000->dsp_mode == DSP_MODE_AUDIO_TX_RX
	 || r2000->sender.loopback)
		super_receive(r2000, samples, length);

	/* do de-emphasis */
	if (r2000->de_emphasis)
		de_emphasis(&r2000->estate, samples, length);

	/* fsk signal */
	ffsk_receive(&r2000->ffsk, samples, length);

	/* we must have audio mode for both ways and a call */
	if (r2000->dsp_mode == DSP_MODE_AUDIO_TX_RX
	 && r2000->callref) {
		int count;

		count = samplerate_downsample(&r2000->sender.srstate, samples, length);
#if 0
		/* compandor only in direction REL->MS */
		if (r2000->compandor)
			expand_audio(&r2000->cstate, samples, count);
#endif
		spl = r2000->sender.rxbuf;
		pos = r2000->sender.rxbuf_pos;
		for (i = 0; i < count; i++) {
			spl[pos++] = samples[i];
			if (pos == 160) {
				call_tx_audio(r2000->callref, spl, 160);
				pos = 0;
			}
		}
		r2000->sender.rxbuf_pos = pos;
	} else
		r2000->sender.rxbuf_pos = 0;
}

static int fsk_frame(r2000_t *r2000, sample_t *samples, int length)
{
	const char *frame;
	sample_t *spl;
	int i;
	int count, max;

next_frame:
	if (!r2000->frame_length) {
		/* request frame */
		frame = r2000_get_frame(r2000);
		if (!frame) {
			PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Stop sending frames.\n");
			return length;
		}
		/* render frame */
		r2000->frame_length = ffsk_render_frame(&r2000->ffsk, frame, 208, r2000->frame_spl);
		r2000->frame_pos = 0;
		if (r2000->frame_length > r2000->frame_size) {
			PDEBUG_CHAN(DDSP, DEBUG_ERROR, "Frame exceeds buffer, please fix!\n");
			abort();
		}
	}

	/* send audio from frame */
	max = r2000->frame_length;
	count = max - r2000->frame_pos;
	if (count > length)
		count = length;
	spl = r2000->frame_spl + r2000->frame_pos;
	for (i = 0; i < count; i++) {
		*samples++ = *spl++;
	}
	length -= count;
	r2000->frame_pos += count;
	/* check for end of telegramm */
	if (r2000->frame_pos == max) {
		r2000->frame_length = 0;
		/* we need more ? */
		if (length)
			goto next_frame;
	}

	return length;
}

static int super_render_frame(r2000_t *r2000, uint32_t word, sample_t *sample)
{
	double phaseshift, phase, bittime, bitpos;
	int count = 0, i;

	phase = r2000->super_phase65536;
	bittime = r2000->super_bittime;
	bitpos = r2000->super_bitpos;
	for (i = 0; i < 20; i++) {
		phaseshift = r2000->super_phaseshift65536[(word >> 19) & 1];
		do {
			*sample++ = super_sine[(uint16_t)phase];
			count++;
			phase += phaseshift;
			if (phase >= 65536.0)
				phase -= 65536.0;
			bitpos += bittime;
		} while (bitpos < 1.0);
		bitpos -= 1.0;
		word <<= 1;
	}
	r2000->super_phase65536 = phase;
	bitpos = r2000->super_bitpos;

	/* return number of samples created for frame */
	return count;
}

static int super_frame(r2000_t *r2000, sample_t *samples, int length)
{
	sample_t *spl;
	int i;
	int count, max;

next_frame:
	if (!r2000->super_length) {
		/* render supervisory rame */
		PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "render word 0x%05x\n", r2000->super_tx_word);
		r2000->super_length = super_render_frame(r2000, r2000->super_tx_word, r2000->super_spl);
		r2000->super_pos = 0;
		if (r2000->super_length > r2000->super_size) {
			PDEBUG_CHAN(DDSP, DEBUG_ERROR, "Frame exceeds buffer, please fix!\n");
			abort();
		}
	}

	/* send audio from frame */
	max = r2000->super_length;
	count = max - r2000->super_pos;
	if (count > length)
		count = length;
	spl = r2000->super_spl + r2000->super_pos;
	for (i = 0; i < count; i++) {
		*samples++ += *spl++;
	}
	length -= count;
	r2000->super_pos += count;
	/* check for end of telegramm */
	if (r2000->super_pos == max) {
		r2000->super_length = 0;
		/* we need more ? */
		if (length)
			goto next_frame;
	}

	return length;
}

/* Provide stream of audio toward radio unit */
void sender_send(sender_t *sender, sample_t *samples, int length)
{
	r2000_t *r2000 = (r2000_t *) sender;
	int len;

again:
	switch (r2000->dsp_mode) {
	case DSP_MODE_OFF:
		memset(samples, 0, sizeof(*samples) * length);
		break;
	case DSP_MODE_AUDIO_TX:
	case DSP_MODE_AUDIO_TX_RX:
		jitter_load(&r2000->sender.dejitter, samples, length);
		/* do pre-emphasis */
		if (r2000->pre_emphasis)
			pre_emphasis(&r2000->estate, samples, length);
		super_frame(r2000, samples, length);
		break;
	case DSP_MODE_FRAME:
		/* Encode frame into audio stream. If frames have
		 * stopped, process again for rest of stream. */
		len = fsk_frame(r2000, samples, length);
		/* do pre-emphasis */
		if (r2000->pre_emphasis)
			pre_emphasis(&r2000->estate, samples, length - len);
		if (len) {
			samples += length - len;
			length = len;
			goto again;
		}
		break;
	}
}

const char *r2000_dsp_mode_name(enum dsp_mode mode)
{
        static char invalid[16];

	switch (mode) {
	case DSP_MODE_OFF:
		return "OFF";
	case DSP_MODE_AUDIO_TX:
		return "AUDIO-TX";
	case DSP_MODE_AUDIO_TX_RX:
		return "AUDIO-TX-RX";
	case DSP_MODE_FRAME:
		return "FRAME";
	}

	sprintf(invalid, "invalid(%d)", mode);
	return invalid;
}

void r2000_set_dsp_mode(r2000_t *r2000, enum dsp_mode mode, int super)
{
	/* reset telegramm */
	if (mode == DSP_MODE_FRAME && r2000->dsp_mode != mode) {
		r2000->frame_length = 0;
	}
	if ((mode == DSP_MODE_AUDIO_TX || mode == DSP_MODE_AUDIO_TX_RX)
	 && (r2000->dsp_mode != DSP_MODE_AUDIO_TX && r2000->dsp_mode != DSP_MODE_AUDIO_TX_RX)) {
		r2000->super_length = 0;
	}

	if (super >= 0) {
		/* encode supervisory word 0100000000 01xxxxxxx1 */
		r2000->super_tx_word = 0x40101 | ((super & 0x7f) << 1);
		/* clear pending data in rx word */
		r2000->super_rx_word = 0x00000;
		PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "DSP mode %s -> %s (super = 0x%05x)\n", r2000_dsp_mode_name(r2000->dsp_mode), r2000_dsp_mode_name(mode), r2000->super_tx_word);
	} else if (r2000->dsp_mode != mode)
		PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "DSP mode %s -> %s\n", r2000_dsp_mode_name(r2000->dsp_mode), r2000_dsp_mode_name(mode));

	r2000->dsp_mode = mode;
}

#warning fixme: high pass filter on tx side to prevent desturbance of supervisory signal
