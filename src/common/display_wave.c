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
#include "sender.h"

#define DISPLAY_INTERVAL 0.04

#define WIDTH	80
#define HEIGHT	10

static int num_sender = 0;
static char screen[HEIGHT][WIDTH+1];
static int wave_on = 0;

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

	if (on < 0)
		wave_on = 1 - wave_on;
	else
		wave_on = on;

	memset(&screen, ' ', sizeof(screen));
	printf("\0337\033[H");
	for (i = 0; i < num_sender; i++) {
		for (j = 0; j < HEIGHT; j++) {
			screen[j][WIDTH] = '\0';
			puts(screen[j]);
		}
	}
	printf("\0338"); fflush(stdout);
}

void display_wave(sender_t *sender, int16_t *samples, int length)
{
	dispwav_t *disp = &sender->dispwav;
	int pos, max;
	int16_t *buffer;
	int i, j, y;

	if (!wave_on)
		return;

	pos = disp->interval_pos;
	max = disp->interval_max;
	buffer = disp->buffer;

	for (i = 0; i < length; i++) {
		if (pos >= WIDTH) {
			if (++pos == max)
				pos = 0;
			continue;
		}
		buffer[pos++] = samples[i];
		if (pos == WIDTH) {
			memset(&screen, ' ', sizeof(screen));
			for (j = 0; j < WIDTH; j++) {
				/* must divide by 65536, because we may never reach HEIGHT*2! */
				y = (32767 - (int)buffer[j]) * HEIGHT * 2 / 65536;
				screen[y >> 1][j] = (y & 1) ? '_' : '-';
			}
			sprintf(screen[0], "(chan %d", sender->kanal);
			*strchr(screen[0], '\0') = ')';
			printf("\0337\033[H");
			for (j = 0; j < disp->offset; j++)
				puts("");
			for (j = 0; j < HEIGHT; j++) {
				screen[j][WIDTH] = '\0';
				puts(screen[j]);
			}
			printf("\0338"); fflush(stdout);
		}
	}

	disp->interval_pos = pos;
}


