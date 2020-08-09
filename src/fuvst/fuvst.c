/* C-Netz FuVSt handling
 *
 * (C) 2020 by Andreas Eversberg <jolly@eversberg.eu>
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

/* Notes:
 *
 * The release timer will release the call, if no release response from BS has
 * been received. This may happen due to signaling link error. Since there
 * is no document available about message timout conditions, I just use
 * that timer when there is no response. I think that the base station does
 * the same and releases the call, if no release response has been received.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <inttypes.h>
#include "../libsample/sample.h"
#include "../libdebug/debug.h"
#include "../libmobile/call.h"
#include "../libmobile/cause.h"
#include "../libtimer/timer.h"
#include "../libosmocc/message.h"
#include "fuvst.h"

/* digital loopback test */
//#define DIGITAL_LOOPBACK

static int base_station_ready = 0;
extern double gebuehren;
extern int alarms;
extern int authentication;
extern int warmstart;

static int send_bit(void *inst)
{
	fuvst_t *fuvst = (fuvst_t *)inst;

#ifdef DIGITAL_LOOPBACK
	return 0;
#else
	return mtp_send_bit(&fuvst->mtp);
#endif
}

static void receive_bit(void *inst, int bit)
{
	fuvst_t *fuvst = (fuvst_t *)inst;
#ifdef DIGITAL_LOOPBACK
	mtp_receive_bit(&fuvst->mtp, mtp_send_bit(&fuvst->mtp));
#else
	mtp_receive_bit(&fuvst->mtp, bit);
#endif
}

/*
 * config handling
 */

static int message_send(uint8_t ident, uint8_t opcode, uint8_t *data, int len);

struct config {
	uint8_t data[0x1800];
	int loaded;
} conf;

/* init, even if no config shall be loaded */
void config_init(void)
{
	memset(&conf, 0, sizeof(conf));
}

/* load config file */
int config_file(const char *filename)
{
	FILE *fp = fopen(filename, "r");
	int rc;
	uint8_t byte;

	if (!fp) {
		PDEBUG(DTRANS, DEBUG_ERROR, "Failed to open data base file: '%s'\n", filename);
		return -EIO;
	}

	rc = fread(conf.data, 1, sizeof(conf.data), fp);
	if (rc < (int)sizeof(conf.data)) {
		fclose(fp);
		PDEBUG(DTRANS, DEBUG_ERROR, "Data base file shorter than %d bytes. This seems not to be a valid config file.\n", (int)sizeof(conf.data));
		return -EIO;
	}

	rc = fread(&byte, 1, 1, fp);
	if (rc == 1) {
		fclose(fp);
		PDEBUG(DTRANS, DEBUG_ERROR, "Data base file larger than %d bytes. (Don't use the EEPROM config format, use the MSC config format.)\n", (int)sizeof(conf.data));
		return -EIO;
	}

	fclose(fp);
	conf.loaded = 1;

	return 0;
}

/* transfer config file towards base station */
static void config_send(uint8_t ident, uint8_t job, uint16_t offset, uint16_t length)
{
	uint8_t opcode;
	uint8_t *data;
	int len;
	int count, num;
	uint8_t block[9];
	int i, j;
	uint32_t checksum = 0;
	uint8_t rc = 1; /* Auftrag angenommen */

	PDEBUG(DCNETZ, DEBUG_NOTICE, "MSC requests data base block. (offset=%d, lenght=%d)\n", offset, length);

	if (!conf.loaded) {
		PDEBUG(DCNETZ, DEBUG_ERROR, "MSC requests data base, but no file name given. Please give file name!\n");
error:
		/* return error */
		len = encode_xedbu_1(&opcode, &data, 16, job, 0);
		message_send(ident, opcode, data, len);
		return;
	}

	if (offset + length > sizeof(conf.data)) {
		PDEBUG(DCNETZ, DEBUG_ERROR, "Requested date out of range!\n");
		goto error;
	}

	count = 0;
	num = (length + 8) / 9;

	/* header */
	len = encode_xedbu_1(&opcode, &data, rc, job, num);
	message_send(ident, opcode, data, len);
	for (i = 0; i < num; i++) {
		for (j = 0; j < 9; j++) {
			if (count < length)
				block[j] = conf.data[offset + count];
			else
				block[j] = 0x00;
			checksum += block[j];
			count++;
		}
		len = encode_xedbu_2(&opcode, &data, i + 1, job, block);
		message_send(ident, opcode, data, len);
	}

	len = encode_xedbu_3(&opcode, &data, i + 1, job, offset, length, checksum);
	message_send(ident, opcode, data, len);
}

/* convert metering pulse duration to a value that can be stored into base station */
static uint32_t duration2ticks(double duration)
{
	double ticks, value;

	/* this is the way the SIM-Rom and real network (trace) does it */
	ticks = duration / 0.0375;
	value = ticks / 16.0 * 4095.0; /* don't know why they did not use ticks * 256 */

	return (uint32_t)value; // round down
}

/* transfer metering pulse duration table towards base station */
static void billing_send(uint8_t ident, uint8_t K)
{
	uint32_t table[32];
	uint16_t checksum = 0;
	int i, last_i = 0;
	uint8_t opcode;
	uint8_t *data;
	int len;

	/* clear table */
	memset(table, 0, sizeof(table));

	/* set entries */
	table[1] = duration2ticks(gebuehren);

	/* generate checksum from table and remember last entry */
	for (i = 0; i < 32; i++) {
		checksum += i; // crazy: table index is included in checksum
		checksum += table[i] & 0xff;
		checksum += (table[i] >> 8) & 0xff;
		checksum += (table[i] >> 16) & 0xff;
		if (table[i])
			last_i = i;
	}

	/* transfer all table entries with values in it */
	for (i = 0; i < 32; i++) {
		if (!table[i])
			continue;
		/* if i == last_i, set last entry flag */
		len = encode_xgtau(&opcode, &data, i, table[i], (i == last_i) ? 1 : 0, K, checksum);
		message_send(ident, opcode, data, len);
	}
}

/*
 * emergency prefixes
 */

typedef struct emergency {
	struct emergency	*next;
	char			number[17];
} emergency_t;

emergency_t *emerg_list = NULL;


