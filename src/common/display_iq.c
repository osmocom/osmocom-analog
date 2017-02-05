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
#include "sample.h"
#include "sender.h"

#define DISPLAY_INTERVAL 0.04

/* must be odd value! */
#define SIZE	23

static char screen[SIZE][MAX_DISPLAY_WIDTH];
static char overdrive[SIZE][MAX_DISPLAY_WIDTH];
static int iq_on = 0;
static double db = 80;

static dispiq_t disp;

void display_iq_init(int samplerate)
{
	memset(&disp, 0, sizeof(disp));
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

	if (iq_on) {
		memset(&screen, ' ', sizeof(screen));
		printf("\0337\033[H");
		for (j = 0; j < SIZE; j++) {
			screen[j][w] = '\0';
			puts(screen[j]);
		}
		printf("\0338"); fflush(stdout);
	}

	if (on < 0) {
		if (++iq_on == 3)
			iq_on = 0;
	} else
		iq_on = on;
}

void display_iq_limit_scroll(int on)
{
	int w, h;

	if (!iq_on)
		return;

	get_win_size(&w, &h);

	printf("\0337");
	printf("\033[%d;%dr", (on) ? SIZE + 1 : 1, h);
	printf("\0338");
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
	int width, h;

	if (!iq_on)
		return;

	get_win_size(&width, &h);

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
			memset(&overdrive, 0, sizeof(overdrive));
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
				if (L > 1.0)
					overdrive[y/2][x] = 1;
			}
			if (iq_on == 1)
				sprintf(screen[0], "(IQ linear");
			else
				sprintf(screen[0], "(IQ log %.0f dB", db);
			*strchr(screen[0], '\0') = ')';
			printf("\0337\033[H");
			for (j = 0; j < SIZE; j++) {
				for (k = 0; k < width; k++) {
					if ((j == y_center || k == x_center) && screen[j][k] == ' ') {
						/* blue cross */
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
						} else
							putchar('|');
					} else if (screen[j][k] == ':' || screen[j][k] == '.' || screen[j][k] == '\'') {
						/* red / green plot */
						if (overdrive[j][k]) {
							if (color != 1) {
								color = 1;
								printf("\033[1;31m");
							}
						} else {
							if (color != 2) {
								color = 2;
								printf("\033[1;32m");
							}
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

	disp.interval_pos = pos;
}


