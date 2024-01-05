/* display spectrum of IQ data
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
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../libsample/sample.h"
#include "../libfft/fft.h"
#include "../liblogging/logging.h"
#include "../libdisplay/display.h"

#define HEIGHT	20

static int has_init = 0;
static double buffer_delay[MAX_DISPLAY_SPECTRUM];
static double buffer_hold[MAX_DISPLAY_SPECTRUM];
static char screen[HEIGHT][MAX_DISPLAY_WIDTH];
static uint8_t screen_color[HEIGHT][MAX_DISPLAY_WIDTH];
static int spectrum_on = 0;
static double db = 120;
static double center_frequency, frequency_range;

static dispspectrum_t disp;

void display_spectrum_init(int samplerate, double _center_frequency)
{
	memset(&disp, 0, sizeof(disp));
	disp.interval_max = (double)samplerate * DISPLAY_INTERVAL + 0.5;
	/* should not happen due to low interval */
	if (disp.interval_max < MAX_DISPLAY_SPECTRUM - 1)
		disp.interval_max = MAX_DISPLAY_SPECTRUM - 1;
	memset(buffer_delay, 0, sizeof(buffer_delay));

	center_frequency = _center_frequency;
	frequency_range = (double)samplerate;

	has_init = 1;
}

void display_spectrum_add_mark(const char *kanal, double frequency)
{
	dispspectrum_mark_t *mark, **mark_p;

	if (!has_init)
		return;

	mark = calloc(1, sizeof(*mark));
	if (!mark) {
		fprintf(stderr, "no mem!");
		abort();
	}
	mark->kanal = kanal;
	mark->frequency = frequency;

	mark_p = &disp.mark;
	while (*mark_p)
		mark_p = &((*mark_p)->next);
	*mark_p = mark;
}

void display_spectrum_exit(void)
{
	dispspectrum_mark_t *mark = disp.mark, *temp;

	while (mark) {
		temp = mark;
		mark = mark->next;
		free(temp);
	}
	disp.mark = NULL;
	has_init = 0;
}

void display_spectrum_on(int on)
{
	int j;
	int w, h;

	get_win_size(&w, &h);
	if (w > MAX_DISPLAY_WIDTH - 1)
		w = MAX_DISPLAY_WIDTH - 1;

	if (spectrum_on) {
		memset(&screen, ' ', sizeof(screen));
		memset(&buffer_hold, 0, sizeof(buffer_hold));
		lock_logging();
		enable_limit_scroll(false);
		printf("\0337\033[H");
		for (j = 0; j < HEIGHT; j++) {
			screen[j][w] = '\0';
			puts(screen[j]);
		}
		printf("\0338"); fflush(stdout);
		enable_limit_scroll(true);
		unlock_logging();
	}

	if (on < 0) {
		if (++spectrum_on == 3)
			spectrum_on = 0;
	} else
		spectrum_on = on;

	if (spectrum_on)
		logging_limit_scroll_top(HEIGHT);
	else
		logging_limit_scroll_top(0);
}

/*
 * plot spectrum data:
 *
 */
