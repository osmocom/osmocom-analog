/* display IQ data form functions
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
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include "../libsample/sample.h"
#include "../liblogging/logging.h"
#include "../libdisplay/display.h"

/* must be odd value! */
#define SIZE	23

static char screen[SIZE][MAX_DISPLAY_WIDTH];
static uint8_t screen_color[SIZE][MAX_DISPLAY_WIDTH];
static uint8_t screen_history[SIZE * 2][MAX_DISPLAY_WIDTH];
static int iq_on = 0;
static double db = 80;

static dispiq_t disp;

void display_iq_init(int samplerate)
{
	memset(&disp, 0, sizeof(disp));
	memset(&screen_history, 0, sizeof(screen_history));
	disp.interval_max = (double)samplerate * DISPLAY_INTERVAL + 0.5;
	/* should not happen due to low interval */
	if (disp.interval_max < MAX_DISPLAY_IQ - 1)
		disp.interval_max = MAX_DISPLAY_IQ - 1;
}

void display_iq_on(int on)
{
	int j;
	int w, h;

	get_win_size(&w, &h);
	if (w > MAX_DISPLAY_WIDTH - 1)
		w = MAX_DISPLAY_WIDTH - 1;

	if (iq_on) {
		memset(&screen, ' ', sizeof(screen));
		memset(&screen_history, 0, sizeof(screen_history));
		lock_logging();
		enable_limit_scroll(false);
		printf("\0337\033[H");
		for (j = 0; j < SIZE; j++) {
			screen[j][w] = '\0';
			puts(screen[j]);
		}
		printf("\0338"); fflush(stdout);
		enable_limit_scroll(true);
		unlock_logging();
	}

	if (on < 0) {
		if (++iq_on == 3)
			iq_on = 0;
	} else
		iq_on = on;

	if (iq_on)
		logging_limit_scroll_top(SIZE);
	else
		logging_limit_scroll_top(0);
}

/*
 * plot IQ data:
 *
 * theoretical example: SIZE = 3 allows 6 steps plotted as dots
 *
 * Line 0:   :
 * Line 1:   :
 * Line 2:   :
 *
 * The level of -1.0 .. 1.0 is scaled to -3 and 3.
 *
 * The lowest of the upper 3 dots ranges from 0.0 .. <1.5.
 * The upper most dot ranges from 2.5 .. <3.5.
 * The highest of the lower 3 dots ranges from <0.0 .. >-1.5;
 * The lower most dot ranges from -2.5 .. >-3.5.
 *
 * The center column ranges from -0.5 .. <0.5.
 * The columns about the center from -1.5 .. <1.5.
 */
