/* Osmo-CC: Socket handling
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
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>
#include "../libdebug/debug.h"
#include "../libtimer/timer.h"
#include "message.h"
#include "cause.h"
#include "socket.h"

static const char version_string[] = OSMO_CC_VERSION;

static int _getaddrinfo(const char *host, uint16_t port, struct addrinfo **result)
{
	char portstr[8];
	struct addrinfo hints;
	int rc;

	sprintf(portstr, "%d", port);

	/* bind socket */
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = 0;
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	rc = getaddrinfo(host, portstr, &hints, result);
	if (rc < 0) {
		PDEBUG(DCC, DEBUG_ERROR, "Failed to create socket for host '%s', port '%d': %s.\n", host, port, gai_strerror(rc));
		return rc;
	}
	return rc;
}

/* send a reject message toward CC process.
 * the CC process will change the reject message to a release message when not in INIT_IN state
 */
static void rej_msg(osmo_cc_socket_t *os, uint32_t callref, uint8_t socket_cause, uint8_t isdn_cause, uint16_t sip_cause)
{
	osmo_cc_msg_t *msg;

	/* create message */
	msg = osmo_cc_new_msg(OSMO_CC_MSG_REJ_REQ);
	if (!msg)
		abort();

	/* add cause */
	osmo_cc_add_ie_cause(msg, os->location, isdn_cause, sip_cause, socket_cause);
	osmo_cc_convert_cause_msg(msg);

	/* message down */
	os->recv_msg_cb(os->priv, callref, msg);
}

void tx_keepalive_timeout(struct timer *timer)
{
	osmo_cc_conn_t *conn = (osmo_cc_conn_t *)timer->priv;
	osmo_cc_msg_t *msg;

	/* send keepalive message */ 
	msg = osmo_cc_new_msg(OSMO_CC_MSG_DUMMY_REQ);
	osmo_cc_msg_list_enqueue(&conn->os->write_list, msg, conn->callref);
	timer_start(&conn->tx_keepalive_timer, OSMO_CC_SOCKET_TX_KEEPALIVE);
}

static void close_conn(osmo_cc_conn_t *conn, uint8_t socket_cause);

void rx_keepalive_timeout(struct timer *timer)
{
	osmo_cc_conn_t *conn = (osmo_cc_conn_t *)timer->priv;

	PDEBUG(DCC, DEBUG_ERROR, "OsmoCC-Socket failed due to timeout.\n");
	close_conn(conn, OSMO_CC_SOCKET_CAUSE_TIMEOUT);
}

/* create socket process and bind socket */
int osmo_cc_open_socket(osmo_cc_socket_t *os, const char *host, uint16_t port, void *priv, void (*recv_msg_cb)(void *priv, uint32_t callref, osmo_cc_msg_t *msg), uint8_t location)
{
	int try = 0, auto_port = 0;
	struct addrinfo *result, *rp;
	int rc, sock, flags;

	memset(os, 0, sizeof(*os));

try_again:
	/* check for given port, if NULL, autoselect port */
	if (!port || auto_port) {
		port = OSMO_CC_DEFAULT_PORT + try;
		try++;
		auto_port = 1;
	}

	PDEBUG(DCC, DEBUG_DEBUG, "Create socket for host %s port %d.\n", host, port);

	rc = _getaddrinfo(host, port, &result);
	if (rc < 0)
		return rc;
	for (rp = result; rp; rp = rp->ai_next) {
		int on = 1;
		sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sock < 0)
			continue;
		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (unsigned char *)&on, sizeof(on));
		rc = bind(sock, rp->ai_addr, rp->ai_addrlen);
		if (rc == 0)
			break;
		close(sock);
	}
	freeaddrinfo(result);
	if (rp == NULL) {
		if (auto_port && port < OSMO_CC_DEFAULT_PORT_MAX) {
			PDEBUG(DCC, DEBUG_DEBUG, "Failed to bind host %s port %d, trying again.\n", host, port);
			goto try_again;
		}
		PDEBUG(DCC, DEBUG_ERROR, "Failed to bind given host %s port %d.\n", host, port);
		return -EIO;
	}

	/* listen to socket */
	rc = listen(sock, 10);
	if (rc < 0) {
		PDEBUG(DCC, DEBUG_ERROR, "Failed to listen on socket.\n");
		return rc;
	}

	/* set nonblocking io */
	flags = fcntl(sock, F_GETFL);
	flags |= O_NONBLOCK;
	fcntl(sock, F_SETFL, flags);

	os->socket = sock;
	os->recv_msg_cb = recv_msg_cb;
	os->priv = priv;
	os->location = location;

	return port;
}

