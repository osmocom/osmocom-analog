/* AMPS audio processing
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

/* How does FSK decoding work:
 * ---------------------------
 *
 * AMPS modulates the carrier frequency. If it is 8 kHz above, it is high level,
 * if it is 8 kHz below, it is low level.  The bits are coded using Manchester
 * code. A 1 is coded by low level, followed by a hight level. A 0 is coded by
 * a high level, followed by a low level. This will cause at least one level
 * change within each bit.  Also the level changes between equal bits, see
 * Manchester coding.  The bit rate is 10 KHz.
 *
 * In order to detect and demodulate a frame, the dotting sequnce is searched.
 * The dotting sequnece are alternate bits: 101010101...  The duration of a
 * level change within the dotting sequnene ist 100uS.  If all offsets of 8
 * level changes lay within +-50% of the expected time, the dotting sequence is
 * valid.  Now the next 12 bits will be searched for sync sequnece.  If better
 * dotting-offsets are found, the counter for searching the sync sequence is
 * reset, so the next 12 bits will be searched for sync too.  If no sync was
 * detected, the state changes to search for next dotting sequence.
 *
 * The average level change offsets of the dotting sequence is used to set the
 * window for the first bit.  When all samples for the window are received, a
 * raise in level is detected as 1, fall in level is detected as 0. This is done
 * by substracting the average sample value of the left side of the window by
 * the average sample value of the right side.  After the bit has been detected,
 * the samples for the next window will be received and detected.
 *
 * +-----+-----+-----+-----+
 * |     |     |   __|__   |
 * |     |     |  /  |  \  |
 * |     |     | /   |   \ |
 * |     |     |/    |    \|
 * +-----+-----+-----+-----+
 * |\    |    /|     |     |
 * | \   |   / |     |     |
 * |  \__|__/  |     |     |
 * |     |     |     |     |
 * +-----+-----+-----+-----+
 *       End   Half  Begin
 *
 * The Rx window is depiced above. In this example there is a raising edge.
 * The window is analyzed in backward direction. The average level between
 * 'Half' position and 'Begin' position is calculated, also the average level
 * between 'End' position and 'Half' position. Because the right (second)
 * side of the average level is higher than the left (first) side, a raising
 * edge is detected.
 *
 * Tests showed that comparing half of the regions of the window will cause
 * more errors than only quarter regions of the regions. Especially this is
 * true with NBFM receivers that are normally not sufficient for AMPS signals.
 *
 * As soon as a sync pattern is detected, the polarity of the pattern is used
 * to decode the following frame bits with correct polarity.  During reception
 * of the frame bits, no sync and no dotting sequnece is searched or detected.
 *
 * After reception of the bit, the bits are re-assembled, parity checked and
 * decoded. Then the process hunts for next dotting sequence.  
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
#include "amps.h"
#include "frame.h"
#include "dsp.h"

#define CHAN amps->sender.kanal

/* uncomment this to debug the encoding process */
//#define DEBUG_ENCODER

/* uncomment this to debug the decoding process */
//#define DEBUG_DECODER

#define PI			M_PI

#define BANDWIDTH		20000.0	/* maximum bandwidth */
#define FSK_DEVIATION		32767.0	/* +-8 KHz */
#define SAT_DEVIATION		8192.0	/* +-2 KHz */
#define COMPANDOR_0DB		45000	/* works quite well */
#define BITRATE			10000
#define SIG_TONE_CROSSINGS	2000	/* 2000 crossings are 100ms @ 10 KHz */
#define SIG_TONE_MINBITS	950	/* minimum bit durations to detect signaling tone (1000 is perfect for 100 ms) */
#define SIG_TONE_MAXBITS	1050	/* as above, maximum bits */
#define SAT_DURATION		0.100	/* duration of SAT signal measurement */
#define SAT_QUALITY		0.85	/* quality needed to detect sat */
#define SAT_DETECT_COUNT	3	/* number of measures to detect SAT signal (specs say 250ms) */
#define SAT_LOST_COUNT		3	/* number of measures to loose SAT signal (specs say 250ms) */
#define SIG_DETECT_COUNT	3	/* number of measures to detect Signaling Tone */
#define SIG_LOST_COUNT		2	/* number of measures to loose Signaling Tone */
#define CUT_OFF_HIGHPASS	300.0   /* cut off frequency for high pass filter to remove dc level from sound card / sample */
#define BEST_QUALITY		0.68	/* Best possible RX quality */

static int16_t ramp_up[256], ramp_down[256];

