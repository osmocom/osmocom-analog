/* Endpoint and call process handling
 *
 * (C) 2019 by Andreas Eversberg <jolly@eversberg.eu>
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
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "../libtimer/timer.h"
#include "../libdebug/debug.h"
#include "endpoint.h"

osmo_cc_endpoint_t *osmo_cc_endpoint_list = NULL;

static osmo_cc_call_t *call_new(osmo_cc_endpoint_t *ep, uint32_t callref)
{
	osmo_cc_call_t *call, **cp;

	call = calloc(1, sizeof(*call));
	if (!call) {
		PDEBUG(DCC, DEBUG_ERROR, "No memory for call process instance.\n");
		abort();
	}

	PDEBUG(DCC, DEBUG_DEBUG, "Creating new call with callref %u.\n", callref);

	call->ep = ep;
	call->callref = callref;

	/* attach to call process list */
	cp = &ep->call_list;
	while (*cp)
		cp = &((*cp)->next);
	*cp = call;

	/* return new entry */
	return call;
}

static void call_delete(osmo_cc_call_t *call)
{
	osmo_cc_call_t **cp;

	PDEBUG(DCC, DEBUG_DEBUG, "Destroying call with callref %u.\n", call->callref);

	/* detach from call process list */
	cp = &call->ep->call_list;
	while (*cp != call)
		cp = &((*cp)->next);
	*cp = call->next;

	/* flush message queue */
	while (call->sock_queue) {
		osmo_cc_msg_t *msg = osmo_cc_msg_list_dequeue(&call->sock_queue, NULL);
		osmo_cc_free_msg(msg);
	}

	/* free remote peer */
	free((char *)call->attached_name);
	free((char *)call->attached_host);

	free(call);
}

static const char *state_names[] = {
	"IDLE",
	"INIT-OUT",
	"INIT-IN",
	"OVERLAP-OUT",
	"OVERLAP-IN",
	"PROCEEDING-OUT",
	"PROCEEDING-IN",
	"ALERTING-OUT",
	"ALERTING-IN",
	"CONNECTING-OUT",
	"CONNECTING-IN",
	"ACTIVE",
	"DISCONNECTING-OUT",
	"DISCONNECTING-IN",
	"DISCONNECT-COLLISION",
	"RELEASING-OUT",
	"ATTACH-SENT",
	"ATTACH-OUT",
	"ATTACH-WAIT",
	"ATTACH-IN",
};

static void new_call_state(osmo_cc_call_t *call, enum osmo_cc_state new_state)
{
	PDEBUG(DCC, DEBUG_DEBUG, "Changing call state with callref %u from %s to %s.\n", call->callref, state_names[call->state], state_names[new_state]);
	call->state = new_state;
}

/* helper to forward message to lower layer */
static void forward_to_ll(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	if (call->lower_layer_released)
		return;

	if (msg->type == OSMO_CC_MSG_SETUP_REQ
	 || msg->type == OSMO_CC_MSG_SETUP_RSP) {
		/* screen towards lower layer */
		msg = osmo_cc_screen_msg(call->ep, msg, 0, NULL);
	}

	osmo_cc_msg_list_enqueue(&call->ep->ll_queue, msg, call->callref);
}

static void sock_reject_msg(osmo_cc_socket_t *os, uint32_t callref, uint8_t location, uint8_t socket_cause, uint8_t isdn_cause, uint16_t sip_cause)
{
	osmo_cc_msg_t *msg;

	/* create message */
	msg = osmo_cc_new_msg(OSMO_CC_MSG_REJ_IND);

	/* add cause */
	osmo_cc_add_ie_cause(msg, location, isdn_cause, sip_cause, socket_cause);
	osmo_cc_convert_cause_msg(msg);

	/* message to socket */
	osmo_cc_sock_send_msg(os, callref, msg, NULL, 0);
}

static void ll_reject_msg(osmo_cc_call_t *call, uint8_t location, uint8_t socket_cause, uint8_t isdn_cause, uint16_t sip_cause)
{
	osmo_cc_msg_t *msg;

	/* create message */
	msg = osmo_cc_new_msg(OSMO_CC_MSG_REJ_REQ);

	/* add cause */
	osmo_cc_add_ie_cause(msg, location, isdn_cause, sip_cause, socket_cause);
	osmo_cc_convert_cause_msg(msg);

	/* message to lower layer */
	forward_to_ll(call, msg);
}

static int split_address(const char *address, const char **host_p, uint16_t *port_p)
{
	const char *portstring;

	*host_p = osmo_cc_host_of_address(address);
	if (!(*host_p)) {
		PDEBUG(DCC, DEBUG_ERROR, "Host IP in given address '%s' is invalid.\n", address);
		return -EINVAL;
	}
	portstring = osmo_cc_port_of_address(address);
	if (!portstring) {
		PDEBUG(DCC, DEBUG_ERROR, "Port number in given address '%s' is not specified or invalid.\n", address);
		return -EINVAL;
	}
	*port_p = atoi(portstring);

	return 0;
}