/* create a connection */
static osmo_cc_conn_t *open_conn(osmo_cc_socket_t *os, int sock, uint32_t callref, int read_setup)
{
	osmo_cc_conn_t *conn, **connp;

	/* create connection */
	conn = calloc(1, sizeof(*conn));
	if (!conn) {
		PDEBUG(DCC, DEBUG_ERROR, "No mem!\n");
		abort();
	}
	conn->os = os;
	conn->socket = sock;
	conn->read_version = 1;
	conn->write_version = 1;
	conn->read_setup = read_setup;
	if (callref)
		conn->callref = callref;
	else
		conn->callref = osmo_cc_new_callref();

	timer_init(&conn->tx_keepalive_timer, tx_keepalive_timeout, conn);
	timer_init(&conn->rx_keepalive_timer, rx_keepalive_timeout, conn);
	timer_start(&conn->tx_keepalive_timer, OSMO_CC_SOCKET_TX_KEEPALIVE);
	timer_start(&conn->rx_keepalive_timer, OSMO_CC_SOCKET_RX_KEEPALIVE);

	PDEBUG(DCC, DEBUG_DEBUG, "New socket connection (callref %d).\n", conn->callref);

	/* attach to list */
	connp = &os->conn_list;
	while (*connp)
		connp = &((*connp)->next);
	*connp = conn;

	return conn;
}

/* remove a connection */
static void close_conn(osmo_cc_conn_t *conn, uint8_t socket_cause)
{
	osmo_cc_conn_t **connp;
	osmo_cc_msg_list_t *ml;

	/* detach connection first, to prevent a destruction during message handling (double free) */
	connp = &conn->os->conn_list;
	while (*connp != conn)
		connp = &((*connp)->next);
	*connp = conn->next;
	/* send reject message, if socket_cause is set */
	if (socket_cause && !conn->read_setup) {
		/* receive a release or reject (depending on state), but only if we sent a setup */
		rej_msg(conn->os, conn->callref, socket_cause, 0, 0);
	}

	PDEBUG(DCC, DEBUG_DEBUG, "Destroy socket connection (callref %d).\n", conn->callref);

	/* close socket */
	if (conn->socket)
		close(conn->socket);
	/* free partly received message */
	if (conn->read_msg)
		osmo_cc_free_msg(conn->read_msg);
	/* free send queue */
	while ((ml = conn->write_list)) {
		osmo_cc_free_msg(ml->msg);
		conn->write_list = ml->next;
		free(ml);
	}
	/* free timers */
	timer_exit(&conn->tx_keepalive_timer);
	timer_exit(&conn->rx_keepalive_timer);
	/* free connection (already detached above) */
	free(conn);
}

/* close socket and remove */
void osmo_cc_close_socket(osmo_cc_socket_t *os)
{
	osmo_cc_msg_list_t *ml;

	PDEBUG(DCC, DEBUG_DEBUG, "Destroy socket.\n");

	/* free all connections */
	while (os->conn_list)
		close_conn(os->conn_list, 0);
	/* close socket */
	if (os->socket > 0) {
		close(os->socket);
		os->socket = 0;
	}
	/* free send queue */
	while ((ml = os->write_list)) {
		osmo_cc_free_msg(ml->msg);
		os->write_list = ml->next;
		free(ml);
	}
}

/* send message to send_queue of sock instance */
int osmo_cc_sock_send_msg(osmo_cc_socket_t *os, uint32_t callref, osmo_cc_msg_t *msg, const char *host, uint16_t port)
{
	osmo_cc_msg_list_t *ml;

	/* turn _IND into _REQ and _CNF into _RSP */
	msg->type &= ~1;

	/* create list entry */
	ml = osmo_cc_msg_list_enqueue(&os->write_list, msg, callref);
	if (host)
		strncpy(ml->host, host, sizeof(ml->host) - 1);
	ml->port = port;

	return 0;
}

/* receive message
 * return 1 if work was done.
 */
