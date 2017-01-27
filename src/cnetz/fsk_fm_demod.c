/* FSK decoder of carrier FSK signals received by simple FM receiver
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

/* How does it work:
 * -----------------
 *
 * C-Netz modulates the carrier frequency. If it is 2.4 kHz above, it is high
 * level, if it is 2.4 kHz below, it is low level. Look at FTZ 171 TR 60
 * Chapter 5 (data exchange) for closer information.
 *
 * Detect level change:
 *
 * We don't just look for high/low level, because we don't know what the actual
 * 0-level of the phone's transmitter is. (level of carrier frequency) Also we
 * use receiver and sound card that cause any level to return to 0 after some
 * time, even if the transmitter still transmits a level above or below the
 * carrier frequnecy. Insted we look at the change of the received signal. An
 * upward change indicates 1. An downward change indicates 0. (This may also be
 * reversed, it we find out, that we received a sync sequence in received
 * polarity.) If there is no significant change in level, we keep the value of
 * last change, regardless of what level we actually receive.
 *
 * To determine a change from noise, we use a theshold. This is set to half of
 * the level of last received change. This means that the next change may be
 * down to a half lower.  There is a special case during distributed signaling.
 * The first level change of each data chunk raises or falls from 0-level
 * (unmodulated carrier), so the threshold for this bit is only a quarter of the
 * last received change.
 *
 * While searching for a sync sequence, the threshold for the next change is set
 * after each change. After synchronization, the the threshold is locked to half
 * of the average change level of the sync sequence.
 *
 * Search window
 *
 * We use a window of one bit length (9 samples at 48 kHz sample rate) and look
 * for a change that is higher than the threshold and has its highest slope in
 * the middle of the window. To determine the level, the min and max value
 * inside the window is searched. The differece is the change level. To
 * determine the highest slope, the highest difference between subsequent
 * samples is used.  For every sample we move the window one bit to the right
 * (next sample), check if change level matches the threshold and highest slope
 * is in the middle and so forth. Only if the highes slope is exactly in the
 * middle, we declare a change.  This means that we detect a slope about half of
 * a bit duration later.
 *
 * When we are not synced:
 * 
 * For every change we record a bit. A positive change is 1 and a negative 0. If
 * it turns out that the receiver or sound card is reversed, we reverse bits.
 * After every change we wait up to 1.5 bit duration for next change. If there
 * is a change, we record our next bit. If there is no change, we record the
 * state of the last bit. After we had no change, we wait 1 bit duration, since
 * we already 0.5 behind the start of the recently recorded bit.
 *
 * When we are synced:
 *
 * After we recorded the time of all level changes during the sync sequence, we
 * calulate an average and use it as a time base for sampling the subsequent 150
 * bit of a message.  From now on, a bit change does not cause any resync. We
 * just remember what change we received. Later we use it for sampling the 150
 * bits.
 *
 * We wait a duration of 1.5 bits after the sync sequence and the start of the
 * bit that follows the sync sequence. We record what we received as last
 * change. For all following 149 bits we wait 1 bit duration and record what we
 * received as last change.
 *
 * Sync clock
 *
 * Because we transmit and receive chunks of sample from buffers of different
 * drivers, we cannot determine the exact latency between received and
 * transmitted samples. Also some sound cards may have different RX and TX
 * speed. One (pure software) solution is to sync ourself to the mobile phone,
 * since the mobile phone is perfectly synced to use.
 *
 * After receiving and decording of a frame, we use the time of received sync
 * sequence to synchronize the reciever to the mobile phone. If we receive a
 * message on the OgK (control channel), we know that this is a response to a
 * message of a specific time slot we recently sent. Then we can fully sync the
 * receiver's clock.  For any other frame, we cannot determine the absolute
 * clock. We just correct the receiver's clock, as the clock differs only
 * slightly from the time the message was received.
 * 
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../common/sample.h"
#include "../common/timer.h"
#include "../common/debug.h"
#include "cnetz.h"
#include "dsp.h"
#include "telegramm.h"

/* use to debug decoder */
//#define DEBUG_DECODER	if (1)
//#define DEBUG_DECODER	if (fsk->cnetz->dsp_mode == DSP_MODE_SPK_V)
//#define DEBUG_DECODER	if (fsk->cnetz->dsp_mode == DSP_MODE_SPK_V && sync)