static double sat_freq[5] = {
	5970.0,
	6000.0,
	6030.0,
	5800.0, /* noise level to check against */
	10000.0, /* signaling tone */
};

static int dsp_sine_sat[256];
static int dsp_sine_test[256];

static uint8_t dsp_sync_check[0x800];

/* global init for FSK */
void dsp_init(void)
{
	int i;
	double s;

	PDEBUG(DDSP, DEBUG_DEBUG, "Generating sine table for SAT signal.\n");
	for (i = 0; i < 256; i++) {
		s = sin((double)i / 256.0 * 2.0 * PI);
		dsp_sine_sat[i] = (int)(s * SAT_DEVIATION);
		dsp_sine_test[i] = (int)(s * FSK_DEVIATION);
	}

	/* sync checker */
	for (i = 0; i < 0x800; i++) {
		dsp_sync_check[i] = 0xff; /* no sync */
	}
	for (i = 0; i < 11; i++) {
		dsp_sync_check[0x712 ^ (1 << i)] = 0x01; /* one bit error */
		dsp_sync_check[0x0ed ^ (1 << i)] = 0x81; /* one bit error */
	}
	dsp_sync_check[0x712] = 0x00; /* no bit error */
	dsp_sync_check[0x0ed] = 0x80; /* no bit error */
}

static void dsp_init_ramp(amps_t *amps)
{
	double c;
        int i;

	PDEBUG(DDSP, DEBUG_DEBUG, "Generating smooth ramp table.\n");
	for (i = 0; i < 256; i++) {
		c = cos((double)i / 256.0 * PI);
#if 0
		if (c < 0)
			c = -sqrt(-c);
		else
			c = sqrt(c);
#endif
		ramp_down[i] = (int)(c * (double)amps->fsk_deviation);
		ramp_up[i] = -ramp_down[i];
	}
}

static void sat_reset(amps_t *amps, const char *reason);

/* Init FSK of transceiver */
int dsp_init_sender(amps_t *amps, int high_pass, int tolerant)
{
	double coeff;
	int16_t *spl;
	int i;
	int rc;
	double RC, dt;
	int half;

	/* attack (3ms) and recovery time (13.5ms) according to amps specs */
	init_compandor(&amps->cstate, 8000, 3.0, 13.5, COMPANDOR_0DB);

	PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Init DSP for transceiver.\n");

	/* set deviation and modulation parameters */
	amps->sender.bandwidth = BANDWIDTH;
	amps->sender.sample_deviation = 8000.0 / (double)FSK_DEVIATION;

	if (amps->sender.samplerate < 96000) {
		PDEBUG(DDSP, DEBUG_ERROR, "Sample rate must be at least 96000 Hz to process FSK and SAT signals.\n");
		return -EINVAL;
	}

	amps->fsk_bitduration = (double)amps->sender.samplerate / (double)BITRATE;
	amps->fsk_bitstep = 1.0 / amps->fsk_bitduration;
	PDEBUG(DDSP, DEBUG_DEBUG, "Use %.4f samples for full bit duration @ %d.\n", amps->fsk_bitduration, amps->sender.samplerate);

	amps->fsk_tx_buffer_size = amps->fsk_bitduration + 10; /* 10 extra to avoid overflow due to rounding */
	amps->fsk_tx_buffer = calloc(sizeof(int16_t), amps->fsk_tx_buffer_size);
	if (!amps->fsk_tx_buffer) {
		PDEBUG(DDSP, DEBUG_DEBUG, "No memory!\n");
		rc = -ENOMEM;
		goto error;
	}

	amps->fsk_rx_window_length = ceil(amps->fsk_bitduration); /* buffer holds one bit (rounded up) */
	half = amps->fsk_rx_window_length >> 1;
	amps->fsk_rx_window_begin = half >> 1;
	amps->fsk_rx_window_half = half;
	amps->fsk_rx_window_end = amps->fsk_rx_window_length - (half >> 1);
	PDEBUG(DDSP, DEBUG_DEBUG, "Bit window length: %d\n", amps->fsk_rx_window_length);
	PDEBUG(DDSP, DEBUG_DEBUG, " -> Samples in window to analyse level left of edge: %d..%d\n", amps->fsk_rx_window_begin, amps->fsk_rx_window_half - 1);
	PDEBUG(DDSP, DEBUG_DEBUG, " -> Samples in window to analyse level right of edge: %d..%d\n", amps->fsk_rx_window_half, amps->fsk_rx_window_end - 1);
	amps->fsk_rx_window = calloc(sizeof(int16_t), amps->fsk_rx_window_length);
	if (!amps->fsk_rx_window) {
		PDEBUG(DDSP, DEBUG_DEBUG, "No memory!\n");
		rc = -ENOMEM;
		goto error;
	}

	/* create devation and ramp */
	amps->fsk_deviation = FSK_DEVIATION; /* be sure not to overflow 32767 */
	dsp_init_ramp(amps);

	/* allocate ring buffer for SAT signal detection */
	amps->sat_samples = (int)((double)amps->sender.samplerate * SAT_DURATION + 0.5);
	spl = calloc(1, amps->sat_samples * sizeof(*spl));
	if (!spl) {
		PDEBUG(DDSP, DEBUG_ERROR, "No memory!\n");
		return -ENOMEM;
	}
	amps->sat_filter_spl = spl;

	/* count SAT tones */
	for (i = 0; i < 5; i++) {
		coeff = 2.0 * cos(2.0 * PI * sat_freq[i] / (double)amps->sender.samplerate);
		amps->sat_coeff[i] = coeff * 32768.0;
		PDEBUG(DDSP, DEBUG_DEBUG, "sat_coeff[%d] = %d\n", i, (int)amps->sat_coeff[i]);

		if (i < 3) {
			amps->sat_phaseshift256[i] = 256.0 / ((double)amps->sender.samplerate / sat_freq[i]);
			PDEBUG(DDSP, DEBUG_DEBUG, "sat_phaseshift256[%d] = %.4f\n", i, amps->sat_phaseshift256[i]);
		}
	}
	sat_reset(amps, "Initial state");

	/* test tone */
	amps->test_phaseshift256 = 256.0 / ((double)amps->sender.samplerate / 1000.0);
	PDEBUG(DDSP, DEBUG_DEBUG, "test_phaseshift256 = %.4f\n", amps->test_phaseshift256);

	/* use this filter to remove dc level for 0-crossing detection
	 * if we have de-emphasis, we don't need it, so high_pass is not set. */
	if (high_pass) {
		RC = 1.0 / (CUT_OFF_HIGHPASS * 2.0 *3.14);
		dt = 1.0 / amps->sender.samplerate;
		amps->highpass_factor = RC / (RC + dt);
	}

	/* be more tolerant when syncing */
	amps->fsk_rx_sync_tolerant = tolerant;

	return 0;

error:
	dsp_cleanup_sender(amps);

	return rc;
}

