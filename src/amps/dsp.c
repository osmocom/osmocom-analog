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
 * code. A 1 is coded by low level, followed by a high level. A 0 is coded by
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
 * by subtracting the average sample value of the left side of the window by
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
#include "../libsample/sample.h"
#include "../liblogging/logging.h"
#include "../libmobile/call.h"
#include "../libmobile/get_time.h"
#include "amps.h"
#include "frame.h"
#include "dsp.h"
#include "main.h"

#define CHAN amps->sender.kanal

/* uncomment this to debug the encoding process */
//#define DEBUG_ENCODER

/* uncomment this to debug the decoding process */
//#define DEBUG_DECODER

#define PI			M_PI

#define AMPS_MAX_DEVIATION	8000.0
#define AMPS_MAX_MODULATION	10000.0
#define AMPS_SPEECH_DEVIATION	2900.0  /* deviation of speech at 1 kHz */
#define AMPS_FSK_DEVIATION	(8000.0 / AMPS_SPEECH_DEVIATION)	/* no emphasis */
#define AMPS_SAT_DEVIATION	(2000.0 / AMPS_SPEECH_DEVIATION)	/* no emphasis */
#define AMPS_MAX_DISPLAY	(10000.0 / AMPS_SPEECH_DEVIATION)	/* no emphasis */
#define AMPS_BITRATE		10000
/* for some reason, 4000 Hz deviation works better */
#define TACS_SPEECH_DEVIATION	4000.0  /* 2300 Hz deviation at 1 kHz (according to panasonic manual) */
#define TACS_MAX_DEVIATION	6400.0	/* (according to texas instruments and other sources) */
#define TACS_MAX_MODULATION	9500.0	/* (according to panasonic manual) */
#define TACS_FSK_DEVIATION	(6400.0 / TACS_SPEECH_DEVIATION)	/* no emphasis */
#define TACS_SAT_DEVIATION	(1700.0 / TACS_SPEECH_DEVIATION)	/* no emphasis (panasonic / TI) */
#define TACS_MAX_DISPLAY	(8000.0 / TACS_SPEECH_DEVIATION)	/* no emphasis */
#define TACS_BITRATE		8000
#define SAT_BANDWIDTH		30.0	/* distance between two SAT tones, also bandwidth for goertzel filter */
#define SAT_QUALITY		0.85	/* quality needed to detect SAT signal */
#define SAT_PRINT		10	/* print sat measurement every 0.5 seconds */
#define DTX_LEVEL		0.50	/* SAT level needed to mute/unmute */
#define SIG_QUALITY		0.80	/* quality needed to detect Signaling Tone */
#define SAT_DETECT_COUNT	5	/* number of measures to detect SAT signal (specs say 250ms) */
#define SAT_LOST_COUNT		5	/* number of measures to loose SAT signal (specs say 250ms) */
#define SIG_DETECT_COUNT	6	/* number of measures to detect Signaling Tone */
#define SIG_LOST_COUNT		4	/* number of measures to loose Signaling Tone */
#define CUT_OFF_HIGHPASS	300.0   /* cut off frequency for high pass filter to remove dc level from sound card / sample */
#define BEST_QUALITY		0.68	/* Best possible RX quality */
#define COMFORT_NOISE		0.02	/* audio level of comfort noise (relative to speech level) */

static sample_t ramp_up[256], ramp_down[256];

static double sat_freq[4] = {
	5970.0,
	6000.0,
	6030.0,
	5790.0, /* noise level to check against */
};

static sample_t dsp_sine_sat[65536];

static uint8_t dsp_sync_check[0x800];

