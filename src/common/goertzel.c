/* Goertzel functions
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "../common/debug.h"
#include "goertzel.h"

/*
 * audio level calculation
 */

/* return average value (rectified value), that can be 0..1 */
double audio_level(int16_t *samples, int length)
{
	int bias;
	double level;
	int sk;
	int n;

	/* level calculation */
	bias = 0;
	for (n = 0; n < length; n++)
		bias += samples[n];
	bias = bias / length;

	level = 0;
	for (n = 0; n < length; n++) {
		sk = samples[n] - bias;
		if (sk < 0)
			level -= (double)sk;
		if (sk > 0)
			level += (double)sk;
	}
	level = level / (double)length / 32767.0;

	return level;
}

/*
 * goertzel filter
 */

/* filter frequencies and return their levels
 *
 * samples: pointer to sample buffer
 * length: length of buffer
 * offset: for ring buffer, start here and wrap arround to 0 when length has been hit
 * coeff: array of coefficients (coeff << 15)
 * result: array of result levels (average value of the sine, that is 1 / (PI/2) of the sine's peak)
 * k: number of frequencies to check
 */
void audio_goertzel(int16_t *samples, int length, int offset, int *coeff, double *result, int k)
{
	int32_t sk, sk1, sk2;
	int64_t cos2pik;
	int i, n;

	/* we do goertzel */
	for (i = 0; i < k; i++) {
		sk = 0;
		sk1 = 0;
		sk2 = 0;
		cos2pik = coeff[i];
		/* note: after 'length' cycles, offset is restored to its initial value */
		for (n = 0; n < length; n++) {
			sk = ((cos2pik * sk1) >> 15) - sk2 + samples[offset++];
			sk2 = sk1;
			sk1 = sk;
			if (offset == length)
				offset = 0;
		}
		/* compute level of signal */
		result[i] = sqrt(
			((double)sk * (double)sk) -
			((double)((cos2pik * sk) >> 15) * (double)sk2) +
			((double)sk2 * (double)sk2)
				) / (double)length / 32767.0 * 2.0 * 0.63662; /* 1 / (PI/2) */
	}
}

