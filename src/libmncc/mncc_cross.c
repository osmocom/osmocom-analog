/* Mobie Network Call Control (MNCC) cross connecting mobiles
 *
 * (C) 2017 by Andreas Eversberg <jolly@eversberg.eu>
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
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "../libsample/sample.h"
#include "../common/debug.h"
#include "../libmobile/call.h"
#include "cause.h"
#include "mncc.h"
#include "mncc_cross.h"

int new_callref = 0; /* toward mobile */

typedef struct cross {
	struct cross	*next;
	int		callref1;
	int		callref2;
} cross_t;

static cross_t *cross_head = NULL;

static int create_cross(int callref)
{
	cross_t *cross;

	cross = calloc(1, sizeof(*cross));
	if (!cross) {
		PDEBUG(DMNCC, DEBUG_ERROR, "No memory!\n");
		abort();
	}

	cross->callref1 = callref;
	cross->callref2 = ++new_callref;

	/* attach to list */
	cross->next = cross_head;
	cross_head = cross;

	PDEBUG(DMNCC, DEBUG_INFO, "Cross connection created\n");

	return cross->callref2;
}

static void destroy_cross(cross_t *cross)
{
	cross_t **crossp;

	/* detach from list */
	crossp = &cross_head;
	while (*crossp && *crossp != cross)
		crossp = &((*crossp)->next);
	if (!(*crossp)) {
		PDEBUG(DMNCC, DEBUG_ERROR, "Transaction not in list, please fix!!\n");
		abort();
	}
	*crossp = cross->next;

	free(cross);

	PDEBUG(DMNCC, DEBUG_INFO, "Cross connection destroyed\n");
}

typedef struct queue {
	struct queue	*next;
	int		length;
	uint8_t		buf[0];
} queue_t;

static queue_t *queue_head;

static void cross_mncc_up(uint8_t *buf, int length);

static int cross_mncc_up_queue(uint8_t *buf, int length)
{
	struct gsm_mncc *mncc = (struct gsm_mncc *)buf;
	queue_t *queue, **queuep;

	/* directly forward voice */
	if (mncc->msg_type == ANALOG_8000HZ) {
		cross_mncc_up(buf, length);
		return 0;
	}

	/* queue all other messages */
	queue = calloc(1, sizeof(*queue) + length);
	if (!queue) {
		PDEBUG(DMNCC, DEBUG_ERROR, "No memory!\n");
		return -CAUSE_TEMPFAIL;
	}
	queue->length = length;
	memcpy(queue->buf, buf, length);
	
	/* add tail */
	queuep = &queue_head;
	while (*queuep)
		queuep = &((*queuep)->next);
	*queuep = queue;

	return 0;
}

static void cross_mncc_up(uint8_t *buf, int length)
{
	struct gsm_mncc *mncc = (struct gsm_mncc *)buf;
	cross_t *cross = NULL;
	int callref = mncc->callref, remote = 0;

	/* find cross instance */
	for (cross = cross_head; cross; cross = cross->next) {
		if (cross->callref1 == callref) {
			remote = cross->callref2;
			break;
		}
		if (cross->callref2 == callref) {
			remote = cross->callref1;
			break;
		}
	}

	if (mncc->msg_type == MNCC_REL_CNF) {
		if (cross)
			destroy_cross(cross);
		return;
	}

	if (!remote && mncc->msg_type != MNCC_SETUP_IND) {
		PDEBUG(DMNCC, DEBUG_ERROR, "invalid call ref.\n");
		/* send down reused MNCC */
		mncc->msg_type = MNCC_REL_REQ;
		mncc->fields |= MNCC_F_CAUSE;
		mncc->cause.location = LOCATION_USER;
		mncc->cause.value = CAUSE_INVALCALLREF;
		mncc_down(buf, length);
		return;
	}

	switch (mncc->msg_type) {
	case ANALOG_8000HZ:
		/* send down reused MNCC */
		mncc->callref = remote;
		mncc_down(buf, length);
		break;
	case MNCC_SETUP_IND:
		remote = create_cross(callref);
		/* send down reused MNCC */
		mncc->msg_type = MNCC_SETUP_REQ;
		mncc->callref = remote;
		mncc_down(buf, length);
		break;
	case MNCC_CALL_CONF_IND:
		/* send down reused MNCC */
		mncc->msg_type = MNCC_CALL_PROC_REQ;
		mncc->callref = remote;
		mncc_down(buf, length);
		break;
	case MNCC_ALERT_IND:
		/* send down reused MNCC */
		mncc->msg_type = MNCC_ALERT_REQ;
		mncc->callref = remote;
		mncc_down(buf, length);
		break;
	case MNCC_SETUP_CNF:
		/* send down reused MNCC */
		mncc->msg_type = MNCC_SETUP_RSP;
		mncc->callref = remote;
		mncc_down(buf, length);
		break;
	case MNCC_SETUP_COMPL_IND:
		/* send down reused MNCC */
		mncc->msg_type = MNCC_SETUP_COMPL_REQ;
		mncc->callref = remote;
		mncc_down(buf, length);
		break;
	case MNCC_DISC_IND:
		/* send down reused MNCC */
		mncc->msg_type = MNCC_DISC_REQ;
		mncc->callref = remote;
		mncc_down(buf, length);
		break;
	case MNCC_REL_IND:
		/* send down reused MNCC */
		mncc->msg_type = MNCC_REL_REQ;
		mncc->callref = remote;
		mncc_down(buf, length);
		destroy_cross(cross);
		break;
	}
}

void mncc_cross_handle(void)
{
	queue_t *queue;

	while (queue_head) {
		/* remove from head */
		queue = queue_head;
		queue_head = queue->next;

		cross_mncc_up(queue->buf, queue->length);
		free(queue);
	}
}

int mncc_cross_init(void)
{
	mncc_up = cross_mncc_up_queue;

	PDEBUG(DMNCC, DEBUG_DEBUG, "MNCC crossconnect initialized, waiting for connection.\n");

	return 0;
}

void mncc_cross_exit(void)
{
	while (cross_head)
		destroy_cross(cross_head);

	PDEBUG(DMNCC, DEBUG_DEBUG, "MNCC crossconnect removed.\n");
}

