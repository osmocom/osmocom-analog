/* VCR test image generator
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
#include <stdint.h>
#include "../libsample/sample.h"
#include "vcr.h"

/* test ID of calibration part:
 *
 * 1. line: ID of 48 Bits in 52 uS
 * 2. line: 50% Gray
 * 3. line: 50% Gray +- 25% deviation, frequency 0 Hz
 * 4. line: 50% Gray +- 12.5% deviation, frequency 0 Hz
 * 5. and 6. line: as line 3 and 4, but frequnency 0.2 MHz
 * each next two lines as above, but increments frequency by 0.2
 * 63. and 64. line: the incement reaches 6 MHz
 */
#define TEST_ID "VHS V1"

/* create cosine ramp, so that the bandwidth is not higher than 2 * width of the ramp(0..1) */
static inline double ramp(double x)
{
	return 0.5 - 0.5 * cos(x * M_PI);
}

int vcr_gen_line(sample_t *sample, double x, double samplerate, sample_t *color_u, sample_t *color_v, int v_polarity, double line_start, double line_end, int line)
{
	double step = 1.0 / samplerate;
	int i = 0;
	double Y, Y2, U, V, frequency, colorphase, saturation;
	double render_end, n, width;
	int b;

	/* skip x to line_start */
	while (x < line_start && x < line_end) {
		i++;
		x += step;
	}
	if (x >= line_end)
		return i;

	/* select test pattern */
	switch (line / 32) {
	case 0:
	case 1:
		/* frequency test
		 *
		 * show frequencies from 0.5 MHz to 3.5 MHz */
		frequency = (double)(line & 63) / 63.0 * 3000000.0 + 500000.0;
		while (x < line_end) {
			Y = 1.0 - (line_end - x) / (line_end - line_start);
			sample[i++] = 0.5 + Y * 0.5 * sin(x * frequency * 2 * M_PI);
			x += step;
		}
		break;
	case 2:
		/* level test
		 *
		 * show levels from 0 to 100% luminance */
		Y = (double)(line & 31) / 31.0;
		while (x < line_end) {
			sample[i++] = Y;
			x += step;
		}
		break;
	case 3:
		/* edge test
		 *
		 * show edges with 0.5 MHz to 3.5 MHz frequency */
		for (n = 0.0; n < 0.99; n += 0.1) {
			frequency = (double)(line & 31) / 31.0 * 3000000.0 + 500000.0;
			width = 1.0 / frequency / 2.0; /* half wave length */
			/* low level before ramping up */
			Y = 0.5 - (n + 0.1) / 2.0;
			Y2 = 1.0 - Y;
			render_end = line_start + (line_end - line_start) / 40.0 * (40.0 * n + 1) - width / 2.0;
			while (x < render_end) {
				sample[i++] = Y;
				x += step;
			}
			/* ramp up */
			render_end += width;
			while (x < render_end) {
				sample[i++] = ramp((render_end - x) / width) * (Y - Y2) + Y2;
				x += step;
			}
			/* high level before ramping down */
			render_end = line_start + (line_end - line_start) / 40.0 * (40.0 * n + 3) - width / 2.0;
			while (x < render_end) {
				sample[i++] = Y2;
				x += step;
			}
			/* ramp down */
			render_end += width;
			while (x < render_end) {
				sample[i++] = ramp((render_end - x) / width) * (Y2 - Y) + Y;
				x += step;
			}
			/* low level after ramping down */
			render_end = line_start + (line_end - line_start) / 40.0 * (40.0 * n + 4);
			while (x < render_end) {
				sample[i++] = Y;
				x += step;
			}
		}
		break;
	case 4:
	case 5:
		/* color test
		 *
		 * show color from 0 to 100% saturation */
		Y = (1.0 - 5.0 / 7.0) * 0.75;
		saturation = (double)(line & 63) / 63.0;
		if (v_polarity < 0)
			colorphase = (360.0 - 103.5) / 180.0 * M_PI;
		else 
			colorphase = 103.5 / 180.0 * M_PI;
		U = cos(colorphase) * saturation * 0.474 / 2.0;
		V = sin(colorphase) * saturation * 0.474 / 2.0;
		while (x < line_end) {
			sample[i++] = Y;
			color_u[i] = U;
			color_v[i] = V;
			x += step;
		}
		break;
	case 6:
	case 7:
		/* calibration signal
		 *
		 * this signal is used to calibrate the frequency response of the de-emphasis
		 */
		if ((line & 63) == 0) {
			/* generate identification line to be detected by the decoder */
			for (b = 0; b < 48; b++) {
				render_end = line_start + (line_end - line_start) / 48.0 * (double)(b + 1);
				Y = (TEST_ID[b / 8] >> (b & 7)) & 1;
				while (x < render_end) {
					sample[i++] = Y;
					x += step;
				}
			}
		} else if ((line & 63) == 1) {
			/* generate zero level (50% brightness) */
			while (x < line_end) {
				sample[i++] = 0.5;
				x += step;
			}
		} else {
			/* each pair of lines: upper uses 50% deviation, lower uses 25% deviation */
			if ((line & 1) == 0)
				Y = 0.5;
			else
				Y = 0.25;
			/* frequency from 0 - 6 MHz in 31 steps (each 0.2 MHz) */
			frequency = (double)(((line & 63) - 2) / 2) * 200000.0;
			while (x < line_end) {
				sample[i++] = 0.5 + Y * 0.5 * cos(x * frequency * 2 * M_PI);
				x += step;
			}
		}
		break;
	}

	return i;
}