/* Cleanup transceiver instance. */
void dsp_cleanup_sender(amps_t *amps)
{
	PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Cleanup DSP for treansceiver.\n");

	if (amps->fsk_tx_buffer)
		free(amps->fsk_tx_buffer);
	if (amps->fsk_rx_window)
		free(amps->fsk_rx_window);
	if (amps->sat_filter_spl) {
		free(amps->sat_filter_spl);
		amps->sat_filter_spl = NULL;
	}
#if 0
	if (amps->frame_spl) {
		free(amps->frame_spl);
		amps->frame_spl = NULL;
	}
#endif
}

static int fsk_encode(amps_t *amps, char bit)
{
	int16_t *spl;
	double phase, bitstep, deviation;
	int count;
	char last;

	deviation = amps->fsk_deviation;
	spl = amps->fsk_tx_buffer;
	phase = amps->fsk_tx_phase;
	last = amps->fsk_tx_last_bit;
	bitstep = amps->fsk_bitstep * 256.0 * 2.0; /* half bit ramp */

//printf("%d %d\n", (bit) & 1, last & 1);
	if ((bit & 1)) {
		if ((last & 1)) {
			/* last bit was 1, this bit is 1, so we ramp down first */
			do {
				*spl++ = ramp_down[(int)phase];
				phase += bitstep;
			} while (phase < 256.0);
			phase -= 256.0;
		} else {
			/* last bit was 0, this bit is 1, so we stay down first */
			do {
				*spl++ = -deviation;
				phase += bitstep;
			} while (phase < 256.0);
			phase -= 256.0;
		}
		/* ramp up */
		do {
			*spl++ = ramp_up[(int)phase];
			phase += bitstep;
		} while (phase < 256.0);
		phase -= 256.0;
	} else {
		if ((last & 1)) {
			/* last bit was 1, this bit is 0, so we stay up first */
			do {
				*spl++ = deviation;
				phase += bitstep;
			} while (phase < 256.0);
			phase -= 256.0;
		} else {
			/* last bit was 0, this bit is 0, so we ramp up first */
			do {
				*spl++ = ramp_up[(int)phase];
				phase += bitstep;
			} while (phase < 256.0);
			phase -= 256.0;
		}
		/* ramp down */
		do {
			*spl++ = ramp_down[(int)phase];
			phase += bitstep;
		} while (phase < 256.0);
		phase -= 256.0;
	}
	last = bit;
	/* depending on the number of samples, return the number */
	count = ((uintptr_t)spl - (uintptr_t)amps->fsk_tx_buffer) / sizeof(*spl);

	amps->fsk_tx_last_bit = last;
	amps->fsk_tx_phase = phase;
	amps->fsk_tx_buffer_length = count;

	return count;
}

