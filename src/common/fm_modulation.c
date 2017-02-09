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
#include <math.h>
#include "sample.h"
#include "filter.h"
#include "fm_modulation.h"

//#define FAST_SINE

/* init FM modulator */
void fm_mod_init(fm_mod_t *mod, double samplerate, double offset, double amplitude)
{
	memset(mod, 0, sizeof(*mod));
	mod->samplerate = samplerate;
	mod->offset = offset;
	mod->amplitude = amplitude;

#ifdef FAST_SINE
	int i;

	mod->sin_tab = calloc(65536+16384, sizeof(*mod->sin_tab));
	if (!mod->sin_tab) {
		fprintf(stderr, "No mem!\n");
		abort();
	}

	/* generate sine and cosine */
	for (i = 0; i < 65536+16384; i++)
		mod->sin_tab[i] = sin(2.0 * M_PI * (double)i / 65536.0) * amplitude;
#endif
}

/* do frequency modulation of samples and add them to existing buff */
void fm_modulate(fm_mod_t *mod, sample_t *samples, int num, float *buff)
{
	double dev, rate, phase, offset;
	int s, ss;
#ifdef FAST_SINE
	double *sin_tab, *cos_tab;
#else
	double amplitude;
#endif

	rate = mod->samplerate;
	phase = mod->phase;
	offset = mod->offset;
#ifdef FAST_SINE
	sin_tab = mod->sin_tab;
	cos_tab = mod->sin_tab + 16384;
#else
	amplitude = mod->amplitude;
#endif

	/* modulate */
	for (s = 0, ss = 0; s < num; s++) {
		/* deviation is defined by the sample value and the offset */
		dev = offset + samples[s];
#ifdef FAST_SINE
		phase += 65536.0 * dev / rate;
		if (phase < 0.0)
			phase += 65536.0;
		else if (phase >= 65536.0)
			phase -= 65536.0;
		buff[ss++] += cos_tab[(uint16_t)phase];
		buff[ss++] += sin_tab[(uint16_t)phase];
#else
		phase += 2.0 * M_PI * dev / rate;
		if (phase < 0.0)
			phase += 2.0 * M_PI;
		else if (phase >= 2.0 * M_PI)
			phase -= 2.0 * M_PI;
		buff[ss++] += cos(phase) * amplitude;
		buff[ss++] += sin(phase) * amplitude;
#endif
	}

	mod->phase = phase;
}

/* init FM demodulator */
void fm_demod_init(fm_demod_t *demod, double samplerate, double offset, double bandwidth)
{
	memset(demod, 0, sizeof(*demod));
	demod->samplerate = samplerate;
#ifdef FAST_SINE
	demod->rot = 65536.0 * -offset / samplerate;
#else
	demod->rot = 2 * M_PI * -offset / samplerate;
#endif

	/* use fourth order (2 iter) filter, since it is as fast as second order (1 iter) filter */
	filter_lowpass_init(&demod->lp[0], bandwidth / 2.0, samplerate, 2);
	filter_lowpass_init(&demod->lp[1], bandwidth / 2.0, samplerate, 2);

#ifdef FAST_SINE
	int i;

	demod->sin_tab = calloc(65536+16384, sizeof(*demod->sin_tab));
	if (!demod->sin_tab) {
		fprintf(stderr, "No mem!\n");
		abort();
	}

	/* generate sine and cosine */
	for (i = 0; i < 65536+16384; i++)
		demod->sin_tab[i] = sin(2.0 * M_PI * (double)i / 65536.0);
#endif
}

/* do frequency demodulation of buff and write them to samples */
void fm_demodulate(fm_demod_t *demod, sample_t *samples, int num, float *buff)
{
	double phase, rot, last_phase, dev, rate;
	double _sin, _cos;
	sample_t I[num], Q[num], i, q;
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
	for (s = 0, ss = 0; s < num; s++) {
		phase += rot;
		i = buff[ss++];
		q = buff[ss++];
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
	filter_process(&demod->lp[0], I, num);
	filter_process(&demod->lp[1], Q, num);
	last_phase = demod->last_phase;
	for (s = 0; s < num; s++) {
		phase = atan2(Q[s], I[s]);
		dev = (phase - last_phase) / 2 / M_PI;
		last_phase = phase;
		if (dev < -0.49)
			dev += 1.0;
		else if (dev > 0.49)
			dev -= 1.0;
		dev *= rate;
		samples[s] = dev;
	}
	demod->last_phase = last_phase;
}