osmo_cc_call_t *osmo_cc_get_attached_interface(osmo_cc_endpoint_t *ep, const char *interface)
{
	osmo_cc_call_t *att;

	for (att = ep->call_list; att; att = att->next) {
		if (att->state != OSMO_CC_STATE_ATTACH_IN)
			continue;
		/* no interface given, just use the attached peer */
		if (!interface[0])
			break;
		/* no interface name given on attached peer, ignore it */
		if (!att->attached_name || !att->attached_name[0])
			continue;
		/* interface given, use the attached peer with the same interface name */
		if (!strcmp(interface, att->attached_name))
			break;
	}

	return att;
}
/* helper to forward message to upper layer */
static void forward_to_ul(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	const char *address = NULL, *host = NULL;
	uint16_t port;
	int rc;

	if (call->upper_layer_released)
		return;

	if (msg->type == OSMO_CC_MSG_SETUP_IND
	 || msg->type == OSMO_CC_MSG_SETUP_CNF) {
		/* screen towards upper layer */
		msg = osmo_cc_screen_msg(call->ep, msg, 1, &address);
	}

	/* no socket: forward message to upper layer */
	if (call->ep->ul_msg_cb) {
		call->ep->ul_msg_cb(call, msg);
		return;
	}

	/* if remote peer is included in the setup message */
	if (address && msg->type == OSMO_CC_MSG_SETUP_IND) {
		rc = split_address(address, &host, &port);
		if (rc < 0) {
			PDEBUG(DCC, DEBUG_ERROR, "Given remote peer's address '%s' in setup message is invalid, rejecting call.\n", address);
reject:
			/* reject, due to error */
			osmo_cc_free_msg(msg);
			new_call_state(call, OSMO_CC_STATE_IDLE);
			ll_reject_msg(call, call->ep->serving_location, 0, OSMO_CC_ISDN_CAUSE_DEST_OOO, 0);
			call_delete(call);
			return;
		}
		PDEBUG(DCC, DEBUG_DEBUG, "Using host IP '%s' and port '%d' from setup message.\n", host, port);
	}

	/* for attach message, use remote peer */
	if (msg->type == OSMO_CC_MSG_ATTACH_IND) {
		host = call->ep->remote_host;
		port = call->ep->remote_port;
		PDEBUG(DCC, DEBUG_DEBUG, "Using host IP '%s' and port '%d' from remote address for attach message.\n", host, port);
	}

	/* if there is no remote peer in the setup message, use remote peer */
	if (!address && msg->type == OSMO_CC_MSG_SETUP_IND && call->ep->remote_host) {
		host = call->ep->remote_host;
		port = call->ep->remote_port;
		PDEBUG(DCC, DEBUG_DEBUG, "Using host IP '%s' and port '%d' from remote address for setup message.\n", host, port);
	}

	/* if there is no remote peer set, try to use the interface name */
	if (!host && msg->type == OSMO_CC_MSG_SETUP_IND) {
		char interface[256];
		osmo_cc_call_t *att;

		rc = osmo_cc_get_ie_called_interface(msg, 0, interface, sizeof(interface));
		if (rc < 0)
			interface[0] = '\0';
		/* check for incoming attachment */
		att = osmo_cc_get_attached_interface(call->ep, interface);
		if (!att && !interface[0]) {
			PDEBUG(DCC, DEBUG_ERROR, "No remote peer attached, rejecting call.\n");
			goto reject;
		}
		if (!att) {
			PDEBUG(DCC, DEBUG_ERROR, "No remote peer attached for given interface '%s', rejecting call.\n", interface);
			goto reject;
		}
		host = att->attached_host;
		port = att->attached_port;
		PDEBUG(DCC, DEBUG_DEBUG, "Using host IP '%s' and port '%d' from attached peer for setup message.\n", host, port);
	}

	/* add local interface name to setup message */
	// FIXME: should we do that if there is already an interface name given?
	if (msg->type == OSMO_CC_MSG_SETUP_IND && call->ep->local_name)
		osmo_cc_add_ie_calling_interface(msg, call->ep->local_name);

	/* forward message to socket */
	osmo_cc_sock_send_msg(&call->ep->os, call->callref, msg, host, port);
}

/* send attach indication to socket */
void send_attach_ind(struct timer *timer)
{
	osmo_cc_endpoint_t *ep = (osmo_cc_endpoint_t *)timer->priv;
	osmo_cc_call_t *call;
	osmo_cc_msg_t *msg;

	PDEBUG(DCC, DEBUG_DEBUG, "Trying to attach to remote peer \"%s\".\n", ep->remote_host);

	/* create new call for attachment */
	call = osmo_cc_call_new(ep);

	/* create attach message */
	msg = osmo_cc_new_msg(OSMO_CC_MSG_ATTACH_IND);

	/* set interface name and address */
	osmo_cc_add_ie_calling_interface(msg, ep->local_name);
	osmo_cc_add_ie_socket_address(msg, ep->local_address);

	/* message to socket */
	forward_to_ul(call, msg);

	/* set state */
	new_call_state(call, OSMO_CC_STATE_ATTACH_SENT);
}

void attach_rsp(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	PDEBUG(DCC, DEBUG_INFO, "Attached to remote peer \"%s\".\n", call->ep->remote_address);

	/* set state */
	new_call_state(call, OSMO_CC_STATE_ATTACH_OUT);

	/* drop message */
	osmo_cc_free_msg(msg);
}

void attach_rel(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* (re-)start timer for next attachment */
	if (call->state == OSMO_CC_STATE_ATTACH_SENT
	 || call->state == OSMO_CC_STATE_ATTACH_OUT) {
		timer_start(&call->ep->attach_timer, OSMO_CC_ATTACH_TIMER);
		PDEBUG(DCC, DEBUG_INFO, "Attachment to remote peer \"%s\" failed, retrying.\n", call->ep->remote_address);
	}

	if (call->attached_name)
		PDEBUG(DCC, DEBUG_INFO, "Peer with remote interface \"%s\" detached from us.\n", call->attached_name);

	/* change state */
	new_call_state(call, OSMO_CC_STATE_IDLE);

	/* unset interface */
	free((char *)call->attached_name);
	call->attached_name = NULL;
	free((char *)call->attached_host);
	call->attached_host = NULL;

	/* drop message */
	osmo_cc_free_msg(msg);

	/* destroy */
	call_delete(call);
}

void attach_req(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	char address[256];
	char interface[256];
	const char *host;
	uint16_t port;
	int rc;

	/* get peer from message */
	rc = osmo_cc_get_ie_socket_address(msg, 0, address, sizeof(address));
	if (rc < 0)
		address[0] = '\0';
	if (!address[0]) {
		PDEBUG(DCC, DEBUG_ERROR, "Attachment request from remote peer has no remote address set, rejecting.\n");

rel:
		/* change to REL_REQ */
		msg->type = OSMO_CC_MSG_REL_IND;
		PDEBUG(DCC, DEBUG_INFO, "Changing message to %s.\n", osmo_cc_msg_value2name(msg->type));

		/* message to socket */
		forward_to_ul(call, msg);

		/* destroy */
		call_delete(call);

		return;
	}
	rc = split_address(address, &host, &port);
	if (rc < 0) {
		PDEBUG(DCC, DEBUG_ERROR, "Given remote peer's address '%s' in attach message is invalid, rejecting call.\n", address);
		goto rel;
	}
	free((char *)call->attached_host);
	call->attached_host = strdup(host);
	call->attached_port = port;

	rc = osmo_cc_get_ie_calling_interface(msg, 0, interface, sizeof(interface));
	if (rc < 0)
		interface[0] = '\0';
	if (interface[0]) {
		free((char *)call->attached_name);
		call->attached_name = strdup(interface);
	}

	PDEBUG(DCC, DEBUG_INFO, "Remote peer with socket address '%s' and port '%d' and interface '%s' attached to us.\n", call->attached_host, call->attached_port, call->attached_name);

	/* changing to confirm message */
	msg->type = OSMO_CC_MSG_ATTACH_CNF;
	PDEBUG(DCC, DEBUG_INFO, "Changing message to %s.\n", osmo_cc_msg_value2name(msg->type));

	/* message to socket */
	forward_to_ul(call, msg);

	/* set state */
	new_call_state(call, OSMO_CC_STATE_ATTACH_IN);
}