void add_emergency(const char *number)
{
	emergency_t *emerg = NULL;
	emergency_t **emergp;

	emerg = calloc(1, sizeof(*emerg));
	if (!emerg) {
		PDEBUG(DTRANS, DEBUG_ERROR, "No memory!\n");
		return;
	}

	strncpy(emerg->number, number, sizeof(emerg->number) - 1);

	/* attach to end of list, so first transaction is served first */
	emergp = &emerg_list;
	while (*emergp)
		emergp = &((*emergp)->next);
	*emergp = emerg;
}

static int check_emerg(const char *number)
{
	emergency_t *emerg = emerg_list;

	while (emerg) {
		if (!strncmp(number, emerg->number, strlen(emerg->number)))
			break;
		emerg = emerg->next;
	}

	if (!emerg)
		return 0;

	PDEBUG(DCNETZ, DEBUG_NOTICE, "Emergency call, matching prefix '%s' in list of emergency numbers.\n", emerg->number);

	return 1;
}

/*
 * subscriber database
 */

typedef struct cnetz_database {
	struct cnetz_database	*next;
	uint8_t			futln_nat;
	uint8_t			futln_fuvst;
	uint16_t		futln_rest;
	uint8_t			chip;
} cnetz_db_t;

static cnetz_db_t *cnetz_db_head;

static cnetz_db_t *find_db(uint8_t futln_nat, uint8_t futln_fuvst, uint16_t futln_rest)
{
        cnetz_db_t *db;

        /* search transaction for this subsriber */
        db = cnetz_db_head;
        while (db) {
                if (db->futln_nat == futln_nat
                 && db->futln_fuvst == futln_fuvst
                 && db->futln_rest == futln_rest)
                        break;
                db = db->next;
        }

	return db;
}

static void remove_db(uint8_t futln_nat, uint8_t futln_fuvst, uint16_t futln_rest)
{
	cnetz_db_t *db, **dbp;

	db = find_db(futln_nat, futln_fuvst, futln_rest);
	if (!db)
		return;

	/* uinlink */
	dbp = &cnetz_db_head;
	while (*dbp && *dbp != db)
		dbp = &((*dbp)->next);
	if (!(*dbp)) {
		PDEBUG(DDB, DEBUG_ERROR, "Subscriber not in list, please fix!!\n");
		abort();
	}
	*dbp = db->next;

	PDEBUG(DDB, DEBUG_INFO, "Removing subscriber '%d,%d,%d' from database.\n", db->futln_nat, db->futln_fuvst, db->futln_rest);

	/* remove */
	free(db);
}

static void flush_db(void)
{
        cnetz_db_t *db;

	while ((db = cnetz_db_head)) {
		PDEBUG(DDB, DEBUG_INFO, "Removing subscriber '%d,%d,%d' from database.\n", db->futln_nat, db->futln_fuvst, db->futln_rest);
		cnetz_db_head = db->next;
		free(db);
	}
}

static void add_db(uint8_t futln_nat, uint8_t futln_fuvst, uint16_t futln_rest, uint8_t chip)
{
        cnetz_db_t *db, **dbp;

	db = find_db(futln_nat, futln_fuvst, futln_rest);
	if (db)
		return;

	/* add */
	db = calloc(1, sizeof(*db));
	if (!db) {
		PDEBUG(DDB, DEBUG_ERROR, "No memory!\n");
		return;
	}
	db->futln_nat = futln_nat;
	db->futln_fuvst = futln_fuvst;
	db->futln_rest = futln_rest;
	db->chip = chip;

	PDEBUG(DDB, DEBUG_INFO, "Adding subscriber '%d,%d,%d' to database.\n", db->futln_nat, db->futln_fuvst, db->futln_rest);

	/* attach to end of list */
	dbp = &cnetz_db_head;
	while (*dbp)
		dbp = &((*dbp)->next);
	*dbp = db;
}

/*
 * transactions
 */

/* Release timeout */
#define RELEASE_TO 3.0

/* BSC originated Ident-Numbers */
#define IDENT_BSC_FROM 0x9f
#define IDENT_BSC_TO   0xff
static uint8_t last_bsc_ident = IDENT_BSC_TO;

enum call_state {
	STATE_NULL = 0,
	STATE_MO,
	STATE_MO_QUEUE,
	STATE_MT,
	STATE_MT_QUEUE,
	STATE_MT_ALERTING,
	STATE_MO_CONNECT,
	STATE_MT_CONNECT,
	STATE_RELEASE,
};

typedef struct transaction {
	struct transaction	*next;			/* pointer to next node in list */
	enum call_state		state;			/* call state */
	int			mo;			/* outgoing flag */
	uint8_t			ident;			/* Identification Number */
	uint8_t			futln_nat;		/* current station ID (3 values) */
	uint8_t			futln_fuvst;
	uint16_t		futln_rest;
	int			callref;		/* callref for transaction */
	int			old_callref;		/* remember callref after MNCC released */
	int			spk_nr;
	fuvst_t			*spk;			/* assigned SPK */
	char			number[17];		/* dialed by mobile */
	int			sonderruf;		/* an emergency call */
	struct timer		timer;			/* release timer */
} transaction_t;

transaction_t *trans_list = NULL;

const char *transaction2rufnummer(transaction_t *trans)
{
	static char rufnummer[32]; /* make GCC happy (overflow check) */

	sprintf(rufnummer, "%d%d%05d", trans->futln_nat, trans->futln_fuvst, trans->futln_rest);

	return rufnummer;
}

const char *state_name(enum call_state state)
{
	static char invalid[16];

	switch (state) {
	case STATE_NULL:
		return "NULL";
	case STATE_MO:
		return "MO (Setup)";
	case STATE_MO_QUEUE:
		return "MO (Queue)";
	case STATE_MT:
		return "MT (Setup)";
	case STATE_MT_QUEUE:
		return "MT (Queue)";
	case STATE_MT_ALERTING:
		return "MT (Alerting)";
	case STATE_MO_CONNECT:
		return "MO (Connected)";
	case STATE_MT_CONNECT:
		return "MT (Connected)";
	case STATE_RELEASE:
		return "Release";
	}

	sprintf(invalid, "invalid(%d)", state);
	return invalid;
}

