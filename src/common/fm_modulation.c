/* FM modulation processing
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "sample.h"
#include "fm_modulation.h"

//#define FAST_SINE

/* init FM modulator */
int fm_mod_init(fm_mod_t *mod, double samplerate, double offset, double amplitude)
{
	int i;

	memset(mod, 0, sizeof(*mod));
	mod->samplerate = samplerate;
	mod->offset = offset;
	mod->amplitude = amplitude;

	mod->ramp_length = samplerate * 0.001;
	mod->ramp_tab = calloc(mod->ramp_length, sizeof(*mod->ramp_tab));
	if (!mod->ramp_tab) {
		fprintf(stderr, "No mem!\n");
		return -ENOMEM;
	}
	mod->state = MOD_STATE_OFF;

	/* generate ramp up with ramp_length */
	for (i = 0; i < mod->ramp_length; i++)
		mod->ramp_tab[i] = 0.5 - cos(M_PI * i / mod->ramp_length) / 2.0;

#ifdef FAST_SINE
	mod->sin_tab = calloc(65536+16384, sizeof(*mod->sin_tab));
	if (!mod->sin_tab) {
		fprintf(stderr, "No mem!\n");
		fm_mod_exit(mod);
		return -ENOMEM;
	}

	/* generate sine and cosine */
	for (i = 0; i < 65536+16384; i++)
		mod->sin_tab[i] = sin(2.0 * M_PI * (double)i / 65536.0) * amplitude;
#endif

	return 0;
}

void fm_mod_exit(fm_mod_t *mod)
{
	if (mod->ramp_tab) {
		free(mod->ramp_tab);
		mod->ramp_tab = NULL;
	}
	if (mod->sin_tab) {
		free(mod->sin_tab);
		mod->sin_tab = NULL;
	}
}

/* do frequency modulation of samples and add them to existing baseband */
void fm_modulate_complex(fm_mod_t *mod, sample_t *frequency, uint8_t *power, int length, float *baseband)
{
	double dev, rate, phase, offset;
	int ramp, ramp_length;
	double *ramp_tab;
#ifdef FAST_SINE
	double *sin_tab, *cos_tab;
#else
	double amplitude;
#endif

	rate = mod->samplerate;
	phase = mod->phase;
	offset = mod->offset;
	ramp = mod->ramp;
	ramp_length = mod->ramp_length;
	ramp_tab = mod->ramp_tab;
#ifdef FAST_SINE
	sin_tab = mod->sin_tab;
	cos_tab = mod->sin_tab + 16384;
#else
	amplitude = mod->amplitude;
#endif

again:
	switch (mod->state) {
	case MOD_STATE_ON:
		/* modulate */
		while (length) {
			/* is power is not set, ramp down */
			if (!(*power)) {
				mod->state = MOD_STATE_RAMP_DOWN;
				break;
			}
			/* deviation is defined by the frequency value and the offset */
			dev = offset + *frequency++;
			power++;
			length--;
#ifdef FAST_SINE
			phase += 65536.0 * dev / rate;
			if (phase < 0.0)
				phase += 65536.0;
			else if (phase >= 65536.0)
				phase -= 65536.0;
			*baseband++ += cos_tab[(uint16_t)phase];
			*baseband++ += sin_tab[(uint16_t)phase];
#else
			phase += 2.0 * M_PI * dev / rate;
			if (phase < 0.0)
				phase += 2.0 * M_PI;
			else if (phase >= 2.0 * M_PI)
				phase -= 2.0 * M_PI;
			*baseband++ += cos(phase) * amplitude;
			*baseband++ += sin(phase) * amplitude;
#endif
		}
		break;
	case MOD_STATE_RAMP_DOWN:
		while (length) {
			/* if power is set, ramp up */
			if (*power) {
				mod->state = MOD_STATE_RAMP_UP;
				break;
			}
			if (ramp == 0) {
				mod->state = MOD_STATE_OFF;
				break;
			}
			dev = offset + *frequency++;
			power++;
			length--;
#ifdef FAST_SINE
			phase += 65536.0 * dev / rate;
			if (phase < 0.0)
				phase += 65536.0;
			else if (phase >= 65536.0)
				phase -= 65536.0;
			*baseband++ += cos_tab[(uint16_t)phase] * ramp_tab[ramp];
			*baseband++ += sin_tab[(uint16_t)phase] * ramp_tab[ramp];
#else
			phase += 2.0 * M_PI * dev / rate;
			if (phase < 0.0)
				phase += 2.0 * M_PI;
			else if (phase >= 2.0 * M_PI)
				phase -= 2.0 * M_PI;
			*baseband++ += cos(phase) * amplitude * ramp_tab[ramp];
			*baseband++ += sin(phase) * amplitude * ramp_tab[ramp];
#endif
			ramp--;
		}
		break;
	case MOD_STATE_OFF:
		while (length) {
			/* if power is set, ramp up */
			if (*power) {
				mod->state = MOD_STATE_RAMP_UP;
				break;
			}
			frequency++;
			power++;
			length--;
			baseband += 2;
		}
		break;
	case MOD_STATE_RAMP_UP:
		while (length) {
			/* is power is not set, ramp down */
			if (!(*power)) {
				mod->state = MOD_STATE_RAMP_DOWN;
				break;
			}
			if (ramp == ramp_length - 1) {
				mod->state = MOD_STATE_ON;
				break;
			}
			/* deviation is defined by the frequency value and the offset */
			dev = offset + *frequency++;
			power++;
			length--;
#ifdef FAST_SINE
			phase += 65536.0 * dev / rate;
			if (phase < 0.0)
				phase += 65536.0;
			else if (phase >= 65536.0)
				phase -= 65536.0;
			*baseband++ += cos_tab[(uint16_t)phase] * ramp_tab[ramp];
			*baseband++ += sin_tab[(uint16_t)phase] * ramp_tab[ramp];
#else
			phase += 2.0 * M_PI * dev / rate;
			if (phase < 0.0)
				phase += 2.0 * M_PI;
			else if (phase >= 2.0 * M_PI)
				phase -= 2.0 * M_PI;
			*baseband++ += cos(phase) * amplitude * ramp_tab[ramp];
			*baseband++ += sin(phase) * amplitude * ramp_tab[ramp];
#endif
			ramp++;
		}
		break;
	}
	if (length)
		goto again;

	mod->phase = phase;
	mod->ramp = ramp;
}