static int fsk_frame(amps_t *amps, int16_t *samples, int length)
{
	int count = 0, len, pos, copy, i;
	int16_t *spl;
	int rc;
	char c;

	len = amps->fsk_tx_buffer_length;
	pos = amps->fsk_tx_buffer_pos;
	spl = amps->fsk_tx_buffer;

again:
	/* there must be length, otherwise we would skip blocks */
	if (count == length)
		goto done;

	/* start of new bit, so generate buffer for one bit */
	if (pos == 0) {
		c = amps->fsk_tx_frame[amps->fsk_tx_frame_pos];
		/* start new frame, so we generate one */
		if (c == '\0') {
			if (amps->dsp_mode == DSP_MODE_AUDIO_RX_FRAME_TX)
				rc = amps_encode_frame_fvc(amps, amps->fsk_tx_frame);
			else
				rc = amps_encode_frame_focc(amps, amps->fsk_tx_frame);
			/* check if we have not bit string (change to tx audio)
			 * we may not store fsk_tx_buffer_pos, because is was reset on a mode achange */
			if (rc)
				return count;
			amps->fsk_tx_frame_pos = 0;
			c = amps->fsk_tx_frame[0];
		}
		if (c == 'i')
			c = (amps->channel_busy) ? '0' : '1';
		len = fsk_encode(amps, c);
		amps->fsk_tx_frame_pos++;
	}

	copy = len - pos;
	if (length - count < copy)
		copy = length - count;
//printf("pos=%d length=%d copy=%d\n", pos, length, copy);
	for (i = 0; i < copy; i++) {
#ifdef DEBUG_ENCODER
		puts(debug_amplitude((double)spl[pos] / 32767.0));
#endif
		*samples++ = spl[pos++];
	}
	count += copy;
	if (pos == len) {
		pos = 0;
		goto again;
	}

done:
	amps->fsk_tx_buffer_length = len;
	amps->fsk_tx_buffer_pos = pos;

	return count;
}

/* Generate audio stream with SAT signal. Keep phase for next call of function. */
static void sat_encode(amps_t *amps, int16_t *samples, int length)
{
        double phaseshift, phase;
	int32_t sample;
	int i;

	phaseshift = amps->sat_phaseshift256[amps->sat];
	phase = amps->sat_phase256;

	for (i = 0; i < length; i++) {
		sample = *samples;
		sample += dsp_sine_sat[((uint8_t)phase) & 0xff];
		if (sample > 32767)
			sample = 32767;
		else if (sample < -32767)
			sample = -32767;
		*samples++ = sample;
		phase += phaseshift;
		if (phase >= 256)
			phase -= 256;
	}

	amps->sat_phase256 = phase;
}

static void test_tone_encode(amps_t *amps, int16_t *samples, int length)
{
        double phaseshift, phase;
	int i;

	phaseshift = amps->test_phaseshift256;
	phase = amps->test_phase256;

	for (i = 0; i < length; i++) {
		*samples++ = dsp_sine_test[((uint8_t)phase) & 0xff];
		phase += phaseshift;
		if (phase >= 256)
			phase -= 256;
	}

	amps->test_phase256 = phase;
}

/* Provide stream of audio toward radio unit */
void sender_send(sender_t *sender, int16_t *samples, int length)
{
	amps_t *amps = (amps_t *) sender;
	int count;

again:
	switch (amps->dsp_mode) {
	case DSP_MODE_OFF:
		/* test tone, if transmitter is off */
		test_tone_encode(amps, samples, length);
		break;
	case DSP_MODE_AUDIO_RX_AUDIO_TX:
		jitter_load(&amps->sender.dejitter, samples, length);
		/* pre-emphasis */
		if (amps->pre_emphasis)
			pre_emphasis(&amps->estate, samples, length);
		/* encode sat */
		sat_encode(amps, samples, length);
		break;
	case DSP_MODE_AUDIO_RX_FRAME_TX:
	case DSP_MODE_FRAME_RX_FRAME_TX:
		/* Encode frame into audio stream. If frames have
		 * stopped, process again for rest of stream. */
		count = fsk_frame(amps, samples, length);
		samples += count;
		length -= count;
		if (length)
			goto again;
	}
}

