#ifndef OSMO_CC_ENDPOINT_H
#define OSMO_CC_ENDPOINT_H

#include "message.h"
#include "socket.h"
#include "cause.h"

/* special osmo-cc error codes */
#define OSMO_CC_RC_SEE_ERRNO			-1
#define OSMO_CC_RC_VERSION_MISMATCH		1

#define OSMO_CC_ATTACH_TIMER			2

/* call control state */
enum osmo_cc_state {
	OSMO_CC_STATE_IDLE = 0,
	/* call states */
	OSMO_CC_STATE_INIT_OUT,			/* outgoing CC-SETUP-REQ sent */
	OSMO_CC_STATE_INIT_IN,			/* incoming CC-SETUP-IND received */
	OSMO_CC_STATE_OVERLAP_OUT,		/* received CC-SETUP-ACK-IND on outgoing call */
	OSMO_CC_STATE_OVERLAP_IN,		/* sent CC-SETUP-ACK-REQ on incoming call */
	OSMO_CC_STATE_PROCEEDING_OUT,		/* received CC-PROC-IND on outgoing call */
	OSMO_CC_STATE_PROCEEDING_IN,		/* sent CC-PROC-REQ on incoming call */
	OSMO_CC_STATE_ALERTING_OUT,		/* received CC-ALERT-IND on outgoing call */
	OSMO_CC_STATE_ALERTING_IN,		/* sent CC-ALERT-REQ on incoming call */
	OSMO_CC_STATE_CONNECTING_OUT,		/* received CC-SETUP-CNF on outgoing call */
	OSMO_CC_STATE_CONNECTING_IN,		/* sent CC-SETUP-RSP on incoming call */
	OSMO_CC_STATE_ACTIVE,			/* received or sent CC-SETUP-COMPL-* */
	OSMO_CC_STATE_DISCONNECTING_OUT,	/* sent CC-DISC-REQ */
	OSMO_CC_STATE_DISCONNECTING_IN,		/* received CC-DISC-IND */
	OSMO_CC_STATE_DISC_COLLISION,		/* received CC-DISC-IND after sending CC-DISC_REQ */
	OSMO_CC_STATE_RELEASING_OUT,		/* sent CC-REL-REQ */
	/* attachment states */
	OSMO_CC_STATE_ATTACH_SENT,		/* outgoing CC-ATT-REQ sent to socket */
	OSMO_CC_STATE_ATTACH_OUT,		/* received CC-ATT-RSP on outgoing socket */
	OSMO_CC_STATE_ATTACH_WAIT,		/* wait for outgoing attachment after failure */
	OSMO_CC_STATE_ATTACH_IN,		/* incoming CC-ATT-REQ received from socket*/
};

/* sample type */
typedef int16_t osmo_cc_sample_t;

#define OSMO_CC_SAMPLE_MILLIWATT 23170 /* peak sine at -3 dB of full sample range */
#define OSMO_CC_SAMPLE_SPEECH 3672 /* peak speech at -16 dB of milliwatt */
#define OSMO_CC_SAMPLE_MIN -32768 /* lowest level */
#define OSMO_CC_SAMPLE_MAX 32767 /* highest level */

struct osmo_cc_call;

typedef struct osmo_cc_screen_list {
	struct osmo_cc_screen_list *next;
	int			has_from_type;
	uint8_t			from_type;
	int			has_from_present;
	uint8_t			from_present;
	char			from[128];
	int			has_to_type;
	uint8_t			to_type;
	int			has_to_present;
	uint8_t			to_present;
	char			to[128];
} osmo_cc_screen_list_t;

/* endpoint instance */
typedef struct osmo_cc_endpoint {
	struct osmo_cc_endpoint	*next;
	void			*priv;
	void			(*ll_msg_cb)(struct osmo_cc_endpoint *ep, uint32_t callref, osmo_cc_msg_t *msg);
	void			(*ul_msg_cb)(struct osmo_cc_call *call, osmo_cc_msg_t *msg);
	osmo_cc_msg_list_t	*ll_queue;	/* messages towards lower layer */
	struct osmo_cc_call	*call_list;
	const char		*local_name; /* name of interface */
	const char		*local_address; /* host+port */
	const char		*local_host;
	uint16_t		local_port;
	const char		*remote_address; /* host+port */
	const char		*remote_host;
	uint16_t		remote_port;
	uint8_t			serving_location;
	osmo_cc_socket_t	os;
	osmo_cc_screen_list_t	*screen_calling_in;
	osmo_cc_screen_list_t	*screen_called_in;
	osmo_cc_screen_list_t	*screen_calling_out;
	osmo_cc_screen_list_t	*screen_called_out;
	int			remote_auto;	/* automatic remote address */
	struct timer		attach_timer;	/* timer to retry attachment */
} osmo_cc_endpoint_t;

extern osmo_cc_endpoint_t *osmo_cc_endpoint_list;

/* call process */
typedef struct osmo_cc_call {
	struct osmo_cc_call	*next;
	osmo_cc_endpoint_t	*ep;
	enum osmo_cc_state	state;
	int			lower_layer_released;	/* when lower layer sent release, while upper layer gets a disconnect */
	int			upper_layer_released;	/* when upper layer sent release, while lower layer gets a disconnect */
	uint32_t		callref;
	osmo_cc_msg_list_t	*sock_queue;	/* messages from socket */
	const char		*attached_host;	/* host and port from remote peer that attached to us */
	uint16_t		attached_port;
	const char		*attached_name;	/* interface name from remote peer that attached to us */
} osmo_cc_call_t;

/* returns 0 if ok
 * returns <0 for error as indicated
 * returns >=1 to indicate osmo-cc error code
 */

void osmo_cc_help(void);
int osmo_cc_new(osmo_cc_endpoint_t *ep, const char *version, const char *name, uint8_t serving_location, void (*ll_msg_cb)(osmo_cc_endpoint_t *ep, uint32_t callref, osmo_cc_msg_t *msg), void (*ul_msg_cb)(osmo_cc_call_t *call, osmo_cc_msg_t *msg), void *priv, int argc, const char *argv[]);
void osmo_cc_delete(struct osmo_cc_endpoint *ep);
int osmo_cc_handle(void);
osmo_cc_call_t *osmo_cc_call_by_callref(osmo_cc_endpoint_t *ep, uint32_t callref);
osmo_cc_call_t *osmo_cc_get_attached_interface(osmo_cc_endpoint_t *ep, const char *interface);
void osmo_cc_ll_msg(osmo_cc_endpoint_t *ep, uint32_t callref, osmo_cc_msg_t *msg);
void osmo_cc_ul_msg(void *priv, uint32_t callref, osmo_cc_msg_t *msg);
osmo_cc_call_t *osmo_cc_call_new(osmo_cc_endpoint_t *ep);
void osmo_cc_call_delete(struct osmo_cc_call *call);
enum osmo_cc_session_addrtype osmo_cc_address_type(const char *address);
const char *osmo_cc_host_of_address(const char *address);
const char *osmo_cc_port_of_address(const char *address);

#include "session.h"
#include "rtp.h"
#include "sdp.h"
#include "screen.h"

#endif /* OSMO_CC_ENDPOINT_H */
