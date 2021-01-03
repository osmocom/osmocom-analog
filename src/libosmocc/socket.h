#ifndef OSMO_CC_SOCKET_H
#define OSMO_CC_SOCKET_H

#define OSMO_CC_DEFAULT_PORT		4200
#define OSMO_CC_DEFAULT_PORT_MAX	4219

#define OSMO_CC_SOCKET_TX_KEEPALIVE	10.0
#define OSMO_CC_SOCKET_RX_KEEPALIVE	20.0

struct osmo_cc_socket;

typedef struct osmo_cc_conn {
	struct osmo_cc_conn	*next;
	struct osmo_cc_socket	*os;
	int			socket;
	uint32_t		callref;
	int			read_setup;
	int			read_version;
	char			read_version_string[sizeof(OSMO_CC_VERSION)]; /* must include 0-termination */
	int			read_version_pos;
	int			write_version;
	osmo_cc_msg_t		read_hdr;
	osmo_cc_msg_t		*read_msg;
	int			read_pos;
	osmo_cc_msg_list_t	*write_list;
	struct timer		tx_keepalive_timer;
	struct timer		rx_keepalive_timer;
} osmo_cc_conn_t;

typedef struct osmo_cc_socket {
	int			socket;
	osmo_cc_conn_t		*conn_list;
	osmo_cc_msg_list_t	*write_list;
	void (*recv_msg_cb)(void *priv, uint32_t callref, osmo_cc_msg_t *msg);
	void *priv;
	uint8_t			location;
} osmo_cc_socket_t;

int osmo_cc_open_socket(osmo_cc_socket_t *os, const char *host, uint16_t port, void *priv, void (*recv_msg_cb)(void *priv, uint32_t callref, osmo_cc_msg_t *msg), uint8_t location);
void osmo_cc_close_socket(osmo_cc_socket_t *os);
int osmo_cc_sock_send_msg(osmo_cc_socket_t *os, uint32_t callref, osmo_cc_msg_t *msg, const char *host, uint16_t port);
int osmo_cc_handle_socket(osmo_cc_socket_t *os);

#endif /* OSMO_CC_SOCKET_H */