static void fsk_rx_bit(amps_t *amps, int16_t *spl, int len, int pos, int begin, int half, int end)
{
	int i;
	int32_t first, second;
	int bit;
	int32_t max = -32768, min = 32767;

	/* decode one bit. substact the first half from the second half.
	 * the result shows the direction of the bit change: 1 == positive.
	 */
	pos -= begin; /* possible wrap is handled below */
	second = first = 0;
	for (i = begin; i < half; i++) {
		if (--pos < 0)
			pos += len;
//printf("second %d: %d\n", pos, spl[pos]);
		second += spl[pos];
		if (spl[pos] > max)
			max = spl[pos];
		if (spl[pos] < min)
			min = spl[pos];
	}
	second /= (half - begin);
	for (i = half; i < end; i++) {
		if (--pos < 0)
			pos += len;
//printf("first %d: %d\n", pos, spl[pos]);
		first += spl[pos];
		if (spl[pos] > max)
			max = spl[pos];
		if (spl[pos] < min)
			min = spl[pos];
	}
	first /= (end - half);
//printf("first = %d second = %d\n", first, second);
	/* get bit */
	if (second > first)
		bit = 1;
	else
		bit = 0;
#ifdef DEBUG_DECODER
	if (amps->fsk_rx_sync != FSK_SYNC_POSITIVE && amps->fsk_rx_sync != FSK_SYNC_NEGATIVE)
		printf("Decoded bit as %d (dotting life = %d)\n", bit, amps->fsk_rx_dotting_life);
	else
		printf("Decoded bit as %d\n", bit);
#endif

	if (amps->fsk_rx_sync != FSK_SYNC_POSITIVE && amps->fsk_rx_sync != FSK_SYNC_NEGATIVE) {
		amps->fsk_rx_sync_register = (amps->fsk_rx_sync_register << 1) | bit;
		/* check if we received a sync */
		switch (dsp_sync_check[amps->fsk_rx_sync_register & 0x7ff]) {
		case 0x01:
			if (!amps->fsk_rx_sync_tolerant)
				break;
		case 0x00:
#ifdef DEBUG_DECODER
			printf("Sync word detected (positive)\n");
#endif
			amps->fsk_rx_sync = FSK_SYNC_POSITIVE;
prepare_frame:
			amps->fsk_rx_frame_count = 0;
			amps->fsk_rx_frame_quality = 0.0;
			amps->fsk_rx_frame_level = 0.0;
			amps->fsk_rx_sync_register = 0x555;
			amps->when_received = get_time() - (21.0 / (double)BITRATE);
			return;
		case 0x81:
			if (!amps->fsk_rx_sync_tolerant)
				break;
		case 0x80:
#ifdef DEBUG_DECODER
			printf("Sync word detected (negative)\n");
#endif
			amps->fsk_rx_sync = FSK_SYNC_NEGATIVE;
			goto prepare_frame;
			return;
		}
		/* if no sync, count down the dotting life counter */
		if (--amps->fsk_rx_dotting_life == 0) {
#ifdef DEBUG_DECODER
			printf("No Sync detected after dotting\n");
#endif
			amps->fsk_rx_sync = FSK_SYNC_NONE;
			amps->channel_busy = 0;
			return;
		}
		return;
	}

	/* count level and quality */
	amps->fsk_rx_frame_level += (double)(max - min) / (double)FSK_DEVIATION / 2.0;
	if (bit)
		amps->fsk_rx_frame_quality += (double)(second - first) / (double)FSK_DEVIATION / 2.0 / BEST_QUALITY;
	else
		amps->fsk_rx_frame_quality += (double)(first - second) / (double)FSK_DEVIATION / 2.0 / BEST_QUALITY;

	/* invert bit if negative sync was detected */
	if (amps->fsk_rx_sync == FSK_SYNC_NEGATIVE)
		bit = 1 - bit;

	/* read next bit. after all bits, we reset to FSK_SYNC_NONE */
	amps->fsk_rx_frame[amps->fsk_rx_frame_count++] = bit + '0';
	if (amps->fsk_rx_frame_count > FSK_MAX_BITS) {
		fprintf(stderr, "our fsk_tx_count (%d) is larger than our max bits we can handle, please fix!\n", amps->fsk_rx_frame_count);
		abort();
	}
	if (amps->fsk_rx_frame_count == amps->fsk_rx_frame_length) {
		int more;

		/* a complete frame was received, so we process it */
		amps->fsk_rx_frame[amps->fsk_rx_frame_count] = '\0';
		more = amps_decode_frame(amps, amps->fsk_rx_frame, amps->fsk_rx_frame_count, amps->fsk_rx_frame_level / (double)amps->fsk_rx_frame_count, amps->fsk_rx_frame_quality / amps->fsk_rx_frame_level, (amps->fsk_rx_sync == FSK_SYNC_NEGATIVE));
		if (more) {
			/* switch to next worda length without DCC included */
			amps->fsk_rx_frame_length = 240;
			goto prepare_frame;
		} else {
			/* switch back to first word length with DCC included */
			if (amps->fsk_rx_frame_length == 240)
				amps->fsk_rx_frame_length = 247;
			amps->fsk_rx_sync = FSK_SYNC_NONE;
			amps->channel_busy = 0;
		}
	}
}

