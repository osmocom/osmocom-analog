/* FUBK test image generator
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
#include "fubk.h"
#include "font.h"

#define GRID_LINES	4
#define GRID_HEIGHT	40
#define CENTER_LINE	(287 - 2)

#define GRID_WIDTH	0.0000027
#define RAMP_WIDTH	0.00000015
#define TEXT_WIDTH	0.0000002

#define GRID_LEVEL	1.0
#define FIELD_LEVEL	0.25

#define CIRCLE_LEVEL	1.0
#define CIRCLE_CENTER	(287 - 1)
#define CIRCLE_HEIGTH	3

static struct color_bar {
	double amplitude, phase;
} color_bar[8] = {
	{0.0, 0.0},
	{0.336, 167.1},
	{0.474, 283.5},
	{0.443, 240.7},
	{0.443, 60.7},
	{0.474, 103.5},
	{0.336, 347.1},
	{0.0, 0.0},
};

static struct multi_burst {
	double width;		/* how whide is this portion */
	double level;		/* level of this portion */
	double frequency;	/* frequency of burst or zero */
	double V;		/* amplitude of V */
} multi_burst[10] = {
	{ 3.9, 1.0, 0.0, 0.0 },		/* white */
	{ 0.7, 0.5, 0.0, 0.0 },
	{ 5.0, 0.5, 1000000.0, 0.0 },	/* 1 MHz */
	{ 0.7, 0.5, 0.0, 0.0 },
	{ 5.0, 0.5, 2000000.0, 0.0 },	/* 2 MHz */
	{ 2.0, 0.5, 0.0, 0.0 },
	{ 5.0, 0.5, 3000000.0, 0.0 },	/* 3 MHz */
	{ 0.7, 0.5, 0.0, 0.0 },
	{ 7.9, 0.5, 0.0, 0.5 },		/* Color burst */
	{ 1.5, 0.5, 0.0, 0.0 },
	/* note that the color burst ist shifted left by 0.4uS when delaying Y.
	 * this makes color burst appear left of the center.
	 * we don't compensate this here, since i have seen this shift in real generators. */
};

/* create cosine ramp, so that the bandwidth is not higher than 2 * width of the ramp(0..1) */
static inline double ramp(double x)
{
	return 0.5 - 0.5 * cos(x * M_PI);
}

