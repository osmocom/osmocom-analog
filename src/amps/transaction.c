/* C-Netz transaction handling
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
#include "../common/debug.h"
#include "../common/timer.h"
#include "amps.h"
//#include "database.h"

static const char *trans_state_name(int state)
{
	switch (state) {
	case 0:
		return "IDLE";
	case TRANS_REGISTER_ACK:
		return "REGISTER ACK";
	case TRANS_REGISTER_ACK_SEND:
		return "REGISTER ACK SEND";
	case TRANS_CALL_MO_ASSIGN:
		return "CALL ASSIGN MOBILE ORIGINATING";
	case TRANS_CALL_MO_ASSIGN_SEND:
		return "CALL ASSIGN MOBILE ORIGINATING SEND";
	case TRANS_CALL_MT_ASSIGN:
		return "CALL ASSIGN MOBILE TERMINATING";
	case TRANS_CALL_MT_ASSIGN_SEND:
		return "CALL ASSIGN MOBILE TERMINATING SEND";
	case TRANS_CALL_MT_ALERT:
		return "CALL ALERT MOBILE TERMINATING";
	case TRANS_CALL_MT_ALERT_SEND:
		return "CALL ALERT MOBILE TERMINATING SEND";
	case TRANS_CALL_REJECT:
		return "CALL REJECT";
	case TRANS_CALL_REJECT_SEND:
		return "CALL REJECT SEND";
	case TRANS_CALL:
		return "CALL";
	case TRANS_CALL_RELEASE:
		return "CALL RELEASE";
	case TRANS_CALL_RELEASE_SEND:
		return "CALL RELEASE SEND";
	case TRANS_PAGE:
		return "PAGE";
	case TRANS_PAGE_SEND:
		return "PAGE SEND";
	case TRANS_PAGE_REPLY:
		return "PAGE REPLY";
	default:
		return "<invald transaction state>";
	}
}

/* create transaction */
transaction_t *create_transaction(amps_t *amps, enum amps_trans_state state, uint32_t min1, uint16_t min2, uint8_t msg_type, uint8_t ordq, uint8_t order, uint16_t chan)
{
	transaction_t *trans;

	/* search transaction for this subsriber */
	trans = search_transaction_number(amps, min1, min2);
	if (trans) {
		const char *number = amps_min2number(trans->min1, trans->min2);
		PDEBUG(DTRANS, DEBUG_NOTICE, "Found alredy pending transaction for subscriber '%s', deleting!\n", number);
		destroy_transaction(trans);
	}

	trans = calloc(1, sizeof(*trans));
	if (!trans) {
		PDEBUG(DTRANS, DEBUG_ERROR, "No memory!\n");
		return NULL;
	}

	timer_init(&trans->timer, transaction_timeout, trans);

	trans_new_state(trans, state);
	trans->min1 = min1;
	trans->min2 = min2;
	trans->msg_type = msg_type;
	trans->ordq = ordq;
	trans->order = order;
	trans->chan = chan;

	const char *number = amps_min2number(trans->min1, trans->min2);
	PDEBUG(DTRANS, DEBUG_INFO, "Created transaction '%s' for subscriber '%s'\n", number, trans_state_name(state));

	link_transaction(trans, amps);

	/* update database: now busy */
//	update_db(amps, min1, min2, 1, 0);

	return trans;
}

/* destroy transaction */
void destroy_transaction(transaction_t *trans)
{
	/* update database: now idle */
//	update_db(trans->amps, trans->min1, trans->min2, 0, trans->ma_failed);

	unlink_transaction(trans);
	
	const char *number = amps_min2number(trans->min1, trans->min2);
	PDEBUG(DTRANS, DEBUG_INFO, "Destroying transaction for subscriber '%s'\n", number);

	timer_exit(&trans->timer);

	trans_new_state(trans, 0);

	free(trans);
}

/* link transaction to list */
void link_transaction(transaction_t *trans, amps_t *amps)
{
	transaction_t **transp;

	/* attach to end of list, so first transaction is served first */
	PDEBUG(DTRANS, DEBUG_DEBUG, "Linking transaction %p to amps %p\n", trans, amps);
	trans->amps = amps;
	trans->next = NULL;
	transp = &amps->trans_list;
	while (*transp)
		transp = &((*transp)->next);
	*transp = trans;
}

/* unlink transaction from list */
void unlink_transaction(transaction_t *trans)
{
	transaction_t **transp;

	/* unlink */
	PDEBUG(DTRANS, DEBUG_DEBUG, "Unlinking transaction %p from amps %p\n", trans, trans->amps);
	transp = &trans->amps->trans_list;
	while (*transp && *transp != trans)
		transp = &((*transp)->next);
	if (!(*transp)) {
		PDEBUG(DTRANS, DEBUG_ERROR, "Transaction not in list, please fix!!\n");
		abort();
	}
	*transp = trans->next;
	trans->amps = NULL;
}

transaction_t *search_transaction_number(amps_t *amps, uint32_t min1, uint16_t min2)
{
	transaction_t *trans = amps->trans_list;

	while (trans) {
		if (trans->min1 == min1
		 && trans->min2 == min2) {
			const char *number = amps_min2number(trans->min1, trans->min2);
			PDEBUG(DTRANS, DEBUG_DEBUG, "Found transaction for subscriber '%s'\n", number);
			return trans;
		}
		trans = trans->next;
	}

	return NULL;
}

transaction_t *search_transaction_callref(amps_t *amps, int callref)
{
	transaction_t *trans = amps->trans_list;

	/* just in case, this should not happen */
	if (!callref)
		return NULL;
	while (trans) {
		if (trans->callref == callref) {
			const char *number = amps_min2number(trans->min1, trans->min2);
			PDEBUG(DTRANS, DEBUG_DEBUG, "Found transaction for subscriber '%s'\n", number);
			return trans;
		}
		trans = trans->next;
	}

	return NULL;
}

void trans_new_state(transaction_t *trans, int state)
{
	PDEBUG(DTRANS, DEBUG_INFO, "Transaction state %s -> %s\n", trans_state_name(trans->state), trans_state_name(state));
	trans->state = state;
}

void amps_flush_other_transactions(amps_t *amps, transaction_t *trans)
{
	/* flush after this very trans */
	while (trans->next) {
		PDEBUG(DTRANS, DEBUG_NOTICE, "Kicking other pending transaction\n");
		destroy_transaction(trans->next);
	}
	/* flush before this very trans */
	while (amps->trans_list != trans) {
		PDEBUG(DTRANS, DEBUG_NOTICE, "Kicking other pending transaction\n");
		destroy_transaction(amps->trans_list);
	}
}

