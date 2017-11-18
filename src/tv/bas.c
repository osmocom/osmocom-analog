/* generate a BAS signal
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

#include <string.h>
#include <stdint.h>
#include <math.h>
#include "../libsample/sample.h"
#include "../libfilter/iir_filter.h"
#include "bas.h"
#include "vcr.h"
#include "fubk.h"
#include "image.h"

#define WHITE_LEVEL	1.0
#define BLACK_LEVEL	0.32
#define	PORCH_LEVEL	0.3
#define SYNC_LEVEL	0.0

#define H_SYNC_START	0.0000015
#define H_SYNC_STOP	0.0000062
#define	H_LINE_START	0.000012
#define H_LINE_END	0.000064
#define H_SYNC2_START	(H_SYNC_START + H_LINE_END/2.0)
#define H_SYNC2_STOP	(H_SYNC_STOP + H_LINE_END/2.0)
#define V_SYNC_STOP	(H_SYNC2_START - (H_SYNC_STOP - H_SYNC_START))
#define V_SYNC2_STOP	(H_SYNC_START - (H_SYNC_STOP - H_SYNC_START) + H_LINE_END) // wraps, so we substract H_LINE_END
#define SYNC_RAMP	0.0000003
#define IMAGE_RAMP	0.0000002
#define H_CBURST_START	0.0000068
#define H_CBURST_STOP	0.0000094
#define COLOR_CARRIER	4433618.75
#define COLOR_OFFSET	0.0000004
#define BURST_AMPLITUDE	0.15
#define COLOR_FILTER_ITER 1

void bas_init(bas_t *bas, double samplerate, enum bas_type type, int fbas, double circle_radius, int color_bar, int grid_only, const char *station_id, unsigned short *img, int width, int height)
{
	memset(bas, 0, sizeof(*bas));
	bas->samplerate = samplerate;
	bas->type = type;
	bas->fbas = fbas;
	bas->v_polarity = 1;
	bas->circle_radius = circle_radius;
	bas->color_bar = color_bar;
	bas->grid_only = grid_only;
	bas->station_id = station_id;
	bas->img = img;
	bas->img_width = width;
	bas->img_height = height;

	/* filter color signal */
	iir_lowpass_init(&bas->lp_u, 1300000.0, samplerate, COLOR_FILTER_ITER);
	iir_lowpass_init(&bas->lp_v, 1300000.0, samplerate, COLOR_FILTER_ITER);
	/* filter final FBAS, so we prevent from beeing in the audio carrier spectrum */
	iir_lowpass_init(&bas->lp_y, 4500000.0, samplerate, COLOR_FILTER_ITER);
}

static inline double ramp(double x)
{
	return 0.5 - 0.5 * cos(x * M_PI);
}