static void fsk_rx_dotting(amps_t *amps, double _elapsed)
{
	uint8_t pos = amps->fsk_rx_dotting_pos++;
	double average, elapsed, offset;
	int i;

#ifdef DEBUG_DECODER
	printf("Level change detected\n");
#endif
	/* store into dotting list */
	amps->fsk_rx_dotting_elapsed[pos++] = _elapsed;

	/* check quality of dotting sequence.
	 * in case this is not a dotting sequence, noise or speech, the quality
	 * should be bad.
	 * count (only) 7 'elapsed' values between 8 zero-crossings.
	 * calculate the average relative to the current position.
	 */
	average = 0.0;
	elapsed = 0.0;
	for (i = 1; i < 8; i++) {
		elapsed += amps->fsk_rx_dotting_elapsed[--pos];
		offset = elapsed - (double)i;
		if (offset >= 0.5 || offset <= -0.5) {
#ifdef DEBUG_DECODER
//			printf("offset %.3f (last but %d) not within -0.5 .. 0.5 bit position, detecting no dotting.\n", offset, i - 1);
#endif
			return;
		}
		average += offset;
	}
	average /= (double)i;

	amps->fsk_rx_dotting_life = 12;

	/* if we are already found dotting, we detect better dotting.
	 * this happens, if dotting was falsely detected due to noise.
	 * then the real dotting causes a reastart of hunting for sync sequence.
	 */
	if (amps->fsk_rx_sync == FSK_SYNC_NONE || fabs(average) < amps->fsk_rx_dotting_average) {
#ifdef DEBUG_DECODER
		printf("Found (better) dotting sequence (average = %.3f)\n", average);
#endif
		amps->fsk_rx_sync = FSK_SYNC_DOTTING;
		amps->fsk_rx_dotting_average = fabs(average);
		amps->fsk_rx_bitcount = 0.5 + average;
		if (amps->si.acc_type.bis)
			amps->channel_busy = 1;
	}
}

/* decode frame */
static void sender_receive_frame(amps_t *amps, int16_t *samples, int length)
{
	int i;

	for (i = 0; i < length; i++) {
#ifdef DEBUG_DECODER
		puts(debug_amplitude((double)samples[i] / (double)FSK_DEVIATION));
#endif
		/* push sample to detection window and shift */
		amps->fsk_rx_window[amps->fsk_rx_window_pos++] = samples[i];
		if (amps->fsk_rx_window_pos == amps->fsk_rx_window_length)
			amps->fsk_rx_window_pos = 0;
		if (amps->fsk_rx_sync != FSK_SYNC_POSITIVE && amps->fsk_rx_sync != FSK_SYNC_NEGATIVE) {
			/* check for change in polarity */
			if (amps->fsk_rx_last_sample <= 0) {
				if (samples[i] > 0) {
					fsk_rx_dotting(amps, amps->fsk_rx_elapsed);
					amps->fsk_rx_elapsed = 0.0;
				}
			} else {
				if (samples[i] <= 0) {
					fsk_rx_dotting(amps, amps->fsk_rx_elapsed);
					amps->fsk_rx_elapsed = 0.0;
				}
			}
		}
		amps->fsk_rx_last_sample = samples[i];
		amps->fsk_rx_elapsed += amps->fsk_bitstep;
//		printf("%.4f\n", bitcount);
		if (amps->fsk_rx_sync != FSK_SYNC_NONE) {
			amps->fsk_rx_bitcount += amps->fsk_bitstep;
			if (amps->fsk_rx_bitcount >= 1.0) {
				amps->fsk_rx_bitcount -= 1.0;
				fsk_rx_bit(amps,
					amps->fsk_rx_window,
					amps->fsk_rx_window_length,
					amps->fsk_rx_window_pos,
					amps->fsk_rx_window_begin,
					amps->fsk_rx_window_half,
					amps->fsk_rx_window_end);
			}
		}
	}
}