static void setup_req(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* change state */
	new_call_state(call, OSMO_CC_STATE_INIT_OUT);

	/* to lower layer */
	forward_to_ll(call, msg);
}

static void setup_ind(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* change state */
	new_call_state(call, OSMO_CC_STATE_INIT_IN);

	/* to upper layer */
	forward_to_ul(call, msg);
}

static void rej_req(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* change state */
	new_call_state(call, OSMO_CC_STATE_IDLE);

	/* to lower layer */
	forward_to_ll(call, msg);

	/* destroy */
	call_delete(call);
}

static void rej_ind(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* change state */
	new_call_state(call, OSMO_CC_STATE_IDLE);

	/* to upper layer */
	forward_to_ul(call, msg);

	/* destroy */
	call_delete(call);
}

static void setup_ack_req(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* change state */
	new_call_state(call, OSMO_CC_STATE_OVERLAP_IN);

	/* to lower layer */
	forward_to_ll(call, msg);
}

static void setup_ack_ind(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* change state */
	new_call_state(call, OSMO_CC_STATE_OVERLAP_OUT);

	/* to upper layer */
	forward_to_ul(call, msg);
}

static void proc_req(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* change state */
	new_call_state(call, OSMO_CC_STATE_PROCEEDING_IN);

	/* to lower layer */
	forward_to_ll(call, msg);
}

static void proc_ind(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* change state */
	new_call_state(call, OSMO_CC_STATE_PROCEEDING_OUT);

	/* to upper layer */
	forward_to_ul(call, msg);
}

static void alert_req(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* change state */
	new_call_state(call, OSMO_CC_STATE_ALERTING_IN);

	/* to lower layer */
	forward_to_ll(call, msg);
}

static void alert_ind(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* change state */
	new_call_state(call, OSMO_CC_STATE_ALERTING_OUT);

	/* to upper layer */
	forward_to_ul(call, msg);
}

static void setup_rsp(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* change state */
	new_call_state(call, OSMO_CC_STATE_CONNECTING_IN);

	/* to lower layer */
	forward_to_ll(call, msg);
}

static void setup_cnf(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* change state */
	new_call_state(call, OSMO_CC_STATE_CONNECTING_OUT);

	/* to upper layer */
	forward_to_ul(call, msg);
}

static void setup_comp_req(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* change state */
	new_call_state(call, OSMO_CC_STATE_ACTIVE);

	/* to lower layer */
	forward_to_ll(call, msg);
}

static void setup_comp_ind(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* change state */
	new_call_state(call, OSMO_CC_STATE_ACTIVE);

	/* to upper layer */
	forward_to_ul(call, msg);
}

static void info_req(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* to lower layer */
	forward_to_ll(call, msg);
}

static void info_ind(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* to upper layer */
	forward_to_ul(call, msg);
}

static void progress_req(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* to lower layer */
	forward_to_ll(call, msg);
}

static void progress_ind(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* to upper layer */
	forward_to_ul(call, msg);
}

static void notify_req(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* to lower layer */
	forward_to_ll(call, msg);
}

static void notify_ind(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* to upper layer */
	forward_to_ul(call, msg);
}

static void update_req(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* to lower layer */
	forward_to_ll(call, msg);
}

static void update_cnf(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* to upper layer */
	forward_to_ul(call, msg);
}

static void disc_req(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* change state */
	new_call_state(call, OSMO_CC_STATE_DISCONNECTING_OUT);

	/* to lower layer */
	forward_to_ll(call, msg);
}

static void disc_ind(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* change state */
	new_call_state(call, OSMO_CC_STATE_DISCONNECTING_IN);

	/* to upper layer */
	forward_to_ul(call, msg);
}

static void rel_req(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* terminate process, if there is no lower layer anmore */
	if (call->lower_layer_released) {
		/* change state */
		new_call_state(call, OSMO_CC_STATE_IDLE);

		/* drop message */
		osmo_cc_free_msg(msg);

		/* destroy */
		call_delete(call);

		return;
	}

	/* change state */
	new_call_state(call, OSMO_CC_STATE_RELEASING_OUT);

	/* to lower layer */
	forward_to_ll(call, msg);
}

static void rel_ind(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* change state */
	new_call_state(call, OSMO_CC_STATE_IDLE);

	/* to upper layer */
	forward_to_ul(call, msg);

	/* destroy */
	call_delete(call);
}

static void rel_cnf(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* change state */
	new_call_state(call, OSMO_CC_STATE_IDLE);

	/* drop message */
	osmo_cc_free_msg(msg);

	/* destroy */
	call_delete(call);
}

static void disc_collision_ind(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* release to lower layer wheen there is no upper layer */
	if (call->upper_layer_released) {
		/* change state */
		new_call_state(call, OSMO_CC_STATE_RELEASING_OUT);

		/* change to REL_REQ */
		msg->type = OSMO_CC_MSG_REL_REQ;
		PDEBUG(DCC, DEBUG_INFO, "Changing message to %s.\n", osmo_cc_msg_value2name(msg->type));

		/* to lower layer */
		forward_to_ll(call, msg);

		return;
	}

	/* change state */
	new_call_state(call, OSMO_CC_STATE_DISC_COLLISION);

	/* to upper layer */
	forward_to_ul(call, msg);
}

static void disc_collision_req(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* release to upper layer wheen there is no lower layer */
	if (call->lower_layer_released) {
		/* change to REL_REQ */
		msg->type = OSMO_CC_MSG_REL_IND;
		PDEBUG(DCC, DEBUG_INFO, "Changing message to %s.\n", osmo_cc_msg_value2name(msg->type));

		/* to upper layer */
		forward_to_ul(call, msg);

		/* destroy */
		call_delete(call);

		return;
	}

	/* change state */
	new_call_state(call, OSMO_CC_STATE_DISC_COLLISION);

	/* to lower layer */
	forward_to_ll(call, msg);
}

static void rel_collision(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* change state */
	if (call->state != OSMO_CC_STATE_IDLE)
		new_call_state(call, OSMO_CC_STATE_IDLE);

	/* drop message */
	osmo_cc_free_msg(msg);

	/* destroy */
	call_delete(call);
}

static void rej_ind_disc(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* change state */
	new_call_state(call, OSMO_CC_STATE_IDLE);

	/* change to REL_IND */
	msg->type = OSMO_CC_MSG_REL_IND;
	PDEBUG(DCC, DEBUG_INFO, "Changing message to %s.\n", osmo_cc_msg_value2name(msg->type));

	/* to upper layer */
	forward_to_ul(call, msg);

	/* destroy */
	call_delete(call);
}