static double mittelfeld(sample_t *sample, double samplerate, int *_i, double *_x, double render_start, double step, sample_t *color_u, sample_t *color_v, int v_polarity, int line, const char *station_id)
{
	double render_end, x = *_x, vline_x = *_x, vline_start = render_start;
	int i = *_i, vline_i = *_i, b;
	double amplitude, phase, phase_step, Y, U, V, colorphase;
	int position = line / GRID_HEIGHT;
	int char_line;
	uint8_t bits;
	int bit, last_bit, c;

	switch (position) {
	case 0:
	case 1:
	case 2:
		/* color bars */
		for (b = 0; b < 8; b++) {
			Y = (1.0 - (double)b / 7.0) * 0.75;
			amplitude = color_bar[b].amplitude;
			if (v_polarity < 0)
				colorphase = (360.0 - color_bar[b].phase) / 180.0 * M_PI;
			else
				colorphase = color_bar[b].phase / 180.0 * M_PI;
			U = cos(colorphase) * amplitude / 2.0;
			V = sin(colorphase) * amplitude / 2.0;
			render_end = render_start + GRID_WIDTH * 1.5;
			while (x < render_end) {
				color_u[i] = U;
				color_v[i] = V;
				sample[i++] = Y;
				x += step;
			}
			render_start = render_end;
		}
		break;
	case 3:
	case 4:
		/* gray steps */
		for (b = 0; b < 5; b++) {
			Y = (double)b / 4.0;
			render_end = render_start + GRID_WIDTH * 2.4;
			while (x < render_end) {
				sample[i++] = Y;
				x += step;
			}
			render_start = render_end;
		}
		break;
	case 5:
		/* station ID */
		/* white bar before text */
		render_end = render_start + GRID_WIDTH * 2.4;
		while (x < render_end) {
			sample[i++] = 1.0;
			x += step;
		}
		render_start = render_end;
		/* black bar before text (exclude one ramp width) */
		render_end = render_start + GRID_WIDTH * 2.4 / 54.0 - RAMP_WIDTH / 2.0;
		while (x < render_end) {
			sample[i++] = 0.0;
			x += step;
		}
		render_start = render_end;
		last_bit = 0;
		/* text */
		/* 12 chars, plus one extra bit */
		for (c = 0; c < 13; c++) {
			char_line = ((line % GRID_HEIGHT) + 1) / 4 - 2;
			if (char_line < 0 || char_line > 7 || c == 12)
				bits = 0x00;
			else
				bits = get_font(station_id[c])[char_line];
			for (b = 0; b < 8; b++) {
				bit = ((bits << b) & 128);
				if (!last_bit && !bit) {
					/* keep black */
					render_end = render_start + TEXT_WIDTH;
					while (x < render_end) {
						sample[i++] = 0.0;
						x += step;
					}
					render_start = render_end;
				}
				if (last_bit && bit) {
					/* keep white */
					render_end = render_start + TEXT_WIDTH;
					while (x < render_end) {
						sample[i++] = 1.0;
						x += step;
					}
					render_start = render_end;
				}
				if (!last_bit && bit) {
					/* ramp to white */
					render_end = render_start + TEXT_WIDTH;
					while (x < render_end) {
						sample[i++] = 1.0 - ramp((render_end - x) / TEXT_WIDTH);
						x += step;
					}
					render_start = render_end;
				}
				if (last_bit && !bit) {
					/* ramp to black */
					render_end = render_start + TEXT_WIDTH;
					while (x < render_end) {
						sample[i++] = ramp((render_end - x) / TEXT_WIDTH);
						x += step;
					}
					render_start = render_end;
				}
				last_bit = bit;
				/* after extra bit after 12 chars, stop rendering */
				if (c == 12)
					break;
			}
		}
		/* black bar after text */
		render_end = render_start + GRID_WIDTH * 2.4 / 54.0 - RAMP_WIDTH / 2.0;
		while (x < render_end) {
			sample[i++] = 0.0;
			x += step;
		}
		render_start = render_end;
		/* white bar after text */
		render_end = render_start + GRID_WIDTH * 2.4;
		while (x < render_end) {
			sample[i++] = 1.0;
			x += step;
		}
		render_start = render_end;
		break;
	case 6:
		/* multi-burst */
		for (b = 0; b < 10; b++) {
			Y = multi_burst[b].level;
			V = multi_burst[b].V;
			phase_step = multi_burst[b].frequency * 2.0 * M_PI / samplerate;
			phase = 0.0;
			render_end = render_start + multi_burst[b].width / 1e6;
			while (x < render_end) {
				if (v_polarity < 0)
					colorphase = (360.0 - 145.9) / 180.0 * M_PI;
				else
					colorphase = 145.9 / 180.0 * M_PI;
				color_u[i] = cos(colorphase) * V / 2.0;
				color_v[i] = sin(colorphase) * V / 2.0;
				sample[i++] = Y + sin(phase) / 2.0;
				phase += phase_step;
				x += step;
			}
			render_start = render_end;
		}
		break;
	case 7:
		/* white bar with black pulse triangle */
		/* white bar */
		render_end = render_start + GRID_WIDTH * 6 - 0.0000005 - RAMP_WIDTH;
		while (x < render_end) {
			sample[i++] = 1.0;
			x += step;
		}
		/* ramp to triangle */
		render_end += RAMP_WIDTH;
		while (x < render_end) {
			sample[i++] = ramp((render_end - x) / RAMP_WIDTH);
			x += step;
		}
		/* triangle */
		render_end += 0.000001 * (1.0 - ((double)(line % GRID_HEIGHT) / (double)GRID_HEIGHT));
		while (x < render_end) {
			sample[i++] = 0.0;
			x += step;
		}
		/* ramp from triangle */
		render_end += RAMP_WIDTH;
		while (x < render_end) {
			sample[i++] = 1.0 - ramp((render_end - x) / RAMP_WIDTH);
			x += step;
		}
		/* white bar */
		render_end = render_start + GRID_WIDTH * 12;
		while (x < render_end) {
			sample[i++] = 1.0;
			x += step;
		}
		render_start = render_end;
		break;
	case 8:
		/* sawtooth +-V and uncolored field */
		render_end = render_start + GRID_WIDTH * 8;
		while (x < render_end) {
			Y = (render_end - x) / (render_end - render_start) / 2;
			if (Y > 0.375) Y = 0.375;
			color_v[i] = (double)v_polarity * Y;
			sample[i++] = Y;
			x += step;
		}
uncolored:
		/* uncolored +V field */
		render_end = render_start + GRID_WIDTH * 10;
		while (x < render_end) {
			color_v[i] = 0.375;
			sample[i++] = 0.375;
			x += step;
		}
		/* uncolored +-U field */
		render_end = render_start + GRID_WIDTH * 12;
		while (x < render_end) {
			color_u[i] = (double)v_polarity * 0.375;
			sample[i++] = 0.375;
			x += step;
		}
		render_start = render_end;
		break;
	case 9:
		/* sawtooth +U and uncolored field */
		render_end = render_start + GRID_WIDTH * 8;
		while (x < render_end) {
			Y = (render_end - x) / (render_end - render_start) / 2;
			if (Y > 0.375) Y = 0.375;
			color_u[i] = Y;
			sample[i++] = Y;
			x += step;
		}
		goto uncolored;
	}

	/* draw vertical line */
	if (position >= 3 && position <= 6) {
		render_end = vline_start + GRID_WIDTH * 6 - RAMP_WIDTH;
		while (vline_x < render_end) {
			vline_i++;
			vline_x += step;
		}
		render_end += RAMP_WIDTH;
		while (vline_x < render_end) {
			sample[vline_i] = ramp((render_end - vline_x) / RAMP_WIDTH) * (sample[vline_i] - GRID_LEVEL) + GRID_LEVEL;
			vline_i++;
			vline_x += step;
		}
		render_end += RAMP_WIDTH;
		while (vline_x < render_end) {
			sample[vline_i] = ramp((render_end - vline_x) / RAMP_WIDTH) * (GRID_LEVEL - sample[vline_i]) + sample[vline_i];
			vline_i++;
			vline_x += step;
		}
	}

	*_x = x;
	*_i = i;
	return render_start;
}

