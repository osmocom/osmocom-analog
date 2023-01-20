
#include <sys/un.h>

/*
 * Procedure:
 *
 * If socket connection is establised, a PH_PRIM_CTRL_REQ message with
 * PH_CTRL_ENABLE information is received by the socket server user.
 * If the socket connection is lost, a PH_PRIM_CTRL_REQ message with
 * PH_CTRL_DISABLE information is received by the user.
 * 
 * If socket connection is establised, a PH_PRIM_CTRL_IND message with
 * PH_CTRL_ENABLE information is received by the socket client user.
 * If the socket connection is lost, a PH_PRIM_CTRL_IND message with
 * PH_CTRL_DISABLE information is received by the user.
 *
 * The socket server should enable or disable interface depending on
 * the PH_CTRL_ENABLE / PH_CTRL_DISABLE information.
 *
 * The socket client user shall keep track of last PH_PRIM_ACT_IND /
 * PH_PRIM_DEACT_IND message and treat a PH_PRIM_CTRL_IND message with
 * PH_CTRL_DISABLE information as a deactivation of all channels that
 * were activated. Also it shall reject every PH_RIM_ACT_REQ with a
 * PH_PRIM_DACT_IND, if the socket is currently unavailable.
 *
 * PH_PRIM_CTRL_REQ and PH_PRIM_CTRL_IND messages with PH_CTRL_ENABLE
 * and PH_CTRL_DISABLE informations are not assoicated with a channel
 * number. The socket sender shall set it to 0, the receiver shall
 * ignore it.
 *
 * A missing MODE in PH_PRIM_ACT_REQ is interepreted as default:
 * HDLC on D-channel, TRANS on B-channel.
 *
 * Each packet on the socket shall have the follwoing header:
 *   uint8_t channel;
 *   uint8_t prim;
 *   uint16_t length;
 *
 * The length shall be in host's endian on UN sockets and in network
 * endian on TCP sockets and not being transmitted on UDP sockets.
 *
 * 0 to 65535 bytes shall follow the header, depending on the length
 * information field.
 */

/* all primitives */
#define PH_PRIM_DATA_REQ	0x00	/* any data sent to channel from upper layer */
#define PH_PRIM_DATA_IND	0x01	/* any data received from channel to upper layer */
#define PH_PRIM_DATA_CNF	0x02	/* confirm data sent to channel */

#define PH_PRIM_CTRL_REQ	0x04	/* implementation specific requests towards interface */
#define PH_PRIM_CTRL_IND	0x05	/* implementation specific indications from interface */

#define PH_PRIM_ACT_REQ		0x08	/* activation request of channel, mode is given as payload */
#define PH_PRIM_ACT_IND		0x09	/* activation indication that the channel is now active */

#define PH_PRIM_DACT_REQ	0x0c	/* deactivation request of channel */
#define PH_PRIM_DACT_IND	0x0d	/* deactivation indication that the channel is now inactive */

/* one byte sent activation request */
#define PH_MODE_TRANS		0x00	/* raw data is sent via B-channel */
#define PH_MODE_HDLC		0x01	/* HDLC transcoding is performed via B-channel */

/* one byte sent with control messages */
#define PH_CTRL_BLOCK		0x00	/* disable (block) interface, when socket is disconnected */
#define PH_CTRL_UNBLOCK		0x01	/* enable (unblock) interface, when socket is connected */
#define PH_CTRL_LOOP_DISABLE	0x04	/* disable loopback */
#define PH_CTRL_LOOP1_ENABLE	0x05	/* enable LT transceier loopback */
#define PH_CTRL_LOOP2_ENABLE	0x06	/* enable NT transceier loopback */
#define PH_CTRL_LOOP_ERROR	0x10	/* frame error report (loopback test) */
#define PH_CTRL_VIOLATION_LT	0x11	/* code violation received by LT */
#define PH_CTRL_VIOLATION_NT	0x12	/* code violation received by NT */

struct socket_msg {
	struct {
		uint8_t channel;
		uint8_t prim;
		uint16_t length;
	} header;
	uint8_t data[65536];
} __attribute__((packed));

struct socket_msg_list {
	struct socket_msg_list *next;
	struct socket_msg msg;
};

#define SOCKET_RETRY_TIMER	2

typedef struct ph_socket {
	const char *name;
	void (*ph_socket_rx_msg)(struct ph_socket *s, int channel, uint8_t prim, uint8_t *data, int length);
	void *priv;
	struct sockaddr_un sock_address;
	struct osmo_fd listen_ofd;		/* socket to listen to incoming connections */
	struct osmo_fd connect_ofd;		/* socket of incoming connection */
	int connect_failed;			/* used to print a failure only once */
	struct osmo_timer_list retry_timer;	/* timer to connect again */
	struct socket_msg_list *tx_list, **tx_list_tail;
	struct socket_msg_list *rx_msg;
	int rx_header_index;
	int rx_data_index;
} ph_socket_t;

int ph_socket_init(ph_socket_t *s, void (*ph_socket_rx_msg)(ph_socket_t *s, int channel, uint8_t prim, uint8_t *data, int length), void *priv, const char *socket_name, int server);
void ph_socket_exit(ph_socket_t *s);
void ph_socket_tx_msg(ph_socket_t *s, int channel, uint8_t prim, uint8_t *data, int length);