static void rej_req_disc(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	/* change state */
	new_call_state(call, OSMO_CC_STATE_IDLE);

	/* change to REL_REQ */
	msg->type = OSMO_CC_MSG_REL_REQ;
	PDEBUG(DCC, DEBUG_INFO, "Changing message to %s.\n", osmo_cc_msg_value2name(msg->type));

	/* to lower layer */
	forward_to_ll(call, msg);

	/* destroy */
	call_delete(call);
}

static void rel_ind_other(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	// FIXME: does this event really happens in this state?
	// just to be safe we handle it
	/* if thereis no upper layer, we are done */
	if (call->upper_layer_released) {
		/* drop message */
		osmo_cc_free_msg(msg);

		/* destroy */
		call_delete(call);

		return;
	}

	/* change state */
	new_call_state(call, OSMO_CC_STATE_DISCONNECTING_IN);

	/* change to DISC_IND */
	msg->type = OSMO_CC_MSG_DISC_IND;
	PDEBUG(DCC, DEBUG_INFO, "Changing message to %s.\n", osmo_cc_msg_value2name(msg->type));
	call->lower_layer_released = 1;

	/* to upper layer */
	forward_to_ul(call, msg);
}

static void rel_req_other(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	// FIXME: does this event really happens in this state?
	// just to be safe we handle it
	/* if thereis no lower layer, we are done */
	if (call->lower_layer_released) {
		/* drop message */
		osmo_cc_free_msg(msg);

		/* destroy */
		call_delete(call);

		return;
	}

	/* change state */
	new_call_state(call, OSMO_CC_STATE_DISCONNECTING_OUT);

	/* change to DISC_REQ */
	msg->type = OSMO_CC_MSG_DISC_REQ;
	PDEBUG(DCC, DEBUG_INFO, "Changing message to %s.\n", osmo_cc_msg_value2name(msg->type));
	call->upper_layer_released = 1;

	/* to lower layer */
	forward_to_ll(call, msg);
}

#define SBIT(a) (1 << a)
#define ALL_STATES (~0)