void display_status(void)
{
	sender_t *sender;
	fuvst_t *fuvst;
	transaction_t *trans;

	display_status_start();
	for (sender = sender_head; sender; sender = sender->next) {
		fuvst = (fuvst_t *) sender;
		if (fuvst->chan_type == CHAN_TYPE_SPK) {
			display_status_channel(fuvst->sender.kanal, "SpK", (fuvst->callref) ? "BUSY" : "IDLE");
			for (trans = trans_list; trans; trans = trans->next) {
				if (trans->spk != fuvst)
					continue;
				display_status_subscriber(transaction2rufnummer(trans), state_name(trans->state));
			}
		} else {
			display_status_channel(fuvst->sender.kanal, "ZZK", (fuvst->link) ? "Link UP" : "Link DOWN");
			for (trans = trans_list; trans; trans = trans->next) {
				if (trans->spk)
					continue;
				display_status_subscriber(transaction2rufnummer(trans), state_name(trans->state));
			}
		}
	}
	display_status_end();
}

static void new_call_state(transaction_t *trans, enum call_state new_state)
{
	if (trans->state == new_state)
		return;
	PDEBUG(DTRANS, DEBUG_INFO, "State change: %s -> %s\n", state_name(trans->state), state_name(new_state));
	trans->state = new_state;
	display_status();
}

transaction_t *search_transaction_number(uint8_t futln_nat, uint8_t futln_fuvst, uint16_t futln_rest)
{
	transaction_t *trans = trans_list;

	while (trans) {
		if (trans->futln_nat == futln_nat
		 && trans->futln_fuvst == futln_fuvst
		 && trans->futln_rest == futln_rest) {
			const char *rufnummer = transaction2rufnummer(trans);
			PDEBUG(DTRANS, DEBUG_DEBUG, "Found transaction for subscriber '%s'\n", rufnummer);
			return trans;
		}
		trans = trans->next;
	}

	return NULL;
}

transaction_t *search_transaction_ident(uint8_t ident)
{
	transaction_t *trans = trans_list;

	while (trans) {
		if (trans->ident == ident) {
			const char *rufnummer = transaction2rufnummer(trans);
			PDEBUG(DTRANS, DEBUG_DEBUG, "Found transaction for subscriber '%s'\n", rufnummer);
			return trans;
		}
		trans = trans->next;
	}

	return NULL;
}

transaction_t *search_transaction_callref(int callref)
{
	transaction_t *trans = trans_list;

	/* just in case, this should not happen */
	if (!callref)
		return NULL;
	while (trans) {
		if (trans->callref == callref) {
			const char *rufnummer = transaction2rufnummer(trans);
			PDEBUG(DTRANS, DEBUG_DEBUG, "Found transaction for subscriber '%s'\n", rufnummer);
			return trans;
		}
		trans = trans->next;
	}

	return NULL;
}

/* destroy transaction */
static void destroy_transaction(transaction_t *trans)
{
	transaction_t **transp;

	const char *rufnummer = transaction2rufnummer(trans);
	PDEBUG(DTRANS, DEBUG_INFO, "Destroying transaction for subscriber '%s'\n", rufnummer);

	timer_exit(&trans->timer);

	/* check for old callref (before removal) then detach SPK
	 * if SPK has been reused by BS, our old callref will not match,
	 * so that we don't detach.
	 */
	if (trans->spk && trans->spk->callref == trans->old_callref)
		trans->spk->callref = 0;

	/* unlink */
	transp = &trans_list;
	while (*transp && *transp != trans)
		transp = &((*transp)->next);
	if (!(*transp)) {
		PDEBUG(DTRANS, DEBUG_ERROR, "Transaction not in list, please fix!!\n");
		abort();
	}
	*transp = trans->next;
	
	free(trans);

	display_status();
}

/* Timeout handling */
void trans_timeout(struct timer *timer)
{
	transaction_t *trans = (transaction_t *)timer->priv;

	PDEBUG(DTRANS, DEBUG_NOTICE, "Releasing transaction due to timeout.\n");
	if (trans->callref)
		call_up_release(trans->callref, CAUSE_NORMAL);
	trans->callref = 0;
	destroy_transaction(trans);
}

/* create new transaction */
static transaction_t *create_transaction(uint8_t ident, uint8_t futln_nat, uint8_t futln_fuvst, uint16_t futln_rest, int mo)
{
	transaction_t *trans = NULL;
	transaction_t **transp;

	trans = search_transaction_number(futln_nat, futln_fuvst, futln_rest);
	if (trans && mo) {
		const char *rufnummer = transaction2rufnummer(trans);
		PDEBUG(DTRANS, DEBUG_NOTICE, "Found alredy pending transaction for subscriber '%s', dropping that!\n", rufnummer);
		if (trans->callref)
			call_up_release(trans->callref, CAUSE_NORMAL);
		trans->callref = 0;
		destroy_transaction(trans);
		trans = NULL;
	}
	if (trans) {
		const char *rufnummer = transaction2rufnummer(trans);
		PDEBUG(DTRANS, DEBUG_NOTICE, "Found alredy pending transaction for subscriber '%s', we are busy!\n", rufnummer);
		return NULL;
	}

	trans = calloc(1, sizeof(*trans));
	if (!trans) {
		PDEBUG(DTRANS, DEBUG_ERROR, "No memory!\n");
		return NULL;
	}

	trans->mo = mo;
	trans->ident = ident;
	trans->futln_nat = futln_nat;
	trans->futln_fuvst = futln_fuvst;
	trans->futln_rest = futln_rest;

	timer_init(&trans->timer, trans_timeout, trans);

	const char *rufnummer = transaction2rufnummer(trans);
	PDEBUG(DTRANS, DEBUG_INFO, "Creating transaction for subscriber '%s'\n", rufnummer);

	/* attach to end of list, so first transaction is served first */
	transp = &trans_list;
	while (*transp)
		transp = &((*transp)->next);
	*transp = trans;

	return trans;
}

/* get free Ident-Number or 0 if not available */
static uint8_t get_free_ident(void)
{
	transaction_t *trans;
	int i;

	/* count as many times as our ID range */
	for (i = IDENT_BSC_FROM; i <= IDENT_BSC_TO; i++) {
		/* select next ident number */
		if (last_bsc_ident == IDENT_BSC_TO)
			last_bsc_ident = IDENT_BSC_FROM;
		else
			last_bsc_ident++;

		/* check if that number is free */
		trans = trans_list;
		while (trans) {
			if (trans->ident == last_bsc_ident)
				break;
			trans = trans->next;
		}

		/* if free (not found), return it */
		if (!trans)
			return last_bsc_ident;
	}

	return 0;
}

