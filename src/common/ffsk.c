/* FFSK audio processing (NMT / Radiocom 2000)
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

#define CHAN ffsk->channel

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "../common/sample.h"
#include "../common/debug.h"
#include "ffsk.h"

#define PI			M_PI

#define BIT_RATE		1200	/* baud rate */
#define FILTER_STEPS		0.1	/* step every 1/12000 sec */

/* two signaling tones */
static double ffsk_freq[2] = {
	1800.0,
	1200.0,
};

static sample_t dsp_tone_bit[2][2][65536]; /* polarity, bit, phase */

/* global init for FFSK */
void ffsk_global_init(double peak_fsk)
{
	int i;
	double s;

	PDEBUG(DDSP, DEBUG_DEBUG, "Generating sine table for FFSK tones.\n");
	for (i = 0; i < 65536; i++) {
		s = sin((double)i / 65536.0 * 2.0 * PI);
		/* bit(1) 1 cycle */
		dsp_tone_bit[0][1][i] = s * peak_fsk;
		dsp_tone_bit[1][1][i] = -s * peak_fsk;
		/* bit(0) 1.5 cycles */
		s = sin((double)i / 65536.0 * 3.0 * PI);
		dsp_tone_bit[0][0][i] = s * peak_fsk;
		dsp_tone_bit[1][0][i] = -s * peak_fsk;
	}
}

/* Init FFSK */
int ffsk_init(ffsk_t *ffsk, void *inst, void (*receive_bit)(void *inst, int bit, double quality, double level), int channel, int samplerate)
{
	sample_t *spl;
	int i;

	/* a symbol rate of 1200 Hz, times check interval of FILTER_STEPS */
	if (samplerate < (double)BIT_RATE / (double)FILTER_STEPS) {
		PDEBUG(DDSP, DEBUG_ERROR, "Sample rate must be at least 12000 Hz to process FSK+supervisory signal.\n");
		return -EINVAL;
	}

	memset(ffsk, 0, sizeof(*ffsk));
	ffsk->inst = inst;
	ffsk->receive_bit = receive_bit;
	ffsk->channel = channel;
	ffsk->samplerate = samplerate;

	ffsk->samples_per_bit = (double)ffsk->samplerate / (double)BIT_RATE;
	ffsk->bits_per_sample = 1.0 / ffsk->samples_per_bit;
	PDEBUG(DDSP, DEBUG_DEBUG, "Use %.4f samples for full bit duration @ %d.\n", ffsk->samples_per_bit, ffsk->samplerate);

	/* allocate ring buffers, one bit duration */
	ffsk->filter_size = floor(ffsk->samples_per_bit); /* buffer holds one bit (rounded down) */
	spl = calloc(1, ffsk->filter_size * sizeof(*spl));
	if (!spl) {
		PDEBUG(DDSP, DEBUG_ERROR, "No memory!\n");
		ffsk_cleanup(ffsk);
		return -ENOMEM;
	}
	ffsk->filter_spl = spl;
	ffsk->filter_bit = -1;

	/* count symbols */
	for (i = 0; i < 2; i++)
		audio_goertzel_init(&ffsk->goertzel[i], ffsk_freq[i], ffsk->samplerate);
	ffsk->phaseshift65536 = 65536.0 / ffsk->samples_per_bit;
	PDEBUG(DDSP, DEBUG_DEBUG, "fsk_phaseshift = %.4f\n", ffsk->phaseshift65536);

	return 0;
}

/* Cleanup transceiver instance. */
void ffsk_cleanup(ffsk_t *ffsk)
{
	PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Cleanup DSP for Transceiver.\n");

	if (ffsk->filter_spl) {
		free(ffsk->filter_spl);
		ffsk->filter_spl = NULL;
	}
}

//#define DEBUG_MODULATOR
//#define DEBUG_FILTER
//#define DEBUG_QUALITY

/* Filter one chunk of audio an detect tone, quality and loss of signal.
 * The chunk is a window of 1/1200s. This window slides over audio stream
 * and is processed every 1/12000s. (one step) */
static inline void ffsk_decode_step(ffsk_t *ffsk, int pos)
{
	double level, result[2], softbit, quality;
	int max;
	sample_t *spl;
	int bit;
	
	max = ffsk->filter_size;
	spl = ffsk->filter_spl;

	level = audio_level(spl, max);
	/* limit level to prevent division by zero */
	if (level < 0.001)
		level = 0.001;

	audio_goertzel(ffsk->goertzel, spl, max, pos, result, 2);

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

	if (ffsk->filter_bit != bit) {
		/* If we have a bit change, move sample counter towards one half bit duration.
		 * We may have noise, so the bit change may be wrong or not at the correct place.
		 * This can cause bit slips.
		 * Therefore we change the sample counter only slightly, so bit slips may not
		 * happen so quickly.
		 * */
#ifdef DEBUG_FILTER
		puts("bit change");
#endif
		ffsk->filter_bit = bit;
		if (ffsk->filter_sample < 5)
			ffsk->filter_sample++;
		if (ffsk->filter_sample > 5)
			ffsk->filter_sample--;
	} else if (--ffsk->filter_sample == 0) {
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
		ffsk->receive_bit(ffsk->inst, bit, quality, level / 0.63662);
		ffsk->filter_sample = 10;
	}
}

void ffsk_receive(ffsk_t *ffsk, sample_t *sample, int length)
{
	sample_t *spl;
	int max, pos;
	double step, bps;
	int i;

	/* write received samples to decode buffer */
	max = ffsk->filter_size;
	pos = ffsk->filter_pos;
	step = ffsk->filter_step;
	bps = ffsk->bits_per_sample;
	spl = ffsk->filter_spl;
	for (i = 0; i < length; i++) {
#ifdef DEBUG_MODULATOR
		printf("|%s|\n", debug_amplitude((double)samples[i] / 2333.0 /*fsk peak*/ / 2.0));
#endif
		/* write into ring buffer */
		spl[pos++] = sample[i];
		if (pos == max)
			pos = 0;
		/* if 1/10th of a bit duration is reached, decode buffer */
		step += bps;
		if (step >= FILTER_STEPS) {
			step -= FILTER_STEPS;
			ffsk_decode_step(ffsk, pos);
		}
	}
	ffsk->filter_step = step;
	ffsk->filter_pos = pos;
}

/* render frame */
int ffsk_render_frame(ffsk_t *ffsk, const char *frame, int length, sample_t *sample)
{
	int bit, polarity;
        double phaseshift, phase;
	int count = 0, i;

	polarity = ffsk->polarity;
	phaseshift = ffsk->phaseshift65536;
	phase = ffsk->phase65536;
	for (i = 0; i < length; i++) {
		bit = (frame[i] == '1');
		do {
			*sample++ = dsp_tone_bit[polarity][bit][(uint16_t)phase];
			count++;
			phase += phaseshift;
		} while (phase < 65536.0);
		phase -= 65536.0;
		/* flip polarity when we have 1.5 sine waves */
		if (bit == 0)
			polarity = 1 - polarity;
	}
	ffsk->phase65536 = phase;
	ffsk->polarity = polarity;

	/* return number of samples created for frame */
	return count;
}

