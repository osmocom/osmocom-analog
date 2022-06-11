/* C-Netz database
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
#include "cnetz.h"
#include "database.h"
#include "sysinfo.h"

/* the network specs say: check every 1 - 6.5 minutes for availability
 * remove from database after 3 subsequent failures
 * the phone will register 20 minutes after no call / no paging from network.
 */
#define MELDE_WIEDERHOLUNG	60.0 /* when busy */

typedef struct cnetz_database {

	struct cnetz_database	*next;
	int			ogk_kanal;	/* available on which channel */
	uint8_t			futln_nat;	/* who ... */
	uint8_t			futln_fuvst;
	uint16_t		futln_rest;
	int			futelg_bit;	/* chip card inside */
	int			extended;	/* mobile supports extended frequencies */
	int			eingebucht;	/* set if still available */
	double			last_seen;
	int			busy;		/* set if currently in a call */
	struct timer		timer;		/* timer for next availability check */
	int			retry;		/* counts number of retries */
} cnetz_db_t;

cnetz_db_t *cnetz_db_head;

static const char *print_meldeaufrufe(int versuche)
{
	static char text[32];

	if (versuche <= 0)
		return "infinite";

	sprintf(text, "%d", versuche);
	return text;
}

/* destroy transaction */
static void remove_db(cnetz_db_t *db)
{
	cnetz_db_t **dbp;

	/* uinlink */
	dbp = &cnetz_db_head;
	while (*dbp && *dbp != db)
		dbp = &((*dbp)->next);
	if (!(*dbp)) {
		PDEBUG(DDB, DEBUG_ERROR, "Subscriber not in list, please fix!!\n");
		abort();
	}
	*dbp = db->next;

	PDEBUG(DDB, DEBUG_INFO, "Removing subscriber '%d,%d,%05d' from database.\n", db->futln_nat, db->futln_fuvst, db->futln_rest);

	timer_exit(&db->timer);

	free(db);
}

/* Timeout handling */
static void db_timeout(struct timer *timer)
{
	cnetz_db_t *db = (cnetz_db_t *)timer->priv;
	int rc;

	PDEBUG(DDB, DEBUG_INFO, "Check, if subscriber '%d,%d,%05d' is still available.\n", db->futln_nat, db->futln_fuvst, db->futln_rest);
	
	rc = cnetz_meldeaufruf(db->futln_nat, db->futln_fuvst, db->futln_rest, db->ogk_kanal);
	if (rc < 0) {
		/* OgK is used for speech, but this never happens in a real
		 * network. We just assume that the phone has responded and
		 * assume we had a response. */
		PDEBUG(DDB, DEBUG_INFO, "OgK busy, so we assume a positive response.\n");
		timer_start(&db->timer, si.meldeinterval); /* when to check avaiability again */
		db->retry = 0;
	}
}