static fuvst_t *get_zzk(int num)
{
	sender_t *sender;
	fuvst_t *fuvst;

	for (sender = sender_head; sender; sender = sender->next) {
		fuvst = (fuvst_t *) sender;
		if (fuvst->chan_type != CHAN_TYPE_ZZK)
			continue;
		if (!fuvst->link)
			continue;
		if (!num || num == fuvst->chan_num)
			return fuvst;
	}
	return NULL;
}

static fuvst_t *get_spk(uint8_t Q)
{
	sender_t *sender;
	fuvst_t *fuvst;

	for (sender = sender_head; sender; sender = sender->next) {
		fuvst = (fuvst_t *) sender;
		if (fuvst->chan_type == CHAN_TYPE_SPK
		 && fuvst->chan_num == Q)
			return fuvst;
	}
	return NULL;
}

/* Convert 'Ausloesegrund' of C-Netz base station to ISDN cause */
int cnetz_fufst2cause(uint8_t X)
{
	switch (X) {
	case 0:		/* undefiniert */
	case 1:		/* Einh.A (A-BS) */
	case 2:		/* Einh.B (B-BS) */
		return CAUSE_NORMAL;
	case 3:		/* Tln-besetzt (A-BS) */
		return CAUSE_BUSY;
	case 4:		/* Gassenbesetzt */
	case 5:		/* Time-Out: kein Sprechkanal */
	case 8:		/* WS ist blockiert (Time out) */
		return CAUSE_NOCHANNEL;
	case 9:		/* Funk-Tln nicht aktiv */
		return CAUSE_OUTOFORDER;
	case 27:	/* Kein Melden B (B-BS), Infobox aktiviert */
		return CAUSE_NOANSWER;
	default:	/* alles andere */
		return CAUSE_TEMPFAIL;
	}
}

/* Convert ISDN cause to 'Ausloesegrund' of C-Netz mobile station */
uint8_t cnetz_cause2futln(int cause)
{
	switch (cause) {
	case CAUSE_NORMAL:
	case CAUSE_BUSY:
	case CAUSE_NOANSWER:
		return 1;
	case CAUSE_OUTOFORDER:
	case CAUSE_INVALNUMBER:
	case CAUSE_NOCHANNEL:
	case CAUSE_TEMPFAIL:
	default:
		return 0;
	}
}

/* MTP data message to lower layer */
static int message_send(uint8_t ident, uint8_t opcode, uint8_t *data, int len)
{
	uint8_t slc;
	fuvst_t *zzk;

	/* hunt for ZZK (1 or 2), depending on LSB of ident (load sharing) */
	zzk = get_zzk((ident & 1) + 1);
	/* if not found, hunt for any ZZK */
	if (!zzk)
		zzk = get_zzk(0);
	if (!zzk) {
		PDEBUG(DCNETZ, DEBUG_ERROR, "No ZZK or link down!\n");
		return -EIO;
	}

	uint8_t buffer[len + 2];

	if (debuglevel == DEBUG_DEBUG || opcode != OPCODE_XEDBU)
		PDEBUG(DCNETZ, DEBUG_INFO, "TX Message to BS: link=%s ident=0x%02x OP=%02XH %s\n", zzk->sender.kanal, ident, opcode, debug_hex(data, len));

	/* assemble Ident, swap Opcode and add data */
	slc = ident & 0xf;
	buffer[0] = ident >> 4;
	buffer[1] = (opcode >> 4) | (opcode << 4);
	if (len)
		memcpy(buffer + 2, data, len);
	data = buffer;
	len += 2;

	return mtp_send(&zzk->mtp, MTP_PRIM_DATA, slc, data, len);
}

static void release_for_emergency(void)
{
	sender_t *sender;
	fuvst_t *fuvst;
	transaction_t *trans, *last_trans = NULL;
	uint8_t opcode, *data;
	int len;

	for (sender = sender_head; sender; sender = sender->next) {
		fuvst = (fuvst_t *) sender;
		if (fuvst->chan_type != CHAN_TYPE_SPK)
			continue;
		if (!fuvst->callref)
			break;
		for (trans = trans_list; trans; trans = trans->next) {
			if (trans->spk == fuvst)
				break;
		}
		if (trans && !trans->sonderruf)
			last_trans = trans;
	}

	/* found idle channel */
	if (sender) {
		PDEBUG(DCNETZ, DEBUG_NOTICE, "Emergency call received. We have a free channel available.\n");
		return;
	}

	/* found no normal call (no emergency) */
	if (!last_trans) {
		PDEBUG(DCNETZ, DEBUG_NOTICE, "Emergency call received. We cannot free a channel, because there is no non-emergency call.\n");
		return;
	}

	/* releasing the last call in list */
	PDEBUG(DCNETZ, DEBUG_NOTICE, "Emergency call received. We free a channel.\n");

	len = encode_aau(&opcode, &data, trans->spk_nr, 0, cnetz_cause2futln(CAUSE_NORMAL));
	message_send(trans->ident, opcode, data, len);
	call_up_release(trans->callref, CAUSE_NORMAL);
	trans->callref = 0;
	new_call_state(trans, STATE_RELEASE);
	timer_start(&trans->timer, RELEASE_TO);
}

