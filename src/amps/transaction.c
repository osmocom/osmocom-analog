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
#include "../libsample/sample.h"
#include "../liblogging/logging.h"
#include "../libmobile/call.h"
#include "../libmobile/cause.h"
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
		return "MO CALL ASSIGNMENT";
	case TRANS_CALL_MO_ASSIGN_SEND:
		return "MO CALL ASSIGNMENT SENDING";
	case TRANS_CALL_MO_ASSIGN_CONFIRM:
		return "MO CALL ASSIGNMENT WAIT CONFIRM";
	case TRANS_CALL_MT_ASSIGN:
		return "MT CALL ASSIGNMENT";
	case TRANS_CALL_MT_ASSIGN_SEND:
		return "MT CALL ASSIGNMENT SENDING";
	case TRANS_CALL_MT_ASSIGN_CONFIRM:
		return "MT CALL ASSIGNMENT WAIT CONFIRM";
	case TRANS_CALL_MT_ALERT:
		return "MT CALL ALERT";
	case TRANS_CALL_MT_ALERT_SEND:
		return "MT CALL ALERT SENDING";
	case TRANS_CALL_MT_ALERT_CONFIRM:
		return "MT CALL ALERT WAIT CONFIRM";
	case TRANS_CALL_MT_ANSWER_WAIT:
		return "MT CALL ANSWER WAIT";
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
		return "<invalid transaction state>";
	}
}

const char *trans_short_state_name(int state)
{
	switch (state) {
	case 0:
		return "IDLE";
	case TRANS_REGISTER_ACK:
	case TRANS_REGISTER_ACK_SEND:
		return "REGISTER";
	case TRANS_CALL_MO_ASSIGN:
	case TRANS_CALL_MO_ASSIGN_SEND:
	case TRANS_CALL_MO_ASSIGN_CONFIRM:
	case TRANS_CALL_MT_ASSIGN:
	case TRANS_CALL_MT_ASSIGN_SEND:
	case TRANS_CALL_MT_ASSIGN_CONFIRM:
		return "ASSIGN";
	case TRANS_CALL_MT_ALERT:
	case TRANS_CALL_MT_ALERT_SEND:
	case TRANS_CALL_MT_ALERT_CONFIRM:
	case TRANS_CALL_MT_ANSWER_WAIT:
		return "ALERT";
	case TRANS_CALL_REJECT:
	case TRANS_CALL_REJECT_SEND:
		return "REJECT";
	case TRANS_CALL:
		return "CALL";
	case TRANS_CALL_RELEASE:
	case TRANS_CALL_RELEASE_SEND:
		return "RELEASE";
	case TRANS_PAGE:
	case TRANS_PAGE_SEND:
	case TRANS_PAGE_REPLY:
		return "PAGE";
	default:
		return "<invalid transaction state>";
	}
}

/* create transaction */
transaction_t *create_transaction(amps_t *amps, enum amps_trans_state state, uint32_t min1, uint16_t min2, uint32_t esn, uint8_t msg_type, uint8_t ordq, uint8_t order, uint16_t chan)
{
	sender_t *sender;
	transaction_t *trans = NULL;
	amps_t *search_amps;

	/* search transaction for this subscriber */
	for (sender = sender_head; sender; sender = sender->next) {
		search_amps = (amps_t *) sender;
		/* search transaction for this callref */
		trans = search_transaction_number(search_amps, min1, min2);
		if (trans)
			break;
	}
	if (trans) {
		const char *number = amps_min2number(trans->min1, trans->min2);
		int old_callref = trans->callref;
		amps_t *old_amps = trans->amps;
		LOGP(DTRANS, LOGL_NOTICE, "Found already pending transaction for subscriber '%s', deleting!\n", number);
		destroy_transaction(trans);
		if (old_amps) /* should be... */
			amps_go_idle(old_amps);
		if (old_callref)
			call_up_release(old_callref, CAUSE_NORMAL);
	}

	trans = calloc(1, sizeof(*trans));
	if (!trans) {
		LOGP(DTRANS, LOGL_ERROR, "No memory!\n");
		return NULL;
	}

	osmo_timer_setup(&trans->timer, transaction_timeout, trans);

	trans_new_state(trans, state);
	trans->min1 = min1;
	trans->min2 = min2;
	trans->esn = esn;
	trans->msg_type = msg_type;
	trans->ordq = ordq;
	trans->order = order;
	trans->chan = chan;

	const char *number = amps_min2number(trans->min1, trans->min2);
	LOGP(DTRANS, LOGL_INFO, "Created transaction for subscriber '%s'\n", number);

	link_transaction(trans, amps);

	return trans;
}

/* destroy transaction */
void destroy_transaction(transaction_t *trans)
{
	unlink_transaction(trans);
	
	const char *number = amps_min2number(trans->min1, trans->min2);
	LOGP(DTRANS, LOGL_INFO, "Destroying transaction for subscriber '%s'\n", number);

	osmo_timer_del(&trans->timer);

	trans_new_state(trans, 0);

	free(trans);
}

/* link transaction to list */
void link_transaction(transaction_t *trans, amps_t *amps)
{
	transaction_t **transp;

	/* attach to end of list, so first transaction is served first */
	LOGP(DTRANS, LOGL_DEBUG, "Linking transaction %p to amps %p\n", trans, amps);
	trans->amps = amps;
	trans->next = NULL;
	transp = &amps->trans_list;
	while (*transp)
		transp = &((*transp)->next);
	*transp = trans;
	amps_display_status();
}

/* unlink transaction from list */
void unlink_transaction(transaction_t *trans)
{
	transaction_t **transp;

	/* unlink */
	LOGP(DTRANS, LOGL_DEBUG, "Unlinking transaction %p from amps %p\n", trans, trans->amps);
	transp = &trans->amps->trans_list;
	while (*transp && *transp != trans)
		transp = &((*transp)->next);
	if (!(*transp)) {
		LOGP(DTRANS, LOGL_ERROR, "Transaction not in list, please fix!!\n");
		abort();
	}
	*transp = trans->next;
	trans->amps = NULL;
	amps_display_status();
}

transaction_t *search_transaction_number(amps_t *amps, uint32_t min1, uint16_t min2)
{
	transaction_t *trans = amps->trans_list;

	while (trans) {
		if (trans->min1 == min1
		 && trans->min2 == min2) {
			const char *number = amps_min2number(trans->min1, trans->min2);
			LOGP(DTRANS, LOGL_DEBUG, "Found transaction for subscriber '%s'\n", number);
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
			LOGP(DTRANS, LOGL_DEBUG, "Found transaction for subscriber '%s'\n", number);
			return trans;
		}
		trans = trans->next;
	}

	return NULL;
}

void trans_new_state(transaction_t *trans, int state)
{
	LOGP(DTRANS, LOGL_INFO, "Transaction state %s -> %s\n", trans_state_name(trans->state), trans_state_name(state));
	trans->state = state;
	amps_display_status();
}

void amps_flush_other_transactions(amps_t *amps, transaction_t *trans)
{
	/* flush after this very trans */
	while (trans->next) {
		LOGP(DTRANS, LOGL_NOTICE, "Kicking other pending transaction\n");
		destroy_transaction(trans->next);
	}
	/* flush before this very trans */
	while (amps->trans_list != trans) {
		LOGP(DTRANS, LOGL_NOTICE, "Kicking other pending transaction\n");
		destroy_transaction(amps->trans_list);
	}
}

