/* Timer handling
 *
 * (C) 2023 by Andreas Eversberg <jolly@eversberg.eu>
 * All Rights Reserved
 *
 * Inspired by libosmocore
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
#include <math.h>
#include <errno.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include "select.h"

//#define DEBUG

#define MAX_OFD	1024

struct osmo_fd *ofd_list = NULL;
int ofd_changed = 0;

int osmo_fd_register(struct osmo_fd *ofd)
{
	struct osmo_fd **ofdp;

	/* attach to list, if not already */
	ofdp = &ofd_list;
	while (*ofdp) {
		if (*ofdp == ofd)
			break;
		ofdp = &((*ofdp)->next);
	}
	if (!*ofdp) {
#ifdef DEBUG
		fprintf(stderr, "%s: ofd=%p fd=%d registers.\n", __func__, ofd, ofd->fd);
#endif
		ofd->next = NULL;
		*ofdp = ofd;
		ofd_changed = 1;
	}

	return 0;
}

void osmo_fd_unregister(struct osmo_fd *ofd)
{
	struct osmo_fd **ofdp;

	/* detach from list, if not already */
	ofdp = &ofd_list;
	while (*ofdp) {
		if (*ofdp == ofd)
			break;
		ofdp = &((*ofdp)->next);
	}
	if (*ofdp) {
#ifdef DEBUG
		fprintf(stderr, "%s: ofd=%p fd=%d unregisters.\n", __func__, ofd, ofd->fd);
#endif
		*ofdp = ofd->next;
		ofd->next = NULL;
		ofd_changed = 1;
	}
}

int osmo_fd_select(double timeout)
{
	fd_set readset;
	fd_set writeset;
	fd_set exceptset;
	struct osmo_fd *ofd;
	struct timeval tv;
	int max_fd;
	unsigned int what;
	int work = 0;
	int rc;

	/* init event sets */
        FD_ZERO(&readset);
        FD_ZERO(&writeset);
        FD_ZERO(&exceptset);

	/* populate event set with all file descriptios */
	ofd = ofd_list;
	max_fd = 0;
	while (ofd) {
		if (ofd->fd > max_fd)
			max_fd = ofd->fd;
		if (ofd->when & OSMO_FD_READ)
			FD_SET(ofd->fd, &readset);
		if (ofd->when & OSMO_FD_WRITE)
			FD_SET(ofd->fd, &writeset);
		if (ofd->when & OSMO_FD_EXCEPT)
			FD_SET(ofd->fd, &exceptset);
		ofd = ofd->next;
	}

	if (timeout >= 0) {
		/* prepare timeout */
		tv.tv_sec = floor(timeout);
		tv.tv_usec = (timeout - tv.tv_sec) * 1000000.0;
		/* wait for event or timeout */
		rc = select(max_fd + 1, &readset, &writeset, &exceptset, &tv);
	} else {
		/* wait for event */
		rc = select(max_fd + 1, &readset, &writeset, &exceptset, NULL);
	}
	if (rc < 0) {
		if (errno != EINTR)
			fprintf(stderr, "%s: select() failed: '%d' with errno %d (%s) Please fix!\n", __func__, rc, errno, strerror(errno));
		return 0;
	}

again:
	/* check the result and call handler */
	ofd_changed = 0;
	ofd = ofd_list;
	while (ofd) {
		what = 0;
		if (FD_ISSET(ofd->fd, &readset)) {
#ifdef DEBUG
			fprintf(stderr, "%s: ofd=%p fd=%d get READ event.\n", __func__, ofd, ofd->fd);
#endif
			what |= OSMO_FD_READ;
			FD_CLR(ofd->fd, &readset);
		}
		if (FD_ISSET(ofd->fd, &writeset)) {
#ifdef DEBUG
			fprintf(stderr, "%s: ofd=%p fd=%d get WRITE event.\n", __func__, ofd, ofd->fd);
#endif
			what |= OSMO_FD_WRITE;
			FD_CLR(ofd->fd, &writeset);
		}
		if (FD_ISSET(ofd->fd, &exceptset)) {
#ifdef DEBUG
			fprintf(stderr, "%s: ofd=%p fd=%d get EXCEPTION event.\n", __func__, ofd, ofd->fd);
#endif
			what |= OSMO_FD_EXCEPT;
			FD_CLR(ofd->fd, &exceptset);
		}
		if (what) {
			work = 1;
			ofd->cb(ofd, what);
			/* list has changed */
			if (ofd_changed)
				goto again;
		}
		ofd = ofd->next;
	}

	return work;
}

