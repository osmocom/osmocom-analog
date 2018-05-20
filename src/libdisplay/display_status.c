/* display status functions
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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include "../libsample/sample.h"
#include "../libdebug/debug.h"
#include "../libdisplay/display.h"

static int status_on = 0;
static int line_count = 0;
static int lines_total = 0;
static char screen[MAX_HEIGHT_STATUS][MAX_DISPLAY_WIDTH];

static void print_status(int on)
{
	int i, j;
	int w, h;

	get_win_size(&w, &h);

	if (w > MAX_DISPLAY_WIDTH)
		w = MAX_DISPLAY_WIDTH;
	h--;
	if (h > lines_total)
		h = lines_total;

	printf("\0337\033[H\033[1;37m");
	for (i = 0; i < h; i++) {
		j = 0;
		if (on) {
			for (j = 0; j < w; j++)
				putchar(screen[i][j]);
		} else {
			for (j = 0; j < w; j++)
				putchar(' ');
		}
		putchar('\n');
	}
	printf("\0338"); fflush(stdout);
}

void display_status_on(int on)
{
	if (status_on)
		print_status(0);

	if (on < 0)
		status_on = 1 - status_on;
	else
		status_on = on;

	if (status_on)
		print_status(1);

	if (status_on)
		debug_limit_scroll = lines_total;
	else
		debug_limit_scroll = 0;
}

/* start status display */
void display_status_start(void)
{
	memset(screen, ' ', sizeof(screen));
	memset(screen[0], '-', sizeof(screen[0]));
	memcpy(screen[0] + 4, "Channel Status", 14);
	line_count = 1;
}

void display_status_channel(int channel, const char *type, const char *state)
{
	char line[MAX_DISPLAY_WIDTH];

	/* add empty line after previous channel+subscriber */
	if (line_count > 1 && line_count < MAX_HEIGHT_STATUS)
		line_count++;

	if (line_count == MAX_HEIGHT_STATUS)
		return;

	if (type)
		snprintf(line, sizeof(line), "Channel: %d Type: %s State: %s", channel, type, state);
	else
		snprintf(line, sizeof(line), "Channel: %d State: %s", channel, state);
	line[sizeof(line) - 1] = '\0';
	memcpy(screen[line_count++], line, strlen(line));
}

void display_status_subscriber(const char *number, const char *state)
{
	char line[MAX_DISPLAY_WIDTH];

	if (line_count == MAX_HEIGHT_STATUS)
		return;

	if (state)
		snprintf(line, sizeof(line), "  Subscriber: %s State: %s", number, state);
	else
		snprintf(line, sizeof(line), "  Subscriber: %s", number);
	line[sizeof(line) - 1] = '\0';
	memcpy(screen[line_count++], line, strlen(line));
}

void display_status_end(void)
{
	if (line_count < MAX_HEIGHT_STATUS) {
		memset(screen[line_count], '-', sizeof(screen[line_count]));
		line_count++;
	}
	/* if last total lines exceed current line count, keep it, so removed lines are overwritten with spaces */
	if (line_count > lines_total)
		lines_total = line_count;
	if (status_on)
		print_status(1);
	/* set new total lines */
	lines_total = line_count;
	if (status_on)
		debug_limit_scroll = lines_total;
}


