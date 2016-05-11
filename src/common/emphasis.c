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
#include "emphasis.h"
#include "debug.h"

#define PI		M_PI

#define CUT_OFF_E	300.0	/* cut-off frequency for emphasis filters */
#define CUT_OFF_H	300.0	/* cut-off frequency for high-pass filters */

int init_emphasis(emphasis_t *state, int samplerate)
{
	double factor, rc, dt;

	memset(state, 0, sizeof(*state));
	if (samplerate < 24000) {
		PDEBUG(DDSP, DEBUG_ERROR, "Sample rate must be at least 24000 Hz!\n");
		return -1;
	}

	/* exp (-2 * PI * CUT_OFF * delta_t) */
	factor = exp(-2.0 * PI * CUT_OFF_E / samplerate); /* 1/samplerate == delta_t */
	PDEBUG(DDSP, DEBUG_DEBUG, "Emphasis factor = %.3f\n", factor);
	state->p.factor = factor;
	state->p.amp = samplerate / 6350.0;
	state->d.d_factor = factor;
	state->d.amp = 1.0 / (samplerate / 5550.0);

	rc = 1.0 / (CUT_OFF_H * 2.0 *3.14);
	dt = 1.0 / samplerate;
	state->d.h_factor = rc / (rc + dt);
	PDEBUG(DDSP, DEBUG_DEBUG, "High-Pass factor = %.3f\n", state->d.h_factor);
	return 0;
}

void pre_emphasis(emphasis_t *state, int16_t *samples, int num)
{
	int32_t sample;
	double x, y, x_last, factor, amp;
	int i;

	x_last = state->p.x_last;
	factor = state->p.factor;
	amp = state->p.amp;

	for (i = 0; i < num; i++) {
		x = (double)(*samples) / 32768.0;

		y = x - factor * x_last;

		x_last = x;

		sample = (int)(amp * y * 32768.0);
		if (sample > 32767)
			sample = 32767;
		else if (sample < -32768)
			sample = -32768;
		*samples++ = sample;
	}

	state->p.x_last = x_last;
}

void de_emphasis(emphasis_t *state, int16_t *samples, int num)
{
	int32_t sample;
	double x, y, z, y_last, z_last, d_factor, h_factor, amp;
	int i;

	y_last = state->d.y_last;
	z_last = state->d.z_last;
	d_factor = state->d.d_factor;
	h_factor = state->d.h_factor;
	amp = state->d.amp;

	for (i = 0; i < num; i++) {
		x = (double)(*samples) / 32768.0;

		/* de-emphasis */
		y = x + d_factor * y_last;

		/* high pass */
		z = h_factor * (z_last + y - y_last);

		y_last = y;
		z_last = z;

		sample = (int)(amp * z * 32768.0);
		if (sample > 32767)
			sample = 32767;
		else if (sample < -32768)
			sample = -32768;
		*samples++ = sample;
	}

	state->d.y_last = y_last;
	state->d.z_last = z_last;
}

