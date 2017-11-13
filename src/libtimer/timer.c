/* Timer handling
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
#include <sys/time.h>
#include "timer.h"

static struct timer *timer_head = NULL;
static struct timer **timer_tail_p = &timer_head;

double get_time(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

void timer_init(struct timer *timer, void (*fn)(struct timer *timer), void *priv)
{
	if (timer->linked) {
		fprintf(stderr, "Timer is already initialized, aborting!\n");
		abort();
	}

	timer->timeout = 0;
	timer->fn = fn;
	timer->priv = priv;
	timer->next = NULL;
	*timer_tail_p = timer;
	timer_tail_p = &timer->next;
	timer->linked = 1;
}

void timer_exit(struct timer *timer)
{
	timer_tail_p = &timer_head;
	while (*timer_tail_p) {
		if (timer == *timer_tail_p)
			*timer_tail_p = (*timer_tail_p)->next;
		else
			timer_tail_p = &((*timer_tail_p)->next);
	}
	timer->linked = 0;
}

void timer_start(struct timer *timer, double duration)
{
	struct timeval tv;

	if (!timer->linked) {
		fprintf(stderr, "Timer is not initialized, aborting!\n");
		abort();
	}

	gettimeofday(&tv, NULL);

	timer->duration = duration;
	timer->timeout = get_time() + duration;
}

void timer_stop(struct timer *timer)
{
	if (!timer->linked) {
		fprintf(stderr, "Timer is not initialized, aborting!\n");
		abort();
	}

	timer->timeout = 0;
}

int timer_running(struct timer *timer)
{
	if (!timer->linked) {
		fprintf(stderr, "Timer is not initialized, aborting!\n");
		abort();
	}

	return (timer->timeout != 0);
}

void process_timer(void)
{
	struct timer *timer;
	double now;

	now = get_time();

again:
	timer = timer_head;

	while (timer) {
		if (timer->linked && timer->timeout > 0 && now >= timer->timeout) {
			timer->timeout = 0;
			timer->fn(timer);
			goto again;
		}
		timer = timer->next;
	}
}

