/* Pre-Emphasis and De-Emphasis implementation
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
#include <string.h>
#include <math.h>
#include "../libsample/sample.h"
#include "../libfilter/iir_filter.h"
#include "emphasis.h"

#define PI		M_PI

#define CUT_OFF_H	100.0	/* cut-off frequency for high-pass filter */
#define CUT_OFF_L	4000.0	/* cut-off frequency for low-pass filter */

static void gen_sine(sample_t *samples, int num, int samplerate, double freq)
{
	int i;

	for (i = 0; i < num; i++)
		samples[i] = cos(2.0 * M_PI * freq / (double)samplerate * (double)i);
}

static double get_level(sample_t *samples, int num)
{
	int i;
	double envelope = 0;
	for (i = num/2; i < num; i++) {
		if (samples[i] > envelope)
			envelope = samples[i];
	}

	return envelope;
}

int init_emphasis(emphasis_t *state, int samplerate, double cut_off)
{
	double factor;
	sample_t test_samples[samplerate / 10];

	memset(state, 0, sizeof(*state));

	/* exp (-2 * PI * CUT_OFF * delta_t) */
	factor = exp(-2.0 * PI * cut_off / (double)samplerate); /* 1/samplerate == delta_t */

//	printf("Emphasis factor = %.3f\n", factor);
	state->p.factor = factor;
	state->p.amp = 1.0;
	state->d.factor = factor;
	state->d.amp = 1.0;

	/* do not de-emphasis below CUT_OFF_H */
	iir_highpass_init(&state->d.hp, CUT_OFF_H, samplerate, 1);

	/* do not pre-emphasis above CUT_OFF_L */
	iir_lowpass_init(&state->p.lp, CUT_OFF_L, samplerate, 1);

	/* calibrate amplification to be neutral at 1000 Hz */
	gen_sine(test_samples, sizeof(test_samples) / sizeof(test_samples[0]), samplerate, 1000.0);
	pre_emphasis(state, test_samples, sizeof(test_samples) / sizeof(test_samples[0]));
	state->p.amp = 1.0 / get_level(test_samples, sizeof(test_samples) / sizeof(test_samples[0]));
	gen_sine(test_samples, sizeof(test_samples) / sizeof(test_samples[0]), samplerate, 1000.0);
	de_emphasis(state, test_samples, sizeof(test_samples) / sizeof(test_samples[0]));
	state->d.amp = 1.0 / get_level(test_samples, sizeof(test_samples) / sizeof(test_samples[0]));

	return 0;
}

void pre_emphasis(emphasis_t *state, sample_t *samples, int num)
{
	double x, y, x_last, factor, amp;
	int i;

	iir_process(&state->p.lp, samples, num);

	x_last = state->p.x_last;
	factor = state->p.factor;
	amp = state->p.amp;

	for (i = 0; i < num; i++) {
		x = *samples;

		/* pre-emphasis */
		y = x - factor * x_last;

		x_last = x;

		*samples++ = amp * y;
	}

	state->p.x_last = x_last;
}

void de_emphasis(emphasis_t *state, sample_t *samples, int num)
{
	double x, y, y_last, factor, amp;
	int i;

	y_last = state->d.y_last;
	factor = state->d.factor;
	amp = state->d.amp;

	for (i = 0; i < num; i++) {
		x = *samples;

		/* de-emphasis */
		y = x + factor * y_last;

		y_last = y;

		*samples++ = amp * y;
	}

	state->d.y_last = y_last;
}

/* high pass filter to remove DC and low frequencies */
void dc_filter(emphasis_t *state, sample_t *samples, int num)
{
	iir_process(&state->d.hp, samples, num);
}

