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
//#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include "timer.h"

//#define DEBUG

static struct timer *timer_head = NULL;
static struct timer **timer_tail_p = &timer_head;

double get_time(void)
{
	static struct timespec tv;

	clock_gettime(CLOCK_REALTIME, &tv);

	return (double)tv.tv_sec + (double)tv.tv_nsec / 1000000000.0;
}

void timer_init(struct timer *timer, void (*fn)(void *data), void *priv)
{
	if (timer->linked) {
		fprintf(stderr, "Timer is already initialized, aborting!\n");
		abort();
	}

	timer->timeout = 0;
	timer->cb = fn;
	timer->data = priv;
	timer->next = NULL;
	*timer_tail_p = timer;
	timer_tail_p = &timer->next;
	timer->linked = 1;
#ifdef DEBUG
	fprintf(stderr, "%s: timer=%p linked.\n", __func__, timer);
#endif
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
#ifdef DEBUG
	fprintf(stderr, "%s: timer=%p unlinked.\n", __func__, timer);
#endif
}

void timer_start(struct timer *timer, double duration)
{
	if (!timer->linked) {
		fprintf(stderr, "Timer is not initialized, aborting!\n");
		abort();
	}

	timer->duration = duration;
	timer->timeout = get_time() + duration;
#ifdef DEBUG
	fprintf(stderr, "%s: timer=%p started %.3f seconds.\n", __func__, timer, duration);
#endif
}

void timer_stop(struct timer *timer)
{
	if (!timer->linked) {
		fprintf(stderr, "Timer is not initialized, aborting!\n");
		abort();
	}

	timer->timeout = 0;
#ifdef DEBUG
	fprintf(stderr, "%s: timer=%p stopped.\n", __func__, timer);
#endif
}

int timer_running(struct timer *timer)
{
	if (!timer->linked) {
		fprintf(stderr, "Timer is not initialized, aborting!\n");
		abort();
	}

	return (timer->timeout != 0);
}

double process_timer(void)
{
	struct timer *timer;
	double now, timeout = -1.0;

	now = get_time();

again:
	timer = timer_head;
	while (timer) {
		if (timer->linked && timer->timeout > 0) {
			/* timeout, handle it, set timeout to 0 */
			if (now >= timer->timeout) {
				timer->timeout = 0;
#ifdef DEBUG
				fprintf(stderr, "%s: timer=%p fired.\n", __func__, timer);
#endif
				if (!timer->cb)
					abort();
				timer->cb(timer->data);
				timeout = 0.0;
				goto again;
			}
			/* in the future, set timeout to future */
			if (timeout < 0.0 || (timer->timeout - now) < timeout)
				timeout = timer->timeout - now;
		}
		timer = timer->next;
	}

	return timeout;
}

void osmo_timer_schedule(struct osmo_timer_list *ti, time_t sec, suseconds_t usec)
{
	if (!ti->linked)
		timer_init(ti, ti->cb, ti->data);
	timer_start(ti, (double)sec + (double)usec / 1000000.0);
}

void osmo_timer_del(struct osmo_timer_list *ti)
{
	timer_exit(ti);
}

int osmo_timer_pending(struct osmo_timer_list *ti)
{
	if (!ti->linked)
		return 0;
	return (ti->timeout != 0);
}