/* init FM demodulator */
int fm_demod_init(fm_demod_t *demod, double samplerate, double offset, double bandwidth)
{
	memset(demod, 0, sizeof(*demod));
	demod->samplerate = samplerate;
#ifdef FAST_SINE
	demod->rot = 65536.0 * -offset / samplerate;
#else
	demod->rot = 2 * M_PI * -offset / samplerate;
#endif

	/* use fourth order (2 iter) filter, since it is as fast as second order (1 iter) filter */
	iir_lowpass_init(&demod->lp[0], bandwidth / 2.0, samplerate, 2);
	iir_lowpass_init(&demod->lp[1], bandwidth / 2.0, samplerate, 2);

#ifdef FAST_SINE
	int i;

	demod->sin_tab = calloc(65536+16384, sizeof(*demod->sin_tab));
	if (!demod->sin_tab) {
		fprintf(stderr, "No mem!\n");
		return -ENOMEM;
	}

	/* generate sine and cosine */
	for (i = 0; i < 65536+16384; i++)
		demod->sin_tab[i] = sin(2.0 * M_PI * (double)i / 65536.0);
#endif

	return 0;
}

void fm_demod_exit(fm_demod_t *demod)
{
	if (demod->sin_tab) {
		free(demod->sin_tab);
		demod->sin_tab = NULL;
	}
}

/* do frequency demodulation of baseband and write them to samples */
void fm_demodulate_complex(fm_demod_t *demod, sample_t *frequency, int length, float *baseband, sample_t *I, sample_t *Q)
{
	double phase, rot, last_phase, dev, rate;
	double _sin, _cos;
	sample_t i, q;
	int s, ss;
#ifdef FAST_SINE
	double *sin_tab, *cos_tab;
#endif

	rate = demod->samplerate;
	phase = demod->phase;
	rot = demod->rot;
#ifdef FAST_SINE
	sin_tab = demod->sin_tab;
	cos_tab = demod->sin_tab + 16384;
#endif
	for (s = 0, ss = 0; s < length; s++) {
		phase += rot;
		i = baseband[ss++];
		q = baseband[ss++];
#ifdef FAST_SINE
		if (phase < 0.0)
			phase += 65536.0;
		else if (phase >= 65536.0)
			phase -= 65536.0;
		_sin = sin_tab[(uint16_t)phase];
		_cos = cos_tab[(uint16_t)phase];
#else
		if (phase < 0.0)
			phase += 2.0 * M_PI;
		else if (phase >= 2.0 * M_PI)
			phase -= 2.0 * M_PI;
		_sin = sin(phase);
		_cos = cos(phase);
#endif
		I[s] = i * _cos - q * _sin;
		Q[s] = i * _sin + q * _cos;
	}
	demod->phase = phase;
	iir_process(&demod->lp[0], I, length);
	iir_process(&demod->lp[1], Q, length);
	last_phase = demod->last_phase;
	for (s = 0; s < length; s++) {
		phase = atan2(Q[s], I[s]);
		dev = (phase - last_phase) / 2 / M_PI;
		last_phase = phase;
		if (dev < -0.49)
			dev += 1.0;
		else if (dev > 0.49)
			dev -= 1.0;
		dev *= rate;
		frequency[s] = dev;
	}
	demod->last_phase = last_phase;
}

void fm_demodulate_real(fm_demod_t *demod, sample_t *frequency, int length, sample_t *baseband, sample_t *I, sample_t *Q)
{
	double phase, rot, last_phase, dev, rate;
	double _sin, _cos;
	sample_t i;
	int s, ss;
#ifdef FAST_SINE
	double *sin_tab, *cos_tab;
#endif

	rate = demod->samplerate;
	phase = demod->phase;
	rot = demod->rot;
#ifdef FAST_SINE
	sin_tab = demod->sin_tab;
	cos_tab = demod->sin_tab + 16384;
#endif
	for (s = 0, ss = 0; s < length; s++) {
		phase += rot;
		i = baseband[ss++];
#ifdef FAST_SINE
		if (phase < 0.0)
			phase += 65536.0;
		else if (phase >= 65536.0)
			phase -= 65536.0;
		_sin = sin_tab[(uint16_t)phase];
		_cos = cos_tab[(uint16_t)phase];
#else
		if (phase < 0.0)
			phase += 2.0 * M_PI;
		else if (phase >= 2.0 * M_PI)
			phase -= 2.0 * M_PI;
		_sin = sin(phase);
		_cos = cos(phase);
#endif
		I[s] = i * _cos;
		Q[s] = i * _sin;
	}
	demod->phase = phase;
	iir_process(&demod->lp[0], I, length);
	iir_process(&demod->lp[1], Q, length);
	last_phase = demod->last_phase;
	for (s = 0; s < length; s++) {
		phase = atan2(Q[s], I[s]);
		dev = (phase - last_phase) / 2 / M_PI;
		last_phase = phase;
		if (dev < -0.49)
			dev += 1.0;
		else if (dev > 0.49)
			dev -= 1.0;
		dev *= rate;
		frequency[s] = dev;
	}
	demod->last_phase = last_phase;
}