void display_iq(float *samples, int length)
{
	int pos, max;
	float *buffer;
	int i, j, k;
	int color = 9; /* default color */
	int x_center, y_center;
	double I, Q, L, l, s;
	int x, y;
	int v, r;
	int width, h;

	if (!iq_on)
		return;

	get_win_size(&width, &h);
	if (width > MAX_DISPLAY_WIDTH - 1)
		width = MAX_DISPLAY_WIDTH - 1;

	/* at what line we draw our zero-line and what character we use */
	x_center = width >> 1;
	y_center = (SIZE - 1) >> 1;

	pos = disp.interval_pos;
	max = disp.interval_max;
	buffer = disp.buffer;

	for (i = 0; i < length; i++) {
		if (pos >= MAX_DISPLAY_IQ) {
			if (++pos == max)
				pos = 0;
			continue;
		}
		buffer[pos * 2] = samples[i * 2];
		buffer[pos * 2 + 1] = samples[i * 2 + 1];
		pos++;
		if (pos == MAX_DISPLAY_IQ) {
			memset(&screen, ' ', sizeof(screen));
			memset(&screen_color, 7, sizeof(screen_color));
			/* render screen history to screen */
			for (y = 0; y < SIZE * 2; y++) {
				for (x = 0; x < width; x++) {
					v = screen_history[y][x];
					v -= 8;
					if (v < 0)
						v = 0;
					screen_history[y][x] = v;
					r = random() & 0x3f;
					if (r >= v)
						continue;
					if (screen[y/2][x] == ':')
						continue;
					if (screen[y/2][x] == '.') {
						if ((y & 1) == 0)
							screen[y/2][x] = ':';
						continue;
					}
					if (screen[y/2][x] == '\'') {
						if ((y & 1))
							screen[y/2][x] = ':';
						continue;
					}
					if ((y & 1) == 0)
						screen[y/2][x] = '\'';
					else
						screen[y/2][x] = '.';
					screen_color[y/2][x] = 4;
				}
			}
			/* plot current IQ date */
			for (j = 0; j < MAX_DISPLAY_IQ; j++) {
				I = buffer[j * 2];
				Q = buffer[j * 2 + 1];
				L = I*I + Q*Q;
				if (iq_on > 1) {
					/* logarithmic scale */
					l = sqrt(L);
					s = log10(l) * 20 + db;
					if (s < 0)
						s = 0;
					I = (I / l) * (s / db);
					Q = (Q / l) * (s / db);
				}
				x = x_center + (int)(I * (double)SIZE + (double)width + 0.5) - width;
				if (x < 0)
					continue;
				if (x > width - 1)
					continue;
				if (Q >= 0)
					y = SIZE - 1 - (int)(Q * (double)SIZE - 0.5);
				else
					y = SIZE - (int)(Q * (double)SIZE + 0.5);
				if (y < 0)
					continue;
				if (y > SIZE * 2 - 1)
					continue;
				if (screen[y/2][x] == ':' && screen_color[y/2][x] >= 10)
					goto cont;
				if (screen[y/2][x] == '.' && screen_color[y/2][x] >= 10) {
					if ((y & 1) == 0)
						screen[y/2][x] = ':';
					goto cont;
				}
				if (screen[y/2][x] == '\'' && screen_color[y/2][x] >= 10) {
					if ((y & 1))
						screen[y/2][x] = ':';
					goto cont;
				}
				if ((y & 1) == 0)
					screen[y/2][x] = '\'';
				else
					screen[y/2][x] = '.';
cont:
				screen_history[y][x] = 255;
				/* overdrive:
				 * red = close to -1..1 or above
				 * yellow = close to -0.5..0.5 or above
				 * Note: L is square of vector length,
				 * so we compare with square values.
				 */
				if (L > 0.9 * 0.9)
					screen_color[y/2][x] = 11;
				else if (L > 0.45 * 0.45 && screen_color[y/2][x] != 11)
					screen_color[y/2][x] = 13;
				else if (screen_color[y/2][x] < 10)
					screen_color[y/2][x] = 12;
			}
			if (iq_on == 1)
				sprintf(screen[0], "(IQ linear");
			else
				sprintf(screen[0], "(IQ log %.0f dB", db);
			*strchr(screen[0], '\0') = ')';
			lock_logging();
			enable_limit_scroll(false);
			printf("\0337\033[H");
			for (j = 0; j < SIZE; j++) {
				for (k = 0; k < width; k++) {
					if ((j == y_center || k == x_center) && screen[j][k] == ' ') {
						/* cross */
						if (color != 4) {
							color = 4;
							printf("\033[0;34m");
						}
						if (j == y_center) {
							if (k == x_center)
								putchar('o');
							else if (k == x_center - SIZE)
								putchar('+');
							else if (k == x_center + SIZE)
								putchar('+');
							else
								putchar('-');
						} else {
							if (j == 0 || j == SIZE - 1)
								putchar('+');
							else
								putchar('|');
						}
					} else {
						if (screen_color[j][k] != color) {
							color = screen_color[j][k];
							printf("\033[%d;3%dm", color / 10, color % 10);
						}
						putchar(screen[j][k]);
					}
				}
				printf("\n");
			}
			/* reset color and position */
			printf("\033[0;39m\0338"); fflush(stdout);
			enable_limit_scroll(true);
			unlock_logging();
		}
	}

	disp.interval_pos = pos;
}


