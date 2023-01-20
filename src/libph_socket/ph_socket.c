/* PH-socket, a lightweight ISDN physical layer interface
 *
 * (C) 2022 by Andreas Eversberg <jolly@eversberg.eu>
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
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "../libtimer/timer.h"
#include "../libselect/select.h"
#include "../libdebug/debug.h"
#include "ph_socket.h"

static int ph_socket_listen_cb(struct osmo_fd *ofd, unsigned int __attribute__((unused)) what);
static int ph_socket_connect_cb(struct osmo_fd *ofd, unsigned int __attribute__((unused)) what);
static void ph_socket_timeout_cb(void *data);

static void open_connection(ph_socket_t *s)
{
	uint8_t enable = PH_CTRL_UNBLOCK;
	int rc, flags;

	if (s->connect_ofd.fd > 0)
		return;

	LOGP(DPH, LOGL_DEBUG, "Trying to connect to PH-socket server.\n");
	rc = socket(PF_UNIX, SOCK_STREAM, 0);
	if (rc < 0) {
		LOGP(DPH, LOGL_ERROR, "Failed to create UNIX socket.\n");
		osmo_timer_schedule(&s->retry_timer, SOCKET_RETRY_TIMER, 0);
		return;
	}
	s->connect_ofd.fd = rc;
	s->connect_ofd.data = s;
	s->connect_ofd.when = BSC_FD_READ;
	s->connect_ofd.cb = ph_socket_connect_cb;
	osmo_fd_register(&s->connect_ofd);
	/* set nonblocking io, because we do multiple reads when handling read event */
	flags = fcntl(s->connect_ofd.fd, F_GETFL);
	flags |= O_NONBLOCK;
	fcntl(s->connect_ofd.fd, F_SETFL, flags);
	/* connect */
	rc = connect(s->connect_ofd.fd, (struct sockaddr *)&s->sock_address, sizeof(s->sock_address));
	if (rc < 0 && errno != EAGAIN) {
		if (!s->connect_failed)
			LOGP(DPH, LOGL_NOTICE, "Failed to connect UNIX socket, retrying...\n");
		close(s->connect_ofd.fd);
		s->connect_failed = 1;
		osmo_fd_unregister(&s->connect_ofd);
		s->connect_ofd.fd = 0;
		osmo_timer_schedule(&s->retry_timer, SOCKET_RETRY_TIMER, 0);
		return;
	}
	s->connect_failed = 0;
	LOGP(DPH, LOGL_INFO, "Connection to PH-socket server.\n");
	/* reset rx buffer */
	s->rx_header_index = 0;
	s->rx_data_index = 0;
	/* indicate established socket connection */
	s->ph_socket_rx_msg(s, 0, PH_PRIM_CTRL_IND, &enable, 1);
}

static void close_connection(ph_socket_t *s)
{
	struct socket_msg_list *ml;
	uint8_t disable = PH_CTRL_BLOCK;

	if (s->connect_ofd.fd <= 0)
		return;

	LOGP(DPH, LOGL_INFO, "Connection from PH-socket closed.\n");

	/* indicate loss of socket connection */
	s->ph_socket_rx_msg(s, 0, (s->listen_ofd.fd > 0) ? PH_PRIM_CTRL_REQ : PH_PRIM_CTRL_IND, &disable, 1);

	osmo_fd_unregister(&s->connect_ofd);
	close(s->connect_ofd.fd);
	s->connect_ofd.fd = 0;

	while ((ml = s->tx_list)) {
		s->tx_list = ml->next;
		free(ml);
	}
	s->tx_list_tail = &s->tx_list;
	if (s->rx_msg) {
		free(s->rx_msg);
		s->rx_msg = NULL;
	}

	if (s->listen_ofd.fd <= 0) {
		/* set timer, so that retry is delayed */
		osmo_timer_schedule(&s->retry_timer, SOCKET_RETRY_TIMER, 0);
	}
}

