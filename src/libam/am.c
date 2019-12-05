/* AM modulation and de-modulation
 *
 * (C) 2018 by Andreas Eversberg <jolly@eversberg.eu>
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
#include "../libsample/sample.h"
#include "am.h"

static int has_init = 0;
static int fast_math = 0;
static float *sin_tab = NULL, *cos_tab = NULL;

/* global init */
int am_init(int _fast_math)
{
	fast_math = _fast_math;

	if (fast_math) {
		int i;

		sin_tab = calloc(65536+16384, sizeof(*sin_tab));
		if (!sin_tab) {
			fprintf(stderr, "No mem!\n");
			return -ENOMEM;
		}
		cos_tab = sin_tab + 16384;

		/* generate sine and cosine */
		for (i = 0; i < 65536+16384; i++)
			sin_tab[i] = sin(2.0 * M_PI * (double)i / 65536.0);
	}

	has_init = 1;

	return 0;
}

/* global exit */
void am_exit(void)
{
	if (sin_tab) {
		free(sin_tab);
		sin_tab = cos_tab = NULL;
	}

	has_init = 0;
}

#define CARRIER_FILTER 30.0

/* Amplitude modulation in SDR:
 * Just use the base band (audio signal) as real value, and 0.0 as imaginary
 * value. The you have two side bands. Be sure to have a DC level, so you
 * have a carrier.
 */

int am_mod_init(am_mod_t *mod, double samplerate, double offset, double gain, double bias)
{
	memset(mod, 0, sizeof(*mod));
	mod->gain = gain;
	mod->bias = bias;
	if (fast_math)
		mod->rot = 65536.0 * offset / samplerate;
	else
		mod->rot = 2.0 * M_PI * offset / samplerate;

	return 0;
}

void am_mod_exit(am_mod_t __attribute__((unused)) *mod)
{
}

void am_modulate_complex(am_mod_t *mod, sample_t *amplitude, int num, float *baseband)
{
	int s;
	double vector;
	double rot = mod->rot;
	double phase = mod->phase;
	double gain = mod->gain;
	double bias = mod->bias;

	for (s = 0; s < num; s++) {
		vector = *amplitude++ * gain + bias;
		if (fast_math) {
			*baseband++ += cos_tab[(uint16_t)phase] * vector;
			*baseband++ += sin_tab[(uint16_t)phase] * vector;
			phase += rot;
			if (phase < 0.0)
				phase += 65536.0;
			else if (phase >= 65536.0)
				phase -= 65536.0;
		} else {
			*baseband++ += cos(phase) * vector;
			*baseband++ += sin(phase) * vector;
			phase += rot;
			if (phase < 0.0)
				phase += 2.0 * M_PI;
			else if (phase >= 2.0 * M_PI)
				phase -= 2.0 * M_PI;
		}
	}

	mod->phase = phase;
}

/* init AM demodulator */
int am_demod_init(am_demod_t *demod, double samplerate, double offset, double bandwidth, double gain)
{
	memset(demod, 0, sizeof(*demod));
	demod->gain = gain;
	if (fast_math)
		demod->rot = 65536.0 * -offset / samplerate;
	else
		demod->rot = 2 * M_PI * -offset / samplerate;

	/* use fourth order (2 iter) filter, since it is as fast as second order (1 iter) filter */
	iir_lowpass_init(&demod->lp[0], bandwidth, samplerate, 2);
	iir_lowpass_init(&demod->lp[1], bandwidth, samplerate, 2);

	/* filter carrier */
	iir_lowpass_init(&demod->lp[2], CARRIER_FILTER, samplerate, 1);

	return 0;
}

void am_demod_exit(am_demod_t __attribute__((unused)) *demod)
{
}

/* do amplitude demodulation of baseband and write them to samples */
void am_demodulate_complex(am_demod_t *demod, sample_t *amplitude, int length, float *baseband, sample_t *I, sample_t *Q, sample_t *carrier)
{
	int s, ss;
	double rot = demod->rot;
	double phase = demod->phase;
	double gain = demod->gain;
	double i, q;
	double _sin, _cos;

	/* rotate spectrum */
	for (s = 0, ss = 0; s < length; s++) {
		i = baseband[ss++];
		q = baseband[ss++];
		phase += rot;
		if (fast_math) {
			if (phase < 0.0)
				phase += 65536.0;
			else if (phase >= 65536.0)
				phase -= 65536.0;
			_sin = sin_tab[(uint16_t)phase];
			_cos = cos_tab[(uint16_t)phase];
		} else {
			if (phase < 0.0)
				phase += 2.0 * M_PI;
			else if (phase >= 2.0 * M_PI)
				phase -= 2.0 * M_PI;
			_sin = sin(phase);
			_cos = cos(phase);
		}
		I[s] = i * _cos - q * _sin;
		Q[s] = i * _sin + q * _cos;
	}
	demod->phase = phase;

	/* filter bandwidth */
	iir_process(&demod->lp[0], I, length);
	iir_process(&demod->lp[1], Q, length);

	/* demod */
	for (s = 0; s < length; s++)
		amplitude[s] = carrier[s] = sqrt(I[s] * I[s] + Q[s] * Q[s]);

	/* filter carrier */
	iir_process(&demod->lp[2], carrier, length);

	/* normalize */
	for (s = 0; s < length; s++)
		amplitude[s] = (amplitude[s] - carrier[s]) / carrier[s] * gain;
}