static struct statemachine {
	uint32_t	states;
	int		type;
	void		(*action)(osmo_cc_call_t *call, osmo_cc_msg_t *msg);
} statemachine_list[] = {
	/* attachment states */
	{SBIT(OSMO_CC_STATE_ATTACH_SENT),
	 OSMO_CC_MSG_ATTACH_RSP, attach_rsp},
	{SBIT(OSMO_CC_STATE_ATTACH_OUT) | SBIT(OSMO_CC_STATE_ATTACH_SENT),
	 OSMO_CC_MSG_REL_REQ, attach_rel},
	{SBIT(OSMO_CC_STATE_IDLE),
	 OSMO_CC_MSG_ATTACH_REQ, attach_req},
	{SBIT(OSMO_CC_STATE_ATTACH_IN),
	 OSMO_CC_MSG_REL_REQ, attach_rel},

	/* call setup toward lower layer protocol */
	{SBIT(OSMO_CC_STATE_IDLE),
	 OSMO_CC_MSG_SETUP_REQ, setup_req},
	{SBIT(OSMO_CC_STATE_INIT_OUT),
	 OSMO_CC_MSG_SETUP_ACK_IND, setup_ack_ind},
	{SBIT(OSMO_CC_STATE_INIT_OUT) | SBIT(OSMO_CC_STATE_OVERLAP_OUT),
	 OSMO_CC_MSG_PROC_IND, proc_ind},
	{SBIT(OSMO_CC_STATE_INIT_OUT) | SBIT(OSMO_CC_STATE_OVERLAP_OUT) |
	 SBIT(OSMO_CC_STATE_PROCEEDING_OUT),
	 OSMO_CC_MSG_ALERT_IND, alert_ind},
	{SBIT(OSMO_CC_STATE_INIT_OUT) | SBIT(OSMO_CC_STATE_OVERLAP_OUT) |
	 SBIT(OSMO_CC_STATE_PROCEEDING_OUT) | SBIT(OSMO_CC_STATE_ALERTING_OUT),
	 OSMO_CC_MSG_SETUP_CNF, setup_cnf},
	{SBIT(OSMO_CC_STATE_OVERLAP_OUT) | SBIT(OSMO_CC_STATE_PROCEEDING_OUT) |
	 SBIT(OSMO_CC_STATE_ALERTING_OUT),
	 OSMO_CC_MSG_PROGRESS_IND, progress_ind},
	{SBIT(OSMO_CC_STATE_OVERLAP_OUT),
	 OSMO_CC_MSG_INFO_REQ, info_req},
	{SBIT(OSMO_CC_STATE_PROCEEDING_OUT) | SBIT(OSMO_CC_STATE_ALERTING_OUT),
	 OSMO_CC_MSG_NOTIFY_IND, notify_ind},
	{SBIT(OSMO_CC_STATE_CONNECTING_OUT),
	 OSMO_CC_MSG_SETUP_COMP_REQ, setup_comp_req},

	/* call setup from lower layer protocol */
	{SBIT(OSMO_CC_STATE_IDLE),
	 OSMO_CC_MSG_SETUP_IND, setup_ind},
	{SBIT(OSMO_CC_STATE_INIT_IN),
	 OSMO_CC_MSG_SETUP_ACK_REQ, setup_ack_req},
	{SBIT(OSMO_CC_STATE_INIT_IN) | SBIT(OSMO_CC_STATE_OVERLAP_IN),
	 OSMO_CC_MSG_PROC_REQ, proc_req},
	{SBIT(OSMO_CC_STATE_INIT_IN) | SBIT(OSMO_CC_STATE_OVERLAP_IN) |
	 SBIT(OSMO_CC_STATE_PROCEEDING_IN),
	 OSMO_CC_MSG_ALERT_REQ, alert_req},
	{SBIT(OSMO_CC_STATE_INIT_IN) | SBIT(OSMO_CC_STATE_OVERLAP_IN) |
	 SBIT(OSMO_CC_STATE_PROCEEDING_IN) | SBIT(OSMO_CC_STATE_ALERTING_IN),
	 OSMO_CC_MSG_SETUP_RSP, setup_rsp},
	{SBIT(OSMO_CC_STATE_OVERLAP_IN) | SBIT(OSMO_CC_STATE_PROCEEDING_IN) |
	 SBIT(OSMO_CC_STATE_ALERTING_IN),
	 OSMO_CC_MSG_PROGRESS_REQ, progress_req},
	{SBIT(OSMO_CC_STATE_OVERLAP_IN),
	 OSMO_CC_MSG_INFO_IND, info_ind},
	{SBIT(OSMO_CC_STATE_PROCEEDING_IN) | SBIT(OSMO_CC_STATE_ALERTING_IN),
	 OSMO_CC_MSG_NOTIFY_REQ, notify_req},
	{SBIT(OSMO_CC_STATE_CONNECTING_IN),
	 OSMO_CC_MSG_SETUP_COMP_IND, setup_comp_ind},

	/* active state */
	{SBIT(OSMO_CC_STATE_ACTIVE),
	 OSMO_CC_MSG_NOTIFY_IND, notify_ind},
	{SBIT(OSMO_CC_STATE_ACTIVE),
	 OSMO_CC_MSG_NOTIFY_REQ, notify_req},
	{SBIT(OSMO_CC_STATE_ACTIVE),
	 OSMO_CC_MSG_INFO_IND, info_ind},
	{SBIT(OSMO_CC_STATE_ACTIVE),
	 OSMO_CC_MSG_INFO_REQ, info_req},
	{SBIT(OSMO_CC_STATE_ACTIVE),
	 OSMO_CC_MSG_UPDATE_REQ, update_req},
	{SBIT(OSMO_CC_STATE_ACTIVE),
	 OSMO_CC_MSG_UPDATE_CNF, update_cnf},

	/* call release */
	{SBIT(OSMO_CC_STATE_INIT_OUT) | SBIT(OSMO_CC_STATE_INIT_IN) |
	 SBIT(OSMO_CC_STATE_OVERLAP_OUT) | SBIT(OSMO_CC_STATE_OVERLAP_IN) |
	 SBIT(OSMO_CC_STATE_PROCEEDING_OUT) | SBIT(OSMO_CC_STATE_PROCEEDING_IN) |
	 SBIT(OSMO_CC_STATE_ALERTING_OUT) | SBIT(OSMO_CC_STATE_ALERTING_IN) |
	 SBIT(OSMO_CC_STATE_CONNECTING_OUT) | SBIT(OSMO_CC_STATE_CONNECTING_IN) |
	 SBIT(OSMO_CC_STATE_ACTIVE),
	 OSMO_CC_MSG_DISC_REQ, disc_req},
	{SBIT(OSMO_CC_STATE_INIT_OUT) | SBIT(OSMO_CC_STATE_INIT_IN) |
	 SBIT(OSMO_CC_STATE_OVERLAP_OUT) | SBIT(OSMO_CC_STATE_OVERLAP_IN) |
	 SBIT(OSMO_CC_STATE_PROCEEDING_OUT) | SBIT(OSMO_CC_STATE_PROCEEDING_IN) |
	 SBIT(OSMO_CC_STATE_ALERTING_OUT) | SBIT(OSMO_CC_STATE_ALERTING_IN) |
	 SBIT(OSMO_CC_STATE_CONNECTING_OUT) | SBIT(OSMO_CC_STATE_CONNECTING_IN) |
	 SBIT(OSMO_CC_STATE_ACTIVE),
	 OSMO_CC_MSG_DISC_IND, disc_ind},
	{SBIT(OSMO_CC_STATE_INIT_OUT),
	 OSMO_CC_MSG_REJ_IND, rej_ind},
	{SBIT(OSMO_CC_STATE_INIT_IN),
	 OSMO_CC_MSG_REJ_REQ, rej_req},
	{SBIT(OSMO_CC_STATE_DISCONNECTING_OUT),
	 OSMO_CC_MSG_REL_IND, rel_ind},
	{SBIT(OSMO_CC_STATE_DISCONNECTING_IN),
	 OSMO_CC_MSG_REL_REQ, rel_req},
	{SBIT(OSMO_CC_STATE_RELEASING_OUT),
	 OSMO_CC_MSG_REL_CNF, rel_cnf},

	/* race condition where disconnect is received after disconnecting (disconnect collision) */
	{SBIT(OSMO_CC_STATE_DISCONNECTING_OUT),
	 OSMO_CC_MSG_DISC_IND, disc_collision_ind},
	{SBIT(OSMO_CC_STATE_DISCONNECTING_IN),
	 OSMO_CC_MSG_DISC_REQ, disc_collision_req},
	{SBIT(OSMO_CC_STATE_DISC_COLLISION),
	 OSMO_CC_MSG_REL_IND, rel_ind},
	{SBIT(OSMO_CC_STATE_DISC_COLLISION),
	 OSMO_CC_MSG_REL_REQ, rel_req},

	/* race condition where release is received after releasing (release collision) */
	{SBIT(OSMO_CC_STATE_RELEASING_OUT),
	 OSMO_CC_MSG_REL_IND, rel_collision},
	{SBIT(OSMO_CC_STATE_IDLE),
	 OSMO_CC_MSG_REL_REQ, rel_collision},

	/* race condition where reject is received after disconnecting */
	{SBIT(OSMO_CC_STATE_DISCONNECTING_OUT),
	 OSMO_CC_MSG_REJ_IND, rej_ind_disc},
	{SBIT(OSMO_CC_STATE_DISCONNECTING_IN),
	 OSMO_CC_MSG_REJ_REQ, rej_req_disc},

	/* turn release into disconnect, so release is possible in any state */
	{ALL_STATES,
	 OSMO_CC_MSG_REL_IND, rel_ind_other},
	{ALL_STATES,
	 OSMO_CC_MSG_REL_REQ, rel_req_other},
};

#define STATEMACHINE_LEN \
	(sizeof(statemachine_list) / sizeof(struct statemachine))

static void handle_msg(osmo_cc_call_t *call, osmo_cc_msg_t *msg)
{
	int i;

	/* Find function for current state and message */
	for (i = 0; i < (int)STATEMACHINE_LEN; i++)
		if ((msg->type == statemachine_list[i].type)
		 && ((1 << call->state) & statemachine_list[i].states))
			break;
	if (i == STATEMACHINE_LEN) {
		PDEBUG(DCC, DEBUG_INFO, "Message %s unhandled at state %s (callref %d)\n",
			osmo_cc_msg_value2name(msg->type), state_names[call->state], call->callref);
		osmo_cc_free_msg(msg);
		return;
	}

	PDEBUG(DCC, DEBUG_INFO, "Handle message %s at state %s (callref %d)\n",
		osmo_cc_msg_value2name(msg->type), state_names[call->state], call->callref);
	if (debuglevel <= DEBUG_INFO)
		osmo_cc_debug_ie(msg, DEBUG_INFO);
	statemachine_list[i].action(call, msg);
}