int ph_socket_init(ph_socket_t *s, void (*ph_socket_rx_msg)(ph_socket_t *s, int channel, uint8_t prim, uint8_t *data,
		int length), void *priv, const char *socket_name, int server)
{
	int rc;

	memset(s, 0, sizeof(*s));
	s->name = socket_name;
	s->ph_socket_rx_msg = ph_socket_rx_msg;
	s->priv = priv;
	s->tx_list_tail = &s->tx_list;

	memset(&s->sock_address, 0, sizeof(s->sock_address));
	s->sock_address.sun_family = AF_UNIX;
	strcpy(s->sock_address.sun_path+1, socket_name);

	s->retry_timer.data = s;
	s->retry_timer.cb = ph_socket_timeout_cb;

	if (server) {
		rc = socket(PF_UNIX, SOCK_STREAM, 0);
		if (rc < 0) {
			LOGP(DPH, LOGL_ERROR, "Failed to create UNIX socket.\n");
			return rc;
		}
		s->listen_ofd.fd = rc;
		s->listen_ofd.data = s;
		s->listen_ofd.when = BSC_FD_READ;
		s->listen_ofd.cb = ph_socket_listen_cb;
		osmo_fd_register(&s->listen_ofd);

		rc = bind(s->listen_ofd.fd, (struct sockaddr *)(&s->sock_address), sizeof(s->sock_address));
		if (rc < 0) {
			LOGP(DPH, LOGL_ERROR, "Failed to bind UNIX socket with path '%s' (errno = %d (%s)).\n",
					s->name, errno, strerror(errno));
			return rc;
		}

		rc = listen(s->listen_ofd.fd, 1);
		if (rc < 0) {
			LOGP(DPH, LOGL_ERROR, "Failed to listen to UNIX socket with path '%s' (errno = %d (%s)).\n",
					s->name, errno, strerror(errno));
			return rc;
		}
	} else
		open_connection(s);

	LOGP(DPH, LOGL_INFO, "Created PH-socket at '%s'.\n", s->name);

	return 0;
}

void ph_socket_exit(ph_socket_t *s)
{
	LOGP(DPH, LOGL_INFO, "Destroyed PH-socket.\n");

	close_connection(s);
	if (s->listen_ofd.fd > 0) {
		osmo_fd_unregister(&s->listen_ofd);
		close(s->listen_ofd.fd);
		s->listen_ofd.fd = 0;
	}

	if (osmo_timer_pending(&s->retry_timer))
		osmo_timer_del(&s->retry_timer);
}

static int ph_socket_listen_cb(struct osmo_fd *ofd, unsigned int __attribute__((unused)) what)
{
	ph_socket_t *s = ofd->data;
	struct sockaddr_un sock_address;
	uint8_t enable = PH_CTRL_UNBLOCK;
	int rc, flags;

	socklen_t sock_len = sizeof(sock_address);
	/* see if there is an incoming connection */
	rc = accept(s->listen_ofd.fd, (struct sockaddr *)&sock_address, &sock_len);
	if (rc > 0) {
		if (s->connect_ofd.fd > 0) {
			LOGP(DPH, LOGL_ERROR, "Rejecting incoming connection, because we already have a client "
					"connected!\n");
			close(rc);
		} else {
			LOGP(DPH, LOGL_INFO, "Connection from PH-socket client.\n");
			s->connect_ofd.fd = rc;
			s->connect_ofd.data = s;
			s->connect_ofd.when = BSC_FD_READ;
			s->connect_ofd.cb = ph_socket_connect_cb;
			osmo_fd_register(&s->connect_ofd);
			/* set nonblocking io, because we do multiple reads when handling read event */
			flags = fcntl(s->connect_ofd.fd, F_GETFL);
			flags |= O_NONBLOCK;
			fcntl(s->connect_ofd.fd, F_SETFL, flags);
			/* reset rx buffer */
			s->rx_header_index = 0;
			s->rx_data_index = 0;
			/* indicate established socket connection */
			s->ph_socket_rx_msg(s, 0, PH_PRIM_CTRL_REQ, &enable, 1);
		}
	}

	return 0;
}