int fsk_fm_init(fsk_fm_demod_t *fsk, cnetz_t *cnetz, int samplerate, double bitrate)
{
	int len, half;

	memset(fsk, 0, sizeof(*fsk));
	if (samplerate < 48000) {
		PDEBUG(DDSP, DEBUG_ERROR, "Sample rate must be at least 48000 Hz!\n");
		return -1;
	}

	fsk->cnetz = cnetz;

	len = (int)((double)samplerate / bitrate + 0.5);
	half = (int)((double)samplerate / bitrate / 2.0 + 0.5);
	fsk->bit_buffer_spl = calloc(sizeof(fsk->bit_buffer_spl[0]), len);
	if (!fsk->bit_buffer_spl) {
		PDEBUG(DDSP, DEBUG_ERROR, "No mem!\n");
		goto error;
	}

	fsk->bit_buffer_len = len;
	fsk->bit_buffer_half = half;
	fsk->bits_per_sample = bitrate / (double)samplerate;

	fsk->speech_size = samplerate * 60 / bitrate + 10; /* 60 bits duration, add 10 to be safe */
	fsk->speech_buffer = calloc(sizeof(fsk->speech_buffer[0]), fsk->speech_size);
	if (!fsk->speech_buffer) {
		PDEBUG(DDSP, DEBUG_ERROR, "No mem!\n");
		goto error;
	}

	fsk->level_threshold = 655;

	return 0;

error:
	fsk_fm_exit(fsk);
	return -1;
}

void fsk_fm_exit(fsk_fm_demod_t *fsk)
{
	if (fsk->bit_buffer_spl) {
		free(fsk->bit_buffer_spl);
		fsk->bit_buffer_spl = NULL;
	}
	if (fsk->speech_buffer) {
		free(fsk->speech_buffer);
		fsk->speech_buffer = NULL;
	}
}

