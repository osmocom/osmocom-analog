/* Mobie Network Call Control (MNCC) socket handling
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
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stddef.h>
#include <unistd.h>
#include "../common/debug.h"
#include "call.h"
#include "mncc_sock.h"

static int listen_sock = -1;
static int mncc_sock = -1;

/* write to mncc socket, return error or -EIO if no socket connection */
int mncc_write(uint8_t *buf, int length)
{
	int rc;

	if (mncc_sock <= 0) {
		PDEBUG(DMNCC, DEBUG_NOTICE, "MNCC not connected.\n");
		return -EIO;
	}
	rc = send(mncc_sock, buf, length, 0);
	if (rc < 0) {
		PDEBUG(DMNCC, DEBUG_ERROR, "MNCC connection failed (errno = %d).\n", errno);
		mncc_sock_close();
		return 0;
	}
	if (rc != length) {
		PDEBUG(DMNCC, DEBUG_NOTICE, "MNCC write failed.\n");
		mncc_sock_close();
		return 0;
	}

	return rc;
}


/* read from mncc socket */
static int mncc_read(void)
{
	uint8_t buf[sizeof(struct gsm_mncc)+1024];
	int rc;

	memset(buf, 0, sizeof(buf));
	rc = recv(mncc_sock, buf, sizeof(buf), 0);
	if (rc == 0) {
		PDEBUG(DMNCC, DEBUG_NOTICE, "MNCC connection closed.\n");
		mncc_sock_close();
		return 0;
	}
	if (rc < 0) {
		if (errno == EWOULDBLOCK)
			return -errno;
		PDEBUG(DMNCC, DEBUG_ERROR, "MNCC connection failed (errno = %d).\n", errno);
		mncc_sock_close();
		return -errno;
	}

	call_mncc_recv(buf, rc);

	return rc;
}

static void mncc_hello(void)
{
	struct gsm_mncc_hello hello;

	memset(&hello, 0, sizeof(hello));
	hello.msg_type = MNCC_SOCKET_HELLO;
	hello.version = MNCC_SOCK_VERSION;
	hello.mncc_size = sizeof(struct gsm_mncc);
	hello.data_frame_size = sizeof(struct gsm_data_frame);
	hello.called_offset = offsetof(struct gsm_mncc, called);
	hello.signal_offset = offsetof(struct gsm_mncc, signal);
	hello.emergency_offset = offsetof(struct gsm_mncc, emergency);
	hello.lchan_type_offset = offsetof(struct gsm_mncc, lchan_type);

	mncc_write((uint8_t *) &hello, sizeof(hello));
}


static int mncc_accept(void)
{
	struct sockaddr_un __attribute__((__unused__)) un_addr;
	socklen_t __attribute__((__unused__)) len;
	int flags;
	int rc;

	len = sizeof(un_addr);
	rc = accept(listen_sock, (struct sockaddr *) &un_addr, &len);
	if (rc < 0) {
		if (errno == EWOULDBLOCK)
			return 0;
		PDEBUG(DMNCC, DEBUG_ERROR, "Failed to accept incoming connection (errno=%d).\n", errno);
		return rc;
	}

	if (mncc_sock > 0) {
		PDEBUG(DMNCC, DEBUG_NOTICE, "Rejecting multiple incoming connections.\n");
		close(rc);
		return -EIO;
	}

	mncc_sock = rc;
	flags = fcntl(mncc_sock, F_GETFL, 0);
	rc = fcntl(mncc_sock, F_SETFL, flags | O_NONBLOCK);
	if (rc < 0) {
		PDEBUG(DMNCC, DEBUG_ERROR, "Failed to set socket into non-blocking IO mode.\n");
		mncc_sock_close();
		return rc;
	}

	PDEBUG(DMNCC, DEBUG_NOTICE, "MNCC socket connected.\n");

	mncc_hello();

	return 1;
}

void mncc_handle(void)
{
	mncc_accept();

	if (mncc_sock > 0) {
		while ((mncc_read()) > 0)
			;
	}
}


void mncc_sock_close(void)
{
	if (mncc_sock > 0) {
		PDEBUG(DMNCC, DEBUG_NOTICE, "MNCC socket disconnected.\n");
		close(mncc_sock);
		mncc_sock = -1;
		/* clear all call instances */
		call_mncc_flush();
	}
}

int mncc_init(const char *sock_name)
{
	struct sockaddr_un local;
	unsigned int namelen;
	int flags;
	int rc;

	listen_sock = socket(PF_UNIX, SOCK_SEQPACKET, 0);
	if (listen_sock < 0) {
		PDEBUG(DMNCC, DEBUG_ERROR, "Failed to create socket.\n");
		return listen_sock;
	}

	local.sun_family = AF_UNIX;
	strncpy(local.sun_path, sock_name, sizeof(local.sun_path));
	local.sun_path[sizeof(local.sun_path) - 1] = '\0';
	unlink(local.sun_path);

	/* we use the same magic that X11 uses in Xtranssock.c for
	 * calculating the proper length of the sockaddr */
#if defined(BSD44SOCKETS) || defined(__UNIXWARE__)
	local.sun_len = strlen(local.sun_path);
#endif
#if defined(BSD44SOCKETS) || defined(SUN_LEN)
	namelen = SUN_LEN(&local);
#else
	namelen = strlen(local.sun_path) +
	offsetof(struct sockaddr_un, sun_path);
#endif

	rc = bind(listen_sock, (struct sockaddr *) &local, namelen);
	if (rc < 0) {
		PDEBUG(DMNCC, DEBUG_ERROR, "Failed to bind the unix domain "
			"socket. '%s'\n", local.sun_path);
		mncc_exit();
		return rc;
	}

	rc = listen(listen_sock, 0);
	if (rc < 0) {
		PDEBUG(DMNCC, DEBUG_ERROR, "Failed to listen.\n");
		mncc_exit();
		return rc;
	}

	flags = fcntl(listen_sock, F_GETFL, 0);
		flags = 0;
	rc = fcntl(listen_sock, F_SETFL, flags | O_NONBLOCK);
	if (rc < 0) {
		PDEBUG(DMNCC, DEBUG_ERROR, "Failed to set socket into non-blocking IO mode.\n");
		mncc_exit();
		return rc;
	}

	PDEBUG(DMNCC, DEBUG_DEBUG, "MNCC socket at '%s' initialized, waiting for connection.\n", sock_name);

	return 0;
}

void mncc_exit(void)
{
	mncc_sock_close();

	if (listen_sock > 0) {
		close(listen_sock);
		listen_sock = -1;
	}

	PDEBUG(DMNCC, DEBUG_DEBUG, "MNCC socket removed.\n");
}

