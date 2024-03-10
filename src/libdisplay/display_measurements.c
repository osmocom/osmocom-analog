/* display measurements functions
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
#include <stdlib.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <math.h>
#include <sys/param.h>
#include "../libsample/sample.h"
#include "../liblogging/logging.h"
#include "../libdisplay/display.h"

#define MAX_NAME_LEN	16
#define MAX_UNIT_LEN	16

static int has_init = 0;
static int measurements_on = 0;
double time_elapsed = 0.0;
static int lines_total = 0;
static char line[MAX_DISPLAY_WIDTH];
static char line_color[MAX_DISPLAY_WIDTH];

dispmeas_t *meas_head = NULL;

void display_measurements_init(dispmeas_t *disp, int __attribute__((unused)) samplerate, const char *kanal)
{
	dispmeas_t **disp_p;

	memset(disp, 0, sizeof(*disp));
	disp->kanal = kanal;
	has_init = 1;
	lines_total = 0;
	time_elapsed = 0.0;

	disp_p = &meas_head;
	while (*disp_p)
		disp_p = &((*disp_p)->next);
	*disp_p = disp;
}

void display_measurements_exit(dispmeas_t *disp)
{
	dispmeasparam_t *param = disp->param, *temp;

	while (param) {
		temp = param;
		param = param->next;
		free(temp);
	}
	disp->param = NULL;
	has_init = 0;
}

static int color;

static void display_line(int on, int w)
{
	int j;

	if (on) {
		for (j = 0; j < w; j++) {
			if (line_color[j] != color && line[j] != ' ') {
				color = line_color[j];
				printf("\033[%d;3%dm", color / 10, color % 10);
			}
			putchar(line[j]);
		}
	} else {
		for (j = 0; j < w; j++)
			putchar(' ');
	}
	putchar('\n');
	lines_total++;
}

static void print_measurements(int on)
{
	dispmeas_t *disp;
	dispmeasparam_t *param;
	int i, j;
	int width, h;
	char text[128];
	double value = 0.0, value2 = 0.0, hold, hold2;
	int bar_width, bar_left, bar_right, bar_hold, bar_mark;

	get_win_size(&width, &h);
	if (width > MAX_DISPLAY_WIDTH - 1)
		width = MAX_DISPLAY_WIDTH - 1;

	/* no display, if bar graph is less than one character */
	bar_width = width - MAX_NAME_LEN - MAX_UNIT_LEN;
	if (bar_width < 1)
		return;

	lines_total = 0;
	color = -1;
	lock_logging();
	enable_limit_scroll(false);
	printf("\0337\033[H");
	for (disp = meas_head; disp; disp = disp->next) {
		memset(line, ' ', width);
		memset(line_color, 7, width);
		sprintf(line, "(chan %s", disp->kanal);
		*strchr(line, '\0') = ')';
		display_line(on, width);
		for (param = disp->param; param; param = param->next) {
			memset(line, ' ', width);
			memset(line_color, 7, width);
			memset(line_color, 3, MAX_NAME_LEN); /* yellow */
			switch (param->type) {
			case DISPLAY_MEAS_LAST:
				value = param->value;
				param->value = -NAN;
				break;
			case DISPLAY_MEAS_PEAK:
				/* peak value */
				value = param->value;
				param->value = -NAN;
				param->value_count = 0;
				break;
			case DISPLAY_MEAS_PEAK2PEAK:
				/* peak to peak value */
				value = param->value;
				value2 = param->value2;
				param->value = -NAN;
				param->value2 = -NAN;
				param->value_count = 0;
				break;
			case DISPLAY_MEAS_AVG:
				/* average value */
				if (param->value_count)
					value = param->value / (double)param->value_count;
				else
					value = -NAN;
				param->value = 0.0;
				param->value_count = 0;
				break;
			}
			/* add current value to history */
			param->value_history[param->value_history_pos] = value;
			param->value2_history[param->value_history_pos] = value2;
			param->value_history_pos = param->value_history_pos % DISPLAY_PARAM_HISTORIES;
			/* calculate hold values */
			hold = -NAN;
			hold2 = -NAN;
			switch (param->type) {
			case DISPLAY_MEAS_LAST:
				/* if we have valid value, we update 'last' */
				if (!isnan(value)) {
					param->last = value;
					hold = value;
				} else
					hold = param->last;
				break;
			case DISPLAY_MEAS_PEAK:
				for (i = 0; i < DISPLAY_PARAM_HISTORIES; i++) {
					if (isnan(param->value_history[i]))
						continue;
					if (isnan(hold) || param->value_history[i] > hold)
						hold = param->value_history[i];
				}
				break;
			case DISPLAY_MEAS_PEAK2PEAK:
				for (i = 0; i < DISPLAY_PARAM_HISTORIES; i++) {
					if (isnan(param->value_history[i]))
						continue;
					if (isnan(hold) || param->value_history[i] < hold)
						hold = param->value_history[i];
					if (isnan(hold2) || param->value2_history[i] > hold2)
						hold2 = param->value2_history[i];
				}
				if (!isnan(hold))
					hold = hold2 - hold;
				if (!isnan(value))
					value = value2 - value;
				break;
			case DISPLAY_MEAS_AVG:
				for (i = 0, j = 0; i < DISPLAY_PARAM_HISTORIES; i++) {
					if (isnan(param->value_history[i]))
						continue;
					if (j == 0)
						hold = 0.0;
					hold += param->value_history[i];
					j++;
				}
				if (j)
					hold /= j;
				break;
			}
			/* "Deviation ::::::::::............   4.5 KHz" */
			memcpy(line, param->name, (strlen(param->name) < MAX_NAME_LEN) ? strlen(param->name) : MAX_NAME_LEN);
			if (isinf(value) || isnan(value)) {
				bar_left = -1;
				bar_right = -1;
			} else if (param->bar == DISPLAY_MEAS_CENTER) {
				if (value >= 0.0) {
					bar_left = (-param->min) / (param->max - param->min) * ((double)bar_width - 1.0);
					bar_right = (value - param->min) / (param->max - param->min) * ((double)bar_width - 1.0);
				} else {
					bar_left = (value - param->min) / (param->max - param->min) * ((double)bar_width - 1.0);
					bar_right = (-param->min) / (param->max - param->min) * ((double)bar_width - 1.0);
				}
			} else {
				bar_left = -1;
				bar_right = (value - param->min) / (param->max - param->min) * ((double)bar_width - 1.0);
			}
			if (isinf(hold) || isnan(hold))
				bar_hold = -1;
			else
				bar_hold = (hold - param->min) / (param->max - param->min) * ((double)bar_width - 1.0);
			if (isinf(param->mark))
				bar_mark = -1;
			else
				bar_mark = (param->mark - param->min) / (param->max - param->min) * ((double)bar_width - 1.0);
			for (i = 0; i < bar_width; i++) {
				line[i + MAX_NAME_LEN] = ':';
				if (i == bar_hold)
					line_color[i + MAX_NAME_LEN] = 13;
				else if (i == bar_mark)
					line_color[i + MAX_NAME_LEN] = 14;
				else if (i >= bar_left && i <= bar_right)
					line_color[i + MAX_NAME_LEN] = 2;
				else
					line_color[i + MAX_NAME_LEN] = 4;
			}
			sprintf(text, param->format, hold);
			if (isnan(hold))
				memset(line_color + width - MAX_UNIT_LEN, 4, MAX_UNIT_LEN); /* blue */
			else
				memset(line_color + width - MAX_UNIT_LEN, 3, MAX_UNIT_LEN); /* yellow */
			memcpy(line + width - MAX_UNIT_LEN + 1, text, MIN(strlen(text), MAX_UNIT_LEN - 1));
			display_line(on, width);
		}
	}
	/* reset color and position */
	printf("\033[0;39m\0338"); fflush(stdout);
	enable_limit_scroll(true);
	unlock_logging();
	/* Set new limit. */
	logging_limit_scroll_top(lines_total);
}