/* MTP data message from lower layer */
static void message_receive(fuvst_t *zzk, uint8_t ident, uint8_t opcode, uint8_t *data, int len)
{
	transaction_t *trans;
	uint16_t T = 0;
	uint8_t F = 0, U = 0, N = 0;
	uint8_t C = 0;
	uint8_t B = 0;
	uint8_t Q = 0, S = 0;
	uint8_t V = 0;
	uint8_t e = 0;
	uint64_t n = 0, a = 0;
	uint8_t X = 0;
	uint8_t PJ = 0;
	uint16_t D = 0, L = 0;
	uint16_t s = 0;
	uint8_t u = 0, b = 0, l = 0;
	uint16_t T_array[3];
	uint8_t U_array[3], N_array[3], l_array[3];
	int i, num;
	char number[17];

	if (debuglevel == DEBUG_DEBUG || opcode != OPCODE_YLSMF)
		PDEBUG(DCNETZ, DEBUG_INFO, "RX Message from BS: link=%s ident=0x%02x OP=%02XH %s\n", zzk->sender.kanal, ident, opcode, debug_hex(data, len));

	switch (opcode) {
	case OPCODE_SWAF: /* BS restarts */
		decode_swaf(data, len, &V, &N, &U, &F, &C, &B);
		len = encode_swqu(&opcode, &data, 0); /* must be 0, or all subscribers will be kicked */
		message_send(ident, opcode, data, len);
#if 0
		does not work
		/* request database */
		encode_sadau(&opcode);
		message_send(5, opcode, NULL, 0);
#endif
		if (warmstart) {
			PDEBUG(DCNETZ, DEBUG_NOTICE, "Forcing a warm start and load the config...\n");
			warmstart = 0;
			len = encode_yaaau(&opcode, &data, 42);
			message_send(0, opcode, data, len);
		}
		break;
	case OPCODE_SSSAF: /* channel allocation map */
		decode_sssaf(data, len);
		encode_sssqu(&opcode);
		message_send(ident, opcode, data, len);
		break;
	case OPCODE_SSAF: /* lock a channel */
		decode_ssaf(data, len, &S);
		len = encode_ssqu(&opcode, &data, S);
		message_send(ident, opcode, data, len);
		break;
	case OPCODE_SFAF: /* unlock a channel */
		decode_sfaf(data, len, &S);
		len = encode_sfqu(&opcode, &data, S);
		message_send(ident, opcode, data, len);
		break;
	case OPCODE_SUAF: /* set time */
		decode_suaf(data, len, &V, &N, &U, &F, &C, &B);
		len = encode_suqu(&opcode, &data, 1, 0, floor(get_time()));
		message_send(ident, opcode, data, len);
		break;
	case OPCODE_SVAF: /* base station ready */
		decode_svaf(data, len);
		len = encode_svqu(&opcode, &data);
		message_send(ident, opcode, data, len);
		base_station_ready = 1;
		break;
	case OPCODE_YLSAF: /* alarm request */
		decode_ylsaf(data, len);
		if (!alarms)
			break;
		len = encode_ylsmu(&opcode, &data);
		message_send(ident, opcode, data, len);
		break;
	case OPCODE_YLSMF: /* alarm data */
		decode_ylsmf(data, len, &N, &C, &zzk->SM);
		if (N == 4)
			memset(&zzk->SM, 0, sizeof(zzk->SM));
		break;
	case OPCODE_YLSEF: /* alarm list ends */
		decode_ylsef(data, len);
		break;
	case OPCODE_STDAF: /* billing information */
		decode_stdaf(data, len);
		/* active table */
		billing_send(ident, 3);
		/* passive table */
		billing_send(ident, 4);
		break;
	case OPCODE_EBAF: /* enter BS (inscription) */
		decode_ebaf(data, len, &T, &U, &N, &s, &u, &b, &l);
		add_db(N, U, T, l);
		len = encode_ebpqu(&opcode, &data);
		message_send(ident, opcode, data, len);
		break;
	case OPCODE_ABAF: /* leave BS */
		decode_abaf(data, len, &T, &U, &N);
		remove_db(N, U, T);
		break;
	case OPCODE_GVAF: /* MO call */
		decode_gvaf(data, len, &T, &U, &N, number);
		PDEBUG(DCNETZ, DEBUG_INFO, "Call from mobile.\n");
		goto outgoing;
	case OPCODE_GVWAF: /* MO call (queue) */
		decode_gvaf(data, len, &T, &U, &N, number);
		PDEBUG(DCNETZ, DEBUG_INFO, "Call from mobile (queue).\n");
outgoing:
		trans = create_transaction(ident, N, U, T, 1);
		if (!trans) {
			/* reject, if no transaction */
			len = encode_gvnqu(&opcode, &data, 0, cnetz_cause2futln(CAUSE_TEMPFAIL));
			message_send(ident, opcode, data, len);
			break;
		}
		strcpy(trans->number, number);
		trans->sonderruf = check_emerg(number);
		new_call_state(trans, (opcode == OPCODE_GVAF) ? STATE_MO : STATE_MO_QUEUE);
		len = encode_gvpqu(&opcode, &data, trans->sonderruf, authentication);
		message_send(ident, OPCODE_GVPQU, data, len);
		/* release a call, if all SPK are in use */
		release_for_emergency();
		break;
	case OPCODE_KVWQF: /* MT call ack (queue) */
		decode_kvwqf(data, len);
		trans = search_transaction_ident(ident);
		if (!trans)
			break;
		PDEBUG(DCNETZ, DEBUG_INFO, "Call to mobile is alerting (queue).\n");
		new_call_state(trans, STATE_MT_QUEUE);
		if (trans->callref)
			call_up_alerting(trans->callref);
		break;
	case OPCODE_KVBAF: /* MT call answer */
		decode_kvbaf(data, len);
		trans = search_transaction_ident(ident);
		if (!trans)
			break;
		PDEBUG(DCNETZ, DEBUG_INFO, "Call to mobile has been answered.\n");
		new_call_state(trans, STATE_MT_CONNECT);
		if (trans->callref)
			call_up_answer(trans->callref, transaction2rufnummer(trans));
		break;
	case OPCODE_STAF: /* assign channel */
		decode_staf(data, len, &Q, &V, &e, &n);
		trans = search_transaction_ident(ident);
		if (!trans) {
			/* race condition: we may get a channel after releaseing a call, so we must release that channel */
			len = encode_aau(&opcode, &data, Q, 0, cnetz_cause2futln(CAUSE_NOCHANNEL));
			message_send(ident, opcode, data, len);
			break;
		}
		trans->spk = get_spk(Q);
		trans->spk_nr = Q;
		/* SPK not exist, release */
		if (!trans->spk) {
			PDEBUG(DCNETZ, DEBUG_ERROR, "SpK '%d' requested by BS not configured, please configure all SpK that base station has avaiable!\n", Q);
			len = encode_stnqu(&opcode, &data, Q);
			message_send(ident, opcode, data, len);
			if (trans->callref)
				call_up_release(trans->callref, CAUSE_NORMAL);
			trans->callref = 0;
			destroy_transaction(trans);
			break;
		}
		if (trans->mo)
			len = encode_stpqu(&opcode, &data, Q, 1, 0, 0, 0, 0, 0, 1);
		else
			len = encode_stpqu(&opcode, &data, Q, 0, 0, 0, 0, 0, 0, 0);
		message_send(ident, opcode, data, len);
		/* no callref == outgoing call */
		if (!trans->callref) {
			PDEBUG(DCNETZ, DEBUG_INFO, "Setup call to network. (Ident = %d, FuTln=%s, number=%s)\n", ident, transaction2rufnummer(trans), trans->number);
			trans->callref = trans->old_callref = call_up_setup(transaction2rufnummer(trans), trans->number, OSMO_CC_NETWORK_CNETZ_NONE, "");
		} else {
			PDEBUG(DCNETZ, DEBUG_NOTICE, "Call to mobile is alerting.\n");
			new_call_state(trans, STATE_MT_ALERTING);
			call_up_alerting(trans->callref);
		}
		trans->spk->callref = trans->callref;
		PDEBUG(DCNETZ, DEBUG_INFO, "Assigned SpK %d to call.\n", trans->spk_nr);
		break;
	case OPCODE_APF: /* auth response */
		decode_apf(data, len, &Q, &a);
		break;
	case OPCODE_FAF: /* MCID request */
		decode_faf(data, len);
		PDEBUG(DCNETZ, DEBUG_NOTICE, "Fangen (MCID) was activated by BS.\n");
		break;
	case OPCODE_NAF: /* incoming release (before SPK assignment) */
		decode_naf(data, len, &X);
		len = encode_equ(&opcode, &data);
		message_send(ident, opcode, data, len);
		/* get transaction */
		trans = search_transaction_ident(ident);
		if (!trans)
			break;
		PDEBUG(DCNETZ, DEBUG_NOTICE, "Call released by BS.\n");
		new_call_state(trans, STATE_RELEASE);
		if (trans->callref)
			call_up_release(trans->callref, cnetz_fufst2cause(X));
		trans->callref = 0;
		destroy_transaction(trans);
		break;
	case OPCODE_EQF: /* outgoing release acknowledge (after SPK assignment) */
		decode_eqf(data, len);
		/* get transaction, should not exist anymore */
		trans = search_transaction_ident(ident);
		if (!trans)
			break;
		if (trans->callref)
			call_up_release(trans->callref, CAUSE_NORMAL);
		trans->callref = 0;
		destroy_transaction(trans);
		break;
	case OPCODE_AAF: /* incoming release (after SPK assignment) */
		decode_aaf(data, len, &Q, &X);
		/* get transaction */
		trans = search_transaction_ident(ident);
		if (!trans) {
			/* race condition: if we have a release collision, we don't need to ack */
			break;
		}
		len = encode_aqu(&opcode, &data, Q);
		message_send(ident, opcode, data, len);
		if (trans->callref)
			call_up_release(trans->callref, cnetz_fufst2cause(X));
		PDEBUG(DCNETZ, DEBUG_NOTICE, "Call released by BS.\n");
		new_call_state(trans, STATE_RELEASE);
		trans->callref = 0;
		destroy_transaction(trans);
		break;
	case OPCODE_AQF: /* outgoing release acknowledge (after SPK assignment) */
		decode_aqf(data, len, &Q);
		/* get transaction, should not exist anymore */
		trans = search_transaction_ident(ident);
		if (!trans)
			break;
		if (trans->callref)
			call_up_release(trans->callref, CAUSE_NORMAL);
		trans->callref = 0;
		destroy_transaction(trans);
		break;
	case OPCODE_XADBF: /* transfer config */
		decode_xadbf(data, len, &PJ, &D, &L);
		config_send(ident, PJ, D, L);
		break;
	case OPCODE_SADQF: /* transfer of inscription list (aktivdatei) */
		num = decode_sadqf(data, len, &s, &e, l_array, T_array, U_array, N_array);
		if (s == 0)
			flush_db();
		for (i = 0; i < num; i++)
			add_db(N_array[i], U_array[i], T_array[i], l_array[i]);
		len = encode_ebpqu(&opcode, &data);
		message_send(ident, opcode, data, len);
		break;
	default:
		PDEBUG(DCNETZ, DEBUG_INFO, "RX Message from BS with unknown OPcode: %02XH\n", opcode);
	}
}

