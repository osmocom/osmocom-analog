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
#include "samplerate.h"

/* generally use filter, but disable for test using quick and dirty replacement */
#define USE_FILTER

/* NOTE: This is quick and dirtry. */

int init_samplerate(samplerate_t *state, int samplerate)
{
	if ((samplerate % 8000)) {
		fprintf(stderr, "Sample rate must be a muliple of 8000 to support MNCC socket interface, aborting!\n");
		return -EINVAL;
	}
	memset(state, 0, sizeof(*state));
	state->factor = samplerate / 8000;

	biquad_init(&state->up.bq, 4000.0, samplerate);
	biquad_init(&state->down.bq, 4000.0, samplerate);

	return 0;
}

/* convert input sample rate to 8000 Hz */
int samplerate_downsample(samplerate_t *state, int16_t *input, int input_num, int16_t *output)
{
#ifdef USE_FILTER
	int output_num, i, j;
	int factor = state->factor;
	double spl[input_num];
	int32_t value;

	/* convert samples to double */
	for (i = 0; i < input_num; i++)
		spl[i] = *input++ / 32768.0;

	/* filter down */
	biquad_process(&state->down.bq, spl, input_num, 1);
	output_num = input_num / factor;

	/* resample filtered result */
	for (i = 0, j = 0; i < output_num; i++, j += factor) {
		value = spl[j] * 32768.0;
		if (value < -32768)
			value = -32768;
		else if (value > 32767)
			value = 32767;
		*output++ = value;
	}

	return output_num;
#else
	int output_num = 0, i;
	double sum;
	int factor, sum_count;

//memcpy(output, input, input_num*2);
//return input_num;
	sum = state->down.sum;
	sum_count = state->down.sum_count;
	factor = state->factor;

	for (i = 0; i < input_num; i++) {
		sum += *input++;
		sum_count++;
		if (sum_count == factor) {
			*output++ = sum / (double)sum_count;
			output_num++;
			sum = 0;
			sum_count = 0;
		}
	}

	state->down.sum = sum;
	state->down.sum_count = sum_count;

	return output_num;
#endif
}

/* convert 8000 Hz sample rate to output sample rate */
int samplerate_upsample(samplerate_t *state, int16_t *input, int input_num, int16_t *output)
{
#ifdef USE_FILTER
	int output_num, i;
	int factor = state->factor;
	double spl[input_num * factor];
	int32_t value;

	output_num = input_num * factor;

	/* resample input */
	for (i = 0; i < output_num; i++)
		spl[i] = input[i / factor] / 32768.0;

	/* filter up */
	biquad_process(&state->up.bq, spl, output_num, 1);

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
#else
	int output_num = 0, i, j;
	double last_sample, sample, slope;
	int factor;

	last_sample = state->up.last_sample;
	factor = state->factor;

	for (i = 0; i < input_num; i++) {
		sample = *input++;
		slope = (double)(sample - last_sample) / (double)factor;
//int jolly = (int)last_sample;
		for (j = 0; j < factor; j++) {
//			if (last_sample > 32767 || last_sample < -32767)
//				printf("%.5f sample=%.0f, last_sample=%d, slope=%.5f\n", last_sample, sample, jolly, slope);
			*output++ = last_sample;
			output_num++;
			last_sample += slope;
		}
		last_sample = sample;
	}

	state->up.last_sample = last_sample;

	return output_num;
#endif
}