static int handle_call(osmo_cc_call_t *call)
{
	/* may handle only one message, since call may be destroyed when handling */
	if (call->sock_queue) {
		osmo_cc_msg_t *msg = osmo_cc_msg_list_dequeue(&call->sock_queue, NULL);
		handle_msg(call, msg);
		return 1;
	}

	return 0;
}

static int osmo_cc_handle_endpoint(osmo_cc_endpoint_t *ep)
{
	int work = 0;
	uint32_t callref;
	osmo_cc_call_t *call;

	/* may handle only one message, since call may be destroyed when handling */
	if (ep->ll_queue) {
		osmo_cc_msg_t *msg = osmo_cc_msg_list_dequeue(&ep->ll_queue, &callref);
		ep->ll_msg_cb(ep, callref, msg);
		work |= 1;
	}

	/* handle only one call, because it might have been removed */
	for (call = ep->call_list; call; call = call->next) {
		work |= handle_call(call);
		if (work)
			break;
	}

	return work;
}

/* main handler
 * note that it must be called in a loop (with other handlers) until no work was done
 */ 
int osmo_cc_handle(void)
{
	int work = 0;
	osmo_cc_endpoint_t *ep;

	for (ep = osmo_cc_endpoint_list; ep; ep = ep->next) {
		work |= osmo_cc_handle_endpoint(ep);
		work |= osmo_cc_handle_socket(&ep->os);
	}

	return work;
}

osmo_cc_call_t *osmo_cc_call_by_callref(osmo_cc_endpoint_t *ep, uint32_t callref)
{
	osmo_cc_call_t *call;

	if (!callref)
		return NULL;

	for (call = ep->call_list; call; call = call->next) {
		if (call->callref == callref) {
			return call;
		}
	}

	return NULL;
}


void osmo_cc_ll_msg(osmo_cc_endpoint_t *ep, uint32_t callref, osmo_cc_msg_t *msg)
{
	osmo_cc_call_t *call;

	if (!(msg->type & 1)) {
		PDEBUG(DCC, DEBUG_ERROR, "Received message from lower layer that is not an _IND nor _CNF, please fix!\n");
		osmo_cc_free_msg(msg);
		return;
	}

	call = osmo_cc_call_by_callref(ep, callref);
	if (call) {
		/* complete cause */
		osmo_cc_convert_cause_msg(msg);
		handle_msg(call, msg);
		return;
	}

	/* if no ref exists */
}

/* message from upper layer (socket) */
void osmo_cc_ul_msg(void *priv, uint32_t callref, osmo_cc_msg_t *msg)
{
	osmo_cc_endpoint_t *ep = priv;
	osmo_cc_call_t *call;

	if ((msg->type & 1)) {
		PDEBUG(DCC, DEBUG_ERROR, "Received message from socket that is not an _REQ nor _RSP, please fix!\n");
		osmo_cc_free_msg(msg);
		return;
	}

	call = osmo_cc_call_by_callref(ep, callref);
	if (call) {
		/* if we are not in INIT-IN state, we change a CC-REJ-REQ into CC-REL_REQ.
		 * this happens, if the socket fails.
		 */
		if (call->state != OSMO_CC_STATE_INIT_IN
		 && msg->type == OSMO_CC_MSG_REJ_REQ)
			msg->type = OSMO_CC_MSG_REL_REQ;

		osmo_cc_msg_list_enqueue(&call->sock_queue, msg, call->callref);
		return;
	}

	/* if no ref exists */

	/* reject and release are ignored */
	if (msg->type == OSMO_CC_MSG_REJ_REQ
	 || msg->type == OSMO_CC_MSG_REL_REQ) {
		osmo_cc_free_msg(msg);
		return;
	}

	/* reject if not a setup/attach or release message */
	if (msg->type != OSMO_CC_MSG_SETUP_REQ
	 && msg->type != OSMO_CC_MSG_ATTACH_REQ) {
		sock_reject_msg(&ep->os, callref, ep->serving_location, 0, OSMO_CC_ISDN_CAUSE_INVAL_CALLREF, 0);
		osmo_cc_free_msg(msg);
		return;
	}

	/* create call instance with one socket reference */
	call = call_new(ep, callref);

	osmo_cc_msg_list_enqueue(&call->sock_queue, msg, call->callref);
}

static void osmo_cc_help_name(void)
{
	printf("Name options:\n\n");

	printf("name <name>\n");

	printf("Allows to override endpoint name given by application.\n");
}

static int osmo_cc_set_name(osmo_cc_endpoint_t *ep, const char *text)
{
	if (!strncasecmp(text, "name", 4)) {
		text += 4;
		/* remove spaces after keyword */
		while (*text) {
			if (*text > 32)
				break;
			text++;
		}
	} else {
		PDEBUG(DCC, DEBUG_ERROR, "Invalid name definition '%s'\n", text);
		return -EINVAL;
	}

	free((char *)ep->local_name);
	ep->local_name = strdup(text);

	return 0;
}

static void osmo_cc_help_address(void)
{
	printf("Address options:\n\n");

	printf("local <IPv4 address>:<port>\n");
	printf("local [<IPv6 address>]:<port>\n");
	printf("remote <IPv4 address>:<port>\n");
	printf("remote [<IPv6 address>]:<port>\n\n");
	printf("remote auto\n\n");
	printf("remote none\n\n");

	printf("These options can be used to define local and remote IP and port for the socket\n");
	printf("interface. Note that IPv6 addresses must be enclosed by '[' and ']'.\n\n");

	printf("If no local address was given, the IPv4 loopback IP and port %d is used. If\n", OSMO_CC_DEFAULT_PORT);
	printf("this port is already in use, the first free higher port is used.\n\n");

	printf("If no remote address is given, the local IP is used. If the local port is %d,\n", OSMO_CC_DEFAULT_PORT);
	printf("the remote port will be %d. If not, the remote port will be %d. This way it is\n", OSMO_CC_DEFAULT_PORT + 1, OSMO_CC_DEFAULT_PORT);
	printf("possible to link two interfaces without any IP configuration required.\n\n");

	printf("Use 'remote auto' to enable and 'remote none' to disable. This can be useful to\n");
	printf("override application default.\n\n");
}