/* decode signaling tone */
/* compare supervisory signal against noise floor on 5800 Hz */
static void sat_decode(amps_t *amps, int16_t *samples, int length)
{
	int coeff[3];
	double result[3], quality[2];

	coeff[0] = amps->sat_coeff[amps->sat];
	coeff[1] = amps->sat_coeff[3]; /* noise floor detection */
	coeff[2] = amps->sat_coeff[4]; /* signaling tone */
	audio_goertzel(samples, length, 0, coeff, result, 3);

	quality[0] = (result[0] - result[1]) / result[0];
	if (quality[0] < 0)
		quality[0] = 0;
	quality[1] = (result[2] - result[1]) / result[2];
	if (quality[1] < 0)
		quality[1] = 0;

	PDEBUG_CHAN(DDSP, DEBUG_NOTICE, "SAT level %.2f%% quality %.0f%%\n", result[0] * 32767.0 / SAT_DEVIATION / 0.63662 * 100.0, quality[0] * 100.0);
	if (amps->sender.loopback || debuglevel == DEBUG_DEBUG) {
		PDEBUG_CHAN(DDSP, debuglevel, "Signaling Tone level %.2f%% quality %.0f%%\n", result[2] * 32767.0 / FSK_DEVIATION / 0.63662 * 100.0, quality[1] * 100.0);
	}
	if (quality[0] > SAT_QUALITY) {
		if (amps->sat_detected == 0) {
			amps->sat_detect_count++;
			if (amps->sat_detect_count == SAT_DETECT_COUNT) {
				amps->sat_detected = 1;
				amps->sat_detect_count = 0;
				PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "SAT signal detected with level=%.0f%%, quality=%.0f%%.\n", result[0] / 0.63662 * 100.0, quality[0] * 100.0);
				amps_rx_sat(amps, 1, quality[0]);
			}
		} else
			amps->sat_detect_count = 0;
	} else {
		if (amps->sat_detected == 1) {
			amps->sat_detect_count++;
			if (amps->sat_detect_count == SAT_LOST_COUNT) {
				amps->sat_detected = 0;
				amps->sat_detect_count = 0;
				PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "SAT signal lost.\n");
				amps_rx_sat(amps, 0, 0.0);
			}
		} else
			amps->sat_detect_count = 0;
	}
	if (quality[1] > 0.8) {
		if (amps->sig_detected == 0) {
			amps->sig_detect_count++;
			if (amps->sig_detect_count == SIG_DETECT_COUNT) {
				amps->sig_detected = 1;
				amps->sig_detect_count = 0;
				PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Signaling Tone detected with level=%.0f%%, quality=%.0f%%.\n", result[2] / 0.63662 * 100.0, quality[1] * 100.0);
				amps_rx_signaling_tone(amps, 1, quality[1]);
			}
		} else
			amps->sig_detect_count = 0;
	} else {
		if (amps->sig_detected == 1) {
			amps->sig_detect_count++;
			if (amps->sig_detect_count == SIG_LOST_COUNT) {
				amps->sig_detected = 0;
				amps->sig_detect_count = 0;
				PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Signaling Tone lost.\n");
				amps_rx_signaling_tone(amps, 0, 0.0);
			}
		} else
			amps->sig_detect_count = 0;
	}
}

/* decode signaling/audio */
/* Count SIG_TONE_CROSSINGS of zero crossings, then check if the elapsed bit
 * time is between SIG_TONE_MINBITS and SIG_TONE_MAXBITS. If it is, the
 * frequency is close to the singalling tone, so it is detected
 */
static void sender_receive_audio(amps_t *amps, int16_t *samples, int length)
{
	transaction_t *trans = amps->trans_list;
	int16_t *spl;
	int max, pos;
	int i;

	/* SAT detection */

	max = amps->sat_samples;
	spl = amps->sat_filter_spl;
	pos = amps->sat_filter_pos;
	for (i = 0; i < length; i++) {
		spl[pos++] = samples[i];
		if (pos == max) {
			pos = 0;
			sat_decode(amps, spl, max);
		}
	}
	amps->sat_filter_pos = pos;

	/* receive audio, but only if call established and SAT detected */

	if ((amps->dsp_mode == DSP_MODE_AUDIO_RX_AUDIO_TX || amps->dsp_mode == DSP_MODE_AUDIO_RX_FRAME_TX)
	 && trans && trans->callref && trans->sat_detected) {
		int16_t down[length]; /* more than enough */
		int pos, count;
		int16_t *spl;
		int i;

		/* de-emphasis */
		if (amps->de_emphasis)
			de_emphasis(&amps->estate, samples, length);
		/* downsample */
		count = samplerate_downsample(&amps->sender.srstate, samples, length, down);
		expand_audio(&amps->cstate, down, count);
		spl = amps->sender.rxbuf;
		pos = amps->sender.rxbuf_pos;
		for (i = 0; i < count; i++) {
			spl[pos++] = down[i];
			if (pos == 160) {
				call_tx_audio(trans->callref, spl, 160);
				pos = 0;
			}
		}
		amps->sender.rxbuf_pos = pos;
	} else
		amps->sender.rxbuf_pos = 0;
}

