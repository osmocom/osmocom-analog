/* OSMO-CC Processing: convert causes
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

#include <stdint.h>
#include <arpa/inet.h>
#include "message.h"
#include "cause.h"

/* stolen from freeswitch, did some corrections */
/* map sip responses to QSIG cause codes ala RFC4497 section 8.4.4 */
static uint8_t status2isdn_cause(uint16_t status)
{
	switch (status) {
	case 200:
		return 16; //SWITCH_CAUSE_NORMAL_CLEARING;
	case 401:
	case 402:
	case 403:
	case 407:
	case 603:
		return 21; //SWITCH_CAUSE_CALL_REJECTED;
	case 404:
	case 485:
	case 604:
		return 1; //SWITCH_CAUSE_UNALLOCATED_NUMBER;
	case 408:
	case 504:
		return 102; //SWITCH_CAUSE_RECOVERY_ON_TIMER_EXPIRE;
	case 410:
		return 22; //SWITCH_CAUSE_NUMBER_CHANGED;
	case 413:
	case 414:
	case 416:
	case 420:
	case 421:
	case 423:
	case 505:
	case 513:
		return 127; //SWITCH_CAUSE_INTERWORKING;
	case 480:
		return 18; //SWITCH_CAUSE_NO_USER_RESPONSE;
	case 400:
	case 481:
	case 500:
	case 503:
		return 41; //SWITCH_CAUSE_NORMAL_TEMPORARY_FAILURE;
	case 486:
	case 600:
		return 17; //SWITCH_CAUSE_USER_BUSY;
	case 484:
		return 28; //SWITCH_CAUSE_INVALID_NUMBER_FORMAT;
	case 488:
	case 606:
		return 65; //SWITCH_CAUSE_BERER_CAPABILITY_NOT_IMPLEMENTED;
	case 502:
		return 38; //SWITCH_CAUSE_NETWORK_OUT_OF_ORDER;
	case 405:
		return 63; //SWITCH_CAUSE_SERVICE_UNAVAILABLE;
	case 406:
	case 415:
	case 501:
		return 79; //SWITCH_CAUSE_SERVICE_NOT_IMPLEMENTED;
	case 482:
	case 483:
		return 25; //SWITCH_CAUSE_EXCHANGE_ROUTING_ERROR;
	case 487:
		return 31; //??? SWITCH_CAUSE_ORIGINATOR_CANCEL; (not specified)
	default:
		return 31; //SWITCH_CAUSE_NORMAL_UNSPECIFIED;
	}
}

static uint16_t isdn2status_cause(uint8_t cause, uint8_t location)
{
	switch (cause) {
	case 1:
		return 404;
	case 2:
		return 404;
	case 3:
		return 404;
	case 17:
		return 486;
	case 18:
		return 408;
	case 19:
		return 480;
	case 20:
		return 480;
	case 21:
		if (location == OSMO_CC_LOCATION_USER)
			return 603;
		return 403;
	case 22:
		//return 301;
		return 410;
	case 23:
		return 410;
	case 26:
		return 404;
	case 27:
		return 502;
	case 28:
		return 484;
	case 29:
		return 501;
	case 31:
		return 480;
	case 34:
		return 503;
	case 38:
		return 503;
	case 41:
		return 503;
	case 42:
		return 503;
	case 47:
		return 503;
	case 55:
		return 403;
	case 57:
		return 403;
	case 58:
		return 503;
	case 65:
		return 488;
	case 69:
		return 501;
	case 70:
		return 488;
	case 79:
		return 501;
	case 87:
		return 403;
	case 88:
		return 503;
	case 102:
		return 504;
	case 111:
		return 500;
	case 127:
		return 500;
	default:
		return 468;
	}
}

static uint8_t socket2isdn_cause(uint8_t sock)
{
	switch (sock) {
	case OSMO_CC_SOCKET_CAUSE_FAILED:
		return 47;
	case OSMO_CC_SOCKET_CAUSE_BROKEN_PIPE:
		return 41;
	case OSMO_CC_SOCKET_CAUSE_VERSION_MISMATCH:
		return 38;
	case OSMO_CC_SOCKET_CAUSE_TIMEOUT:
		return 41;
	default:
		return 31;
	}
}

void osmo_cc_convert_cause(struct osmo_cc_ie_cause *cause)
{
	/* complete cause, from socket cause */
	if (cause->socket_cause && cause->isdn_cause == 0 && ntohs(cause->sip_cause_networkorder) == 0)
		cause->isdn_cause = socket2isdn_cause(cause->socket_cause);

	/* convert ISDN cause to SIP cause */
	if (cause->isdn_cause && ntohs(cause->sip_cause_networkorder) == 0) {
		cause->sip_cause_networkorder = htons(isdn2status_cause(cause->isdn_cause, cause->location));
	}

	/* convert SIP cause to ISDN cause */
	if (ntohs(cause->sip_cause_networkorder) && cause->isdn_cause == 0) {
		cause->isdn_cause = status2isdn_cause(ntohs(cause->sip_cause_networkorder));
	}

	/* no cause at all: use Normal Call Clearing */
	if (cause->isdn_cause == 0 && ntohs(cause->sip_cause_networkorder) == 0) {
		cause->isdn_cause = OSMO_CC_ISDN_CAUSE_NORM_CALL_CLEAR;
		cause->sip_cause_networkorder = htons(486);
	}
}

void osmo_cc_convert_cause_msg(osmo_cc_msg_t *msg)
{
	void *ie;
	uint8_t type;
	uint16_t length;
	void *value;

	/* search for (all) cause IE and convert the values, if needed */
	ie = msg->data;
	while ((value = osmo_cc_msg_sep_ie(msg, &ie, &type, &length))) {
		if (type == OSMO_CC_IE_CAUSE && length >= sizeof(struct osmo_cc_ie_cause)) {
			osmo_cc_convert_cause(value);
		}
	}
}

uint8_t osmo_cc_collect_cause(uint8_t old_cause, uint8_t new_cause)
{
	/* first cause */
	if (old_cause == 0)
		return new_cause;

	/* first prio: return 17 */
	if (old_cause == OSMO_CC_ISDN_CAUSE_USER_BUSY
	 || new_cause == OSMO_CC_ISDN_CAUSE_USER_BUSY)
		return OSMO_CC_ISDN_CAUSE_USER_BUSY;

	/* second prio: return 21 */
	if (old_cause == OSMO_CC_ISDN_CAUSE_CALL_REJECTED
	 || new_cause == OSMO_CC_ISDN_CAUSE_CALL_REJECTED)
		return OSMO_CC_ISDN_CAUSE_CALL_REJECTED;

	/* third prio: return other than 88 and 18 (what ever was first) */
	if (old_cause != OSMO_CC_ISDN_CAUSE_INCOMPAT_DEST
	 && old_cause != OSMO_CC_ISDN_CAUSE_USER_NOTRESPOND)
		return old_cause;
	if (new_cause != OSMO_CC_ISDN_CAUSE_INCOMPAT_DEST
	 && new_cause != OSMO_CC_ISDN_CAUSE_USER_NOTRESPOND)
		return new_cause;

	/* fourth prio: return 88 */
	if (old_cause == OSMO_CC_ISDN_CAUSE_INCOMPAT_DEST
	 || new_cause == OSMO_CC_ISDN_CAUSE_INCOMPAT_DEST)
		return OSMO_CC_ISDN_CAUSE_INCOMPAT_DEST;

	/* fith prio: return 18 */
	return OSMO_CC_ISDN_CAUSE_USER_NOTRESPOND;
}

