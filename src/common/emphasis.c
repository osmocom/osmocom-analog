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


int init_emphasis(emphasis_t *state, int samplerate)
{
	double factor;

	memset(state, 0, sizeof(*state));
	if (samplerate < 24000) {
		PDEBUG(DDSP, DEBUG_ERROR, "Sample rate must be at least 24000 Hz!\n");
		return -1;
	}

	factor = 0.95;
	state->p.factor = factor;
	state->p.amp = samplerate / 48000.0 * 4.0; /* mysterious 48000 */
	state->d.factor = factor;
	state->d.amp = 1.0 / (samplerate / 48000.0 * 4.0); /* mysterious 48000 */

	return 0;
}

void pre_emphasis(emphasis_t *state, int16_t *samples, int num)
{
	int32_t sample;
	double old_value, new_value, last_value, factor, amp;
	int i;

	last_value = state->p.last_value;
	factor = state->p.factor;
	amp = state->p.amp;

	for (i = 0; i < num; i++) {
		old_value = (double)(*samples) / 32768.0;

		new_value = old_value - factor * last_value;

		last_value = old_value;

		sample = (int)(amp * new_value * 32768.0);
		if (sample > 32767)
			sample = 32767;
		else if (sample < -32768)
			sample = -32768;
		*samples++ = sample;
	}

	state->p.last_value = last_value;
}

void de_emphasis(emphasis_t *state, int16_t *samples, int num)
{
	int32_t sample;
	double old_value, new_value, last_value, factor, amp;
	int i;

	last_value = state->d.last_value;
	factor = state->d.factor;
	amp = state->d.amp;

	for (i = 0; i < num; i++) {
		old_value = (double)(*samples) / 32768.0;

		new_value = old_value + factor * last_value;

		last_value = new_value;

		sample = (int)(amp * new_value * 32768.0);
		if (sample > 32767)
			sample = 32767;
		else if (sample < -32768)
			sample = -32768;
		*samples++ = sample;
	}

	state->d.last_value = last_value;
}

