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
#include <math.h>
#include "../libsample/sample.h"
#include "../liblogging/logging.h"
#include "../libmobile/call.h"
#include "../libmobile/cause.h"
#include "cnetz.h"
#include "telegramm.h"
#include "database.h"

static int new_cueue_position = 0;

const char *transaction2rufnummer(transaction_t *trans)
{
	static char rufnummer[32]; /* make GCC happy (overflow check) */

	sprintf(rufnummer, "%d%d%05d", trans->futln_nat, trans->futln_fuvst, trans->futln_rest);

	return rufnummer;
}

/* create transaction */
transaction_t *create_transaction(cnetz_t *cnetz, uint64_t state, uint8_t futln_nat, uint8_t futln_fuvst, uint16_t futln_rest, int futelg_bit, int extended, double rf_level_db)
{
	transaction_t *trans;

	trans = search_transaction_number_global(futln_nat, futln_fuvst, futln_rest);
	if (trans) {
		const char *rufnummer = transaction2rufnummer(trans);
		int old_callref = trans->callref;
		cnetz_t *old_cnetz = trans->cnetz;
		/* both states must be the same and one of the give selection */
		if ((trans->state & state & (TRANS_EM | TRANS_UM | TRANS_VWG | TRANS_ATQ_IDLE))) {
			if (!isnan(trans->rf_level_db) && !isnan(rf_level_db) && trans->cnetz->kanal != cnetz->kanal) {
				if (rf_level_db > trans->rf_level_db) {
					LOGP(DTRANS, LOGL_NOTICE, "Found already pending transaction for subscriber '%s' on channel #%d, but this message on channel #%d is stronger, so we move to that channel!\n", rufnummer, trans->cnetz->kanal, cnetz->kanal);
					trans->rf_level_db = rf_level_db;
					unlink_transaction(trans);
					link_transaction(trans, cnetz);
					update_db(trans->futln_nat, trans->futln_fuvst, trans->futln_rest, cnetz->kanal, NULL, NULL, 1, 0);
					return trans;
				}
				if (rf_level_db < trans->rf_level_db) {
					LOGP(DTRANS, LOGL_NOTICE, "Found already pending transaction for subscriber '%s' on channel #%d, but this message on channel #%d is weaker, so we ignore that channel!\n", rufnummer, trans->cnetz->kanal, cnetz->kanal);
					return trans;
				}
			}
			LOGP(DTRANS, LOGL_NOTICE, "Found already pending transaction for subscriber '%s' on channel #%d, but this message on channel #%d is also received. Try to avoid multiple OgK channels!\n", rufnummer, trans->cnetz->kanal, cnetz->kanal);
			return trans;
		}
		LOGP(DTRANS, LOGL_NOTICE, "Found already pending transaction for subscriber '%s', deleting!\n", rufnummer);
		destroy_transaction(trans);
		if (old_cnetz) /* should be... */
			cnetz_go_idle(old_cnetz);
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
	trans->futln_nat = futln_nat;
	trans->futln_fuvst = futln_fuvst;
	trans->futln_rest = futln_rest;

	if (state == TRANS_VWG)
		trans->mo_call = 1;
	if (state == TRANS_VAK || state == TRANS_WSK)
		trans->mt_call = 1;

	const char *rufnummer = transaction2rufnummer(trans);
	LOGP(DTRANS, LOGL_INFO, "Created transaction for subscriber '%s'\n", rufnummer);

	link_transaction(trans, cnetz);

	/* update database: now busy */
	update_db(futln_nat, futln_fuvst, futln_rest, cnetz->kanal, &futelg_bit, &extended, 1, 0);
	trans->futelg_bit = futelg_bit;
	trans->extended = extended;

	trans->rf_level_db = rf_level_db;

	return trans;
}

/* destroy transaction */
void destroy_transaction(transaction_t *trans)
{
	/* update database: now idle */
	update_db(trans->futln_nat, trans->futln_fuvst, trans->futln_rest, 0, NULL, NULL, 0, trans->page_failed);

	unlink_transaction(trans);
	
	const char *rufnummer = transaction2rufnummer(trans);
	LOGP(DTRANS, LOGL_INFO, "Destroying transaction for subscriber '%s'\n", rufnummer);

	osmo_timer_del(&trans->timer);

	trans_new_state(trans, 0);

	free(trans);
}

/* link transaction to list */
void link_transaction(transaction_t *trans, cnetz_t *cnetz)
{
	transaction_t **transp;

	/* attach to end of list, so first transaction is served first */
	LOGP(DTRANS, LOGL_DEBUG, "Linking transaction %p to cnetz %p\n", trans, cnetz);
	trans->cnetz = cnetz;
	trans->next = NULL;
	transp = &cnetz->trans_list;
	while (*transp)
		transp = &((*transp)->next);
	*transp = trans;
	cnetz_display_status();
}

/* unlink transaction from list */
void unlink_transaction(transaction_t *trans)
{
	transaction_t **transp;

	/* unlink */
	LOGP(DTRANS, LOGL_DEBUG, "Unlinking transaction %p from cnetz %p\n", trans, trans->cnetz);
	transp = &trans->cnetz->trans_list;
	while (*transp && *transp != trans)
		transp = &((*transp)->next);
	if (!(*transp)) {
		LOGP(DTRANS, LOGL_ERROR, "Transaction not in list, please fix!!\n");
		abort();
	}
	*transp = trans->next;
	trans->cnetz = NULL;
	cnetz_display_status();
}

transaction_t *search_transaction(cnetz_t *cnetz, uint64_t state_mask)
{
	transaction_t *trans = cnetz->trans_list;

	while (trans) {
		if ((trans->state & state_mask)) {
			const char *rufnummer = transaction2rufnummer(trans);
			LOGP(DTRANS, LOGL_DEBUG, "Found transaction for subscriber '%s'\n", rufnummer);
			return trans;
		}
		trans = trans->next;
	}

	return NULL;
}

transaction_t *search_transaction_number(cnetz_t *cnetz, uint8_t futln_nat, uint8_t futln_fuvst, uint16_t futln_rest)
{
	transaction_t *trans = cnetz->trans_list;

	while (trans) {
		if (trans->futln_nat == futln_nat
		 && trans->futln_fuvst == futln_fuvst
		 && trans->futln_rest == futln_rest) {
			const char *rufnummer = transaction2rufnummer(trans);
			LOGP(DTRANS, LOGL_DEBUG, "Found transaction for subscriber '%s'\n", rufnummer);
			return trans;
		}
		trans = trans->next;
	}

	return NULL;
}

transaction_t *search_transaction_number_global(uint8_t futln_nat, uint8_t futln_fuvst, uint16_t futln_rest)
{
	sender_t *sender;
	cnetz_t *cnetz;
	transaction_t *trans = NULL;

	/* search transaction for this subscriber */
	for (sender = sender_head; sender; sender = sender->next) {
		cnetz = (cnetz_t *) sender;
		/* search transaction for this callref */
		trans = search_transaction_number(cnetz, futln_nat, futln_fuvst, futln_rest);
		if (trans)
			break;
	}

	return trans;
}
transaction_t *search_transaction_callref(cnetz_t *cnetz, int callref)
{
	transaction_t *trans = cnetz->trans_list;

	/* just in case, this should not happen */
	if (!callref)
		return NULL;
	while (trans) {
		if (trans->callref == callref) {
			const char *rufnummer = transaction2rufnummer(trans);
			LOGP(DTRANS, LOGL_DEBUG, "Found transaction for subscriber '%s'\n", rufnummer);
			return trans;
		}
		trans = trans->next;
	}

	return NULL;
}

/* get oldest transaction in queue:
 * 
 * oldest means that the queue number is the smallest.
 * all candidates (transactions) must be in queue state.
 */
transaction_t *search_transaction_queue(void)
{
	sender_t *sender;
	transaction_t *trans, *found = NULL;
	cnetz_t *cnetz;
	int queue_max = 0;

	for (sender = sender_head; sender; sender = sender->next) {
		cnetz = (cnetz_t *) sender;
		trans = cnetz->trans_list;
		while (trans) {
			if ((trans->state & (TRANS_MO_QUEUE | TRANS_MT_QUEUE))) {
				/* select if first or lower number */
				if (!found || trans->queue_position < queue_max) {
					queue_max = trans->queue_position;
					found = trans;
				}
			}
			trans = trans->next;
		}
	}

	if (found) {
		const char *rufnummer = transaction2rufnummer(found);
		LOGP(DTRANS, LOGL_DEBUG, "Found oldest transaction in queue for subscriber '%s'\n", rufnummer);
		return found;
	}

	return NULL;
}

static const char *trans_state_name(uint64_t state)
{
	switch (state) {
	case 0:
		return "IDLE";
	case TRANS_EM:
		return "EM";
	case TRANS_UM:
		return "UM";
	case TRANS_MA:
		return "MA";
	case TRANS_MFT:
		return "MFT";
	case TRANS_VWG:
		return "VWG";
	case TRANS_WAF:
		return "WAF";
	case TRANS_WBP:
		return "WBP";
	case TRANS_WBN:
		return "WBN";
	case TRANS_VAG:
		return "VAG";
	case TRANS_WSK:
		return "WSK";
	case TRANS_VAK:
		return "VAK";
	case TRANS_BQ:
		return "BQ";
	case TRANS_ZFZ:
		return "ZFZ";
	case TRANS_VHQ_K:
		return "VHQ_K";
	case TRANS_VHQ_V:
		return "VHQ_V";
	case TRANS_AP:
		return "AP";
	case TRANS_RTA:
		return "RTA";
	case TRANS_DS:
		return "DS";
	case TRANS_AHQ:
		return "AHQ";
	case TRANS_VA:
		return "VA";
	case TRANS_AF:
		return "AF";
	case TRANS_AT:
		return "AT";
	case TRANS_ATQ:
		return "ATQ";
	case TRANS_ATQ_IDLE:
		return "ATQ_IDLE";
	case TRANS_MO_QUEUE:
		return "MO_QUEUE";
	case TRANS_MT_QUEUE:
		return "MT_QUEUE";
	case TRANS_MO_DELAY:
		return "MO_DELAY";
	case TRANS_MT_DELAY:
		return "MT_DELAY";
	default:
		return "<invalid transaction state>";
	}
}

const char *trans_short_state_name(uint64_t state)
{
	switch (state) {
	case 0:
		return "IDLE";
	case TRANS_EM:
	case TRANS_UM:
		return "REGISTER";
	case TRANS_MA:
	case TRANS_MFT:
		return "PING";
	case TRANS_VWG:
	case TRANS_WAF:
	case TRANS_WBP:
	case TRANS_WBN:
		return "DIALING";
	case TRANS_ZFZ:
	case TRANS_AP:
		return "AUTHENTICATE";
	case TRANS_VAG:
	case TRANS_WSK:
	case TRANS_VAK:
	case TRANS_BQ:
	case TRANS_VHQ_K:
	case TRANS_VHQ_V:
		return "ASSIGN";
	case TRANS_RTA:
		return "ALERT";
	case TRANS_DS:
		return "DS";
	case TRANS_AHQ:
		return "AHQ";
	case TRANS_VA:
	case TRANS_AF:
	case TRANS_AT:
	case TRANS_ATQ:
	case TRANS_ATQ_IDLE:
		return "RELEASE";
	case TRANS_MO_QUEUE:
	case TRANS_MO_DELAY:
		return "OUT QUEUE";
	case TRANS_MT_QUEUE:
	case TRANS_MT_DELAY:
		return "IN QUEUE";
	default:
		return "<invalid transaction state>";
	}
}

void trans_new_state(transaction_t *trans, uint64_t state)
{
	LOGP(DTRANS, LOGL_INFO, "Transaction (%s) state %s -> %s\n", transaction2rufnummer(trans), trans_state_name(trans->state), trans_state_name(state));
	trans->state = state;
	/* in case of a queue, set new positon */
	if (!trans->queue_position && (state == TRANS_MO_QUEUE || state == TRANS_MT_QUEUE))
		trans->queue_position = ++new_cueue_position;
	cnetz_display_status();
}

void cnetz_flush_other_transactions(cnetz_t *cnetz, transaction_t *trans)
{
	/* flush after this very trans */
	while (trans->next) {
		LOGP(DTRANS, LOGL_NOTICE, "Kicking other pending transaction\n");
		destroy_transaction(trans->next);
	}
	/* flush before this very trans */
	while (cnetz->trans_list != trans) {
		LOGP(DTRANS, LOGL_NOTICE, "Kicking other pending transaction\n");
		destroy_transaction(cnetz->trans_list);
	}
}

