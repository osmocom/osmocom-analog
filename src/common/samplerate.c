/* Sample rate conversion
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
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "samplerate.h"

/* NOTE: This is quick and dirtry. */

int init_samplerate(samplerate_t *state, double samplerate)
{
#if 0
	if ((samplerate % 8000)) {
		fprintf(stderr, "Sample rate must be a muliple of 8000 to support MNCC socket interface, aborting!\n");
		return -EINVAL;
	}
#endif
	memset(state, 0, sizeof(*state));
	state->factor = samplerate / 8000.0;

	filter_lowpass_init(&state->up.lp, 4000.0, samplerate, 1);
	filter_lowpass_init(&state->down.lp, 4000.0, samplerate, 1);

	return 0;
}

/* convert input sample rate to 8000 Hz */
int samplerate_downsample(samplerate_t *state, int16_t *input, int input_num, int16_t *output)
{
	int output_num = 0, i, idx;
	double factor = state->factor, in_index;
	double spl[input_num];
	int32_t value;

	/* convert samples to double */
	for (i = 0; i < input_num; i++)
		spl[i] = *input++ / 32768.0;

	/* filter down */
	filter_process(&state->down.lp, spl, input_num);

	/* resample filtered result */
	in_index = state->down.in_index;

	for (i = 0; ; i++) {
		/* convert index to int */
		idx = (int)in_index;
		/* if index is outside input sample range, we are done */
		if (idx >= input_num)
			break;
		/* copy value from input to output */
		value = spl[idx] * 32768.0;
		if (value < -32768)
			value = -32768;
		else if (value > 32767)
			value = 32767;
		*output++ = value;
		/* count output number */
		output_num++;
		/* increment input index */
		in_index += factor;
	}

	/* remove number of input samples from index */
	in_index -= (double)input_num;
	/* in_index cannot be negative, excpet due to rounding error, so... */
	if ((int)in_index < 0)
		in_index = 0.0;

	state->down.in_index = in_index;

	return output_num;
}

/* convert 8000 Hz sample rate to output sample rate */
int samplerate_upsample(samplerate_t *state, int16_t *input, int input_num, int16_t *output)
{
	int output_num = 0, i, idx;
	double factor = 1.0 / state->factor, in_index;
	double spl[(int)((double)input_num / factor + 0.5) + 10]; /* add some fafety */
	int32_t value;

	/* resample input */
	in_index = state->up.in_index;

	for (i = 0; ; i++) {
		/* convert index to int */
		idx = (int)in_index;
		/* if index is outside input sample range, we are done */
		if (idx >= input_num)
			break;
		/* copy value */
		spl[i] = input[idx] / 32768.0;
		/* count output number */
		output_num++;
		/* increment input index */
		in_index += factor;
	}

	/* remove number of input samples from index */
	in_index -= (double)input_num;
	/* in_index cannot be negative, excpet due to rounding error, so... */
	if ((int)in_index < 0)
		in_index = 0.0;

	state->up.in_index = in_index;

	/* filter up */
	filter_process(&state->up.lp, spl, output_num);

	/* convert double to samples */
	for (i = 0; i < output_num; i++) {
		value = spl[i] * 32768.0;
		if (value < -32768)
			value = -32768;
		else if (value > 32767)
			value = 32767;
		*output++ = value;
	}

	return output_num;
}