/* get levels, sync time and jitter from sync sequence or frame data */
static inline void get_levels(fsk_fm_demod_t *fsk, int *_min, int *_max, int *_avg, int *_probes, int num, double *_time, double *_jitter)
{
	int min = 32767, max = -32768, avg = 0, count = 0, level;
	double time = 0, t, sync_average, sync_time, jitter = 0;
	int bit_offset;
	int i;

	/* get levels an the average receive time */
	for (i = 0; i < num; i++) {
		level = fsk->change_levels[(fsk->change_pos - 1 - i) & 0xff];
		if (level <= 0)
			continue;

		/* in spk mode, we skip the voice part (62 bits) */
		if (fsk->cnetz->dsp_mode == DSP_MODE_SPK_V)
			bit_offset = i + ((i + 2) >> 2) * 62;
		else
			bit_offset = i;
		t = fmod(fsk->change_when[(fsk->change_pos - 1 - i) & 0xff] - fsk->bit_time + (double)bit_offset + BITS_PER_SUPERFRAME, BITS_PER_SUPERFRAME);
		if (t > BITS_PER_SUPERFRAME / 2)
			t -= BITS_PER_SUPERFRAME;
//if (fsk->cnetz->dsp_mode == DSP_MODE_SPK_V)
//	printf("%d: level=%d%% @%.2f difference=%.2f\n", bit_offset, level * 100 / 65536, fsk->change_when[(fsk->change_pos - 1 - i) & 0xff], t);
		time += t;

		if (level < min)
			min = level;
		if (level > max)
			max = level;
		avg += level;
		count++;
	}

	if (!count) {
		*_min = *_max = *_avg = 0;
		return;
	}

	/* when did we received the sync?
	 * sync_average is the average about how early (negative) or
	 * late (positive) we received the sync relative to current bit_time.
	 * sync_time is the absolute time within the super frame.
	 */
	sync_average = time / (double)count;
	sync_time = fmod(sync_average + fsk->bit_time + BITS_PER_SUPERFRAME, BITS_PER_SUPERFRAME);

	*_probes = count;
	*_min = min;
	*_max = max;
	*_avg = avg / count;

	if (_time) {
//		if (fsk->cnetz->dsp_mode == DSP_MODE_SPK_V)
//			printf("sync at distributed mode\n");
//		printf("sync at bit_time=%.2f (sync_average = %.2f)\n", sync_time, sync_average);
		/* if our average sync is later (greater) than the current
		 * bit_time, we must wait longer (next_bit above 1.5)
		 * for the time to sample the bit.
		 * if sync is earlier, bit_time is already too late, so
		 * we must wait less than 1.5 bits */
		fsk->next_bit = 1.5 + sync_average;
		*_time = sync_time;
	}
	if (_jitter) {
		/* get jitter of received changes */
		for (i = 0; i < num; i++) {
			level = fsk->change_levels[(fsk->change_pos - 1 - i) & 0xff];
			if (level <= 0)
				continue;

			/* in spk mode, we skip the voice part (62 bits) */
			if (fsk->cnetz->dsp_mode == DSP_MODE_SPK_V)
				bit_offset = i + ((i + 2) >> 2) * 62;
			else
				bit_offset = i;
			t = fmod(fsk->change_when[(fsk->change_pos - 1 - i) & 0xff] - sync_time + (double)bit_offset + BITS_PER_SUPERFRAME, BITS_PER_SUPERFRAME);
			if (t > BITS_PER_SUPERFRAME / 2)
				t = BITS_PER_SUPERFRAME - t; /* turn negative into positive */
			jitter += t;
		}
		*_jitter = jitter / (double)count;
	}
}

static inline void got_bit(fsk_fm_demod_t *fsk, int bit, int change_level)
{
	int min, max, avg, probes;

	/* count bits, but do not exceed 4 bits per SPK block */
	if (fsk->cnetz->dsp_mode == DSP_MODE_SPK_V) {
		/* for first bit, we have only half of the modulation deviation, so we multiply level by two */
		if (fsk->bit_count == 0)
			change_level *= 2;
		if (fsk->bit_count == 4)
			return;
	}
	fsk->bit_count++;

//printf("bit %d\n", bit);
	fsk->change_levels[fsk->change_pos] = change_level;
	fsk->change_when[fsk->change_pos++] = fsk->bit_time;


	switch (fsk->sync) {
	case FSK_SYNC_NONE:
		fsk->rx_sync = (fsk->rx_sync << 1) | bit;
		/* use half level of last change for threshold change detection.
		 * if there is no change detected for 5 bits, set theshold to
		 * 1 percent, so the 7 pause bits before a frame will make sure
		 * that the change is below noise level, so the first sync
		 * bit is detected. then the change is set and adjusted
		 * for all other bits in the sync sequence.
		 * after sync, the theshold is set to half of the average of
		 * all changes in the sync sequence */
		if (change_level) {
			fsk->level_threshold = (double)change_level / 2.0;
		} else if ((fsk->rx_sync & 0x1f) == 0x00 || (fsk->rx_sync & 0x1f) == 0x1f) {
			if (fsk->cnetz->dsp_mode != DSP_MODE_SPK_V)
				fsk->level_threshold = 655;
		}
		if (detect_sync(fsk->rx_sync)) {
			fsk->sync = FSK_SYNC_POSITIVE;
got_sync:
			get_levels(fsk, &min, &max, &avg, &probes, 30, &fsk->sync_time, &fsk->sync_jitter);
			fsk->sync_level = (double)avg / 65535.0;
			if (fsk->sync == FSK_SYNC_NEGATIVE)
				fsk->sync_level = -fsk->sync_level;
//			printf("sync (change min=%d%% max=%d%% avg=%d%% sync_time=%.2f jitter=%.2f probes=%d)\n", min * 100 / 65535, max * 100 / 65535, avg * 100 / 65535, fsk->sync_time, fsk->sync_jitter, probes);
			fsk->level_threshold = (double)avg / 2.0;
			fsk->rx_sync = 0;
			fsk->rx_buffer_count = 0;
			break;
		}
		if (detect_sync(fsk->rx_sync ^ 0xfffffffff)) {
			fsk->sync = FSK_SYNC_NEGATIVE;
			goto got_sync;
		}
		break;
	case FSK_SYNC_NEGATIVE:
		bit = 1 - bit;
		/* fall through */
	case FSK_SYNC_POSITIVE:
		fsk->rx_buffer[fsk->rx_buffer_count] = bit + '0';
		if (++fsk->rx_buffer_count == 150) {
			fsk->sync = FSK_SYNC_NONE;
			if (fsk->cnetz->dsp_mode != DSP_MODE_SPK_V) {
				/* received 40 bits after start of block */
				fsk->sync_time = fmod(fsk->sync_time - (7+33) + BITS_PER_SUPERFRAME, BITS_PER_SUPERFRAME);
			} else {
				/* received 662 bits after start of block (10 SPK blocks + 1 bit (== 2 level changes)) */
				fsk->sync_time = fmod(fsk->sync_time - (66*10+2) + BITS_PER_SUPERFRAME, BITS_PER_SUPERFRAME);
			}
			cnetz_decode_telegramm(fsk->cnetz, fsk->rx_buffer, fsk->sync_level, fsk->sync_time, fsk->sync_jitter);
		}
		break;
	}
}