void display_measurements_on(int on)
{
	if (measurements_on)
		print_measurements(0);

	if (on < 0)
		measurements_on = 1 - measurements_on;
	else
		measurements_on = on;

	logging_limit_scroll_top(0);
}

/* add new parameter on startup to the list of measurements */
dispmeasparam_t *display_measurements_add(dispmeas_t *disp, char *name, char *format, enum display_measurements_type type, enum display_measurements_bar bar, double min, double max, double mark)
{
	dispmeasparam_t *param, **param_p = &disp->param;
	int i;

	if (!has_init) {
		fprintf(stderr, "Not initialized prior adding measurement, please fix!\n");
		abort();
	}

	while (*param_p)
		param_p = &((*param_p)->next);
	*param_p = calloc(sizeof(dispmeasparam_t), 1);
	if (!*param_p)
		return NULL;
	param = *param_p;
	strncpy(param->name, name, sizeof(param->name) - 1);
	strncpy(param->format, format, sizeof(param->format) - 1);
	param->type = type;
	param->bar = bar;
	param->min = min;
	param->max = max;
	param->mark = mark;
	param->value = -NAN;
	param->value2 = -NAN;
	param->last = -NAN;
	for (i = 0; i < DISPLAY_PARAM_HISTORIES; i++)
		param->value_history[i] = -NAN;
	param->value_count = 0;

	return param;
}

void display_measurements_update(dispmeasparam_t *param, double value, double value2)
{
	/* special case where we do not have an instance of the parameter */
	if (!param)
		return;

	if (!has_init) {
		fprintf(stderr, "Not initialized prior updating measurement value, please fix!\n");
		abort();
	}

	switch (param->type) {
	case DISPLAY_MEAS_LAST:
		param->value = value;
		break;
	case DISPLAY_MEAS_PEAK:
		if (isnan(param->value) || value > param->value)
			param->value = value;
		break;
	case DISPLAY_MEAS_PEAK2PEAK:
		if (param->value_count == 0 || value < param->value)
			param->value = value;
		if (param->value_count == 0 || value2 > param->value2)
			param->value2 = value2;
		param->value_count++;
		break;
	case DISPLAY_MEAS_AVG:
		param->value += value;
		param->value_count++;
		break;
	default:
		fprintf(stderr, "Parameter '%s' has unknown type %d, please fix!\n", param->name, param->type);
		abort();
	}
}

void display_measurements(double elapsed)
{
	if (!measurements_on)
		return;

	if (!has_init)
		return;

	/* count and check if we need to display this time */
	time_elapsed += elapsed;
	if (time_elapsed < DISPLAY_MEAS_INTERVAL)
		return;
	time_elapsed = fmod(time_elapsed, DISPLAY_MEAS_INTERVAL);

	print_measurements(1);
}