/* create/update db entry */
int update_db(uint8_t futln_nat, uint8_t futln_fuvst, uint16_t futln_rest, int ogk_kanal, int *futelg_bit, int *extended, int busy, int failed)
{
	cnetz_db_t *db, **dbp;

	/* search transaction for this subscriber */
	db = cnetz_db_head;
	while (db) {
		if (db->futln_nat == futln_nat
		 && db->futln_fuvst == futln_fuvst
		 && db->futln_rest == futln_rest)
			break;
		db = db->next;
	}
	if (!db) {
		db = calloc(1, sizeof(*db));
		if (!db) {
			PDEBUG(DDB, DEBUG_ERROR, "No memory!\n");
			return 0;
		}
		timer_init(&db->timer, db_timeout, db);

		db->eingebucht = 1;
		db->futln_nat = futln_nat;
		db->futln_fuvst = futln_fuvst;
		db->futln_rest = futln_rest;

		/* attach to end of list */
		dbp = &cnetz_db_head;
		while (*dbp)
			dbp = &((*dbp)->next);
		*dbp = db;

		PDEBUG(DDB, DEBUG_INFO, "Adding subscriber '%d,%d,%05d' to database.\n", db->futln_nat, db->futln_fuvst, db->futln_rest);
	}

	if (ogk_kanal)
		db->ogk_kanal = ogk_kanal;

	if (futelg_bit && *futelg_bit >= 0)
		db->futelg_bit = *futelg_bit;

	if (extended && *extended >= 0)
		db->extended = *extended;

	db->busy = busy;
	if (busy) {
		PDEBUG(DDB, DEBUG_INFO, "Subscriber '%d,%d,%05d' on OGK channel #%d is busy now.\n", db->futln_nat, db->futln_fuvst, db->futln_rest, db->ogk_kanal);
		timer_stop(&db->timer);
	} else if (!failed) {
		PDEBUG(DDB, DEBUG_INFO, "Subscriber '%d,%d,%05d' on OGK channel #%d is idle now.\n", db->futln_nat, db->futln_fuvst, db->futln_rest, db->ogk_kanal);
		timer_start(&db->timer, si.meldeinterval); /* when to check avaiability (again) */
		db->retry = 0;
		db->eingebucht = 1;
		db->last_seen = get_time();
	} else {
		db->retry++;
		PDEBUG(DDB, DEBUG_NOTICE, "Paging subscriber '%d,%d,%05d' on OGK channel #%d failed (try %d of %s).\n", db->futln_nat, db->futln_fuvst, db->futln_rest, db->ogk_kanal, db->retry, print_meldeaufrufe(si.meldeaufrufe));
		if (si.meldeaufrufe && db->retry == si.meldeaufrufe) {
			PDEBUG(DDB, DEBUG_INFO, "Marking subscriber as gone.\n");
			db->eingebucht = 0;
			return db->extended;
		}
		timer_start(&db->timer, (si.meldeinterval < MELDE_WIEDERHOLUNG) ? si.meldeinterval : MELDE_WIEDERHOLUNG); /* when to do retry */
	}

	if (futelg_bit)
		*futelg_bit = db->futelg_bit;
	if (extended)
		*extended = db->extended;
	return 0;
}

int find_db(uint8_t futln_nat, uint8_t futln_fuvst, uint16_t futln_rest, int *ogk_kanal, int *futelg_bit, int *extended)
{
	cnetz_db_t *db = cnetz_db_head;

	while (db) {
		if (db->eingebucht
		 && db->futln_nat == futln_nat
		 && db->futln_fuvst == futln_fuvst
		 && db->futln_rest == futln_rest) {
			if (ogk_kanal)
				*ogk_kanal = db->ogk_kanal;
			if (futelg_bit)
				*futelg_bit = db->futelg_bit;
			if (extended)
				*extended = db->extended;
			return 0;
		}
		db = db->next;
	}
	return -1;
}

void flush_db(void)
{
	while (cnetz_db_head)
		remove_db(cnetz_db_head);
}

void dump_db(void)
{
	cnetz_db_t *db = cnetz_db_head;
	double now = get_time();
	int last;
	char attached[16];

	PDEBUG(DDB, DEBUG_NOTICE, "Dump of subscriber database:\n");
	if (!db) {
		PDEBUG(DDB, DEBUG_NOTICE, " - No subscribers attached!\n");
		return;
	}

	PDEBUG(DDB, DEBUG_NOTICE, "Subscriber\tAttached\tBusy\t\tLast seen\tMeldeaufrufe\n");
	PDEBUG(DDB, DEBUG_NOTICE, "-------------------------------------------------------------------------------\n");
	while (db) {
		last = (db->busy) ? 0 : (uint32_t)(now - db->last_seen);
		sprintf(attached, "YES (OGK %d)", db->ogk_kanal);
		PDEBUG(DDB, DEBUG_NOTICE, "%d,%d,%05d\t%s\t%s\t\t%02d:%02d:%02d \t%d/%s\n", db->futln_nat, db->futln_fuvst, db->futln_rest, (db->eingebucht) ? attached : "-no-\t", (db->busy) ? "YES" : "-no-", last / 3600, (last / 60) % 60, last % 60, db->retry, print_meldeaufrufe(si.meldeaufrufe));
		db = db->next;
	}
}