int bas_generate(bas_t *bas, sample_t *sample)
{
	double step = 1.0 / bas->samplerate;
	int total_i = 0, i, c, line, middlefield_line;
	double x = 0, render_start, render_end;
	int have_image;
	sample_t color_u[(int)(bas->samplerate / 15625.0) + 10];
	sample_t color_v[(int)(bas->samplerate / 15625.0) + 10];
	double _sin, _cos;
	double color_step = COLOR_CARRIER / bas->samplerate * 2 * M_PI;
	/* the offset is specified by delaying Y signal by 0.4 uS. */
// additianlly we compensate the delay caused by the color filter, that is 2 samples per iteration */
	int color_offset = (int)(bas->samplerate * COLOR_OFFSET); // + 2 * COLOR_FILTER_ITER;

	for (line = 0; line < 625; line++) {
		/* reset color */
		memset(color_u, 0, sizeof(color_u));
		memset(color_v, 0, sizeof(color_v));

		/* render image interlaced */
		have_image = 1;
/* switch off to have black image */
#if 1
		if (line >= 24-1 && line <= 310-1)
			middlefield_line = (line - (24-1)) * 2 + 1;
		else if (line >= 336-1 && line <= 622-1)
			middlefield_line = (line - (336-1)) * 2;
		else
			have_image = 0;
		if (have_image) {
			switch (bas->type) {
			case BAS_FUBK:
				/* render FUBK test image */
				fubk_gen_line(sample, x, bas->samplerate, color_u, color_v, bas->v_polarity, H_LINE_START, H_LINE_END, middlefield_line, bas->circle_radius, bas->color_bar, bas->grid_only, bas->station_id);
				break;
			case BAS_IMAGE: {
				/* 574 lines of image are to be rendered */
				int img_line = middlefield_line - (574 - bas->img_height) / 2;
				if (img_line >= 0 && img_line < bas->img_height) {
					/* render image data */
					image_gen_line(sample, x, bas->samplerate, color_u, color_v, bas->v_polarity, H_LINE_START, H_LINE_END, bas->img + bas->img_width * img_line * 3, bas->img_width);
				}
			    }
			    	break;
			case BAS_VCR:
				/* render VCR test image */
				vcr_gen_line(sample, x, bas->samplerate, color_u, color_v, bas->v_polarity, H_LINE_START, H_LINE_END, middlefield_line / 2);
				break;
			}
		}
#endif

		i = 0;

		/* porch before sync */
		render_start = H_SYNC_START - SYNC_RAMP / 2;
		while (x < render_start) {
			sample[i++] = PORCH_LEVEL;
			x += step;
		}
		/* ramp to sync level */
		render_end = render_start + SYNC_RAMP;
		while (x < render_end) {
			sample[i++] = ramp((x - render_start) / SYNC_RAMP) * (SYNC_LEVEL - PORCH_LEVEL) + PORCH_LEVEL;
			x += step;
		}
		/* sync (long sync for vertical blank) */
		if (line <= 3-1 || line == 314-1 || line == 315-1)
			render_start = V_SYNC_STOP - SYNC_RAMP / 2;
		else
			render_start = H_SYNC_STOP - SYNC_RAMP / 2;
		while (x < render_start) {
			sample[i++] = SYNC_LEVEL;
			x += step;
		}
		/* ramp to porch level */
		render_end = render_start + SYNC_RAMP;
		while (x < render_end) {
			sample[i++] = ramp((x - render_start) / SYNC_RAMP) * (PORCH_LEVEL - SYNC_LEVEL) + SYNC_LEVEL;
			x += step;
		}
		if (have_image) {
			/* porch after sync, before color burst */
			render_start = H_CBURST_START;
			while (x < render_start) {
				sample[i++] = PORCH_LEVEL;
				x += step;
			}
			/* porch after sync, color burst */
			render_start = H_CBURST_STOP;
			while (x < render_start) {
				/* shift color burst to the right, it is shifted back when modulating */
				color_u[i+color_offset] = -0.5 * BURST_AMPLITUDE; /* - 180 degrees */
				color_v[i+color_offset] = 0.5 * BURST_AMPLITUDE * (double)bas->v_polarity; /* +- 90 degrees */
				sample[i++] = PORCH_LEVEL;
				x += step;
			}
			/* porch after sync, after color burst */
			render_start = H_LINE_START;
			while (x < render_start) {
				sample[i++] = PORCH_LEVEL;
				x += step;
			}
			/* ramp to image */
			render_end = render_start + IMAGE_RAMP;
			while (x < render_end) {
				/* scale level of image to range of BAS signal */
				sample[i] = sample[i] * (WHITE_LEVEL - BLACK_LEVEL) + BLACK_LEVEL;
				/* ramp from porch level to image level */
				sample[i] = ramp((x - render_start) / IMAGE_RAMP) * (sample[i] - PORCH_LEVEL) + PORCH_LEVEL;
				i++;
				x += step;
			}
			/* image */
			render_start = H_LINE_END - IMAGE_RAMP;
			while (x < render_start) {
				/* scale level of image to range of BAS signal */
				sample[i] = sample[i] * (WHITE_LEVEL - BLACK_LEVEL) + BLACK_LEVEL;
				i++;
				x += step;
			}
			/* ramp to porch level */
			render_end = H_LINE_END;
			while (x < render_end) {
				/* scale level of image to range of BAS signal */
				sample[i] = sample[i] * (WHITE_LEVEL - BLACK_LEVEL) + BLACK_LEVEL;
				/* ramp from image level to porch level */
				sample[i] = ramp((x - render_start) / IMAGE_RAMP) * (PORCH_LEVEL - sample[i]) + sample[i];
				i++;
				x += step;
			}
		} else {
			/* draw porch to second sync */
			if (line <= 5-1 || (line >= 311-1 && line <= 317-1) || line >= 623-1) {
				/* porch before sync */
				render_start = H_SYNC2_START - SYNC_RAMP / 2;
				while (x < render_start) {
					sample[i++] = PORCH_LEVEL;
					x += step;
				}
				/* ramp to sync level */
				render_end = render_start + SYNC_RAMP;
				while (x < render_end) {
					sample[i++] = ramp((x - render_start) / SYNC_RAMP) * (SYNC_LEVEL - PORCH_LEVEL) + PORCH_LEVEL;
					x += step;
				}
				/* sync (long sync for vertical blank) */
				if (line <= 2-1 || line == 313-1 || line == 314-1 || line == 315-1)
					render_start = V_SYNC2_STOP - SYNC_RAMP / 2;
				else
					render_start = H_SYNC2_STOP - SYNC_RAMP / 2;
				while (x < render_start) {
					sample[i++] = SYNC_LEVEL;
					x += step;
				}
				/* ramp to porch level */
				render_end = render_start + SYNC_RAMP;
				while (x < render_end) {
					sample[i++] = ramp((x - render_start) / SYNC_RAMP) * (PORCH_LEVEL - SYNC_LEVEL) + SYNC_LEVEL;
					x += step;
				}
			}
			/* porch to end of line */
			render_end = H_LINE_END;
			while (x < render_end) {
				sample[i++] = PORCH_LEVEL;
				x += step;
			}
		}

		if (bas->fbas) {
			/* filter color carrier */
			iir_process(&bas->lp_u, color_u, i);
			iir_process(&bas->lp_v, color_v, i);

			/* modulate color to sample */
			bas->color_phase = fmod(bas->color_phase + color_step * (double)color_offset, 2.0 * M_PI);
			for (c = color_offset; c < i; c++) {
				bas->color_phase += color_step;
				if (bas->color_phase >= 2.0 * M_PI)
					bas->color_phase -= 2.0 * M_PI;
				_sin = sin(bas->color_phase);
				_cos = cos(bas->color_phase);
				sample[c-color_offset] += color_u[c] * _cos - color_v[c] * _sin;
				sample[c-color_offset] += color_u[c] * _sin + color_v[c] * _cos;
	//			puts(debug_amplitude(sample[c-color_offset]));
			}

			/* filter bas signal */
			iir_process(&bas->lp_y, sample, i);
		}

		/* flip polarity of V signal */
		bas->v_polarity = -bas->v_polarity;

		/* increment sample buffer to next line */
		sample += i;
		/* return x */
		x -= H_LINE_END;
		/* sum total i */
		total_i += i;
	}

	return total_i;
}

