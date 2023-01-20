/* NMT transaction handling
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
#include <string.h>
#include "../libsample/sample.h"
#include "../libdebug/debug.h"
#include "nmt.h"
#include "transaction.h"

static transaction_t *trans_list = NULL;
static void transaction_timeout(void *data);

/* link transaction to list */
static void link_transaction(transaction_t *trans)
{
	transaction_t **transp;

	/* attach to end of list, so first transaction is served first */
	PDEBUG(DTRANS, DEBUG_DEBUG, "Linking transaction %p to list\n", trans);
	trans->next = NULL;
	transp = &trans_list;
	while (*transp)
		transp = &((*transp)->next);
	*transp = trans;
}

/* unlink transaction from list */
static void unlink_transaction(transaction_t *trans)
{
	transaction_t **transp;
	sender_t *sender;
	nmt_t *nmt;

	/* unlink */
	PDEBUG(DTRANS, DEBUG_DEBUG, "Unlinking transaction %p from list\n", trans);
	transp = &trans_list;
	while (*transp && *transp != trans)
		transp = &((*transp)->next);
	if (!(*transp)) {
		PDEBUG(DTRANS, DEBUG_ERROR, "Transaction not in list, please fix!!\n");
		abort();
	}
	*transp = trans->next;
	trans->next = NULL;

	/* unbind from channel */
	trans->nmt = NULL;
	for (sender = sender_head; sender; sender = sender->next) {
		nmt = (nmt_t *)sender;
		if (nmt->trans == trans)
			nmt_go_idle(nmt);
	}
}

/* create transaction */
transaction_t *create_transaction(struct nmt_subscriber *subscr)
{
	transaction_t *trans;

	trans = calloc(1, sizeof(*trans));
	if (!trans) {
		PDEBUG(DTRANS, DEBUG_ERROR, "No memory!\n");
		return NULL;
	}

	timer_init(&trans->timer, transaction_timeout, trans);

	memcpy(&trans->subscriber, subscr, sizeof(struct nmt_subscriber));

	PDEBUG(DTRANS, DEBUG_INFO, "Created transaction for subscriber '%c,%s'\n", subscr->country, subscr->number);

	link_transaction(trans);

	return trans;
}

/* destroy transaction */
void destroy_transaction(transaction_t *trans)
{
	unlink_transaction(trans);

	PDEBUG(DTRANS, DEBUG_INFO, "Destroying transaction for subscriber '%c,%s'\n", trans->subscriber.country, trans->subscriber.number);

	timer_exit(&trans->timer);

	free(trans);
}

/* Timeout handling */
static void transaction_timeout(void *data)
{
	transaction_t *trans = data;

	timeout_mt_paging(trans);
}

transaction_t *get_transaction_by_callref(int callref)
{
	transaction_t *trans;

	trans = trans_list;
	while (trans) {
		if (trans->callref == callref)
			break;
		trans = trans->next;
	}

	return trans;
}

transaction_t *get_transaction_by_number(struct nmt_subscriber *subscr)
{
	transaction_t *trans;

	trans = trans_list;
	while (trans) {
		if (trans->subscriber.country == subscr->country
		 && !strcmp(trans->subscriber.number, subscr->number))
			break;
		trans = trans->next;
	}

	return trans;
}