/* receive MTP primitive from lower layer */
static void mtp_receive(void *inst, enum mtp_prim prim, uint8_t slc, uint8_t *data, int len)
{
	fuvst_t *zzk = (fuvst_t *)inst;
	uint8_t ident;
	uint8_t opcode;
	const char *cause_text;

	switch (prim) {
	case MTP_PRIM_OUT_OF_SERVICE:
		switch (*data) {
		case MTP_CAUSE_ALIGNMENT_TIMEOUT:
			cause_text =  "MTP link '%s' alignment timeout! Trying again.\n";
			break;
		case MTP_CAUSE_LINK_FAILURE_LOCAL:
			cause_text =  "MTP link '%s' from remote to local failed! Starting recovery.\n";
			break;
		case MTP_CAUSE_LINK_FAILURE_REMOTE:
			cause_text =  "MTP link '%s' from local to remote failed! Starting recovery.\n";
			break;
		case MTP_CAUSE_PROVING_FAILURE_LOCAL:
			cause_text =  "MTP link '%s' proving failed locally! Trying again.\n";
			break;
		case MTP_CAUSE_PROVING_FAILURE_REMOTE:
			cause_text =  "MTP link '%s' proving failed remotely! Trying again.\n";
			break;
		case MTP_CAUSE_PROVING_TIMEOUT:
			cause_text =  "MTP link '%s' proving timeout! Trying again.\n";
			break;
		default:
			cause_text =  "MTP link '%s' failed! Trying again.\n";
		}
		PDEBUG(DCNETZ, DEBUG_NOTICE, cause_text, zzk->sender.kanal);
		mtp_send(&zzk->mtp, MTP_PRIM_START, 0, NULL, 0);
		zzk->link = 0;
		display_status();
		break;
	case MTP_PRIM_IN_SERVICE:
		PDEBUG(DCNETZ, DEBUG_NOTICE, "Link '%s' established.\n", zzk->sender.kanal);
		zzk->link = 1;
		display_status();
		break;
	case MTP_PRIM_REMOTE_PROCESSOR_OUTAGE:
		PDEBUG(DCNETZ, DEBUG_NOTICE, "Link '%s' indicates remote processor outage.\n", zzk->sender.kanal);
		break;
	case MTP_PRIM_REMOTE_PROCESSOR_RECOVERED:
		PDEBUG(DCNETZ, DEBUG_NOTICE, "Link '%s' indicates remote processor outage is recovered.\n", zzk->sender.kanal);
		break;
	case MTP_PRIM_DATA:
		if (len < 2) {
			PDEBUG(DCNETZ, DEBUG_NOTICE, "No Opcode, message too short!\n");
			return;
		}

		/* assemble Ident swap Opcode and remove from data */
		ident = slc | ((*data++) << 4);
		opcode = (*data >> 4) | (*data << 4);
		data++;
		len -= 2;

		message_receive(zzk, ident, opcode, data, len);
		break;
	default:
		break;
	}
}

