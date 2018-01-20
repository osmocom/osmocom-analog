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

#include <stdint.h>
#include <string.h>
#include <math.h>
#include "../libsample/sample.h"
#include "am.h"

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
	mod->phasestep = 2.0 * M_PI * offset / samplerate;

	return 0;
}

void am_mod_exit(am_mod_t __attribute__((unused)) *mod)
{
}

void am_modulate_complex(am_mod_t *mod, sample_t *amplitude, int num, float *baseband)
{
	int s;
	double vector;
	double phasestep = mod->phasestep;
	double phase = mod->phase;
	double gain = mod->gain;
	double bias = mod->bias;

	for (s = 0; s < num; s++) {
		vector = *amplitude++ * gain + bias;
		*baseband++ = cos(phase) * vector;
		*baseband++ = sin(phase) * vector;
		phase += phasestep;
		if (phase < 0.0)
			phase += 2.0 * M_PI;
		else if (phase >= 2.0 * M_PI)
			phase -= 2.0 * M_PI;
	}

	mod->phase = phase;
}

/* init AM demodulator */
int am_demod_init(am_demod_t *demod, double samplerate, double offset, double bandwidth, double gain)
{
	memset(demod, 0, sizeof(*demod));
	demod->gain = gain;
	demod->phasestep = 2 * M_PI * -offset / samplerate;

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
	double phasestep = demod->phasestep;
	double phase = demod->phase;
	double gain = demod->gain;
	double i, q;
	double _sin, _cos;

	/* rotate spectrum */
	for (s = 0, ss = 0; s < length; s++) {
		i = baseband[ss++];
		q = baseband[ss++];
		_sin = sin(phase);
		_cos = cos(phase);
		phase += phasestep;
		if (phase < 0.0)
			phase += 2.0 * M_PI;
		else if (phase >= 2.0 * M_PI)
			phase -= 2.0 * M_PI;
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