void display_spectrum(float *samples, int length)
{
	dispspectrum_mark_t *mark;
	char print_channel[32], print_frequency[32];
	int width, h;
	int pos, max;
	double *buffer_I, *buffer_Q;
	int color = 9; /* default color */
	int i, j, k, o;
	double I, Q, v;
	int s, e, l, n;

	if (!spectrum_on)
		return;

	get_win_size(&width, &h);
	if (width > MAX_DISPLAY_WIDTH - 1)
		width = MAX_DISPLAY_WIDTH - 1;

	/* calculate size of FFT */
	int m, fft_size = 0, fft_taps = 0;
	for (m = 0; m < 16; m++) {
		if ((1 << m) > MAX_DISPLAY_SPECTRUM)
			break;
		if ((1 << m) <= width) {
			fft_taps = m;
			fft_size = 1 << m;
		}
	}
	if (m == 16) {
		fprintf(stderr, "Size of spectrum is not a power of 2, please fix!\n");
		abort();
	}

	int hold[fft_size], delay[fft_size], current[fft_size];

	pos = disp.interval_pos;
	max = disp.interval_max;
	buffer_I = disp.buffer_I;
	buffer_Q = disp.buffer_Q;

	for (i = 0; i < length; i++) {
		if (pos >= fft_size) {
			if (++pos == max)
				pos = 0;
			continue;
		}
		buffer_I[pos] = samples[i * 2];
		buffer_Q[pos] = samples[i * 2 + 1];
		pos++;
		if (pos == fft_size) {
			fft_process(1, fft_taps, buffer_I, buffer_Q);
			k = 0;
			for (j = 0; j < fft_size; j++) {
				/* scale result vertically */
				I = buffer_I[(j + fft_size / 2) % fft_size];
				Q = buffer_Q[(j + fft_size / 2) % fft_size];
				v = sqrt(I*I + Q*Q);
				v = log10(v) * 20 + db;
				if (v < 0)
					v = 0;
				v /= db;
				/* delayed */
				buffer_delay[j] -= DISPLAY_INTERVAL / 10.0;
				if (v > buffer_delay[j])
					buffer_delay[j] = v;
				delay[j] = (double)(HEIGHT * 2 - 1) * (1.0 - buffer_delay[j]);
				if (delay[j] < 0)
					delay[j] = 0;
				if (delay[j] >= (HEIGHT * 2))
					delay[j] = (HEIGHT * 2) - 1;
				/* hold */
				if (spectrum_on == 2) {
					if (v > buffer_hold[j])
						buffer_hold[j] = v;
					hold[j] = (double)(HEIGHT * 2 - 1) * (1.0 - buffer_hold[j]);
					if (hold[j] < 0)
						hold[j] = 0;
					if (hold[j] >= (HEIGHT * 2))
						hold[j] = (HEIGHT * 2) - 1;
				}
				/* current */
				current[j] = (double)(HEIGHT * 2 - 1) * (1.0 - v);
				if (current[j] < 0)
					current[j] = 0;
				if (current[j] >= (HEIGHT * 2))
					current[j] = (HEIGHT * 2) - 1;
			}
			/* plot scaled buffer */
			memset(&screen, ' ', sizeof(screen));
			memset(&screen_color, 7, sizeof(screen_color)); /* all white */
			sprintf(screen[0], "(spectrum log %.0f dB%s", db, (spectrum_on == 2) ? " HOLD" : "");
			*strchr(screen[0], '\0') = ')';
			for (j = 2; j < HEIGHT; j += 2) {
				memset(screen_color[j], 4, 7); /* blue */
				sprintf(screen[j], "%4.0f dB", -(double)(j+1) * db / (double)(HEIGHT - 1));
				screen[j][7] = ' ';
			}
			o = (width - fft_size) / 2; /* offset from left border */
			for (j = 0; j < fft_size; j++) {
				/* show current spectrum in yellow */
				s = l = n = current[j];
					/* get last and next value */
				if (j > 0)
					l = (current[j - 1] + s) / 2;
				if (j < fft_size - 1)
					n = (current[j + 1] + s) / 2;
				if (s > l && s > n) {
					/* current value is a minimum */
					e = s;
					s = (l < n) ? (l + 1) : (n + 1);
				} else if (s < l && s < n) {
					/* current value is a maximum */
					e = (l > n) ? l : n;
				} else if (l < n) {
					/* last value is higher, next value is lower */
					s = l + 1;
					e = n;
				} else if (l > n) {
					/* last value is lower, next value is higher */
					s = n + 1;
					e = l;
				} else {
					/* current, last and next values are equal */
					e = s;
				}
				if (s == e) {
					if ((s & 1) == 0)
						screen[s >> 1][j + o] = '\'';
					else
						screen[s >> 1][j + o] = '.';
					screen_color[s >> 1][j + o] = 13;
				} else {
					if ((s & 1) == 0)
						screen[s >> 1][j + o] = '|';
					else
						screen[s >> 1][j + o] = '.';
					screen_color[s >> 1][j + o] = 13;
					if ((e & 1) == 0)
						screen[e >> 1][j + o] = '\'';
					else
						screen[e >> 1][j + o] = '|';
					screen_color[e >> 1][j + o] = 13;
					for (k = (s >> 1) + 1; k < (e >> 1); k++) {
						screen[k][j + o] = '|';
						screen_color[k][j + o] = 13;
					}
				}
				/* show delayed spectrum in blue */
				e = s;
				s = delay[j];
				if ((s >> 1) < (e >> 1)) {
					if ((s & 1) == 0)
						screen[s >> 1][j + o] = '|';
					else
						screen[s >> 1][j + o] = '.';
					screen_color[s >> 1][j + o] = 4;
					for (k = (s >> 1) + 1; k < (e >> 1); k++) {
						screen[k][j + o] = '|';
						screen_color[k][j + o] = 4;
					}
				}
				if (spectrum_on == 2) {
					/* show hold spectrum in white */
					s = l = n = hold[j];
						/* get last and next value */
					if (j > 0)
						l = (hold[j - 1] + s) / 2;
					if (j < fft_size - 1)
						n = (hold[j + 1] + s) / 2;
					if (s > l && s > n) {
						/* hold value is a minimum */
						e = s;
						s = (l < n) ? (l + 1) : (n + 1);
					} else if (s < l && s < n) {
						/* hold value is a maximum */
						e = (l > n) ? l : n;
					} else if (l < n) {
						/* last value is higher, next value is lower */
						s = l + 1;
						e = n;
					} else if (l > n) {
						/* last value is lower, next value is higher */
						s = n + 1;
						e = l;
					} else {
						/* hold, last and next values are equal */
						e = s;
					}
					if (s == e) {
						if ((s & 1) == 0)
							screen[s >> 1][j + o] = '\'';
						else
							screen[s >> 1][j + o] = '.';
						screen_color[s >> 1][j + o] = 17;
					} else {
						if ((s & 1) == 0)
							screen[s >> 1][j + o] = '|';
						else
							screen[s >> 1][j + o] = '.';
						screen_color[s >> 1][j + o] = 17;
						if ((e & 1) == 0)
							screen[e >> 1][j + o] = '\'';
						else
							screen[e >> 1][j + o] = '|';
						screen_color[e >> 1][j + o] = 17;
						for (k = (s >> 1) + 1; k < (e >> 1); k++) {
							screen[k][j + o] = '|';
							screen_color[k][j + o] = 17;
						}
					}
				}
			}
			/* add channel positions in spectrum */
			for (mark = disp.mark; mark; mark = mark->next) {
				j = (int)((mark->frequency - center_frequency) / frequency_range * (double) fft_size + width / 2 + 0.5);
				if (j < 0 || j >= width) /* check out-of-range, should not happen */
					continue;
				for (k = 0; k < HEIGHT; k++) {
					/* skip yellow/white graph */
					if (screen_color[k][j] == 13 || screen_color[k][j] == 17)
						continue;
					screen[k][j] = ':';
					screen_color[k][j] = 12;
				}
				sprintf(print_channel, "Ch%s", mark->kanal);
				for (o = 0; o < (int)strlen(print_channel); o++) {
					s = j - strlen(print_channel) + o;
					if (s >= 0 && s < width) {
						screen[HEIGHT - 1][s] = print_channel[o];
						screen_color[HEIGHT - 1][s] = 7;
					}
				}
				if (fmod(mark->frequency, 1000.0))
					sprintf(print_frequency, "%.4f", mark->frequency / 1e6);
				else
					sprintf(print_frequency, "%.3f", mark->frequency / 1e6);
				for (o = 0; o < (int)strlen(print_frequency); o++) {
					s = j + o + 1;
					if (s >= 0 && s < width) {
						screen[HEIGHT - 1][s] = print_frequency[o];
						screen_color[HEIGHT - 1][s] = 7;
					}
				}
			}
			/* add center (DC line) to spectrum */
			j = width / 2 + 0.5;
			if (j < 1 || j >= width-1) /* check out-of-range, should not happen */
				continue;
			for (k = 0; k < HEIGHT; k++) {
				/* skip green/yellow/white graph */
				if (screen_color[k][j] == 13 || screen_color[k][j] == 17 || screen_color[k][j] == 12)
					continue;
				screen[k][j] = '.';
				screen_color[k][j] = 7;
			}
			screen[0][j-1] = 'D';
			screen[0][j+1] = 'C';
			screen_color[0][j-1] = 7;
			screen_color[0][j+1] = 7;
			/* display buffer */
			lock_logging();
			enable_limit_scroll(false);
			printf("\0337\033[H");
			for (j = 0; j < HEIGHT; j++) {
				for (k = 0; k < width; k++) {
					if (screen_color[j][k] != color) {
						color = screen_color[j][k];
						printf("\033[%d;3%dm", color / 10, color % 10);
					}
					putchar(screen[j][k]);
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