/* Create transceiver instance and link to a list. */
int fuvst_create(const char *kanal, enum fuvst_chan_type chan_type, const char *audiodev, int samplerate, double rx_gain, double tx_gain, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, const char *read_tx_wave, int loopback, int ignore_link_monitor, uint8_t sio, uint16_t local_pc, uint16_t remote_pc)
{
	fuvst_t *fuvst;
	int rc;
	char chan_name[64];

	if (chan_type == CHAN_TYPE_ZZK)
		sprintf(chan_name, "ZZK-%s", kanal);
	else
		sprintf(chan_name, "SPK-%s", kanal);

	fuvst = calloc(1, sizeof(fuvst_t));
	if (!fuvst) {
		PDEBUG(DCNETZ, DEBUG_ERROR, "No memory!\n");
		return -ENOMEM;
	}

	PDEBUG(DCNETZ, DEBUG_DEBUG, "Creating 'C-Netz' instance for 'Kanal' = %s (sample rate %d).\n", chan_name, samplerate);

	/* init general part of transceiver */
	/* do not enable emphasis, since it is done by fuvst code, not by common sender code */
	rc = sender_create(&fuvst->sender, strdup(chan_name), 0, 0, audiodev, 0, samplerate, rx_gain, tx_gain, 0, 0, write_rx_wave, write_tx_wave, read_rx_wave, read_tx_wave, loopback, PAGING_SIGNAL_NONE);
	if (rc < 0) {
		PDEBUG(DCNETZ, DEBUG_ERROR, "Failed to init transceiver process!\n");
		goto error;
	}
	fuvst->chan_num = atoi(kanal);
	fuvst->chan_type = chan_type;

	sender_set_fm(&fuvst->sender, 1.0, 4000.0, 1.0, 1.0);

	if (fuvst->chan_type == CHAN_TYPE_ZZK) {
		/* create link */
		rc = mtp_init(&fuvst->mtp, strdup(chan_name), fuvst, mtp_receive, 4800, ignore_link_monitor, sio, local_pc, remote_pc);
		if (rc < 0)
			goto error;
		/* power on stack */
		mtp_send(&fuvst->mtp, MTP_PRIM_POWER_ON, 0, NULL, 0);
		/* use emegency alignment */
		mtp_send(&fuvst->mtp, MTP_PRIM_EMERGENCY, 0, NULL, 0);
		/* start link */
		mtp_send(&fuvst->mtp, MTP_PRIM_START, 0, NULL, 0);

		/* create modem */
		rc = v27_modem_init(&fuvst->modem, fuvst, send_bit, receive_bit, samplerate, 1);
		if (rc < 0)
			goto error;
	}

	PDEBUG(DCNETZ, DEBUG_NOTICE, "Created 'Kanal' %s\n", chan_name);

	display_status();

	return 0;

error:
	fuvst_destroy(&fuvst->sender);

	return rc;
}

/* Destroy transceiver instance and unlink from list. */
void fuvst_destroy(sender_t *sender)
{
	fuvst_t *fuvst = (fuvst_t *) sender;

	PDEBUG(DCNETZ, DEBUG_DEBUG, "Destroying 'C-Netz' instance for 'Kanal' = %s.\n", sender->kanal);

	if (fuvst->chan_type == CHAN_TYPE_ZZK) {
		mtp_exit(&fuvst->mtp);

		v27_modem_exit(&fuvst->modem);
	}

	sender_destroy(&fuvst->sender);
	free(fuvst);
}

void sender_send(sender_t *sender, sample_t *samples, uint8_t *power, int length)
{
        fuvst_t *fuvst = (fuvst_t *) sender;

        memset(power, 1, length);

	if (fuvst->chan_type == CHAN_TYPE_ZZK)
		v27_modem_send(&fuvst->modem, samples, length);
	else
		jitter_load(&fuvst->sender.dejitter, samples, length);
}

void sender_receive(sender_t *sender, sample_t *samples, int length, double __attribute__((unused)) rf_level_db)
{
	fuvst_t *fuvst = (fuvst_t *) sender;
	sample_t *spl;
        int pos;
        int i;

	if (fuvst->chan_type == CHAN_TYPE_ZZK)
		v27_modem_receive(&fuvst->modem, samples, length);
	else {
		/* Forward audio to network (call process). */
		if (fuvst->callref) {
			int count;

			count = samplerate_downsample(&fuvst->sender.srstate, samples, length);
			spl = fuvst->sender.rxbuf;
			pos = fuvst->sender.rxbuf_pos;
			for (i = 0; i < count; i++) {
				spl[pos++] = samples[i];
				if (pos == 160) {
					call_up_audio(fuvst->callref, spl, 160);
					pos = 0;
				}
			}
			fuvst->sender.rxbuf_pos = pos;
		} else
			fuvst->sender.rxbuf_pos = 0;
	}
}

/* Receive audio from call instance. */
void call_down_audio(int callref, sample_t *samples, int count)
{
	sender_t *sender;
	fuvst_t *fuvst;

	for (sender = sender_head; sender; sender = sender->next) {
		fuvst = (fuvst_t *) sender;
		if (fuvst->callref == callref)
			break;
	}
	if (!sender)
		return;

	if (fuvst->callref) {
		sample_t up[(int)((double)count * fuvst->sender.srstate.factor + 0.5) + 10];
		count = samplerate_upsample(&fuvst->sender.srstate, samples, count, up);
		jitter_save(&fuvst->sender.dejitter, up, count);
	}
}