/* global init for FSK */
void dsp_init(void)
{
	int i;
	double s;

	LOGP(DDSP, LOGL_DEBUG, "Generating sine table for SAT signal.\n");
	for (i = 0; i < 65536; i++) {
		s = sin((double)i / 65536.0 * 2.0 * PI);
		dsp_sine_sat[i] = s * ((!tacs) ? AMPS_SAT_DEVIATION : TACS_SAT_DEVIATION);
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

	compandor_init();
}

static void dsp_init_ramp(amps_t *amps)
{
	double c;
        int i;

	LOGP(DDSP, LOGL_DEBUG, "Generating smooth ramp table.\n");
	for (i = 0; i < 256; i++) {
		c = cos((double)i / 256.0 * PI);
#if 0
		if (c < 0)
			c = -sqrt(-c);
		else
			c = sqrt(c);
#endif
		ramp_down[i] = c * (double)amps->fsk_deviation;
		ramp_up[i] = -ramp_down[i];
	}
}

static void sat_reset(amps_t *amps, const char *reason);

/* Init FSK of transceiver */
int dsp_init_sender(amps_t *amps, int tolerant)
{
	sample_t *spl;
	int i;
	int rc;
	int half;

	/* attack (3ms) and recovery time (13.5ms) according to amps specs */
	setup_compandor(&amps->cstate, 8000, 3.0, 13.5);

	LOGP_CHAN(DDSP, LOGL_DEBUG, "Init DSP for transceiver.\n");

	/* set modulation parameters */
	sender_set_fm(&amps->sender,
		(!tacs) ? AMPS_MAX_DEVIATION : TACS_MAX_DEVIATION,
		(!tacs) ? AMPS_MAX_MODULATION : TACS_MAX_MODULATION,
		(!tacs) ? AMPS_SPEECH_DEVIATION : TACS_SPEECH_DEVIATION,
		(!tacs) ? AMPS_MAX_DISPLAY : TACS_MAX_DISPLAY);

	if (amps->sender.samplerate < 96000) {
		LOGP(DDSP, LOGL_ERROR, "Sample rate must be at least 96000 Hz to process FSK and SAT signals.\n");
		return -EINVAL;
	}

	amps->fsk_bitduration = (double)amps->sender.samplerate / (double)((!tacs) ? AMPS_BITRATE : TACS_BITRATE);
	amps->fsk_bitstep = 1.0 / amps->fsk_bitduration;
	LOGP(DDSP, LOGL_DEBUG, "Use %.4f samples for full bit duration @ %d.\n", amps->fsk_bitduration, amps->sender.samplerate);

	amps->fsk_tx_buffer_size = amps->fsk_bitduration + 10; /* 10 extra to avoid overflow due to rounding */
	spl = calloc(sizeof(*spl), amps->fsk_tx_buffer_size);
	if (!spl) {
		LOGP(DDSP, LOGL_ERROR, "No memory!\n");
		rc = -ENOMEM;
		goto error;
	}
	amps->fsk_tx_buffer = spl;

	amps->fsk_rx_window_length = ceil(amps->fsk_bitduration); /* buffer holds one bit (rounded up) */
	half = amps->fsk_rx_window_length >> 1;
	amps->fsk_rx_window_begin = half >> 1;
	amps->fsk_rx_window_half = half;
	amps->fsk_rx_window_end = amps->fsk_rx_window_length - (half >> 1);
	LOGP(DDSP, LOGL_DEBUG, "Bit window length: %d\n", amps->fsk_rx_window_length);
	LOGP(DDSP, LOGL_DEBUG, " -> Samples in window to analyse level left of edge: %d..%d\n", amps->fsk_rx_window_begin, amps->fsk_rx_window_half - 1);
	LOGP(DDSP, LOGL_DEBUG, " -> Samples in window to analyse level right of edge: %d..%d\n", amps->fsk_rx_window_half, amps->fsk_rx_window_end - 1);
	spl = calloc(sizeof(*amps->fsk_rx_window), amps->fsk_rx_window_length);
	if (!spl) {
		LOGP(DDSP, LOGL_ERROR, "No memory!\n");
		rc = -ENOMEM;
		goto error;
	}
	amps->fsk_rx_window = spl;

	/* create deviation and ramp */
	amps->fsk_deviation = (!tacs) ? AMPS_FSK_DEVIATION : TACS_FSK_DEVIATION;
	dsp_init_ramp(amps);

	/* allocate ring buffer for SAT signal detection
	 * the bandwidth of the Goertzel filter is the reciprocal of the duration
	 * we half our bandwidth, so that other supervisory signals will be canceled out completely by goertzel filter
	 */
	amps->sat_samples = (int)((double)amps->sender.samplerate * (1.0 / (SAT_BANDWIDTH / 2.0)) + 0.5);
	spl = calloc(sizeof(*spl), amps->sat_samples);
	if (!spl) {
		LOGP(DDSP, LOGL_ERROR, "No memory!\n");
		return -ENOMEM;
	}
	LOGP(DDSP, LOGL_DEBUG, "Sat detection interval is %d ms.\n", amps->sat_samples * 1000 / amps->sender.samplerate);
	amps->sat_filter_spl = spl;

	/* count SAT tones */
	for (i = 0; i < 4; i++) {
		audio_goertzel_init(&amps->sat_goertzel[i], sat_freq[i], amps->sender.samplerate);
		if (i < 3)
			amps->sat_phaseshift65536[i] = 65536.0 / ((double)amps->sender.samplerate / sat_freq[i]);
	}
	/* signaling tone */
	audio_goertzel_init(&amps->sat_goertzel[4], (!tacs) ? 10000.0 : 8000.0, amps->sender.samplerate);
	sat_reset(amps, "Initial state");

	/* be more tolerant when syncing */
	amps->fsk_rx_sync_tolerant = tolerant;

	amps->dmp_frame_level = display_measurements_add(&amps->sender.dispmeas, "Frame Level", "%.1f %% (last)", DISPLAY_MEAS_LAST, DISPLAY_MEAS_LEFT, 0.0, 150.0, 100.0);
	amps->dmp_frame_quality = display_measurements_add(&amps->sender.dispmeas, "Frame Quality", "%.1f %% (last)", DISPLAY_MEAS_LAST, DISPLAY_MEAS_LEFT, 0.0, 100.0, 100.0);
	if (amps->chan_type == CHAN_TYPE_VC || amps->chan_type == CHAN_TYPE_CC_PC_VC) {
		amps->dmp_sat_level = display_measurements_add(&amps->sender.dispmeas, "SAT Level", "%.1f %%", DISPLAY_MEAS_AVG, DISPLAY_MEAS_LEFT, 0.0, 150.0, 100.0);
		amps->dmp_sat_quality = display_measurements_add(&amps->sender.dispmeas, "SAT Quality", "%.1f %%", DISPLAY_MEAS_AVG, DISPLAY_MEAS_LEFT, 0.0, 100.0, 100.0);
	}

	return 0;

error:
	dsp_cleanup_sender(amps);

	return rc;
}

/* Cleanup transceiver instance. */
void dsp_cleanup_sender(amps_t *amps)
{
	LOGP_CHAN(DDSP, LOGL_DEBUG, "Cleanup DSP for treansceiver.\n");

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
	sample_t *spl;
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
				*spl++ = ramp_down[(uint8_t)phase];
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
			*spl++ = ramp_up[(uint8_t)phase];
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
				*spl++ = ramp_up[(uint8_t)phase];
				phase += bitstep;
			} while (phase < 256.0);
			phase -= 256.0;
		}
		/* ramp down */
		do {
			*spl++ = ramp_down[(uint8_t)phase];
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

static int fsk_frame(amps_t *amps, sample_t *samples, int length)
{
	int count = 0, len, pos, copy, i;
	sample_t *spl;
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
			/* check if we have no bit string (change to tx audio / silence)
			 * we may not store fsk_tx_buffer_pos, because is was reset on a mode change */
			if (rc)
				return count;
			amps->fsk_tx_frame_pos = 0;
			c = amps->fsk_tx_frame[0];
		}
		if (c == 'i')
			c = (amps->channel_busy) ? '0' : '1';
		/* invert, if polarity of the cell is negative */
		if (amps->flip_polarity)
			c ^= 1;
		len = fsk_encode(amps, c);
		amps->fsk_tx_frame_pos++;
	}

	copy = len - pos;
	if (length - count < copy)
		copy = length - count;