static int osmo_cc_set_address(osmo_cc_endpoint_t *ep, const char *text)
{
	const char **address_p, **host_p;
	uint16_t *port_p;
	int local = 0;
	int rc;

	if (!strncasecmp(text, "local", 5)) {
		text += 5;
		/* remove spaces after keyword */
		while (*text) {
			if (*text > 32)
				break;
			text++;
		}
		address_p = &ep->local_address;
		host_p = &ep->local_host;
		port_p = &ep->local_port;
		local = 1;
	} else if (!strncasecmp(text, "remote", 6)) {
		text += 6;
		/* remove spaces after keyword */
		while (*text) {
			if (*text > 32)
				break;
			text++;
		}
		if (!strcasecmp(text, "auto")) {
			PDEBUG(DCC, DEBUG_DEBUG, "setting automatic remote peer selection\n");
			ep->remote_auto = 1;
			return 0;
		}
		if (!strcasecmp(text, "none")) {
			PDEBUG(DCC, DEBUG_DEBUG, "disable automatic remote peer selection\n");
			ep->remote_auto = 0;
			return 0;
		}
		ep->remote_auto = 0;
		address_p = &ep->remote_address;
		host_p = &ep->remote_host;
		port_p = &ep->remote_port;
	} else {
		PDEBUG(DCC, DEBUG_ERROR, "Invalid local or remote address definition '%s'\n", text);
		return -EINVAL;
	}

	if (*address_p) {
		free((char *)*address_p);
		*address_p = NULL;
	}
	if (*host_p) {
		free((char *)*host_p);
		*host_p = NULL;
	}
	rc = split_address(text, host_p, port_p);
	if (rc < 0) {
		/* unset, so that this is not treated with free() */
		*host_p = NULL;
		return rc;
	}
	*address_p = strdup(text);
	*host_p = strdup(*host_p);

	if (local) {
		enum osmo_cc_session_addrtype addrtype;
		addrtype = osmo_cc_address_type(*host_p);
		if (addrtype == osmo_cc_session_addrtype_unknown) {
			PDEBUG(DCC, DEBUG_ERROR, "Given local address '%s' is invalid.\n", *host_p);
			return -EINVAL;
		}
		osmo_cc_set_local_peer(&ep->session_config, osmo_cc_session_nettype_inet, addrtype, *host_p);
		return 0;
	}

	return 0;
}

static void osmo_cc_help_rtp(void)
{
	printf("RTP options:\n\n");

	printf("rtp-peer <IPv4 address>\n");
	printf("rtp-peer <IPv6 address>\n");
	printf("rtp-ports <first> <last>\n\n");

	printf("These options can be used to alter the local IP and port range for RTP traffic.\n");
	printf("By default the local peer is used, which is loopback by default. To connect\n");
	printf("interfaces, between machines, local machine's IP must be given.\n\n");
}

static int osmo_cc_set_rtp(osmo_cc_endpoint_t *ep, const char *text)
{
	int peer = 0, ports = 0;

	if (!strncasecmp(text, "rtp-peer", 8)) {
		text += 8;
		peer = 1;
	} else if (!strncasecmp(text, "rtp-ports", 9)) {
		text += 9;
		ports = 1;
	} else {
		PDEBUG(DCC, DEBUG_ERROR, "Invalid RTP definition '%s'\n", text);
		return -EINVAL;
	}

	/* remove spaces after keyword */
	while (*text) {
		if (*text > 32)
			break;
		text++;
	}

	if (peer) {
		enum osmo_cc_session_addrtype addrtype;
		addrtype = osmo_cc_address_type(text);
		if (addrtype == osmo_cc_session_addrtype_unknown) {
			PDEBUG(DCC, DEBUG_ERROR, "Given RTP address '%s' is invalid.\n", text);
			return -EINVAL;
		}
		osmo_cc_set_local_peer(&ep->session_config, osmo_cc_session_nettype_inet, addrtype, text);
		return 0;
	}

	if (ports) {
		int from = 0, to = 0;

		/* from port */
		while (*text > ' ') {
			if (*text < '0' || *text > '9') {
				PDEBUG(DCC, DEBUG_ERROR, "Given 'from' port in '%s' is invalid.\n", text);
				return -EINVAL;
			}
			from = from * 10 + *text - '0';
			text++;
		}

		/* remove spaces after keyword */
		while (*text) {
			if (*text > 32)
				break;
			text++;
		}

		/* to port */
		while (*text > ' ') {
			if (*text < '0' || *text > '9') {
				PDEBUG(DCC, DEBUG_ERROR, "Given 'to' port in '%s' is invalid.\n", text);
				return -EINVAL;
			}
			to = to * 10 + *text - '0';
			text++;
		}

		osmo_cc_set_rtp_ports(&ep->session_config, from, to);
		return 0;
	}

	return -EINVAL;
}

void osmo_cc_help(void)
{
	osmo_cc_help_name();
	osmo_cc_help_address();
	osmo_cc_help_rtp();
	osmo_cc_help_screen();
}