static void draw_line(sample_t *sample, sample_t *color_u, sample_t *color_v, int i, double step, double x, double line_end, double x_start, double x_stop)
{
	double render_start, render_end;

	/* go to ramp start */
	render_start = x_start - RAMP_WIDTH;
	while (x < render_start && x < line_end) {
		i++;
		x += step;
	}
	/* ramp up to line */
	render_end = render_start + RAMP_WIDTH;
	while (x < render_end && x < line_end) {
		color_u[i] = color_v[i] = 0;
		sample[i] = ramp((x - render_start) / RAMP_WIDTH) * (CIRCLE_LEVEL - sample[i]) + sample[i];
		i++;
		x += step;
	}
	/* draw line */
	render_start = x_stop;
	while (x < render_start && x < line_end) {
		color_u[i] = color_v[i] = 0;
		sample[i++] = CIRCLE_LEVEL;
		x += step;
	}
	/* ramp down from line */
	render_end = render_start + RAMP_WIDTH;
	while (x < render_end && x < line_end) {
		color_u[i] = color_v[i] = 0;
		sample[i] = ramp((x - render_start) / RAMP_WIDTH) * (sample[i] - CIRCLE_LEVEL) + CIRCLE_LEVEL;
		i++;
		x += step;
	}
}

static void kreislinie(sample_t *sample, sample_t *color_u, sample_t *color_v, int i, double step, double x, double line_end, int line, double circle_radius)
{
	double dist_center = abs(CIRCLE_CENTER - line), y, oc, ic;
	double center_x = (line_end - x) / 2.0 + x;

	/* no radius, no circle */
	if (circle_radius < 0.1)
		return;

	/* check if we are  above or below outer circle */
	y = dist_center / ((double)GRID_HEIGHT * circle_radius);
	if (y > 1.0)
		return;

	/* calc outer circle */
	oc = sqrt(2*(y+1) - (y+1)*(y+1)) * GRID_WIDTH * circle_radius + (double)(line & 1)/40000000.0;

	/* check if we are above or below inner circle */
	y = dist_center / ((double)GRID_HEIGHT * circle_radius - CIRCLE_HEIGTH);
	if (y > 1.0) {
		/* draw outer circle only */
		draw_line(sample, color_u, color_v, i, step, x, line_end, center_x - oc, center_x + oc);
		return;
	}

	/* calc inner circle */
	ic = sqrt(2*(y+1) - (y+1)*(y+1)) * GRID_WIDTH * circle_radius;

	/* draw both circles */
	draw_line(sample, color_u, color_v, i, step, x, line_end, center_x - oc, center_x - ic);
	draw_line(sample, color_u, color_v, i, step, x, line_end, center_x + ic, center_x + oc);
}

