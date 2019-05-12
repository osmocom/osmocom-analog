/* Color Convergence test image generator
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
#include "convergence.h"

#define GRID_LINES	(4 * thick)
#define GRID_HEIGHT	40
#define GRID_HEIGHT2	20
#define CENTER_LINE	(287 - 2)

#define GRID_WIDTH	0.0000027
#define GRID_WIDTH2	0.00000135
#define RAMP_WIDTH	(0.0000002 * thick)

#define GRID_LEVEL	1.0
#define FIELD_LEVEL	0.00

/* create cosine ramp, so that the bandwidth is not higher than 2 * width of the ramp(0..1) */
static inline double ramp(double x)
{
	return 0.5 - 0.5 * cos(x * M_PI);
}

int convergence_gen_line(sample_t *sample, double x, double samplerate, double line_start, double line_end, int line, double thick)
{
	double step = 1.0 / samplerate;
	int i = 0;
	double render_start, render_end, center_x;

	/* skip x to line_start */
	while (x < line_start && x < line_end) {
		i++;
		x += step;
	}
	if (x >= line_end)
		return i;

	/* calculate phase for ramp start of center line */
	center_x = (line_end - line_start) / 2.0 + line_start;

	/* calculate position in grid:
	 * get the distance below the center line (line - CENTER_LINE)
	 * then be sure not to get negative value: add a multiple of the grid period
	 * then use modulo to get the distance below the grid line */
	if (((line - CENTER_LINE + GRID_HEIGHT*40) % GRID_HEIGHT) < GRID_LINES) {
		/* grid line after middle field */
		while (x < line_end) {
			sample[i++] = GRID_LEVEL;
			x += step;
		}
	} else {
		while (1) {
			/* calculate position for next ramp:
			 * get the distance to center (center_x - x - RAMP_WIDTH)
			 * then be sure not to get negative value: add a multiple of the grid period
			 * then use fmod to get the next ramp start */
			if (((line - CENTER_LINE + GRID_HEIGHT2*40) % GRID_HEIGHT2) < GRID_LINES)
				render_start = fmod(center_x - x - RAMP_WIDTH + GRID_WIDTH2*40.0, GRID_WIDTH2) + x;
			else
				render_start = fmod(center_x - x - RAMP_WIDTH + GRID_WIDTH*40.0, GRID_WIDTH) + x;
			/* draw background field up to grid start */
			while (x < render_start && x < line_end) {
				sample[i++] = FIELD_LEVEL;
				x += step;
			}
			if (x >= line_end)
				break;
			/* ramp up to grid level */
			render_end = render_start + RAMP_WIDTH;
			while (x < render_end && x < line_end) {
				sample[i++] = ramp((x - render_start) / RAMP_WIDTH) * (GRID_LEVEL - FIELD_LEVEL) + FIELD_LEVEL;
				x += step;
			}
			if (x >= line_end)
				break;
			render_start = render_end;
			/* ramp down to field level */
			render_end = render_start + RAMP_WIDTH;
			while (x < render_end && x < line_end) {
				sample[i++] = ramp((x - render_start) / RAMP_WIDTH) * (FIELD_LEVEL - GRID_LEVEL) + GRID_LEVEL;
				x += step;
			}
			if (x >= line_end)
				break;
		}
	}

	return i;
}
