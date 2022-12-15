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
#include "../libsample/sample.h"
#include "samplerate.h"

int init_samplerate(samplerate_t *state, double low_samplerate, double high_samplerate, double filter_cutoff)
{
	memset(state, 0, sizeof(*state));
	state->factor = high_samplerate / low_samplerate;
	if (state->factor < 1.0) {
		fprintf(stderr, "Software error: Low sample rate must be lower than high sample rate, aborting!\n");
		abort();
	}

	state->filter_cutoff = filter_cutoff;
	if (state->filter_cutoff) {
		iir_lowpass_init(&state->up.lp, filter_cutoff, high_samplerate, 2);
		iir_lowpass_init(&state->down.lp, filter_cutoff, high_samplerate, 2);
	}

	return 0;
}

/* convert high sample rate to low sample rate */
int samplerate_downsample(samplerate_t *state, sample_t *samples, int input_num)
{
	int output_num = 0, i, idx;
	double factor = state->factor, in_index, diff;
	sample_t output[(int)((double)input_num / factor + 0.5) + 10]; /* add some safety */
	sample_t last_sample;

	/* filter down */
	if (state->filter_cutoff)
		iir_process(&state->down.lp, samples, input_num);

	/* get last sample for interpolation */
	last_sample = state->down.last_sample;

	/* resample filtered result */
	in_index = state->down.in_index;

	for (i = 0; ; i++) {
		/* convert index to int */
		idx = (int)in_index;
		/* if index is outside input sample range, we are done */
		if (idx >= input_num)
			break;
		/* linear interpolation */
		diff = in_index - (double)idx;
		if (idx)
			output[i] = samples[idx - 1] * (1.0 - diff) + samples[idx] * diff;
		else
			output[i] = last_sample * (1.0 - diff) + samples[idx] * diff;
		/* count output number */
		output_num++;
		/* increment input index */
		in_index += factor;
	}

	/* store last sample for interpolation */
	if (input_num)
		state->down.last_sample = samples[input_num - 1];

	/* remove number of input samples from index */
	in_index -= (double)input_num;
	/* in_index cannot be negative, except due to rounding error, so... */
	if ((int)in_index < 0)
		in_index = 0.0;

	state->down.in_index = in_index;

	/* copy samples */
	for (i = 0; i < output_num; i++)
		*samples++ = output[i];

	return output_num;
}

int samplerate_upsample_input_num(samplerate_t *state, int output_num)
{
	double factor = 1.0 / state->factor, in_index;
	int idx = 0;

	/* count output */
	in_index = state->up.in_index;

	while (output_num--) {
		/* increment input index */
		in_index += factor;
		/* count index on overflow */
		if (in_index >= 1.0) {
			idx++;
			in_index -= 1.0;
		}
	}

	return idx;
}

int samplerate_upsample_output_num(samplerate_t *state, int input_num)
{
	double factor = 1.0 / state->factor, in_index;
	int output_num = 0, idx = 0;

	/* count output */
	in_index = state->up.in_index;

	while (idx < input_num) {
		/* count output number */
		output_num++;
		/* increment input index */
		in_index += factor;
		/* count index on overflow */
		if (in_index >= 1.0) {
			idx++;
			in_index -= 1.0;
		}
	}

	return output_num;
}

/* convert low sample rate to high sample rate */
void samplerate_upsample(samplerate_t *state, sample_t *input, int input_num, sample_t *output, int output_num)
{
	int i, idx;
	double factor = 1.0 / state->factor, in_index;
	sample_t buff[output_num];
	sample_t *samples, current_sample, last_sample;

	/* get last sample for interpolation */
	current_sample = state->up.current_sample;
	last_sample = state->up.last_sample;

	if (input == output)
		samples = buff;
	else
		samples = output;

	/* resample input */
	in_index = state->up.in_index;
	idx = 0;

	for (i = 0; i < output_num; i++) {
		/* linear interpolation */
		samples[i] = last_sample * (1.0 - in_index) + current_sample * in_index;
		/* increment input index */
		in_index += factor;
		/* get next sample on overflow */
		if (in_index >= 1.0) {
			if (idx == input_num) {
				fprintf(stderr, "Given input_num is too small, please fix!\n");
			}
			last_sample = current_sample;
			current_sample = input[idx++];
			in_index -= 1.0;
		}
	}
	if (idx < input_num) {
		fprintf(stderr, "Given input_num is too large, please fix!\n");
		abort();
	}

	/* store last sample for interpolation */
	state->up.last_sample = last_sample;
	state->up.current_sample = current_sample;
	state->up.in_index = in_index;

	/* filter up */
	if (state->filter_cutoff)
		iir_process(&state->up.lp, samples, output_num);

	if (input == output) {
		/* copy samples */
		for (i = 0; i < output_num; i++)
			*output++ = samples[i];
	}
}

