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
#include "debug.h"

const char *debug_level[] = {
	"debug  ",
	"info   ",
	"notice ",
	"error  ",
};

struct debug_cat {
	const char *name;
	const char *color;
} debug_cat[] = {
	{ "sender", "\033[1;33m" },
	{ "sound", "\033[0;35m" },
	{ "dsp", "\033[0;31m" },
	{ "anetz", "\033[1;34m" },
	{ "bnetz", "\033[1;34m" },
	{ "cnetz", "\033[1;34m" },
	{ "nmt", "\033[1;34m" },
	{ "frame", "\033[0;36m" },
	{ "call", "\033[1;37m" },
	{ "mncc", "\033[1;32m" },
	{ "database", "\033[0;33m" },
};

int debuglevel = DEBUG_INFO;

void _printdebug(const char *file, const char *function, int line, int cat, int level, const char *fmt, ...)
{
	char buffer[4096];
	const char *p;
	va_list args;

	if (debuglevel > level)
		return;

	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer) - 1, fmt, args);
	buffer[sizeof(buffer) - 1] = '\0';
	va_end(args);

	while ((p = strchr(file, '/')))
		file = p + 1;

//	printf("%s%s:%d %s() %s: %s\033[0;39m", debug_cat[cat].color, file, line, function, debug_level[level], buffer);
	printf("%s%s:%d %s: %s\033[0;39m", debug_cat[cat].color, file, line, debug_level[level], buffer);
	fflush(stdout);
}

