/* Pixle image / PAL conversion
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

#include <math.h>
#include <stdlib.h>
#include "../libsample/sample.h"
#include "image.h"

/* render image line starting with x and end with LINE_LENGTH
 *
 * sample: pointer to samples, starting at x = 0
 * samplerate: is the sample rate in hertz
 * x: at what x to start rendering
 * line: line no. to render
 */
int image_gen_line(sample_t *sample, double x, double samplerate, sample_t *color_u, sample_t *color_v, int v_polarity, double line_start, double line_end, unsigned short *img, int width)
{
	double img_x = 0;
	double step = 1.0 / samplerate;
	double img_step = (double)width / (samplerate * (line_end - line_start));
	int i = 0;
	double R, G, B, Y, U, V;

	/* skip x to line_start */
	while (x < line_start && x < line_end) {
		i++;
		x += step;
	}
	if (x >= line_end)
		return i;

	/* draw pixle into image */
	while (x < line_end) {
		R = (double)(img[(int)img_x*3+0]) / 65535.0;
		G = (double)(img[(int)img_x*3+1]) / 65535.0;
		B = (double)(img[(int)img_x*3+2]) / 65535.0;
		Y = 0.299 * R + 0.587 * G + 0.114 * B;
		U = 0.492 * (B - Y);
		V = 0.877 * (R - Y);
		sample[i] = Y;
		color_u[i] = U;
		color_v[i] = V * (double)v_polarity;
		i++;
		x += step;
		img_x += img_step;
		if ((int)img_x == width)
			break;
	}

	return i;
}