/* create a new endpoint instance */
int osmo_cc_new(osmo_cc_endpoint_t *ep, const char *version, const char *name, uint8_t serving_location, void (*ll_msg_cb)(osmo_cc_endpoint_t *ep, uint32_t callref, osmo_cc_msg_t *msg), void (*ul_msg_cb)(osmo_cc_call_t *call, osmo_cc_msg_t *msg), void *priv, int argc, const char *argv[])
{
	osmo_cc_endpoint_t **epp;
	int rc;
	int i;

	PDEBUG(DCC, DEBUG_DEBUG, "Creating new endpoint instance.\n");

	if (!!strcmp(version, OSMO_CC_VERSION)) {
		PDEBUG(DCC, DEBUG_ERROR, "Application was compiled for different Osmo-CC version.\n");
		return OSMO_CC_RC_VERSION_MISMATCH;
	}

	memset(ep, 0, sizeof(*ep));

	/* attach to list */
	epp = &osmo_cc_endpoint_list;
	while (*epp)
		epp = &((*epp)->next);
	*epp = ep;

	if (name)
		ep->local_name = strdup(name);
	ep->ll_msg_cb = ll_msg_cb;
	ep->ul_msg_cb = ul_msg_cb;
	ep->serving_location = serving_location;
	ep->priv = priv;

	osmo_cc_set_local_peer(&ep->session_config, osmo_cc_session_nettype_inet, osmo_cc_session_addrtype_ipv4, "127.0.0.1");
	osmo_cc_set_rtp_ports(&ep->session_config, 16384, 32767);

	/* apply args */
	for (i = 0; i < argc; i++) {
		if (!strncasecmp(argv[i], "name", 4)) {
			rc = osmo_cc_set_name(ep, argv[i]);
			if (rc < 0) {
				return rc;
			}
		} else
		if (!strncasecmp(argv[i], "local", 5)) {
			rc = osmo_cc_set_address(ep, argv[i]);
			if (rc < 0) {
				return rc;
			}
		} else
		if (!strncasecmp(argv[i], "remote", 6)) {
			rc = osmo_cc_set_address(ep, argv[i]);
			if (rc < 0) {
				return rc;
			}
		} else
		if (!strncasecmp(argv[i], "rtp", 3)) {
			rc = osmo_cc_set_rtp(ep, argv[i]);
			if (rc < 0) {
				return rc;
			}
		} else
		if (!strncasecmp(argv[i], "screen", 6)) {
			rc = osmo_cc_add_screen(ep, argv[i]);
			if (rc < 0) {
				return rc;
			}
		} else {
			PDEBUG(DCC, DEBUG_ERROR, "Unknown osmo-cc argument \"%s\"\n", argv[i]);
			return -EINVAL;
		}
	}

	/* open socket */
	if (!ul_msg_cb) {
		char address[256];
		const char *host;
		uint16_t port;
		enum osmo_cc_session_addrtype addrtype;

		host = ep->local_host;
		port = ep->local_port;
		if (!host) {
			host = "127.0.0.1";
			PDEBUG(DCC, DEBUG_DEBUG, "No local peer set, using default \"%s\"\n", host);
		}
		rc = osmo_cc_open_socket(&ep->os, host, port, ep, osmo_cc_ul_msg, serving_location);
		if (rc < 0) {
			return rc;
		}
		port = rc;
		if (!ep->local_host) {
			ep->local_host = strdup(host);
			/* create address string */
			addrtype = osmo_cc_address_type(host);
			if (addrtype == osmo_cc_session_addrtype_ipv6)
				sprintf(address, "[%s]:%d", host, port);
			else
				sprintf(address, "%s:%d", host, port);
			ep->local_address = strdup(address);
		}
		ep->local_port = port;
		/* auto configure */
		if (ep->remote_auto) {
			free((char *)ep->remote_host);
			ep->remote_host = strdup(ep->local_host);
			PDEBUG(DCC, DEBUG_DEBUG, "Remote peer set to auto, using local peer's host \"%s\" for remote peer.\n", ep->remote_host);
			if (rc == OSMO_CC_DEFAULT_PORT)
				ep->remote_port = OSMO_CC_DEFAULT_PORT + 1;
			else
				ep->remote_port = OSMO_CC_DEFAULT_PORT;
			PDEBUG(DCC, DEBUG_DEBUG, " -> Using remote port %d.\n", ep->remote_port);
			/* create address string */
			free((char *)ep->remote_address);
			addrtype = osmo_cc_address_type(ep->remote_host);
			if (addrtype == osmo_cc_session_addrtype_ipv6)
				sprintf(address, "[%s]:%d", ep->remote_host, ep->remote_port);
			else
				sprintf(address, "%s:%d", ep->remote_host, ep->remote_port);
			ep->remote_address = strdup(address);
		}
		/* attach to remote host */
		timer_init(&ep->attach_timer, send_attach_ind, ep);
		if (ep->remote_host) {
			send_attach_ind(&ep->attach_timer);
		}
	}

	return 0;
}

/* destroy an endpoint instance */
void osmo_cc_delete(osmo_cc_endpoint_t *ep)
{
	osmo_cc_endpoint_t **epp;

	PDEBUG(DCC, DEBUG_DEBUG, "Destroying endpoint instance.\n");

	/* detach from list >*/
	epp = &osmo_cc_endpoint_list;
	while (*epp && *epp != ep)
		epp = &((*epp)->next);
	if (*epp)
		*epp = ep->next;

	/* remove timer */
	timer_exit(&ep->attach_timer);

	/* flush screen lists */
	osmo_cc_flush_screen(ep->screen_calling_in);
	osmo_cc_flush_screen(ep->screen_called_in);
	osmo_cc_flush_screen(ep->screen_calling_out);
	osmo_cc_flush_screen(ep->screen_called_out);

	/* free local and remote peer */
	free((char *)ep->local_name);
	free((char *)ep->local_address);
	free((char *)ep->local_host);
	free((char *)ep->remote_address);
	free((char *)ep->remote_host);

	/* destroying all child callesses (calls) */
	while(ep->call_list)
		call_delete(ep->call_list);

	/* flush message queue */
	while(ep->ll_queue) {
		osmo_cc_msg_t *msg = osmo_cc_msg_list_dequeue(&ep->ll_queue, NULL);
		osmo_cc_free_msg(msg);
	}

	/* remove socket */
	osmo_cc_close_socket(&ep->os);

	memset(ep, 0, sizeof(*ep));
}

/* create new call instance */
osmo_cc_call_t *osmo_cc_call_new(osmo_cc_endpoint_t *ep)
{
	return call_new(ep, osmo_cc_new_callref());
}

/* destroy call instance */
void osmo_cc_call_delete(osmo_cc_call_t *call)
{
	call_delete(call);
}

/* check valid IP and return address type (protocol) */
enum osmo_cc_session_addrtype osmo_cc_address_type(const char *address)
{
	struct sockaddr_storage sa;
	int rc;

	rc = inet_pton(AF_INET, address, &sa);
	if (rc > 0)
		return osmo_cc_session_addrtype_ipv4;
	rc = inet_pton(AF_INET6, address, &sa);
	if (rc > 0)
		return osmo_cc_session_addrtype_ipv6;

	return osmo_cc_session_addrtype_unknown;
}

/* get host from address */
const char *osmo_cc_host_of_address(const char *address)
{
	static char host[256];
	char *p;

	if (strlen(address) >= sizeof(host)) {
		PDEBUG(DCC, DEBUG_ERROR, "String way too long!\n");
		return NULL;
	}

	if (address[0] == '[' && (p = strchr(address, ']'))) {
		memcpy(host, address + 1, p - address - 1);
		host[p - address - 1] = '\0';
		return host;
	}

	strcpy(host, address);
	if ((p = strchr(host, ':')))
		*p = '\0';

	return host;
}

/* get port from address */
const char *osmo_cc_port_of_address(const char *address)
{
	const char *p;
	int i;

	if (address[0] == '[' && (p = strchr(address, ']')))
		address = p + 1;
	
	if (!(p = strchr(address, ':')))
		return NULL;
	p++;

	/* check for zero */
	if (p[0] == '0')
		return NULL;

	/* check for digits */
	for (i = 0; i < (int)strlen(p); i++) {
		if (p[i] < '0' || p[i] > '9')
			return NULL;
	}

	/* check for magnitude */
	if (atoi(p) > 65535)
		return NULL;

	return p;
}