/* DOC TBD: find change for bit change */
static inline void find_change(fsk_fm_demod_t *fsk)
{
	sample_t level_min = 0, level_max = 0, change_max = -1;
	int change_at = -1, change_positive = -1;
	sample_t s, last_s = 0;
	sample_t threshold;
	int i;
	
	/* levels at total reverse */
	change_max = -1;
	change_at = -1;
	change_positive = -1;

	for (i = 0; i < fsk->bit_buffer_len; i++) {
		last_s = s;
		s = fsk->bit_buffer_spl[fsk->bit_buffer_pos++];
		if (fsk->bit_buffer_pos == fsk->bit_buffer_len)
			fsk->bit_buffer_pos = 0;
		if (i > 0) {
			if (s - last_s > change_max) {
				change_max = s - last_s;
				change_at = i;
				change_positive = 1;
			} else if (last_s - s > change_max) {
				change_max = last_s - s;
				change_at = i;
				change_positive = 0;
			}
		}
		if (i == 0 || s > level_max)
			level_max = s;
		if (i == 0 || s < level_min)
			level_min = s;
	}
	/* for first bit, we have only half of the modulation deviation, so we divide the threshold by two */
	if (fsk->cnetz->dsp_mode == DSP_MODE_SPK_V && fsk->bit_count == 0)
		threshold = fsk->level_threshold / 2.0;
	else
		threshold = fsk->level_threshold;
	/* if we are not in sync, for every detected change we set
	 * next_bit to 1.5, so we wait 1.5 bits for next change
	 * if it is not received within this time, there is no change,
	 * so the bit does not change.
	 * if we are in sync, we remember last change. after 1.5
	 * bits after sync average, we measure the first bit
	 * and then all subsequent bits after 1.0 bits */
//DEBUG_DECODER printf("next_bit=%.4f\n", next_bit);
	if (level_max - level_min > threshold && change_at == fsk->bit_buffer_half) {
#ifdef DEBUG_DECODER
		DEBUG_DECODER
			printf("receive bit change to %d (level=%.3f, threshold=%.3f)\n", change_positive, level_max - level_min, threshold);
#endif
		fsk->last_change_positive = change_positive;
		if (!fsk->sync) {
			fsk->next_bit = 1.5;
			got_bit(fsk, change_positive, level_max - level_min);
		}
	}
	if (fsk->next_bit <= 0.0) {
#ifdef DEBUG_DECODER
		DEBUG_DECODER {
			if (fsk->sync)
				printf("sampling here bit %d\n", fsk->last_change_positive);
			else
				printf("no bit change\n");
		}
#endif
		fsk->next_bit += 1.0;
		got_bit(fsk, fsk->last_change_positive, 0);
	}
	fsk->next_bit -= fsk->bits_per_sample;
}