static int receive_conn(osmo_cc_conn_t *conn)
{
	uint8_t socket_cause = OSMO_CC_SOCKET_CAUSE_BROKEN_PIPE;
	int rc;
	osmo_cc_msg_t *msg;
	uint8_t msg_type;
	int len;
	int work = 0;

	/* get version from remote */
	if (conn->read_version) {
		rc = recv(conn->socket, conn->read_version_string + conn->read_version_pos, strlen(version_string) - conn->read_version_pos, 0);
		if (rc < 0 && errno == EAGAIN)
			return work;
		work = 1;
		if (rc <= 0) {
			goto close;
		}
		conn->read_version_pos += rc;
		if (conn->read_version_pos == strlen(version_string)) {
			conn->read_version = 0;
			if (!!memcmp(conn->read_version_string, version_string, strlen(version_string) - 1)) {
				PDEBUG(DCC, DEBUG_NOTICE, "Remote does not seem to be an Osmo-CC socket, rejecting!\n");
				socket_cause = OSMO_CC_SOCKET_CAUSE_FAILED;
				goto close;
			}
			if (conn->read_version_string[strlen(version_string) - 1] != version_string[strlen(version_string) - 1]) {
				PDEBUG(DCC, DEBUG_NOTICE, "Remote Osmo-CC socket has wrong version (local=%s, remote=%s), rejecting!\n", version_string, conn->read_version_string);
				socket_cause = OSMO_CC_SOCKET_CAUSE_VERSION_MISMATCH;
				goto close;
			}
		} else
			return work;
	}

try_next_message:
	/* read message header from remote */
	if (!conn->read_msg) {
		rc = recv(conn->socket, ((uint8_t *)&conn->read_hdr) + conn->read_pos, sizeof(conn->read_hdr) - conn->read_pos, 0);
		if (rc < 0 && errno == EAGAIN)
			return work;
		work = 1;
		if (rc <= 0) {
			goto close;
		}
		conn->read_pos += rc;
		if (conn->read_pos == sizeof(conn->read_hdr)) {
			conn->read_msg = osmo_cc_new_msg(conn->read_hdr.type);
			if (!conn->read_msg)
				abort();
			conn->read_msg->length_networkorder = conn->read_hdr.length_networkorder;
			/* prepare for reading message */
			conn->read_pos = 0;
		} else
			return work;
	}

	/* read message data from remote */
	msg = conn->read_msg;
	len = ntohs(msg->length_networkorder);
	if (len == 0)
		goto empty_message;
	rc = recv(conn->socket, msg->data + conn->read_pos, len - conn->read_pos, 0);
	if (rc < 0 && errno == EAGAIN)
		return work;
	work = 1;
	if (rc <= 0) {
		goto close;
	}
	conn->read_pos += rc;
	if (conn->read_pos == len) {
empty_message:
		/* start RX keepalive timeer, if not already */
		timer_start(&conn->rx_keepalive_timer, OSMO_CC_SOCKET_RX_KEEPALIVE);
		/* we got our setup message, so we clear the flag */
		conn->read_setup = 0;
		/* prepare for reading header */
		conn->read_pos = 0;
		/* detach message first, because the connection might be destroyed during message handling */
		msg_type = conn->read_msg->type;
		conn->read_msg = NULL;
		/* drop dummy or forward message */
		if (msg_type == OSMO_CC_MSG_DUMMY_REQ)
			osmo_cc_free_msg(msg);
		else
			conn->os->recv_msg_cb(conn->os->priv, conn->callref, msg);
		if (msg_type == OSMO_CC_MSG_REL_REQ || msg_type == OSMO_CC_MSG_REJ_REQ) {
			PDEBUG(DCC, DEBUG_DEBUG, "closing socket because we received a release or reject message.\n");
			close_conn(conn, 0);
			return 1; /* conn removed */
		}
		goto try_next_message;
	}
	return work;

close:
	PDEBUG(DCC, DEBUG_ERROR, "OsmoCC-Socket failed, socket cause %d.\n", socket_cause);
	close_conn(conn, socket_cause);
	return work; /* conn removed */
}

/* transmit message
 * return 1 if work was done.
 */
static int transmit_conn(osmo_cc_conn_t *conn)
{
	uint8_t socket_cause = OSMO_CC_SOCKET_CAUSE_BROKEN_PIPE;
	int rc;
	osmo_cc_msg_t *msg;
	int len;
	osmo_cc_msg_list_t *ml;
	int work = 0;

	/* send socket version to remote */
	if (conn->write_version) {
		rc = write(conn->socket, version_string, strlen(version_string));
		if (rc < 0 && errno == EAGAIN)
			return work;
		work = 1;
		if (rc <= 0) {
			goto close;
		}
		if (rc != strlen(version_string)) {
			PDEBUG(DCC, DEBUG_ERROR, "short write, please fix handling!\n");
			abort();
		}
		conn->write_version = 0;
	}

	/* send message to remote */
	while (conn->write_list) {
		timer_stop(&conn->tx_keepalive_timer);
		msg = conn->write_list->msg;
		len = sizeof(*msg) + ntohs(msg->length_networkorder);
		rc = write(conn->socket, msg, len);
		if (rc < 0 && errno == EAGAIN)
			return work;
		work = 1;
		if (rc <= 0) {
			goto close;
		}
		if (rc != len) {
			PDEBUG(DCC, DEBUG_ERROR, "short write, please fix handling!\n");
			abort();
		}
		/* close socket after sending release/reject message */
		if (msg->type == OSMO_CC_MSG_REL_REQ || msg->type == OSMO_CC_MSG_REJ_REQ) {
			PDEBUG(DCC, DEBUG_DEBUG, "closing socket because we sent a release or reject message.\n");
			close_conn(conn, 0);
			return work; /* conn removed */
		}
		/* free message after sending */
		ml = conn->write_list;
		conn->write_list = ml->next;
		osmo_cc_free_msg(msg);
		free(ml);
	}

	/* start TX keepalive timeer, if not already
	 * because we stop at every message above, we actually restart the timer here.
	 * only if there is no message for the amout of time, the timer fires.
	 */
	if (!timer_running(&conn->tx_keepalive_timer))
		timer_start(&conn->tx_keepalive_timer, OSMO_CC_SOCKET_TX_KEEPALIVE);

	return work;

close:
	PDEBUG(DCC, DEBUG_NOTICE, "OsmoCC-Socket failed.\n");
	close_conn(conn, socket_cause);
	return work; /* conn removed */
}

