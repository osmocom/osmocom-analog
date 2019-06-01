/* color test image generator
 *
 * (C) 2019 by Andreas Eversberg <jolly@eversberg.eu>
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

#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include "../libsample/sample.h"
#include "ebu.h"

#define RAMP_WIDTH	0.0000002

static struct color_bar {
	double luminance, amplitude, phase;
} color_bar[8] = {
	{1.0,   0.0,     0.0},
	{0.664, 0.336, 167.1},
	{0.526, 0.474, 283.5},
	{0.440, 0.443, 240.7},
	{0.310, 0.443,  60.7},
	{0.224, 0.474, 103.5},
	{0.086, 0.336, 347.1},
	{0.0,   0.0,     0.0},
};

int ebu_gen_line(sample_t *sample, double x, double samplerate, sample_t *color_u, sample_t *color_v, int v_polarity, double line_start, double line_end)
{
	int b = 5;
	double step = 1.0 / samplerate, render_end;
	int i = 0;
	double amplitude, Y, U, V, colorphase;

	/* skip x to line_start */
	while (x < line_start && x < line_end) {
		i++;
		x += step;
	}
	if (x >= line_end)
		return i;

	/* color */
	for (b = 0; b < 8; b++) {
		Y = color_bar[b].luminance;
		amplitude = color_bar[b].amplitude;
		if (v_polarity < 0)
			colorphase = (360.0 - color_bar[b].phase) / 180.0 * M_PI;
		else
			colorphase = color_bar[b].phase / 180.0 * M_PI;
		U = cos(colorphase) * amplitude;
		V = sin(colorphase) * amplitude;
		render_end = (line_end - line_start) / 8 * (b + 1) + line_start;
		while (x < render_end) {
			color_u[i] = U;
			color_v[i] = V;
			sample[i++] = Y;
			x += step;
		}
	}

	return i;
}