void call_down_clock(void) {}

int call_down_setup(int callref, const char __attribute__((unused)) *caller_id, enum number_type __attribute__((unused)) caller_type, const char *dialing)
{
	uint8_t futln_nat;
	uint8_t futln_fuvst;
	int futln_rest; /* use int for checking size > 65535 */
	int len;
	int i;
	transaction_t *trans;
	uint8_t ident;
	uint8_t opcode, *data;

	/* 1. check if number is invalid, return INVALNUMBER */
	len = strlen(dialing);
	if (len >= 11 && !strncmp(dialing, "0161", 4)) {
		dialing += 4;
		len -= 4;
	}
	if (len < 7 || len > 8) {
inval:
		PDEBUG(DCNETZ, DEBUG_NOTICE, "Outgoing call to invalid number '%s', rejecting!\n", dialing);
		return -CAUSE_INVALNUMBER;
	}
	for (i = 0; i < len; i++) {
		if (dialing[i] < '0' || dialing[i] > '9')
			goto inval;
	}

	futln_nat = dialing[0] - '0';
	if (len == 7)
		futln_fuvst = dialing[1] - '0';
	else {
		futln_fuvst = (dialing[1] - '0') * 10 + (dialing[2] - '0');
		if (futln_fuvst > 31) {
			PDEBUG(DCNETZ, DEBUG_NOTICE, "Digit 2 and 3 '%02d' must not exceed '31', but they do!\n", futln_fuvst);
			goto inval;
		}
	}
	futln_rest = atoi(dialing + len - 5);
	if (futln_rest > 65535) {
		PDEBUG(DCNETZ, DEBUG_NOTICE, "Last 5 digits '%05d' must not exceed '65535', but they do!\n", futln_rest);
		goto inval;
	}

	/* 2. base station ready? */
	if (!base_station_ready) {
		PDEBUG(DCNETZ, DEBUG_NOTICE, "Outgoing call not possible, base station not ready, rejecting!\n");
		return -CAUSE_TEMPFAIL;
	}

	/* 3. create transaction */
	ident = get_free_ident();
	if (!ident) {
		PDEBUG(DCNETZ, DEBUG_NOTICE, "Outgoing call not possible, no free Ident code?? What the hack?\n");
		return -CAUSE_TEMPFAIL;
	}
	trans = create_transaction(ident, futln_nat, futln_fuvst, futln_rest, 0);
	if (!trans) {
		PDEBUG(DCNETZ, DEBUG_NOTICE, "Outgoing call not possible, Transaction already exists: Subscriber busy!\n");
		return -CAUSE_BUSY;
	}
	trans->callref = trans->old_callref = callref;

	/* 4. start call */
	len = encode_kvau(&opcode, &data, futln_rest, futln_fuvst, futln_nat, 0, authentication);
	message_send(trans->ident, opcode, data, len);
	PDEBUG(DCNETZ, DEBUG_INFO, "Send call for mobile towards BS. (Ident = %d, FuTln=%s)\n", ident, transaction2rufnummer(trans));
	new_call_state(trans, STATE_MT);

	return 0;
}

void call_down_answer(int callref)
{
	transaction_t *trans;
	uint8_t opcode, *data;
	int len;

	trans = search_transaction_callref(callref);
	if (!trans) {
		PDEBUG(DCNETZ, DEBUG_NOTICE, "Answer to unknown callref.\n");
		return;
	}

	len = encode_gstau(&opcode, &data, trans->spk_nr, 1, 0, 1, 1, 0);
	message_send(trans->ident, opcode, data, len);
	new_call_state(trans, STATE_MO_CONNECT);
}

static void _disconnect_release(transaction_t *trans, int callref, int cause)
{
	uint8_t opcode, *data;
	int len;

	if (trans->spk) {
		/* release after SPK assignment */
		len = encode_aau(&opcode, &data, trans->spk_nr, 0, cnetz_cause2futln(cause));
		message_send(trans->ident, opcode, data, len);
	} else {
		/* release before SPK assignment */
		len = encode_nau(&opcode, &data, 0, cnetz_cause2futln(cause));
		message_send(trans->ident, opcode, data, len);
	}

	if (callref)
		call_up_release(callref, cause);
	trans->callref = 0;
	new_call_state(trans, STATE_RELEASE);
	timer_start(&trans->timer, RELEASE_TO);
}

/* Call control sends disconnect (with tones).
 * An active call stays active, so tones and annoucements can be received
 * by mobile station.
 */
void call_down_disconnect(int callref, int cause)
{
	transaction_t *trans;

	PDEBUG(DCNETZ, DEBUG_INFO, "Call has been disconnected by network.\n");

	trans = search_transaction_callref(callref);
	if (!trans) {
		PDEBUG(DCNETZ, DEBUG_NOTICE, "Outgoing disconnect to unknown callref.\n");
		call_up_release(callref, CAUSE_INVALCALLREF);
		return;
	}

	/* already a channel, so we stay on to listen to disconnect tones */
	if (trans->state != STATE_MT
	 && trans->state != STATE_MT_QUEUE
	 && trans->state != STATE_MT_ALERTING)
		return;

	_disconnect_release(trans, callref, cause);

}

/* Call control releases call toward mobile station. */
void call_down_release(int callref, int cause)
{
	transaction_t *trans;

	PDEBUG(DCNETZ, DEBUG_INFO, "Call has been released by network.\n");

	trans = search_transaction_callref(callref);
	if (!trans) {
		PDEBUG(DCNETZ, DEBUG_NOTICE, "Outgoing released to unknown callref.\n");
		call_up_release(callref, CAUSE_INVALCALLREF);
		return;
	}

	_disconnect_release(trans, 0, cause);
}

void dump_info(void)
{
	cnetz_db_t *db = cnetz_db_head;

	PDEBUG(DDB, DEBUG_NOTICE, "Dump of subscriber database:\n");
	if (!db) {
		PDEBUG(DDB, DEBUG_NOTICE, " - No subscribers attached!\n");
		return;
	}

	while (db) {
		PDEBUG(DDB, DEBUG_NOTICE, " - Subscriber '%d,%d,%d' is attached.\n", db->futln_nat, db->futln_fuvst, db->futln_rest);
		db = db->next;
	}
}