/* handle all sockets of a socket interface
 * return 1 if work was done.
 */
int osmo_cc_handle_socket(osmo_cc_socket_t *os)
{
	struct sockaddr_storage sa;
	socklen_t slen = sizeof(sa);
	int sock;
	osmo_cc_conn_t *conn;
	osmo_cc_msg_list_t *ml, **mlp;
	int flags;
	struct addrinfo *result, *rp;
	int rc;
	int work = 0;

	/* handle messages in send queue */
	while ((ml = os->write_list)) {
		work = 1;
		/* detach list entry */
		os->write_list = ml->next;
		ml->next = NULL;
		/* search for socket connection */
		for (conn = os->conn_list; conn; conn=conn->next) {
			if (conn->callref == ml->callref)
				break;
		}
		if (conn) {
			/* attach to list */
			mlp = &conn->write_list;
			while (*mlp)
				mlp = &((*mlp)->next);
			*mlp = ml;
			/* done with message */
			continue;
		}

		/* reject and release are ignored */
		if (ml->msg->type == OSMO_CC_MSG_REJ_REQ
		 || ml->msg->type == OSMO_CC_MSG_REL_REQ) {
			/* drop message */
			osmo_cc_free_msg(ml->msg);
			free(ml);
			/* done with message */
			continue;
		}

		/* reject, if this is not a setup message */
		if (ml->msg->type != OSMO_CC_MSG_SETUP_REQ
		 && ml->msg->type != OSMO_CC_MSG_ATTACH_REQ) {
			PDEBUG(DCC, DEBUG_ERROR, "Message with unknown callref.\n");
			rej_msg(os, ml->callref, 0, OSMO_CC_ISDN_CAUSE_INVAL_CALLREF, 0);
			/* drop message */
			osmo_cc_free_msg(ml->msg);
			free(ml);
			/* done with message */
			continue;
		}
		/* connect to remote */
		rc = _getaddrinfo(ml->host, ml->port, &result);
		if (rc < 0) {
			rej_msg(os, ml->callref, OSMO_CC_SOCKET_CAUSE_FAILED, 0, 0);
			/* drop message */
			osmo_cc_free_msg(ml->msg);
			free(ml);
			/* done with message */
			continue;
		}
		for (rp = result; rp; rp = rp->ai_next) {
			sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
			if (sock < 0)
				continue;
			/* set nonblocking io */
			flags = fcntl(sock, F_GETFL);
			flags |= O_NONBLOCK;
			fcntl(sock, F_SETFL, flags);
			/* connect */
			rc = connect(sock, rp->ai_addr, rp->ai_addrlen);
			if (rc == 0 || errno == EINPROGRESS)
				break;
			close(sock);
		}
		freeaddrinfo(result);
		if (rp == NULL) {
			PDEBUG(DCC, DEBUG_ERROR, "Failed to connect to given host %s port %d.\n", ml->host, ml->port);
			rej_msg(os, ml->callref, OSMO_CC_SOCKET_CAUSE_FAILED, 0, 0);
			/* drop message */
			osmo_cc_free_msg(ml->msg);
			free(ml);
			/* done with message */
			continue;
		}
		/* create connection */
		conn = open_conn(os, sock, ml->callref, 0);
		/* attach to list */
		conn->write_list = ml;
		/* done with (setup) message */
	}

	/* handle new socket connection */
	while ((sock = accept(os->socket, (struct sockaddr *)&sa, &slen)) > 0) {
		work = 1;
		/* set nonblocking io */
		flags = fcntl(sock, F_GETFL);
		flags |= O_NONBLOCK;
		fcntl(sock, F_SETFL, flags);
		/* create connection */
		open_conn(os, sock, 0, 1);
	}

	/* start with list after each read/write, because while handling (the message), one or more connections may be destroyed */
	for (conn = os->conn_list; conn; conn=conn->next) {
		/* check for rx */
		work = receive_conn(conn);
		/* if "change" is set, connection list might have changed, so we restart processing the list */
		if (work)
			break;
		/* check for tx */
		work = transmit_conn(conn);
		/* if "change" is set, connection list might have changed, so we restart processing the list */
		if (work)
			break;
	}

	return work;
}