//printf("pos=%d length=%d copy=%d\n", pos, length, copy);
	for (i = 0; i < copy; i++) {
#ifdef DEBUG_ENCODER
		puts(debug_amplitude((double)spl[pos]));
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

/* send comfort noise */
static void comfort_noise(sample_t *samples, int length)
{
	int i;
	int16_t r;

	for (i = 0; i < length; i++) {
		r = random();
		samples[i] = (double)r / 32768.0 * COMFORT_NOISE;
	}
}

/* Generate audio stream with SAT signal. Keep phase for next call of function. */
static void sat_encode(amps_t *amps, sample_t *samples, int length)
{
        double phaseshift, phase;
	int i;

	phaseshift = amps->sat_phaseshift65536[amps->sat];
	phase = amps->sat_phase65536;

	for (i = 0; i < length; i++) {
		*samples++ += dsp_sine_sat[(uint16_t)phase];
		phase += phaseshift;
		if (phase >= 65536)
			phase -= 65536;
	}

	amps->sat_phase65536 = phase;
}

/* Provide stream of audio toward radio unit */
void sender_send(sender_t *sender, sample_t *samples, uint8_t *power, int length)
{
	amps_t *amps = (amps_t *) sender;
	int count, input_num;

again:
	switch (amps->dsp_mode) {
	case DSP_MODE_OFF:
		memset(power, 0, length);
		memset(samples, 0, sizeof(*samples) * length);
		break;
	case DSP_MODE_AUDIO_RX_AUDIO_TX:
		memset(power, 1, length);
		input_num = samplerate_upsample_input_num(&sender->srstate, length);
		jitter_load(&sender->dejitter, samples, input_num);
		samplerate_upsample(&sender->srstate, samples, input_num, samples, length);
		/* pre-emphasis */
		if (amps->pre_emphasis)
			pre_emphasis(&amps->estate, samples, length);
		/* encode SAT during call */
		sat_encode(amps, samples, length);
		break;
	case DSP_MODE_AUDIO_RX_SILENCE_TX:
		memset(power, 1, length);
		memset(samples, 0, sizeof(*samples) * length);
		/* encode SAT while waiting for alert response or answer */
		sat_encode(amps, samples, length);
		break;
	case DSP_MODE_AUDIO_RX_FRAME_TX:
	case DSP_MODE_FRAME_RX_FRAME_TX:
		/* Encode frame into audio stream. If frames have
		 * stopped, process again for rest of stream. */
		count = fsk_frame(amps, samples, length);
		memset(power, 1, count);
		// no SAT during frame transmission, according to specs
		samples += count;
		power += count;
		length -= count;
		if (length)
			goto again;
	}
}

static void fsk_rx_bit(amps_t *amps, sample_t *spl, int len, int pos, int begin, int half, int end)
{
	int i;
	double first, second;
	int bit;
	double max = 0, min = 0;

	/* decode one bit. subtract the first half from the second half.
	 * the result shows the direction of the bit change: 1 == positive.
	 */
	pos -= begin; /* possible wrap is handled below */
	second = first = 0;
	for (i = begin; i < half; i++) {
		if (--pos < 0)
			pos += len;
//printf("second %d: %d\n", pos, spl[pos]);
		second += spl[pos];
		if (i == 0 || spl[pos] > max)
			max = spl[pos];
		if (i == 0 || spl[pos] < min)
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
			/* FALLTHRU */
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
			amps->when_received = get_time() - (21.0 / (double)((!tacs) ? AMPS_BITRATE : TACS_BITRATE));
			return;
		case 0x81:
			if (!amps->fsk_rx_sync_tolerant)
				break;
			/* FALLTHRU */
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
	amps->fsk_rx_frame_level += (double)(max - min) / (double)((!tacs) ? AMPS_FSK_DEVIATION : TACS_FSK_DEVIATION) / 2.0;
	if (bit)
		amps->fsk_rx_frame_quality += (double)(second - first) / (double)((!tacs) ? AMPS_FSK_DEVIATION : TACS_FSK_DEVIATION) / 2.0 / BEST_QUALITY;
	else
		amps->fsk_rx_frame_quality += (double)(first - second) / (double)((!tacs) ? AMPS_FSK_DEVIATION : TACS_FSK_DEVIATION) / 2.0 / BEST_QUALITY;

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

		/* update measurements */
		display_measurements_update(amps->dmp_frame_level, amps->fsk_rx_frame_level / (double)amps->fsk_rx_frame_count * 100.0, 0.0);
		display_measurements_update(amps->dmp_frame_quality, amps->fsk_rx_frame_quality / (double)amps->fsk_rx_frame_count * 100.0, 0.0);

		/* a complete frame was received, so we process it */
		amps->fsk_rx_frame[amps->fsk_rx_frame_count] = '\0';
		more = amps_decode_frame(amps, amps->fsk_rx_frame, amps->fsk_rx_frame_count, amps->fsk_rx_frame_level / (double)amps->fsk_rx_frame_count, amps->fsk_rx_frame_quality / amps->fsk_rx_frame_level, (amps->fsk_rx_sync == FSK_SYNC_NEGATIVE));
		if (more) {
			/* switch to next word length without DCC included */
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
static void sender_receive_frame(amps_t *amps, sample_t *samples, int length)
{
	int i;

	for (i = 0; i < length; i++) {
#ifdef DEBUG_DECODER
		puts(debug_amplitude(samples[i] / (double)FSK_DEVIATION));
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


/* decode SAT and signaling tone */
/* compare supervisory signal against noise floor at 5790 Hz */
static void sat_decode(amps_t *amps, sample_t *samples, int length)
{
	double result[3], sat_quality, sig_quality, sat_level, sig_level;

	audio_goertzel(&amps->sat_goertzel[amps->sat], samples, length, 0, &result[0], 1);
	audio_goertzel(&amps->sat_goertzel[3], samples, length, 0, &result[1], 1);
	audio_goertzel(&amps->sat_goertzel[4], samples, length, 0, &result[2], 1);

	/* normalize sat level and signaling tone level */
	sat_level = result[0] / ((!tacs) ? AMPS_SAT_DEVIATION : TACS_SAT_DEVIATION);
	sig_level = result[2] / ((!tacs) ? AMPS_FSK_DEVIATION : TACS_FSK_DEVIATION);

	/* get normalized quality of SAT and signaling tone */
	sat_quality = (result[0] - result[1]) / result[0];
	if (sat_quality < 0)
		sat_quality = 0;
	sig_quality = (result[2] - result[1]) / result[2];
	if (sig_quality < 0)
		sig_quality = 0;

	/* debug SAT */
	if (++amps->sat_print == SAT_PRINT) {
		LOGP_CHAN(DDSP, LOGL_NOTICE, "SAT level %.2f%% quality %.0f%%\n", sat_level * 100.0, sat_quality * 100.0);
		amps->sat_print = 0;
	}

	/* update measurements (if dmp_* params are NULL, we omit this) */
	display_measurements_update(amps->dmp_sat_level, sat_level * 100.0, 0.0);
	display_measurements_update(amps->dmp_sat_quality, sat_quality * 100.0, 0.0);

	/* debug signaling tone */
	if (amps->sender.loopback || loglevel == LOGL_DEBUG) {
		LOGP_CHAN(DDSP, loglevel, "Signaling Tone level %.2f%% quality %.0f%%\n", sig_level * 100.0, sig_quality * 100.0);
	}

	/* mute if SAT quality or level is below threshold */
	if (sat_quality > SAT_QUALITY && sat_level > DTX_LEVEL)
		amps->dtx_state = 1;
	else
		amps->dtx_state = 0;

	/* detect SAT */
	if (sat_quality > SAT_QUALITY) {
		if (amps->sat_detected == 0) {
			amps->sat_detect_count++;
			if (amps->sat_detect_count == SAT_DETECT_COUNT) {
				amps->sat_detected = 1;
				amps->sat_detect_count = 0;
				LOGP_CHAN(DDSP, LOGL_DEBUG, "SAT signal detected with level=%.0f%%, quality=%.0f%%.\n", sat_level * 100.0, sat_quality * 100.0);
				amps_rx_sat(amps, 1, sat_quality);
			}
		} else
			amps->sat_detect_count = 0;
	} else {
		if (amps->sat_detected == 1) {
			amps->sat_detect_count++;
			if (amps->sat_detect_count == SAT_LOST_COUNT) {
				amps->sat_detected = 0;
				amps->sat_detect_count = 0;
				LOGP_CHAN(DDSP, LOGL_DEBUG, "SAT signal lost.\n");
				amps_rx_sat(amps, 0, 0.0);
			}
		} else
			amps->sat_detect_count = 0;
	}

	/* detect signaling tone */
	if (sig_quality > SIG_QUALITY) {
		if (amps->sig_detected == 0) {
			amps->sig_detect_count++;
			if (amps->sig_detect_count == SIG_DETECT_COUNT) {
				amps->sig_detected = 1;
				amps->sig_detect_count = 0;
				LOGP_CHAN(DDSP, LOGL_DEBUG, "Signaling Tone detected with level=%.0f%%, quality=%.0f%%.\n", sig_level * 100.0, sig_quality * 100.0);
				amps_rx_signaling_tone(amps, 1, sig_quality);
			}
		} else
			amps->sig_detect_count = 0;
	} else {
		if (amps->sig_detected == 1) {
			amps->sig_detect_count++;
			if (amps->sig_detect_count == SIG_LOST_COUNT) {
				amps->sig_detected = 0;
				amps->sig_detect_count = 0;
				LOGP_CHAN(DDSP, LOGL_DEBUG, "Signaling Tone lost.\n");
				amps_rx_signaling_tone(amps, 0, 0.0);
			}
		} else
			amps->sig_detect_count = 0;
	}
}

static void sender_receive_audio(amps_t *amps, sample_t *samples, int length)
{
	transaction_t *trans = amps->trans_list;
	sample_t *spl, s;
	int max, pos;
	int i;

	/* SAT / signalling tone detection */
	max = amps->sat_samples;
	spl = amps->sat_filter_spl;
	pos = amps->sat_filter_pos;
	for (i = 0; i < length; i++) {
		/* unmute: use buffer, to delay audio, so we do not miss that chunk when SAT is detected */
		s = spl[pos];
		spl[pos++] = samples[i];
		samples[i] = s;
		if (pos == max) {
			pos = 0;
			sat_decode(amps, spl, max);
		}
	}
	amps->sat_filter_pos = pos;

	/* receive audio, but only if call established and SAT detected */

	if ((amps->dsp_mode == DSP_MODE_AUDIO_RX_AUDIO_TX || amps->dsp_mode == DSP_MODE_AUDIO_RX_FRAME_TX)
	 && trans && trans->callref) {
		int pos, count;
		int i;

		/* de-emphasis */
		if (amps->de_emphasis)
			de_emphasis(&amps->estate, samples, length);
		/* downsample */
		count = samplerate_downsample(&amps->sender.srstate, samples, length);
		expand_audio(&amps->cstate, samples, count);
		spl = amps->sender.rxbuf;
		pos = amps->sender.rxbuf_pos;
		for (i = 0; i < count; i++) {
			spl[pos++] = samples[i];
			if (pos == 160) {
				if (amps->dtx_state == 0)
					comfort_noise(spl, 160);
				call_up_audio(trans->callref, spl, 160);
				pos = 0;
			}
		}
		amps->sender.rxbuf_pos = pos;
	} else
		amps->sender.rxbuf_pos = 0;
}

/* Process received audio stream from radio unit. */
void sender_receive(sender_t *sender, sample_t *samples, int length, double __attribute__((unused)) rf_level_db)
{
	amps_t *amps = (amps_t *) sender;

	/* dc filter required for FSK decoding and tone detection */
	if (amps->de_emphasis)
		dc_filter(&amps->estate, samples, length);

	switch (amps->dsp_mode) {
	case DSP_MODE_OFF:
		break;
	case DSP_MODE_FRAME_RX_FRAME_TX:
		sender_receive_frame(amps, samples, length);
		break;
	case DSP_MODE_AUDIO_RX_AUDIO_TX:
	case DSP_MODE_AUDIO_RX_FRAME_TX:
	case DSP_MODE_AUDIO_RX_SILENCE_TX:
		sender_receive_audio(amps, samples, length);
		break;
	}
}

/* Reset SAT detection states, so ongoing tone will be detected again. */
static void sat_reset(amps_t *amps, const char *reason)
{
	LOGP_CHAN(DDSP, LOGL_DEBUG, "SAT detector reset: %s.\n", reason);
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
		LOGP_CHAN(DDSP, LOGL_INFO, "Change mode to FOCC\n");
		amps->tx_focc_debugged = 0;
	}
	if (amps->dsp_mode == DSP_MODE_FRAME_RX_FRAME_TX
	 && (mode == DSP_MODE_AUDIO_RX_AUDIO_TX || mode == DSP_MODE_AUDIO_RX_FRAME_TX || mode == DSP_MODE_AUDIO_RX_SILENCE_TX)) {
		/* reset SAT detection */
		sat_reset(amps, "Change from FOCC to FVC");
		LOGP_CHAN(DDSP, LOGL_INFO, "Change mode from FOCC to FVC\n");
	}
	if (amps->dsp_mode == DSP_MODE_OFF
	 && (mode == DSP_MODE_AUDIO_RX_AUDIO_TX || mode == DSP_MODE_AUDIO_RX_FRAME_TX || mode == DSP_MODE_AUDIO_RX_SILENCE_TX)) {
		/* reset SAT detection */
		sat_reset(amps, "Enable FVC");
		LOGP_CHAN(DDSP, LOGL_INFO, "Change mode from OFF to FVC\n");
	}
	if (mode == DSP_MODE_OFF) {
		/* reset SAT detection */
		sat_reset(amps, "Disable FVC");
		LOGP_CHAN(DDSP, LOGL_INFO, "Change mode from FVC to OFF\n");
	}

	LOGP_CHAN(DDSP, LOGL_DEBUG, "Reset FSK frame transmitter, due to setting dsp mode.\n");

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

