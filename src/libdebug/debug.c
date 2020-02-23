/* Simple debug functions for level and category filtering
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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <math.h>
#include <sys/ioctl.h>
#include "../libsample/sample.h"
#include "debug.h"
#include "../libdisplay/display.h"

const char *debug_level[] = {
	"debug  ",
	"info   ",
	"notice ",
	"error  ",
	NULL,
};

struct debug_cat {
	const char *name;
	const char *color;
} debug_cat[] = {
	{ "options", "\033[0;33m" },
	{ "sender", "\033[1;33m" },
	{ "sound", "\033[0;35m" },
	{ "dsp", "\033[0;31m" },
	{ "anetz", "\033[1;34m" },
	{ "bnetz", "\033[1;34m" },
	{ "cnetz", "\033[1;34m" },
	{ "nmt", "\033[1;34m" },
	{ "amps", "\033[1;34m" },
	{ "r2000", "\033[1;34m" },
	{ "imts", "\033[1;34m" },
	{ "jollycom", "\033[1;34m" },
	{ "eurosignal", "\033[1;34m" },
	{ "frame", "\033[0;36m" },
	{ "call", "\033[0;37m" },
	{ "mncc", "\033[1;32m" },
	{ "database", "\033[0;33m" },
	{ "transaction", "\033[0;32m" },
	{ "dms", "\033[0;33m" },
	{ "sms", "\033[1;37m" },
	{ "sdr", "\033[1;31m" },
	{ "uhd", "\033[1;35m" },
	{ "soapy", "\033[1;35m" },
	{ "wave", "\033[1;33m" },
	{ "radio", "\033[1;34m" },
	{ "am791x", "\033[0;31m" },
	{ "uart", "\033[0;32m" },
	{ "device", "\033[0;33m" },
	{ "datenklo", "\033[1;34m" },
	{ "zeit", "\033[1;34m" },
	{ "sim layer 1", "\033[0;31m" },
	{ "sim layer 2", "\033[0;33m" },
	{ "sim ICL layer", "\033[0;36m" },
	{ "sim layer 7", "\033[0;37m" },
	{ "mtp layer 2", "\033[1;33m" },
	{ "mtp layer 3", "\033[1;36m" },
	{ "MuP", "\033[1;37m" },
	{ NULL, NULL }
};

int debuglevel = DEBUG_INFO;
uint64_t debug_mask = ~0;
extern int num_kanal;

void (*clear_console_text)(void) = NULL;
void (*print_console_text)(void) = NULL;

int debug_limit_scroll = 0;

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
}

void _printdebug(const char *file, const char __attribute__((unused)) *function, int line, int cat, int level, const char *kanal, const char *fmt, ...)
{
	char buffer[4096], *b = buffer;
	int s = sizeof(buffer) - 1;
	const char *p;
	va_list args;
	int w, h;

	if (debuglevel > level)
		return;

	buffer[sizeof(buffer) - 1] = '\0';

	/* if kanal is used, prefix the channel number */
	if (num_kanal > 1 && kanal) {
		sprintf(buffer, "(chan %s) ", kanal);
		b = strchr(buffer, '\0');
		s -= strlen(buffer);
	}

	if (!(debug_mask & ((uint64_t)1 << cat)))
		return;

	va_start(args, fmt);
	vsnprintf(b, s, fmt, args);
	va_end(args);

	while ((p = strchr(file, '/')))
		file = p + 1;
	if (clear_console_text)
		clear_console_text();
	if (debug_limit_scroll) {
		get_win_size(&w, &h);
		printf("\0337\033[%d;%dr\0338", debug_limit_scroll + 1, h);
	}
	printf("%s%s:%4d %s: %s\033[0;39m", debug_cat[cat].color, file, line, debug_level[level], buffer);
	if (debug_limit_scroll)
		printf("\0337\033[%d;%dr\0338", 1, h);
	if (print_console_text)
		print_console_text();
	fflush(stdout);
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

void debug_list_cat(void)
{
	int i;

	printf("Give number of debug level:\n");
	for (i = 0; debug_level[i]; i++)
		printf(" %d = %s\n", i, debug_level[i]);
	printf("\n");

	printf("Give name(s) of debug category:\n");
	for (i = 0; debug_cat[i].name; i++)
		printf(" %s%s\033[0;39m\n", debug_cat[i].color, debug_cat[i].name);
	printf("\n");
}

int parse_debug_opt(const char *optarg)
{
	int i, max_level = 0;
	char *dstring, *p;

	for (i = 0; debug_level[i]; i++)
		max_level = i;
	
	dstring = strdup(optarg);
	p = strsep(&dstring, ",");
	for (i = 0; i < p[i]; i++) {
		if (p[i] < '0' || p[i] > '9') {
			fprintf(stderr, "Only digits are allowed for debug level!\n");
			return -EINVAL;
		}
	}
	debuglevel = atoi(p);
	if (debuglevel > max_level) {
		fprintf(stderr, "Debug level too high, use 'list' to show available levels!\n");
		return -EINVAL;
	}
	if (dstring)
		debug_mask = 0;
	while((p = strsep(&dstring, ","))) {
		for (i = 0; debug_cat[i].name; i++) {
			if (!strcasecmp(p, debug_cat[i].name))
				break;
		}
		if (!debug_cat[i].name) {
			fprintf(stderr, "Given debug category '%s' unknown, use 'list' to show available categories!\n", p);
			return -EINVAL;
		}
		debug_mask |= ((uint64_t)1 << i);
	}

	return 0;
}

const char *debug_hex(const uint8_t *data, int len)
{
	static char *text = NULL;
	char *p;
	int i;

	if (text)
		free(text);
	p = text = calloc(1, len * 3 + 1);
	for (i = 0; i < len; i++) {
		sprintf(p, "%02x ", *data++);
		p += 3;
	}
	if (text[0])
		p[-1] = '\0';

	return text;
}

