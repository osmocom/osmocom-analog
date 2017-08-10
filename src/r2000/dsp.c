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
 * I assume that the deviation at 1500 Hz is +-1425 Hz. (according to R&S)
 * This would lead to a deviation at 1800 Hz (Bit 0) about +-1700 Hz. (emphasis)
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
#define TX_PEAK_FSK		(1425.0 / 1500.0 * 1000.0 / DBM0_DEVIATION) /* with emphasis */
#define TX_PEAK_SUPER		(300.0 / DBM0_DEVIATION) /* no emphasis */
#define FSK_BIT_RATE		1200.0
#define FSK_BIT_ADJUST		0.1	/* how much do we adjust bit clock on frequency change */
#define FSK_F0			1800.0
#define FSK_F1			1200.0
#define SUPER_BIT_RATE		50.0
#define SUPER_BIT_ADJUST	0.5	/* how much do we adjust bit clock on frequency change */
#define SUPER_F0		136.0
#define SUPER_F1		164.0
#define FILTER_STEP		0.002	/* step every 2 ms */
#define MAX_DISPLAY		1.4	/* something above dBm0 */

/* global init for FSK */
void dsp_init(void)
{
}

static int fsk_send_bit(void *inst);
static void fsk_receive_bit(void *inst, int bit, double quality, double level);
static int super_send_bit(void *inst);
static void super_receive_bit(void *inst, int bit, double quality, double level);

/* Init FSK of transceiver */
int dsp_init_sender(r2000_t *r2000)
{
	/* attack (3ms) and recovery time (13.5ms) according to NMT specs */
	init_compandor(&r2000->cstate, 8000, 3.0, 13.5, COMPANDOR_0DB);

	PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Init DSP for Transceiver.\n");

	/* set modulation parameters */
	sender_set_fm(&r2000->sender, MAX_DEVIATION, MAX_MODULATION, DBM0_DEVIATION, MAX_DISPLAY);

	PDEBUG(DDSP, DEBUG_DEBUG, "Using FSK level of %.3f\n", TX_PEAK_FSK);

	/* init fsk */
	if (fsk_init(&r2000->fsk, r2000, fsk_send_bit, fsk_receive_bit, r2000->sender.samplerate, FSK_BIT_RATE, FSK_F0, FSK_F1, TX_PEAK_FSK, 1, FSK_BIT_ADJUST) < 0) {
		PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "FSK init failed!\n");
		return -EINVAL;
	}
	if (r2000->sender.loopback)
		r2000->rx_max = 176;
	else
		r2000->rx_max = 144;

	/* init supervisorty fsk */
	if (fsk_init(&r2000->super_fsk, r2000, super_send_bit, super_receive_bit, r2000->sender.samplerate, SUPER_BIT_RATE, SUPER_F0, SUPER_F1, TX_PEAK_SUPER, 0, SUPER_BIT_ADJUST) < 0) {
		PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "FSK init failed!\n");
		return -EINVAL;
	}

	return 0;
}

/* Cleanup transceiver instance. */
void dsp_cleanup_sender(r2000_t *r2000)
{
	PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Cleanup DSP for Transceiver.\n");

	fsk_cleanup(&r2000->fsk);
	fsk_cleanup(&r2000->super_fsk);
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

static void super_receive_bit(void *inst, int bit, double quality, double level)
{
	r2000_t *r2000 = (r2000_t *)inst;
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
		fsk_receive(&r2000->super_fsk, samples, length);

	/* do de-emphasis */
	if (r2000->de_emphasis)
		de_emphasis(&r2000->estate, samples, length);

	/* fsk signal */
	fsk_receive(&r2000->fsk, samples, length);

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

static int fsk_send_bit(void *inst)
{
	r2000_t *r2000 = (r2000_t *)inst;
	const char *frame;

	if (!r2000->tx_frame_length || r2000->tx_frame_pos == r2000->tx_frame_length) {
		frame = r2000_get_frame(r2000);
		if (!frame) {
			r2000->tx_frame_length = 0;
			PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Stop sending frames.\n");
			return -1;
		}
		memcpy(r2000->tx_frame, frame, 208);
		r2000->tx_frame_length = 208;
		r2000->tx_frame_pos = 0;
	}

	return r2000->tx_frame[r2000->tx_frame_pos++];
}

static int super_send_bit(void *inst)
{
	r2000_t *r2000 = (r2000_t *)inst;

	if (!r2000->super_tx_word_length || r2000->super_tx_word_pos == r2000->super_tx_word_length) {
		r2000->super_tx_word_length = 20;
		r2000->super_tx_word_pos = 0;
	}

	return (r2000->super_tx_word >> (r2000->super_tx_word_length - (++r2000->super_tx_word_pos))) & 1;
}

/* Provide stream of audio toward radio unit */
void sender_send(sender_t *sender, sample_t *samples, int length)
{
	r2000_t *r2000 = (r2000_t *) sender;
	int count;

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
		/* add supervisory to sample buffer */
		fsk_send(&r2000->super_fsk, samples, length, 1);
		break;
	case DSP_MODE_FRAME:
		/* Encode frame into audio stream. If frames have
		 * stopped, process again for rest of stream. */
		count = fsk_send(&r2000->fsk, samples, length, 0);
		/* do pre-emphasis */
		if (r2000->pre_emphasis)
			pre_emphasis(&r2000->estate, samples, count);
		/* special case: add supervisory signal to frame at loop test */
		if (r2000->sender.loopback) {
			/* add supervisory to sample buffer */
			fsk_send(&r2000->super_fsk, samples, count, 1);
		}
		samples += count;
		length -= count;
		if (length)
			goto again;
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
		r2000->tx_frame_length = 0;
		fsk_tx_reset(&r2000->fsk);
	}
	if ((mode == DSP_MODE_AUDIO_TX || mode == DSP_MODE_AUDIO_TX_RX)
	 && (r2000->dsp_mode != DSP_MODE_AUDIO_TX && r2000->dsp_mode != DSP_MODE_AUDIO_TX_RX)) {
		r2000->super_tx_word_length = 0;
		fsk_tx_reset(&r2000->super_fsk);
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
