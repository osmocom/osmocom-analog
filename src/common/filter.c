/* cut-off filter (biquad) based on Nigel Redmon (www.earlevel.com)
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
#include <stdlib.h>
#include <math.h>
#include "sample.h"
#include "filter.h"

#define PI M_PI

void filter_lowpass_init(filter_t *bq, double frequency, int samplerate, int iterations)
{
	double Fc, Q, K, norm;

	if (iterations > 64) {
		fprintf(stderr, "%s failed: too many iterations, please fix!\n", __func__);
		abort();
	}

	memset(bq, 0, sizeof(*bq));
	bq->iter = iterations;
	Q = pow(sqrt(0.5), 1.0 / (double)iterations); /* 0.7071 @ 1 iteration */
	Fc = frequency / (double)samplerate;
	K = tan(PI * Fc);
	norm = 1 / (1 + K / Q + K * K);
	bq->a0 = K * K * norm;
	bq->a1 = 2 * bq->a0;
	bq->a2 = bq->a0;
	bq->b1 = 2 * (K * K - 1) * norm;
	bq->b2 = (1 - K / Q + K * K) * norm;
}

void filter_highpass_init(filter_t *bq, double frequency, int samplerate, int iterations)
{
	double Fc, Q, K, norm;

	memset(bq, 0, sizeof(*bq));
	bq->iter = iterations;
	Q = pow(sqrt(0.5), 1.0 / (double)iterations); /* 0.7071 @ 1 iteration */
	Fc = frequency / (double)samplerate;
	K = tan(PI * Fc);
	norm = 1 / (1 + K / Q + K * K);
	bq->a0 = 1 * norm;
	bq->a1 = -2 * bq->a0;
	bq->a2 = bq->a0;
	bq->b1 = 2 * (K * K - 1) * norm;
	bq->b2 = (1 - K / Q + K * K) * norm;
}

void filter_process(filter_t *bq, sample_t *samples, int length)
{
	double a0, a1, a2, b1, b2;
	double *z1, *z2;
	double in, out;
	int iterations = bq->iter;
	int i, j;

	/* get states */
	a0 = bq->a0;
	a1 = bq->a1;
	a2 = bq->a2;
	b1 = bq->b1;
	b2 = bq->b2;

	z1 = bq->z1;
	z2 = bq->z2;

	/* process filter */
	for (i = 0; i < length; i++) {
		in = *samples;
		for (j = 0; j < iterations; j++) {
			out = in * a0 + z1[j];
			z1[j] = in * a1 + z2[j] - b1 * out;
			z2[j] = in * a2 - b2 * out;
			in = out;
		}
		*samples++ = in;
	}
}

