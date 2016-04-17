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

	biquad_init(&state->up.bq, 4000.0, samplerate);
	biquad_init(&state->down.bq, 4000.0, samplerate);

	return 0;
}

/* convert input sample rate to 8000 Hz */
int samplerate_downsample(samplerate_t *state, int16_t *input, int input_num, int16_t *output)
{
	int output_num, i;
	double factor = state->factor, step;
	double spl[input_num + 10]; /* add some safety */
	int32_t value;

	/* convert samples to double */
	for (i = 0; i < input_num; i++)
		spl[i] = *input++ / 32768.0;

	/* filter down */
	biquad_process(&state->down.bq, spl, input_num, 1);
	output_num = (int)((double)input_num / factor);

	/* resample filtered result */
	for (i = 0, step = 0.5 / (double)output_num; i < output_num; i++, step += factor) {
		value = spl[(int)step] * 32768.0;
		if (value < -32768)
			value = -32768;
		else if (value > 32767)
			value = 32767;
		*output++ = value;
	}
	if ((int)(step - factor) >= input_num) {
		fprintf(stderr, "Error: input_num is %d, so step should be close to 0.5 below that, but it is %.4f. Please fix!\n", input_num, step);
		abort();
	}

	return output_num;
}

/* convert 8000 Hz sample rate to output sample rate */
int samplerate_upsample(samplerate_t *state, int16_t *input, int input_num, int16_t *output)
{
	int output_num, i;
	double factor = 1.0 / state->factor, step;
	double spl[(int)((double)input_num / factor + 0.5) + 10]; /* add some fafety */
	int32_t value;

	output_num = (int)((double)input_num / factor + 0.5);

	/* resample input */
	for (i = 0, step = 0.5 / (double)output_num; i < output_num; i++, step += factor)
		spl[i] = input[(int)step] / 32768.0;
	if ((int)(step - factor) >= input_num) {
		fprintf(stderr, "Error: input_num is %d, so step should be close to 0.5 below that, but it is %.4f. Please fix!\n", input_num, step);
		abort();
	}

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
}

