/* display wave form functions
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
#include <pthread.h>
#include <math.h>
#include <sys/ioctl.h>
#include "../libsample/sample.h"
#include "../libdebug/debug.h"
#include "../libdisplay/display.h"

#define HEIGHT	11

static int num_sender = 0;
static char screen[HEIGHT][MAX_DISPLAY_WIDTH];
static int wave_on = 0;

void display_wave_init(dispwav_t *disp, int samplerate, const char *kanal)
{
	memset(disp, 0, sizeof(*disp));
	disp->offset = (num_sender++) * HEIGHT;
	disp->interval_max = (double)samplerate * DISPLAY_INTERVAL + 0.5;
	disp->kanal = kanal;
}

void display_wave_on(int on)
{
	int i, j;
	int w, h;

	get_win_size(&w, &h);
	if (w > MAX_DISPLAY_WIDTH - 1)
		w = MAX_DISPLAY_WIDTH - 1;

	if (wave_on) {
		memset(&screen, ' ', sizeof(screen));
		lock_debug();
		printf("\0337\033[H");
		for (i = 0; i < num_sender; i++) {
			for (j = 0; j < HEIGHT; j++) {
				screen[j][w] = '\0';
				puts(screen[j]);
			}
		}
		printf("\0338"); fflush(stdout);
		unlock_debug();
	}

	if (on < 0)
		wave_on = 1 - wave_on;
	else
		wave_on = on;

	if (wave_on)
		debug_limit_scroll = HEIGHT * num_sender;
	else
		debug_limit_scroll = 0;
}

/*
 * draw wave form:
 *
 * theoretical example: HEIGHT = 3 allows 5 steps
 *
 * Line 0: '.
 * Line 1:   '.
 * Line 2:     '
 *
 * HEIGHT is odd, so the center line's char is ''' (otherwise '.')
 * (HEIGHT - 1) / 2 = 1, so the center line is drawn in line 1
 *
 * y is in range of 0..4, so these are 5 steps, where 2 is the
 * center line. this is calculated by (HEIGHT * 2 - 1)
 */
void display_wave(dispwav_t *disp, sample_t *samples, int length, double range)
{
	int pos, max;
	sample_t *buffer;
	int i, j, k, s, e;
	double last, current, next;
	int color = 9; /* default color */
	int center_line;
	char center_char;
	int width, h;

	if (!wave_on)
		return;

	lock_debug();

	get_win_size(&width, &h);
	if (width > MAX_DISPLAY_WIDTH - 1)
		width = MAX_DISPLAY_WIDTH - 1;

	/* at what line we draw our zero-line and what character we use */
	center_line = (HEIGHT - 1) >> 1;
	center_char = (HEIGHT & 1) ? '\'' : '.';

	pos = disp->interval_pos;
	max = disp->interval_max;
	buffer = disp->buffer;

	for (i = 0; i < length; i++) {
		if (pos >= width + 2) {
			if (++pos == max)
				pos = 0;
			continue;
		}
		buffer[pos++] = samples[i];
		if (pos == width + 2) {
			memset(&screen, ' ', sizeof(screen));
			for (j = 0; j < width; j++) {
				/* Input value is scaled to range -1 .. 1 and then subtracted from 1,
				 * so the result ranges from 0 .. 2.
				 * HEIGHT-1 is multiplied with the range, so a HEIGHT of 3 would allow
				 * 0..4 (5 steps) and a HEIGHT of 11 would allow 0..20 (21 steps).
				 * We always use odd number of steps, so there will be a center between
				 * values.
				 */
				last = (1.0 - buffer[j] / range) * (double)(HEIGHT - 1);
				current = (1.0 - buffer[j + 1] / range) * (double)(HEIGHT - 1);
				next = (1.0 - buffer[j + 2] / range) * (double)(HEIGHT - 1);
				/* calculate start and end for vertical line
				 * if the current value is a peak (above or below last AND next point),
				 * round this peak point to become one end of the vertical line.
				 * the other end is rounded up or down, so the end of the line will
				 * not overlap with the ends of the surrounding lines.
				 */
				if (last > current) {
					if (next > current) {
						/* current point is a peak up */
						s = round(current);
						/* use lowest neighbor point and end is half way */
						if (last > next)
							e = floor((last + current) / 2.0);
						else
							e = floor((next + current) / 2.0);
						/* end point must not be above start point */
						if (e < s)
							e = s;
					} else {
						/* current point is a transition upwards */
						s = ceil((next + current) / 2.0);
						e = floor((last + current) / 2.0);
						/* end point must not be above start point */
						if (e < s)
							s = e = round(current);
					}
				} else {
					if (next <= current) {
						/* current point is a peak down */
						e = round(current);
						/* use heighes neighbor point and start is half way */
						if (last <= next)
							s = ceil((last + current) / 2.0);
						else
							s = ceil((next + current) / 2.0);
						/* start point must not be below end point */
						if (s > e)
							s = e;
					} else {
						/* current point is a transition downwards */
						s = ceil((last + current) / 2.0);
						e = floor((next + current) / 2.0);
						/* start point must not be below end point */
						if (s > e)
							s = e = round(current);
					}
				}
				/* only draw line, if it is in range */
				if (e >= 0 && s < HEIGHT * 2 - 1) {
					/* clip */
					if (s < 0)
						s = 0;
					if (e >= HEIGHT * 2 - 1)
						e = HEIGHT * 2 - 1;
					/* plot start and end point */
					if ((s & 1))
						screen[s >> 1][j] = '.';
					else if (e != s)
						screen[s >> 1][j] = '|';
					if (!(e & 1))
						screen[e >> 1][j] = '\'';
					else if (e != s)
						screen[e >> 1][j] = '|';
					/* plot line between start and end point */
					for (k = (s >> 1) + 1; k < (e >> 1); k++)
						screen[k][j] = '|';
				}
			}
			sprintf(screen[0], "(chan %s", disp->kanal);
			*strchr(screen[0], '\0') = ')';
			printf("\0337\033[H");
			for (j = 0; j < disp->offset; j++)
				puts("");
			for (j = 0; j < HEIGHT; j++) {
				for (k = 0; k < width; k++) {
					if (j == center_line && screen[j][k] == ' ') {
						/* blue 0-line */
						if (color != 4) {
							color = 4;
							printf("\033[0;34m");
						}
						putchar(center_char);
					} else if (screen[j][k] == '\'' || screen[j][k] == '.' || screen[j][k] == '|') {
						/* green scope curve */
						if (color != 2) {
							color = 2;
							printf("\033[1;32m");
						}
						putchar(screen[j][k]);
					} else if (screen[j][k] != ' ') {
						/* white other characters */
						if (color != 7) {
							color = 7;
							printf("\033[1;37m");
						}
						putchar(screen[j][k]);
					} else
						putchar(screen[j][k]);
				}
				printf("\n");
			}
			/* reset color and position */
			printf("\033[0;39m\0338"); fflush(stdout);
		}
	}

	disp->interval_pos = pos;

	unlock_debug();
}