/* Process received audio stream from radio unit. */
void sender_receive(sender_t *sender, int16_t *samples, int length)
{
	amps_t *amps = (amps_t *) sender;
	double x, y, x_last, y_last, factor;
	int32_t value;
	int i;

	/* high pass filter to remove 0-level
	 * if factor is not set, we should already have 0-level. */
	factor = amps->highpass_factor;
	if (factor) {
		x_last = amps->highpass_x_last;
		y_last = amps->highpass_y_last;
		for (i = 0; i < length; i++) {
			x = (double)samples[i];
			y = factor * (y_last + x - x_last);
			x_last = x;
			y_last = y;
			value = (int32_t)(y + 0.5);
			if (value < -32768.0)
				value = -32768.0;
			else if (value > 32767)
				value = 32767;
			samples[i] = value;
		}
		amps->highpass_x_last = x_last;
		amps->highpass_y_last = y_last;
	}

	switch (amps->dsp_mode) {
	case DSP_MODE_OFF:
		break;
	case DSP_MODE_FRAME_RX_FRAME_TX:
		sender_receive_frame(amps, samples, length);
		break;
	case DSP_MODE_AUDIO_RX_AUDIO_TX:
	case DSP_MODE_AUDIO_RX_FRAME_TX:
		sender_receive_audio(amps, samples, length);
		break;
	}
}

/* Reset SAT detection states, so ongoing tone will be detected again. */
static void sat_reset(amps_t *amps, const char *reason)
{
	PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "SAT detector reset: %s.\n", reason);
	amps->sat_detected = 0;
	amps->sat_detect_count = 0;
	amps->sig_detected = 0;
	amps->sig_detect_count = 0;
}

void amps_set_dsp_mode(amps_t *amps, enum dsp_mode mode, int frame_length)
{
#if 0
	/* reset telegramm */
	if (mode == DSP_MODE_FRAME && amps->dsp_mode != mode)
		amps->frame = 0;
#endif
	if (mode == DSP_MODE_FRAME_RX_FRAME_TX) {
		/* reset SAT detection */
		sat_reset(amps, "Change to FOCC");
		PDEBUG_CHAN(DDSP, DEBUG_INFO, "Change mode to FOCC\n");
	}
	if (amps->dsp_mode == DSP_MODE_FRAME_RX_FRAME_TX
	 && (mode == DSP_MODE_AUDIO_RX_AUDIO_TX || mode == DSP_MODE_AUDIO_RX_FRAME_TX)) {
		/* reset SAT detection */
		sat_reset(amps, "Change from FOCC to FVC");
		PDEBUG_CHAN(DDSP, DEBUG_INFO, "Change mode from FOCC to FVC\n");
	}
	if (amps->dsp_mode == DSP_MODE_OFF
	 && (mode == DSP_MODE_AUDIO_RX_AUDIO_TX || mode == DSP_MODE_AUDIO_RX_FRAME_TX)) {
		/* reset SAT detection */
		sat_reset(amps, "Enable FVC");
		PDEBUG_CHAN(DDSP, DEBUG_INFO, "Change mode from OFF to FVC\n");
	}
	if (mode == DSP_MODE_OFF) {
		/* reset SAT detection */
		sat_reset(amps, "Disable FVC");
		PDEBUG_CHAN(DDSP, DEBUG_INFO, "Change mode from FVC to OFF\n");
	}

	amps->dsp_mode = mode;
	if (frame_length)
		amps->fsk_rx_frame_length = frame_length;

	/* reset detection process */
	amps->fsk_rx_sync = FSK_SYNC_NONE;
	amps->channel_busy = 0;
	amps->fsk_rx_sync_register = 0x555;

	/* reset transmitter */
	amps->fsk_tx_buffer_pos = 0;
	amps->fsk_tx_frame[0] = '\0';
	amps->fsk_tx_frame_pos = 0;
}