static int ph_socket_connect_cb(struct osmo_fd *ofd, unsigned __attribute__((unused)) int what)
{
	ph_socket_t *s = ofd->data;
	int rc;

	if (what & BSC_FD_READ) {
rx_again:
		if (!s->rx_msg)
			s->rx_msg = calloc(1, sizeof(*s->rx_msg));
		if (s->rx_header_index < (int)sizeof(s->rx_msg->msg.header)) {
			/* read header until complete */
			rc = recv(s->connect_ofd.fd, ((uint8_t *)&s->rx_msg->msg.header) + s->rx_header_index,
					sizeof(s->rx_msg->msg.header) - s->rx_header_index, 0);
			if (rc > 0) {
				s->rx_header_index += rc;
				goto rx_again;
			} else if (rc == 0 || errno != EAGAIN) {
				close_connection(s);
				return 0;
			}
		} else if (s->rx_data_index < s->rx_msg->msg.header.length) {
			/* read data until complete */
			rc = recv(s->connect_ofd.fd, s->rx_msg->msg.data + s->rx_data_index,
					s->rx_msg->msg.header.length - s->rx_data_index, 0);
			if (rc > 0) {
				s->rx_data_index += rc;
				goto rx_again;
			} else if (rc == 0 || errno != EAGAIN) {
				close_connection(s);
				return 0;
			}
		} else {
			/* process and free message */
			if (s->rx_msg->msg.header.prim != PH_PRIM_DATA_REQ
			 && s->rx_msg->msg.header.prim != PH_PRIM_DATA_IND
			 && s->rx_msg->msg.header.prim != PH_PRIM_DATA_CNF) {
				LOGP(DPH, LOGL_DEBUG, "message 0x%02x channel %d from socket\n",
						s->rx_msg->msg.header.prim, s->rx_msg->msg.header.channel);
				if (s->rx_msg->msg.header.length)
					LOGP(DPH, LOGL_DEBUG, " -> data:%s\n", osmo_hexdump(s->rx_msg->msg.data,
								s->rx_msg->msg.header.length));
			}
			s->ph_socket_rx_msg(s, s->rx_msg->msg.header.channel, s->rx_msg->msg.header.prim,
					s->rx_msg->msg.data, s->rx_msg->msg.header.length);
			free(s->rx_msg);
			s->rx_msg = NULL;
			/* reset rx buffer */
			s->rx_header_index = 0;
			s->rx_data_index = 0;
		}
	}

	if (what & BSC_FD_WRITE) {
		if (s->tx_list) {
			/* some frame in tx list, so try sending it */
			rc = send(s->connect_ofd.fd, ((uint8_t *)&s->tx_list->msg.header),
					sizeof(s->tx_list->msg.header) + s->tx_list->msg.header.length, 0);
			if (rc > 0) {
				struct socket_msg_list *ml;
				if (rc != (int)sizeof(s->tx_list->msg.header) + s->tx_list->msg.header.length) {
					LOGP(DPH, LOGL_ERROR, "Short write, please fix handling!\n");
				}
				/* remove list entry */
				ml = s->tx_list;
				s->tx_list = ml->next;
				if (s->tx_list == NULL)
					s->tx_list_tail = &s->tx_list;
				free(ml);
			} else if (rc == 0 || errno != EAGAIN) {
				close_connection(s);
				return 0;
			}
		} else
			s->connect_ofd.when &= ~BSC_FD_WRITE;
	}

	return 0;
}

static void ph_socket_timeout_cb(void *data)
{
	ph_socket_t *s = data;

	open_connection(s);
}

void ph_socket_tx_msg(ph_socket_t *s, int channel, uint8_t prim, uint8_t *data, int length)
{
	struct socket_msg_list *tx_msg;

	if (prim != PH_PRIM_DATA_REQ
	 && prim != PH_PRIM_DATA_IND
	 && prim != PH_PRIM_DATA_CNF) {
		LOGP(DPH, LOGL_DEBUG, "message 0x%02x channel %d to socket\n", prim, channel);
		if (length)
			LOGP(DPH, LOGL_DEBUG, " -> data:%s\n", osmo_hexdump(data, length));
	}

	if (length > (int)sizeof(tx_msg->msg.data)) {
		LOGP(DPH, LOGL_NOTICE, "Frame from HDLC process too large for socket, dropping!\n");
		return;
	}

	if (s->connect_ofd.fd <= 0) {
		LOGP(DPH, LOGL_NOTICE, "Dropping message for socket, socket is closed!\n");
		return;
	}

	tx_msg = calloc(1, sizeof(*tx_msg));
	tx_msg->msg.header.channel = channel;
	tx_msg->msg.header.prim = prim;
	if (length) {
		tx_msg->msg.header.length = length;
		memcpy(tx_msg->msg.data, data, length);
	}
	/* move message to list */
	*s->tx_list_tail = tx_msg;
	s->tx_list_tail = &tx_msg->next;
	s->connect_ofd.when |= BSC_FD_WRITE;
}

