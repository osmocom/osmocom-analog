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
#include <math.h>
#include <sys/ioctl.h>
#include "sample.h"
#include "sender.h"

#define DISPLAY_INTERVAL 0.04

#define HEIGHT	11

static int num_sender = 0;
static char screen[HEIGHT][MAX_DISPLAY_WIDTH];
static int wave_on = 0;

void get_win_size(int *w, int *h)
{
	struct winsize win;
	int rc;

	rc = ioctl(0, TIOCGWINSZ, &win);
	if (rc) {
		*w = 80;
		*h = 25;
		return;
	}

	*h = win.ws_row;
	*w = win.ws_col;
	if (*w > MAX_DISPLAY_WIDTH - 1)
		*w = MAX_DISPLAY_WIDTH - 1;
}

void display_wave_init(sender_t *sender, int samplerate)
{
	dispwav_t *disp = &sender->dispwav;

	memset(disp, 0, sizeof(*disp));
	disp->offset = (num_sender++) * HEIGHT;
	disp->interval_max = (double)samplerate * DISPLAY_INTERVAL + 0.5;
}

void display_wave_on(int on)
{
	int i, j;
	int w, h;

	get_win_size(&w, &h);

	if (wave_on) {
		memset(&screen, ' ', sizeof(screen));
		printf("\0337\033[H");
		for (i = 0; i < num_sender; i++) {
			for (j = 0; j < HEIGHT; j++) {
				screen[j][w] = '\0';
				puts(screen[j]);
			}
		}
		printf("\0338"); fflush(stdout);
	}

	if (on < 0)
		wave_on = 1 - wave_on;
	else
		wave_on = on;
}

void display_wave_limit_scroll(int on)
{
	int w, h;

	if (!wave_on)
		return;

	get_win_size(&w, &h);

	printf("\0337");
	printf("\033[%d;%dr", (on) ? num_sender * HEIGHT + 1 : 1, h);
	printf("\0338");
}

/*
 * draw wave form:
 *
 * theoretical example: HEIGHT = 3 allows 5 steps
 *
 * Line 0: -_
 * Line 1:   -_
 * Line 2:     -
 *
 * HEIGHT is odd, so the center line's char is '-' (otherwise '_')
 * (HEIGHT - 1) / 2 = 1, so the center line is drawn in line 1
 *
 * y is in range of 0..4, so these are 5 steps, where 2 is the
 * center line. this is calculated by (HEIGHT * 2 - 1)
 */
void display_wave(sender_t *sender, sample_t *samples, int length, double range)
{
	dispwav_t *disp = &sender->dispwav;
	int pos, max;
	sample_t *buffer;
	int i, j, k, y;
	int color = 9; /* default color */
	int center_line;
	char center_char;
	int width, h;

	if (!wave_on)
		return;

	get_win_size(&width, &h);

	/* at what line we draw our zero-line and what character we use */
	center_line = (HEIGHT - 1) >> 1;
	center_char = (HEIGHT & 1) ? '-' : '_';

	pos = disp->interval_pos;
	max = disp->interval_max;
	buffer = disp->buffer;

	for (i = 0; i < length; i++) {
		if (pos >= width) {
			if (++pos == max)
				pos = 0;
			continue;
		}
		buffer[pos++] = samples[i];
		if (pos == width) {
			memset(&screen, ' ', sizeof(screen));
			for (j = 0; j < width; j++) {
				/* Input value is scaled to range -1 .. 1 and then substracted from 1,
				 * so the result ranges from 0 .. 2.
				 * HEIGHT-1 is multiplied with the range, so a HEIGHT of 3 would allow
				 * 0..4 (5 steps) and a HEIGHT of 11 would allow 0..20 (21 steps).
				 * We always use odd number of steps, so there will be a center between
				 * values.
				 */
				y = round((1.0 - buffer[j] / range) * (double)(HEIGHT - 1));
				/* only display level, if it is in range */
				if (y >= 0 && y < HEIGHT * 2 - 1)
					screen[y >> 1][j] = (y & 1) ? '_' : '-';
			}
			sprintf(screen[0], "(chan %d", sender->kanal);
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
					} else if (screen[j][k] == '-' || screen[j][k] == '_') {
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
}