/* render test image line starting with x and end with LINE_LENGTH
 *
 * sample: pointer to samples, starting at x = 0
 * samplerate: is the sample rate in hertz
 * x: at what x to start rendering
 * line: line no. to render
 */
int fubk_gen_line(sample_t *sample, double x, double samplerate, sample_t *color_u, sample_t *color_v, int v_polarity, double line_start, double line_end, int line, double radius, int color_bar, int grid_only, const char *station_id)
{
	double initial_x;
	double step = 1.0 / samplerate;
	int i = 0, initial_i;
	double render_start, render_end, center_x;
	int grid_count = 0;
	int in_mittelfeld;

	/* skip x to line_start */
	while (x < line_start && x < line_end) {
		i++;
		x += step;
	}
	if (x >= line_end)
		return i;
	initial_i = i;
	initial_x = x;

	/* calculate phase for ramp start of center line */
	center_x = (line_end - line_start) / 2.0 + line_start;

	/* check if we are are rendering middle field and set line number */
	if (grid_only)
		in_mittelfeld = -1;
	else if (color_bar)
		in_mittelfeld = 0;
	else {
		in_mittelfeld = line - (CENTER_LINE - GRID_HEIGHT*5) -GRID_LINES/2;
		if (in_mittelfeld >= GRID_HEIGHT*10)
			in_mittelfeld = -1;
	}

	/* calculate position in grid:
	 * get the distance below the center line (line - CENTER_LINE)
	 * then be sure not to get negative value: add a multiple of the grid period
	 * then use modulo to get the distance below the grid line */
	if (((line - CENTER_LINE + GRID_HEIGHT*20) % GRID_HEIGHT) < GRID_LINES) {
		if (in_mittelfeld < 0)
			goto no_mittelfeld;
		/* special case where the center line is always drawn */
		if (in_mittelfeld >= 198 && in_mittelfeld <= 201)
			goto no_mittelfeld;
		/* grid line before middle field */
		render_end = center_x - GRID_WIDTH*6.0;
		while (x < render_end && x < line_end) {
			sample[i++] = GRID_LEVEL;
			x += step;
		}
		if (x >= line_end)
			return i;
		render_start = render_end;
		/* middle field */
		render_start = mittelfeld(sample, samplerate, &i, &x, render_start, step, color_u, color_v, v_polarity, in_mittelfeld, station_id);
no_mittelfeld:
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
			render_start = fmod(center_x - x - RAMP_WIDTH + GRID_WIDTH*20.0, GRID_WIDTH) + x;
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
			/* middle field */
			if (in_mittelfeld >= 0) {
				if (++grid_count == 4)
					render_start = mittelfeld(sample, samplerate, &i, &x, render_start, step, color_u, color_v, v_polarity, in_mittelfeld, station_id);
			}
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

	kreislinie(sample, color_u, color_v, initial_i, step, initial_x, line_end, line, radius);

	return i;
}
