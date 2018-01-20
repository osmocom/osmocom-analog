/* Clipper implementation, based on code by Jonathan Olds
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
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include "../libsample/sample.h"
#include "clipper.h"

static double clipper_lut[6000];

static double clipper_point = NAN;

void clipper_init(double point)
{
	double a;
	int i;

	if (point > 0.99)
		point = 0.99;
	if (point < 0.01)
		point = 0.01;
	clipper_point = point;

	a = M_PI / (2.0 * (1.0 - clipper_point));

	for (i = 0; i < 6000; i++)
		clipper_lut[i] = clipper_point + atan(a * i / 1000.0) / a;
}

void clipper_process(sample_t *samples, int length)
{
	int i;
	double val, inv, shiftmultval;
	int n, q;

	if (isnan(clipper_point)) {
		fprintf(stderr, "Clipper not initialized, aborting!\n");
		abort();
	}

	for (i = 0; i < length; i++) {
		val = samples[i];
		if (val < 0) {
			inv = -1.0;
			val = -val;
		} else
			inv = 1.0;
		shiftmultval = (val - clipper_point) * 1000.0;
		/* no clipping up to clipping point */
		if (shiftmultval <= 0.0)
			continue;
		n = (int)shiftmultval;
		q = n + 1;
		if (q >= 6000) {
			samples[i] = inv;
			continue;
		}
		/* get clipped value from lut, interpolate between table entries */
		val = clipper_lut[n] + (shiftmultval - (double)n) * (clipper_lut[q] - clipper_lut[n]);
		samples[i] = val * inv;
	}
}

