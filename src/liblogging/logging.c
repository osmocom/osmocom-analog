/* Logging (on segmented part of the window)
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

#include <sys/ioctl.h>
#include <math.h>
#include <errno.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/application.h>
#include "logging.h"
#ifdef DLCC_DEFINED
#include <osmocom/cc/misc.h>
#endif

int loglevel = LOGL_INFO;

static int scroll_window_start = 0;
static int scroll_window_end = 0;
static int scroll_window_height = 0;

void lock_logging(void)
{
	log_tgt_mutex_lock();
}

void unlock_logging(void)
{
	log_tgt_mutex_unlock();
}

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

	if (h)
		*h = win.ws_row;
	if (w)
		*w = win.ws_col;
}

void enable_limit_scroll(bool enable)
{
	/* Before the window is set, keep scrolling everything. */
	if (scroll_window_height == 0)
		return;

	/* If window is too small. */
	if (scroll_window_end - scroll_window_start <= 0)
		return;

	if (enable) {
		printf("\0337\033[%d;%dr\0338", scroll_window_start, scroll_window_end);
	} else
		printf("\0337\033[%d;%dr\0338", 1, scroll_window_height);
	fflush(stdout);
}

void logging_limit_scroll_top(int lines)
{
	lock_logging();

	get_win_size(NULL, &scroll_window_height);
	scroll_window_start = lines + 1;
	if (scroll_window_end == 0)
		scroll_window_end = scroll_window_height;

	enable_limit_scroll(true);

	unlock_logging();
}

void logging_limit_scroll_bottom(int lines)
{
	int i;

	lock_logging();

	get_win_size(NULL, &scroll_window_height);
	scroll_window_end = scroll_window_height - lines;
	if (scroll_window_start == 0)
		scroll_window_start = 1;

	/* Make space by adding empty lines. */
	for (i = scroll_window_end; i < scroll_window_height; i++)
		printf("\n");
	/* Go up by number of lines to be in window. */
	printf("\033[%dA", scroll_window_height - scroll_window_end);
	/* Enable window. */
	enable_limit_scroll(true);

	unlock_logging();
}

const char *debug_amplitude(double level)
{
	static char text[42];

	strcpy(text, "                    :                    ");
	if (level > 1.0)
		level = 1.0;
	if (level < -1.0)
		level = -1.0;
	text[20 + (int)(level * 20)] = '*';

	return text;
}

#define level2db(level)         (20 * log10(level))

const char *debug_db(double level_db)
{
	static char text[128];
	int l;

	strcpy(text, ":  .  :  .  :  .  :  .  :  .  :  .  :  .  :  .  |  .  :  .  :  .  :  .  :  .  :  .  :  .  :  .  :");
	if (level_db <= 0.0)
		return text;
	l = (int)round(level2db(level_db));
	if (l > 48)
		return text;
	if (l < -48)
		return text;
	text[l + 48] = '*';

	return text;
}

void logging_print_help(void)
{
	printf(" -v --verbose <level> | <level>,<category>[,<category>[,...]] | list\n");
	printf("        Use 'list' to get a list of all levels and categories.\n");
	printf("        Verbose level: digit of debug level (default = '%d')\n", loglevel);
	printf("        Verbose level+category: level digit followed by one or more categories\n");
	printf("        -> If no category is specified, all categories are selected\n");
	printf(" -v --verbose date\n");
	printf("        Show date with debug output\n");
}

static unsigned char log_levels[] = { LOGL_DEBUG, LOGL_INFO, LOGL_NOTICE, LOGL_ERROR };
static char *log_level_names[] = { "debug", "info", "notice", "error" };

static void list_cat(void)
{
	int i;

	printf("Give number of debug level:\n");
	for (i = 0; i < (int)sizeof(log_levels); i++)
		printf(" %d = %s\n", log_levels[i], log_level_names[i]);
	printf("\n");

	printf("Give name(s) of debug category:\n");
	for (i = 0; i < (int)log_categories_size; i++) {
		if (!log_categories[i].name)
			continue;
		printf(" ");
		if (log_categories[i].color)
			printf("%s", log_categories[i].color);
		if (log_categories[i].name)
		printf("%s\033[0;39m = %s\n", log_categories[i].name, log_categories[i].description);
	}
	printf("\n");
}

int parse_logging_opt(const char *optarg)
{
	int i;
	char *dup, *dstring, *p;

	if (!strcasecmp(optarg, "list")) {
		list_cat();
		return 1;
	}

	if (!strcasecmp(optarg, "date")) {
		log_set_print_timestamp(osmo_stderr_target, 1);
		return 0;
	}

	dup = dstring = strdup(optarg);
	p = strsep(&dstring, ",");
	for (i = 0; i < p[i]; i++) {
		if (p[i] < '0' || p[i] > '9') {
			fprintf(stderr, "Only digits are allowed for debug level!\n");
			free(dup);
			return -EINVAL;
		}
	}
	loglevel = atoi(p);
	for (i = 0; i < (int)sizeof(log_levels); i++) {
		if (log_levels[i] == loglevel)
			break;
	}
	if (i == (int)sizeof(log_levels)) {
		fprintf(stderr, "Logging level does not exist, use '-v list' to show available levels!\n");
		free(dup);
		return -EINVAL;
	}
	/* Set loglevel and enable all categories, if dstring is not set. Else set loglevel and disable all categories. */
	for (i = 0; i < (int)log_categories_size; i++)
		log_set_category_filter(osmo_stderr_target, i, (!dstring), loglevel);
	/* Enable each given category. */
	while((p = strsep(&dstring, ","))) {
		for (i = 0; i < (int)log_categories_size; i++) {
			if (!log_category_name(i))
				continue;
			if (!strcasecmp(p, log_category_name(i)))
				break;
		}
		if (i == (int)log_categories_size) {
			fprintf(stderr, "Given logging category '%s' unknown, use '-v list' to show available categories!\n", p);
			free(dup);
			return -EINVAL;
		}
		log_set_category_filter(osmo_stderr_target, i, 1, loglevel);
	}

	free(dup);
	return 0;
}

/* Call after configuation above. */
void logging_init(void)
{
	int i;

	struct log_info log_info = {
		.cat = log_categories,
		.num_cat = log_categories_size,
	};

#ifdef DLCC_DEFINED
	osmo_cc_set_log_cat(DLCC);
#endif

	osmo_init_logging2(NULL, &log_info);
	log_set_print_timestamp(osmo_stderr_target, 0);
	log_set_print_level(osmo_stderr_target, 1);
	log_set_print_category_hex(osmo_stderr_target, 0);
	log_set_print_category(osmo_stderr_target, 1);

	/* Set loglevel and enable all categories. */
	for (i = 0; i < (int)log_categories_size; i++)
		log_set_category_filter(osmo_stderr_target, i, 1, loglevel);
}