/* receive FM signal from receiver */
void fsk_fm_demod(fsk_fm_demod_t *fsk, sample_t *samples, int length)
{
	int i;
	double t;

	/* process signaling block, sample by sample */
	for (i = 0; i < length; i++) {
		fsk->bit_buffer_spl[fsk->bit_buffer_pos++] = samples[i];
		if (fsk->bit_buffer_pos == fsk->bit_buffer_len)
			fsk->bit_buffer_pos = 0;
		/* for each sample process buffer */
		if (fsk->cnetz->dsp_mode != DSP_MODE_SPK_V) {
#ifdef DEBUG_DECODER
			DEBUG_DECODER
				puts(debug_amplitude(samples[i] / 32768.0));
#endif
			find_change(fsk);
		} else {
			/* in distributed signaling, measure over 5 bits, but ignore 5th bit.
			 * also reset next_bit, as soon as we reach the window */

			/* note that we start from 0.5, because we detect change 0.5 bits later,
			 * because the detector of the change is in the middle of the 1 bit
			 * search window */
			t = fmod(fsk->bit_time, BITS_PER_SPK_BLOCK);
			if (t < 0.5) {
				fsk->next_bit = 1.0 - fsk->bits_per_sample;
#ifdef DEBUG_DECODER
				if (fsk->bit_count) {
					DEBUG_DECODER
						printf("start spk_block bit count=%d\n", fsk->bit_count);
				}
#endif
				fsk->bit_count = 0;
			} else
			if (t >= 0.5 && t < 5.5) {
#ifdef DEBUG_DECODER
				DEBUG_DECODER
					puts(debug_amplitude(samples[i] / 32768.0));
#endif
				find_change(fsk);
			} else
			if (t >= 5.5 && t < 65.5) {
				/* get audio for the duration of 60 bits */
				/* prevent overflow, if speech_size != 0 and SPK_V
				 * has been restarted. */
				if (fsk->speech_count <= fsk->speech_size)
					fsk->speech_buffer[fsk->speech_count++] = samples[i];
			} else
			if (t >= 65.5) {
				if (fsk->speech_count) {
					unshrink_speech(fsk->cnetz, fsk->speech_buffer, fsk->speech_count);
					fsk->speech_count = 0;
				}
			}

		}
		fsk->bit_time += fsk->bits_per_sample;
		if (fsk->bit_time >= BITS_PER_SUPERFRAME) {
			fsk->bit_time -= BITS_PER_SUPERFRAME;
		}
		/* another clock is used to measure actual super frame time */
		fsk->bit_time_uncorrected += fsk->bits_per_sample;
		if (fsk->bit_time_uncorrected >= BITS_PER_SUPERFRAME) {
			fsk->bit_time_uncorrected -= BITS_PER_SUPERFRAME;
			calc_clock_speed(fsk->cnetz, fsk->cnetz->sender.samplerate * 24 / 10, 0, 1);
		}
	}
}

void fsk_correct_sync(fsk_fm_demod_t *fsk, double offset)
{
	fsk->bit_time = fmod(fsk->bit_time - offset + BITS_PER_SUPERFRAME, BITS_PER_SUPERFRAME);
}

/* copy sync from one instance to another (used to sync RX of SpK to OgK */
void fsk_copy_sync(fsk_fm_demod_t *fsk_to, fsk_fm_demod_t *fsk_from)
{
	fsk_to->bit_time = fsk_from->bit_time;
}

void fsk_demod_reset(fsk_fm_demod_t *fsk)
{
	fsk->sync = FSK_SYNC_NONE;
}

