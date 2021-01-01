/* Jolly's implementation of MTP layer 2
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

/*
 * This implementation is incomplete, especially:
 *
 * - No Cyclic preventive retransmission
 * - No timers when transmitting/receiving
 * - No flow control.
 */

#define CHAN mtp->name

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include "../libtimer/timer.h"
#include "../libdebug/debug.h"
#include "crc16.h"
#include "mtp.h"

#define STATUS_O	0
#define STATUS_N	1
#define STATUS_E	2
#define STATUS_OS	3
#define STATUS_PO	4
#define STATUS_B	5

#define T1_64k	50.0
#define T1_4k8	600.0
#define T1	((mtp->bitrate >= 64000) ? T1_64k : T1_4k8)
#define T2	30.0
#define T3	2.0
#define T4n_64k	8.2
#define T4n_4k8	110.0
#define T4n	((mtp->bitrate >= 64000) ? T4n_64k : T4n_4k8)
#define T4e_64k	0.5
#define T4e_4k8	7.0
#define T4e	((mtp->bitrate >= 64000) ? T4e_64k : T4e_4k8)

#define Tin	4
#define Tie	1
#define Ti	((mtp->local_emergency | mtp->remote_emergency) ? Tie : Tin)
#define M	5
#define N	16
#define T_64k	64
#define T_4k8	32
#define T	((mtp->bitrate >= 64000) ? T_64k : T_4k8)
#define D	256

const char *mtp_sf_names[8] = {
	"SIO (Out of alignment)",
	"SIN (Normal alignment)",
	"SIE (Emergency alignment)",
	"SIOS (Out of Service)",
	"SIOP (Processor Outage)",
	"SIB (Busy)",
	"<unknown 6>",
	"<unknown 7>",
};

const char *mtp_state_names[] = {
	"Power off",
	"Out of service",
	"Not aligned",
	"Aligned",
	"Proving",
	"Aligned ready",
	"Aligned not ready",
	"In service",
	"Processor outage",
};

const char *mtp_prim_names[] = {
	"Power on",
	"Emergency",
	"Emergency ceases",
	"Local processor outage",
	"Local processor recovered",
	"Remote processor outage",
	"Remote processor recovered",
	"Start",
	"Stop",
	"Data",
	"In service",
	"Out of service",
	"SIOS",
	"SIO",
	"SIN",
	"SIE",
	"SIPO",
	"SIB",
	"MSU",
	"FISU",
	"T1 timeout",
	"T2 timeout",
	"T3 timeout",
	"T4 timeout",
	"Correct SU",
	"Abort Proving",
	"Link Failure",
};

void mtp_l2_new_state(mtp_t *mtp, enum mtp_l2state state)
{
	if (mtp->l2_state == state)
		return;
	PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Change state '%s' -> '%s'\n", mtp_state_names[mtp->l2_state], mtp_state_names[state]);
	mtp->l2_state = state;
}

/* all "Stop" from L3 (or initial "Power on") */
static void mtp_stop(mtp_t *mtp, enum mtp_prim __attribute__((unused)) prim, uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len)
{
	/* flush pending messages */
	mtp_flush(mtp);

	/* Send SIOS */
	PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Now sending SIOS\n");
	mtp->tx_lssu = STATUS_OS;

	/* Cancel processor outage */
	if (mtp->remote_outage) {
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Cancel remote processor outage\n");
		mtp->remote_outage = 0;
	}

	/* Cancel remote emegency, but keep our local setting */
	mtp->remote_emergency = 0;

	/* stop all timers */
	if (timer_running(&mtp->t1)) {
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Stop timer T1\n");
		timer_stop(&mtp->t1);
	}
	if (timer_running(&mtp->t2)) {
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Stop timer T2\n");
		timer_stop(&mtp->t2);
	}
	if (timer_running(&mtp->t3)) {
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Stop timer T3\n");
		timer_stop(&mtp->t3);
	}
	if (timer_running(&mtp->t4)) {
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Stop timer T4\n");
		timer_stop(&mtp->t4);
	}

	/* reset sequence numbers */
	mtp->tx_queue_seq = mtp->tx_seq = 127;
	mtp->fib = 1;
	mtp->rx_seq = 127;
	mtp->bib = 1;

	/* Out of service */
	mtp_l2_new_state(mtp, MTP_L2STATE_OUT_OF_SERVICE);
}

/* Handling Initial Alignment */

static void mtp_l3_start(mtp_t *mtp, enum mtp_prim __attribute__((unused)) prim, uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len)
{
	/* Send SIO */
	PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Now sending SIO\n");
	mtp->tx_lssu = STATUS_O;

	/* Start T2 */
	PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Start timer T2 for %.3f seconds\n", T2);
	timer_start(&mtp->t2, T2);

	/* reset monitor counters */
	mtp->proving_errors = 0;
	mtp->monitor_errors = 0;
	mtp->monitor_good = 0;

	/* Not aligned */
	mtp_l2_new_state(mtp, MTP_L2STATE_NOT_ALIGNED);
}

static void mtp_t2_timeout(mtp_t *mtp, enum mtp_prim __attribute__((unused)) prim, uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len)
{
	uint8_t cause = MTP_CAUSE_ALIGNMENT_TIMEOUT;

	/* stop process */
	mtp_stop(mtp, prim, data, len);

	/* send message to upper layer */
	PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Telling L3 that we are out of service because of alignment failure\n");
	mtp_l2l3(mtp, MTP_PRIM_OUT_OF_SERVICE, 0, &cause, 1);
}

static void mtp_go_aligned(mtp_t *mtp, enum mtp_prim __attribute__((unused)) prim, uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len)
{
	/* stop timers */
	if (timer_running(&mtp->t2)) {
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Stop timer T2\n");
		timer_stop(&mtp->t2);
	}
	if (timer_running(&mtp->t4)) {
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Stop timer T4\n");
		timer_stop(&mtp->t4);
	}

	if (prim == MTP_PRIM_SIE) {
		/* remember */
		mtp->remote_emergency = 1;
	}

	if (mtp->local_emergency) {
		/* Send SIE */
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Now sending SIE\n");
		mtp->tx_lssu = STATUS_E;
	} else {
		/* Send SIN */
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Now sending SIN\n");
		mtp->tx_lssu = STATUS_N;
	}

	/* Start T3 */
	PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Start timer T3 for %.3f seconds\n", T3);
	timer_start(&mtp->t3, T3);

	/* Aligned */
	mtp_l2_new_state(mtp, MTP_L2STATE_ALIGNED);
}

static void mtp_go_proving(mtp_t *mtp, enum mtp_prim __attribute__((unused)) prim, uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len)
{
	if (prim == MTP_PRIM_SIE) {
		/* remember */
		mtp->remote_emergency = 1;
	}

	/* stop timer */
	if (timer_running(&mtp->t3)) {
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Stop timer T3\n");
		timer_stop(&mtp->t3);
	}

	/* reset proving try counter M */
	mtp->proving_try = 0;

	/* Cancel further proving */
	mtp->further_proving = 0;

	/* Start T4 */
	if (mtp->local_emergency || mtp->remote_emergency) {
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Start timer T4 for %.3f seconds\n", T4e);
		timer_start(&mtp->t4, T4e);
	} else {
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Start timer T4 for %.3f seconds\n", T4n);
		timer_start(&mtp->t4, T4n);
	}

	/* Proving */
	mtp_l2_new_state(mtp, MTP_L2STATE_PROVING);
}

static void mtp_align_fail(mtp_t *mtp, enum mtp_prim __attribute__((unused)) prim, uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len)
{
	uint8_t cause;

	switch (prim){
	case MTP_PRIM_ABORT_PROVING:
		cause = MTP_CAUSE_PROVING_FAILURE_LOCAL;
		break;
	case MTP_PRIM_SIOS:
		cause = MTP_CAUSE_PROVING_FAILURE_REMOTE;
		break;
	case MTP_PRIM_T3_TIMEOUT:
		cause = MTP_CAUSE_PROVING_TIMEOUT;
		break;
	default:
		cause = 0;
	}

	/* stop process */
	mtp_stop(mtp, prim, data, len);

	/* send message to upper layer */
	PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Telling L3 that we are out of service because of alignment failure\n");
	mtp_l2l3(mtp, MTP_PRIM_OUT_OF_SERVICE, 0, &cause, 1);
}

static void mtp_correct_su(mtp_t *mtp, enum mtp_prim __attribute__((unused)) prim, uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len)
{
	/* only continue when further proving is performed */
	if (!mtp->further_proving)
		return;

	/* Stop T4 */
	if (timer_running(&mtp->t4)) {
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Stop timer T4\n");
		timer_stop(&mtp->t4);
	}

	/* Cancel further proving */
	mtp->further_proving = 0;

	/* Start T4 */
	if (mtp->local_emergency || mtp->remote_emergency) {
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Start timer T4 for %.3f seconds\n", T4e);
		timer_start(&mtp->t4, T4e);
	} else {
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Start timer T4 for %.3f seconds\n", T4n);
		timer_start(&mtp->t4, T4n);
	}
}

static void mtp_align_complete(mtp_t *mtp, enum mtp_prim __attribute__((unused)) prim, uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len)
{
	/* if proving failed, further proving flag is set */
	if (mtp->further_proving) {
		/* Cancel further proving */
		mtp->further_proving = 0;

		/* Start T4 */
		if (mtp->local_emergency || mtp->remote_emergency) {
			PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Start timer T4 for %.3f seconds\n", T4e);
			timer_start(&mtp->t4, T4e);
		} else {
			PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Start timer T4 for %.3f seconds\n", T4n);
			timer_start(&mtp->t4, T4n);
		}
		return;
	}

	/* Start T1 */
	PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Start timer T1 for %.3f seconds\n", T1);
	timer_start(&mtp->t1, T1);

	if (mtp->local_outage) {
		/* Send SIPO */
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Local processor outage, now sending SIPO\n");
		mtp->tx_lssu = STATUS_PO;

		/* Aligned */
		mtp_l2_new_state(mtp, MTP_L2STATE_ALIGNED_NOT_READY);
	} else {
		/* Send FISU */
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "No local processor outage, now sending FISU\n");
		mtp->tx_lssu = -1;

		/* Aligned */
		mtp_l2_new_state(mtp, MTP_L2STATE_ALIGNED_READY);
	}
}

static void mtp_abort_proving(mtp_t *mtp, enum mtp_prim __attribute__((unused)) prim, uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len)
{
	/* maximum number of tries */
	if (++mtp->proving_try == M) {
		mtp_align_fail(mtp, prim, data, len);
		return;
	}

	PDEBUG_CHAN(DMTP2, DEBUG_NOTICE, "Proving failed, try again!\n");

	/* Mark further proving */
	mtp->further_proving = 1;
}

static void mtp_proving_emerg(mtp_t *mtp, enum mtp_prim __attribute__((unused)) prim, uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len)
{
	if (prim == MTP_PRIM_EMERGENCY) {
		/* Send SIE */
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Now sending SIE\n");
		mtp->tx_lssu = STATUS_E;
	}

	if (!mtp->local_emergency && !mtp->remote_emergency) {
		/* Stop T4 */
		if (timer_running(&mtp->t4)) {
			PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Stop timer T4\n");
			timer_stop(&mtp->t4);
		}

		/* Cancel further proving */
		mtp->further_proving = 0;

		/* Sart T4 */
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Start timer T4 for %.3f seconds\n", T4e);
		timer_start(&mtp->t4, T4e);
	}

	if (prim == MTP_PRIM_EMERGENCY)
		mtp->local_emergency = 1;
	else
		mtp->remote_emergency = 1;
}

static void mtp_set_emerg(mtp_t *mtp, enum mtp_prim __attribute__((unused)) prim, uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len)
{
	mtp->local_emergency = 1;
}

static void mtp_unset_emerg(mtp_t *mtp, enum mtp_prim __attribute__((unused)) prim, uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len)
{
	mtp->local_emergency = 0;
}

/* Handling Link state control */

static void mtp_link_failure(mtp_t *mtp, enum mtp_prim __attribute__((unused)) prim, uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len)
{
	uint8_t cause;

	/* stop process */
	mtp_stop(mtp, prim, data, len);

	if (prim == MTP_PRIM_LINK_FAILURE)
		cause = MTP_CAUSE_LINK_FAILURE_LOCAL;
	else
		cause = MTP_CAUSE_LINK_FAILURE_REMOTE;
	/* send message to upper layer */
	PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Telling L3 that we are out of service because of link failure\n");
	mtp_l2l3(mtp, MTP_PRIM_OUT_OF_SERVICE, 0, &cause, 1);
}

static void mtp_remote_outage(mtp_t *mtp, enum mtp_prim __attribute__((unused)) prim, uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len)
{
	mtp->remote_outage = 1;
	PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Set remote processor outage\n");

	/* Stop T1 */
	if (timer_running(&mtp->t1)) {
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Stop timer T1\n");
		timer_stop(&mtp->t1);
	}

	/* Processor outage */
	mtp_l2_new_state(mtp, MTP_L2STATE_PROCESSOR_OUTAGE);

	/* send message to upper layer */
	PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Telling L3 about remote processor outage\n");
	mtp_l2l3(mtp, MTP_PRIM_REMOTE_PROCESSOR_OUTAGE, 0, NULL, 0);
}

static void mtp_fisu_msu(mtp_t *mtp, enum mtp_prim __attribute__((unused)) prim, uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len)
{
	/* Stop T1 */
	if (timer_running(&mtp->t1)) {
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Stop timer T1\n");
		timer_stop(&mtp->t1);
	}

	if (mtp->l2_state == MTP_L2STATE_ALIGNED_READY) {
		/* In service */
		mtp_l2_new_state(mtp, MTP_L2STATE_IN_SERVICE);
	} else {
		/* In service */
		mtp_l2_new_state(mtp, MTP_L2STATE_PROCESSOR_OUTAGE);
	}

	/* send message to upper layer */
	PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Telling L3 that we are in service\n");
	mtp_l2l3(mtp, MTP_PRIM_IN_SERVICE, 0, NULL, 0);
}

static void mtp_aligned_outage(mtp_t *mtp, enum mtp_prim __attribute__((unused)) prim, uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len)
{
	mtp->local_outage = 1;

	/* Send SIPO */
	PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Local processor outage, now sending SIPO\n");
	mtp->tx_lssu = STATUS_PO;

	/* Aligned not ready */
	mtp_l2_new_state(mtp, MTP_L2STATE_ALIGNED_NOT_READY);
}

static void mtp_not_aligned_recovered(mtp_t *mtp, enum mtp_prim __attribute__((unused)) prim, uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len)
{
	mtp->local_outage = 0;

	/* Send FISU */
	PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "No local processor outage, now sending FISU\n");
	mtp->tx_lssu = -1;

	/* Aligned ready */
	mtp_l2_new_state(mtp, MTP_L2STATE_ALIGNED_READY);
}

static void mtp_in_service_outage(mtp_t *mtp, enum mtp_prim __attribute__((unused)) prim, uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len)
{
	mtp->local_outage = 1;

	/* Send SIPO */
	PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Local processor outage, now sending SIPO\n");
	mtp->tx_lssu = STATUS_PO;

	/* Processor outage */
	mtp_l2_new_state(mtp, MTP_L2STATE_PROCESSOR_OUTAGE);
}

static void mtp_outage(mtp_t *mtp, enum mtp_prim __attribute__((unused)) prim, uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len)
{
	/* local outage */
	if (prim == MTP_PRIM_LOCAL_PROCESSOR_OUTAGE && !mtp->local_outage) {
		mtp->local_outage = 1;

		/* Send SIPO */
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Local processor outage, now sending SIPO\n");
		mtp->tx_lssu = STATUS_PO;
	}

	/* local recovered */
	if (prim == MTP_PRIM_LOCAL_PROCESSOR_RECOVERED && mtp->local_outage) {
		mtp->local_outage = 0;

		/* Send FISU */
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "No local processor outage, now sending FISU\n");
		mtp->tx_lssu = -1;
	}

	/* remote outage */
	if (prim == MTP_PRIM_SIPO && !mtp->remote_outage) {
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Set remote processor outage\n");
		mtp->remote_outage = 1;

		/* send message to upper layer */
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Telling L3 about remote processor outage\n");
		mtp_l2l3(mtp, MTP_PRIM_REMOTE_PROCESSOR_OUTAGE, 0, NULL, 0);
	}

	/* remote recovered */
	if ((prim == MTP_PRIM_FISU || prim == MTP_PRIM_MSU) && mtp->remote_outage) {
		mtp->remote_outage = 0;
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Cancel remote processor outage\n");

		/* send message to upper layer */
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Telling L3 about remote processor recovered\n");
		mtp_l2l3(mtp, MTP_PRIM_REMOTE_PROCESSOR_RECOVERED, 0, NULL, 0);
	}

	/* remote outage, if local and remote are recovered */
	if (!mtp->local_outage && !mtp->remote_outage) {
		/* In service */
		mtp_l2_new_state(mtp, MTP_L2STATE_IN_SERVICE);
	}
}

static void mtp_set_outage(mtp_t *mtp, enum mtp_prim __attribute__((unused)) prim, uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len)
{
	mtp->local_outage = 1;
}

static void mtp_unset_outage(mtp_t *mtp, enum mtp_prim __attribute__((unused)) prim, uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len)
{
	mtp->local_outage = 0;
}

static void mtp_ignore_lssu(mtp_t __attribute__((unused)) *mtp, enum mtp_prim __attribute__((unused)) prim, uint8_t __attribute__((unused)) *data, int __attribute__((unused)) len)
{
}

#define SBIT(a) (1 << a)
#define ALL_STATES (~0)

static struct statemachine {
	uint32_t	states;
	enum mtp_prim	prim;
	void		(*action)(mtp_t *mtp, enum mtp_prim prim, uint8_t *data, int len);
} statemachine_list[] = {

	{ALL_STATES,
	 MTP_PRIM_POWER_ON, mtp_stop},
	{ALL_STATES,
	 MTP_PRIM_STOP, mtp_stop},

	/* Initial alignment */
	{SBIT(MTP_L2STATE_OUT_OF_SERVICE),
	 MTP_PRIM_START, mtp_l3_start},

	{SBIT(MTP_L2STATE_NOT_ALIGNED),
	 MTP_PRIM_T2_TIMEOUT, mtp_t2_timeout},

	{SBIT(MTP_L2STATE_NOT_ALIGNED),
	 MTP_PRIM_SIOS, mtp_ignore_lssu},
	{SBIT(MTP_L2STATE_NOT_ALIGNED),
	 MTP_PRIM_SIO, mtp_go_aligned},
	{SBIT(MTP_L2STATE_NOT_ALIGNED),
	 MTP_PRIM_SIN, mtp_go_aligned},
	{SBIT(MTP_L2STATE_NOT_ALIGNED),
	 MTP_PRIM_SIE, mtp_go_aligned},

	{SBIT(MTP_L2STATE_ALIGNED),
	 MTP_PRIM_SIO, mtp_ignore_lssu},
	{SBIT(MTP_L2STATE_ALIGNED),
	 MTP_PRIM_SIN, mtp_go_proving},
	{SBIT(MTP_L2STATE_ALIGNED),
	 MTP_PRIM_SIE, mtp_go_proving},

	{SBIT(MTP_L2STATE_ALIGNED),
	 MTP_PRIM_T3_TIMEOUT, mtp_align_fail},

	{SBIT(MTP_L2STATE_ALIGNED),
	 MTP_PRIM_SIOS, mtp_align_fail},

	{SBIT(MTP_L2STATE_PROVING),
	 MTP_PRIM_SIO, mtp_go_aligned},
	{SBIT(MTP_L2STATE_PROVING),
	 MTP_PRIM_SIN, mtp_ignore_lssu},
	{SBIT(MTP_L2STATE_PROVING),
	 MTP_PRIM_SIE, mtp_ignore_lssu},

	{SBIT(MTP_L2STATE_PROVING),
	 MTP_PRIM_CORRECT_SU, mtp_correct_su},

	{SBIT(MTP_L2STATE_PROVING),
	 MTP_PRIM_T4_TIMEOUT, mtp_align_complete},

	{SBIT(MTP_L2STATE_PROVING),
	 MTP_PRIM_SIOS, mtp_align_fail},

	{SBIT(MTP_L2STATE_PROVING),
	 MTP_PRIM_ABORT_PROVING, mtp_abort_proving},

	{SBIT(MTP_L2STATE_PROVING),
	 MTP_PRIM_EMERGENCY, mtp_proving_emerg},

	{ALL_STATES,
	 MTP_PRIM_EMERGENCY, mtp_set_emerg},

	{ALL_STATES,
	 MTP_PRIM_EMERGENCY_CEASES, mtp_unset_emerg},

	/* Link state control */
	{SBIT(MTP_L2STATE_ALIGNED_READY) | SBIT(MTP_L2STATE_ALIGNED_NOT_READY) | SBIT(MTP_L2STATE_IN_SERVICE),
	 MTP_PRIM_LINK_FAILURE, mtp_link_failure},
	{SBIT(MTP_L2STATE_ALIGNED_READY) | SBIT(MTP_L2STATE_ALIGNED_NOT_READY) | SBIT(MTP_L2STATE_IN_SERVICE),
	 MTP_PRIM_SIO, mtp_link_failure},
	{SBIT(MTP_L2STATE_ALIGNED_READY) | SBIT(MTP_L2STATE_ALIGNED_NOT_READY) | SBIT(MTP_L2STATE_IN_SERVICE),
	 MTP_PRIM_SIOS, mtp_link_failure},
	{SBIT(MTP_L2STATE_IN_SERVICE),
	 MTP_PRIM_SIN, mtp_link_failure},
	{SBIT(MTP_L2STATE_IN_SERVICE),
	 MTP_PRIM_SIE, mtp_link_failure},

	{SBIT(MTP_L2STATE_ALIGNED_READY) | SBIT(MTP_L2STATE_ALIGNED_NOT_READY),
	 MTP_PRIM_T1_TIMEOUT, mtp_link_failure},

	{SBIT(MTP_L2STATE_ALIGNED_READY) | SBIT(MTP_L2STATE_ALIGNED_NOT_READY) | SBIT(MTP_L2STATE_IN_SERVICE),
	 MTP_PRIM_SIPO, mtp_remote_outage},

	{SBIT(MTP_L2STATE_ALIGNED_READY) | SBIT(MTP_L2STATE_ALIGNED_NOT_READY),
	 MTP_PRIM_FISU, mtp_fisu_msu},
	{SBIT(MTP_L2STATE_ALIGNED_READY) | SBIT(MTP_L2STATE_ALIGNED_NOT_READY),
	 MTP_PRIM_MSU, mtp_fisu_msu},

	{SBIT(MTP_L2STATE_ALIGNED_READY),
	 MTP_PRIM_LOCAL_PROCESSOR_OUTAGE, mtp_aligned_outage},

	{SBIT(MTP_L2STATE_ALIGNED_NOT_READY),
	 MTP_PRIM_LOCAL_PROCESSOR_RECOVERED, mtp_not_aligned_recovered},

	{SBIT(MTP_L2STATE_IN_SERVICE),
	 MTP_PRIM_LOCAL_PROCESSOR_OUTAGE, mtp_in_service_outage},

	{SBIT(MTP_L2STATE_PROCESSOR_OUTAGE),
	 MTP_PRIM_SIPO, mtp_outage},
	{SBIT(MTP_L2STATE_PROCESSOR_OUTAGE),
	 MTP_PRIM_FISU, mtp_outage},
	{SBIT(MTP_L2STATE_PROCESSOR_OUTAGE),
	 MTP_PRIM_MSU, mtp_outage},
	{SBIT(MTP_L2STATE_PROCESSOR_OUTAGE),
	 MTP_PRIM_LOCAL_PROCESSOR_OUTAGE, mtp_outage},
	{SBIT(MTP_L2STATE_PROCESSOR_OUTAGE),
	 MTP_PRIM_LOCAL_PROCESSOR_RECOVERED, mtp_outage},

	{ALL_STATES,
	 MTP_PRIM_LOCAL_PROCESSOR_OUTAGE, mtp_set_outage},

	{ALL_STATES,
	 MTP_PRIM_LOCAL_PROCESSOR_RECOVERED, mtp_unset_outage},
};

#define STATEMACHINE_LEN \
	(sizeof(statemachine_list) / sizeof(struct statemachine))

static void handle_event(mtp_t *mtp, enum mtp_prim prim, uint8_t *data, int len)
{
	int i;

	PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Handling message '%s' in state '%s'\n", mtp_prim_names[prim], mtp_state_names[mtp->l2_state]);

	/* Find function for current state and message */
	for (i = 0; i < (int)STATEMACHINE_LEN; i++)
		if ((prim == statemachine_list[i].prim)
		 && ((1 << mtp->l2_state) & statemachine_list[i].states))
			break;
	if (i == STATEMACHINE_LEN) {
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Message '%s' unhandled at state '%s'\n", mtp_prim_names[prim], mtp_state_names[mtp->l2_state]);
		return;
	}

	return statemachine_list[i].action(mtp, prim, data, len);
}

/* schedule a data frame from upper layer OR call the state machine */
int mtp_l3l2(mtp_t *mtp, enum mtp_prim prim, uint8_t sio, uint8_t *data, int len)
{
	struct mtp_msg *msg, **tailp;

	if (prim != MTP_PRIM_DATA) {
		handle_event(mtp, prim, data, len);
		return -EINVAL;
	}

	if (mtp->l2_state != MTP_L2STATE_IN_SERVICE) {
		PDEBUG_CHAN(DMTP2, DEBUG_ERROR, "Rejecting data message in state '%s'\n", mtp_state_names[mtp->l2_state]);
		return -EIO;
	}

	PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Queueing data message.\n");

	/* go to end of queue */
	tailp = &mtp->tx_queue;
	while (*tailp)
		tailp = &((*tailp)->next);

	/* add new message to queue */
	msg = calloc(sizeof(*msg) + len, 1);
	if (!msg) {
		PDEBUG_CHAN(DMTP2, DEBUG_ERROR, "No mem!\n");
		abort();
	}
	mtp->tx_queue_seq = (mtp->tx_queue_seq + 1) & 0x7f;
	msg->sequence = mtp->tx_queue_seq;
	msg->sio = sio;
	msg->len = len;
	memcpy(msg->data, data, len);
	*tailp = msg;

	return 0;
}

/* send a Fill In Signal Unit */
static void mtp_send_fisu(mtp_t *mtp, uint8_t *bsn, uint8_t *bib, uint8_t *fsn, uint8_t *fib)
{
	/* create frame */
	if (mtp->tx_nack) {
		mtp->bib = 1 - mtp->bib;
		mtp->tx_nack = 0;
	}
	*bsn = mtp->rx_seq;
	*bib = mtp->bib;
	*fsn = mtp->tx_seq;
	*fib = mtp->fib;
}

/* send a Link Status Signal Unit, return 0 to send MSU/FISU */
static int mtp_send_lssu(mtp_t *mtp, uint8_t *bsn, uint8_t *bib, uint8_t *fsn, uint8_t *fib, uint8_t *sf)
{
	if (mtp->tx_lssu < 0)
		return 0;

	*sf = mtp->tx_lssu;

	if (mtp->tx_nack) {
		mtp->bib = 1 - mtp->bib;
		mtp->tx_nack = 0;
	}
	*bsn = mtp->rx_seq;
	*bib = mtp->bib;
	*fsn = mtp->tx_seq;
	*fib = mtp->fib;

	return 1;
}

/* send a Message Signal Unit, return 0 to send FISU */
static int mtp_send_msu(mtp_t *mtp, uint8_t *bsn, uint8_t *bib, uint8_t *fsn, uint8_t *fib, uint8_t *sio, uint8_t *data, int max, int *resending)
{
	struct mtp_msg *msg = mtp->tx_queue;
	int i;

	/* don't send, if remote outage or when aligned */
	if (mtp->l2_state != MTP_L2STATE_IN_SERVICE)
		return 0;

	/* get next message to be sent and check for outstanding messages */
	for (i = 0; i < 128; i++) {
		if (!msg)
			return 0;
		if (msg->sequence == ((mtp->tx_seq + 1) & 0x7f))
			break;
		msg = msg->next;
	}
	if (i == 128) {
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Cannot send MSU, because more than 127 unacknowledged messages have been sent.\n");
		return 0;
	}

	/* set current sequence number */
	mtp->tx_seq = msg->sequence;

	/* mark message as transmitted */
	if (msg->transmitted)
		*resending = 1;
	else {
		*resending = 0;
		msg->transmitted = 1;
	}

	/* create frame */
	if (mtp->tx_nack) {
		mtp->bib = 1 - mtp->bib;
		mtp->tx_nack = 0;
	}
	*bsn = mtp->rx_seq;
	*bib = mtp->bib;
	*fsn = mtp->tx_seq;
	*fib = mtp->fib;
	*sio = msg->sio;
	if (msg->len > max) {
		PDEBUG_CHAN(DMTP2, DEBUG_ERROR, "Message from layer 3 tructated, because of length %d.\n", msg->len);
		msg->len = max;
	}
	memcpy(data, msg->data, msg->len);

	return msg->len;
}

/* subroutine to ack (remove) messages from queue (Sub Clause 5.3.1) */
static int ack_msg(mtp_t *mtp, uint8_t bsn)
{
	struct mtp_msg *msg = mtp->tx_queue, *temp;

	/* search for frame that has been transmitted and acked */
	while (msg) {
		/* is not transmitted, we are done */
		if (!msg->transmitted) {
			msg = NULL;
			break;
		}
		if (msg->sequence == bsn)
			break;
		msg = msg->next;
	}
	if (!msg)
		return 0;

	/* remove all messages up to the one found */
	while (mtp->tx_queue != msg) {
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "ACK: Message with sequence number %d has been acked and is removed.\n", mtp->tx_queue->sequence);
		temp = mtp->tx_queue;
		mtp->tx_queue = temp->next;
		free(temp);
	}

	/* remove the message found */
	PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "ACK: Message with sequence number %d has been acked and is removed.\n", mtp->tx_queue->sequence);
	mtp->tx_queue = msg->next;
	free(msg);

	return 0;
}

/* subroutine to nack (repeat) messages from queue (Sub Clause 5.3.2) */
static int nack_msg(mtp_t *mtp, uint8_t bsn, uint8_t bib)
{
	struct mtp_msg *msg = mtp->tx_queue;

	/* resend message that has sequence number one higher than BSN */
	while (msg) {
		/* if not transmitted, we are done */
		if (!msg->transmitted) {
			msg = NULL;
			break;
		}
		if (msg->sequence == ((bsn + 1) & 0x7f))
			break;
		msg = msg->next;
	}
	if (!msg)
		return 0;

	/* rewind tx_seq to retransmit */
	PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "NACK: Lost messages, retransmitting from sequence number %d.\n", (bsn + 1) & 0x7f);
	mtp->tx_seq = bsn;

	/* flip bit */
	mtp->fib = bib;

	return 0;
}

/* processing a received Link Status Signal Unit */
static void mtp_receive_lssu(mtp_t *mtp, uint8_t fsn, uint8_t bib, uint8_t status)
{
	switch (status) {
	case STATUS_OS:
		handle_event(mtp, MTP_PRIM_SIOS, NULL, 0);
		break;
	case STATUS_O:
		handle_event(mtp, MTP_PRIM_SIO, NULL, 0);
		break;
	case STATUS_N:
		/* Adopt initial sequence numbers: SAE does, so do we */
		mtp->rx_seq = fsn;
		mtp->fib = bib;
		handle_event(mtp, MTP_PRIM_SIN, NULL, 0);
		break;
	case STATUS_E:
		/* Adopt initial sequence numbers: SAE does, so do we */
		mtp->rx_seq = fsn;
		mtp->fib = bib;
		handle_event(mtp, MTP_PRIM_SIE, NULL, 0);
		break;
	case STATUS_PO:
		handle_event(mtp, MTP_PRIM_SIPO, NULL, 0);
		break;
	case STATUS_B:
		handle_event(mtp, MTP_PRIM_SIB, NULL, 0);
		break;
	}
}

/* processing a received Fill In Signal Unit */
static void mtp_receive_fisu(mtp_t *mtp, uint8_t bsn, uint8_t bib, uint8_t fsn, uint8_t fib)
{
	/* when we are just aliged, we need to know when the remote is also aligned */
	if (mtp->l2_state == MTP_L2STATE_ALIGNED_READY
	 || mtp->l2_state == MTP_L2STATE_ALIGNED_NOT_READY
	 || mtp->l2_state == MTP_L2STATE_PROCESSOR_OUTAGE)
		handle_event(mtp, MTP_PRIM_FISU, NULL, 0);

	/* reject, if local outage */
	if (mtp->local_outage) {
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, " -> Ignoring, we have local outage.\n");
		return;
	}

	if (bib == mtp->fib) {
		/* ACK */
		if (ack_msg(mtp, bsn) < 0)
			return;
	} else {
		/* NACK */
		if (nack_msg(mtp, bsn, bib) < 0)
			return;
	}
		
	/* if the FSN is different and received FIB equals last BIB sent */
	if (fsn != mtp->rx_seq && fib == mtp->bib) {
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, " -> Send nack, because we missed a frame and FIB equals last transmitted BIB.\n");
		/* schedule NACK */
		mtp->tx_nack = 1;
	}
}

/* processing a received Message Signal Unit and forward it to layer 3 */
static void mtp_receive_msu(mtp_t *mtp, uint8_t bsn, uint8_t bib, uint8_t fsn, uint8_t fib, uint8_t sio, uint8_t *data, int len)
{
	/* when we are just aliged, we need to know when the remote is also aligned */
	if (mtp->l2_state == MTP_L2STATE_ALIGNED_READY
	 || mtp->l2_state == MTP_L2STATE_ALIGNED_NOT_READY
	 || mtp->l2_state == MTP_L2STATE_PROCESSOR_OUTAGE)
		handle_event(mtp, MTP_PRIM_MSU, NULL, 0);

	/* reject, if local outage */
	if (mtp->local_outage) {
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, " -> Ignoring, we have local outage.\n");
		return;
	}

	/* ack/nack messages of send buffer */
	if (bib == mtp->fib) {
		if (ack_msg(mtp, bsn) < 0)
			return;
	} else {
		if (nack_msg(mtp, bsn, bib) < 0)
			return;
	}

	/* i) if sequence equals last received, drop it, regardless of FIB */
	if (fsn == mtp->rx_seq) {
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, " -> Ignoring, because sequence number did not increase.\n");
		return;
	}

	/* ii) if sequence is one more the last received */
	if (fsn == ((mtp->rx_seq + 1) & 0x7f)) {
		/* if FIB equals last BIB */
		if (fib == mtp->bib) {
			PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, " -> Accepting, because sequence number increases.\n");
			mtp_l2l3(mtp, MTP_PRIM_DATA, sio, data, len);
			/* acknowledge */
			mtp->rx_seq = fsn;
		} else {
			/* discard if not equal */
			PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, " -> Ignoring, because FIB does not equal last transmitted BIB.\n");
		}
		return;
	}

	/* iii) if sequence number is not equal and not one more than the last received,
	 *      a NACK is sent, if FIB equals last BIB */
	if (fib == mtp->bib) {
		/* schedule NACK */
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, " -> Send nack, because we missed a frame and FIB equals last transmitted BIB.\n");
		mtp->tx_nack = 1;
	} else
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, " -> Ignoring, because we missed a frame and FIB dos not equal last transmitted BIB.\n");
}

/*
 * send frame towards layer 1
 * request Signal Unit
 * generate header and add crc
 */
static int mtp_send_frame(mtp_t *mtp, uint8_t *data, int max)
{
	uint8_t bsn, bib, fsn, fib, sf, sio;
	int len, resending;
	uint16_t crc;

	if (mtp_send_lssu(mtp, &bsn, &bib, &fsn, &fib, &sf)) {
		/* transmit LSSU */
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Sending LSSU with status flag '%s'\n", mtp_sf_names[sf]);
		data[3] = sf;
		len = 1;
	} else if ((len = mtp_send_msu(mtp, &bsn, &bib, &fsn, &fib, &sio, data + 4, max - 5, &resending))) {
		/* transmit MSU */
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "%sSending MSU with SIO '%02x' data '%s'\n", (resending) ? "Re-" : "", sio, debug_hex(data + 4, len));
		data[3] = sio;
		len++;
	} else {
		/* transmit FISU */
		mtp_send_fisu(mtp, &bsn, &bib, &fsn, &fib);
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Sending FISU\n");
		len = 0;
	}
	PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, " -> FSN %d, FIB %d, BSN %d, BIB %d\n", fsn, fib, bsn, bib);
	data[0] = (bsn & 0x7f) | (bib << 7);
	data[1] = (fsn & 0x7f) | (fib << 7);
	data[2] = (len > 63) ? 63 : len;
	crc = calc_crc16(data, len + 3);
	data[3 + len] = crc;
	data[4 + len] = crc >> 8;
	return len + 5;
}

/*
 * a frame is received from layer 1
 * check if it is valid and correct or report error
 * forward Signal Unit
 */
void (*func_mtp_receive_lssu)(mtp_t *mtp, uint8_t fsn, uint8_t bib, uint8_t status) = mtp_receive_lssu;
void (*func_mtp_receive_fisu)(mtp_t *mtp, uint8_t bsn, uint8_t bib, uint8_t fsn, uint8_t fib) = mtp_receive_fisu;
void (*func_mtp_receive_msu)(mtp_t *mtp, uint8_t bsn, uint8_t bib, uint8_t fsn, uint8_t fib, uint8_t sio, uint8_t *data, int len) = mtp_receive_msu;
static int mtp_receive_frame(mtp_t *mtp, uint8_t *data, int len)
{
	uint8_t bsn, bib, fsn, fib, li;
	uint16_t crc;

	if (len < 5) {
		/* frame too short */
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Receiving frame is too short (got: %d bytes)\n", len);
		return -EINVAL;
	}
	len -= 5;

	bsn = data[0] & 0x7f;
	bib = data[0] >> 7;
	fsn = data[1] & 0x7f;
	fib = data[1] >> 7;
	li = data[2] & 0x3f;

	if (li != len && (li != 63 || len <= 63)) {
		/* frame wrong length */
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Receiving frame has wrong length (got %d bytes, length %d bytes)\n", li, len);
		return -EINVAL;
	}

	crc = calc_crc16(data, len + 3);
	if (data[3 + len] != (crc & 0xff) || data[4 + len] != (crc >> 8)) {
		/* crc error */
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Receiving frame has wrong CRC\n");
		return -EINVAL;
	}

	if (len == 1)
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Receiving LSSU (length = %d) with status flag 0x%02x (%s)\n", len, data[3], mtp_sf_names[data[3] & 0x7]);
	if (len == 2)
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Receiving LSSU (length = %d) with status flag 0x%04x (%s)\n", len, data[3] | (data[4] << 8), mtp_sf_names[data[3] & 0x7]);
	if (len == 1 || len == 2) {
		/* receive LSSU */
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, " -> FSN %d, FIB %d, BSN %d, BIB %d\n", fsn, fib, bsn, bib);
		func_mtp_receive_lssu(mtp, fsn, bib, data[3] & 0x7);
	} else if (len > 2) {
		/* receive MSU */
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Receiving MSU with SIO '%02x' data '%s'\n", data[3], debug_hex(data + 4, len - 1));
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, " -> FSN %d, FIB %d, BSN %d, BIB %d\n", fsn, fib, bsn, bib);
		func_mtp_receive_msu(mtp, bsn, bib, fsn, fib, data[3], data + 4, len - 1);
	} else {
		/* receive FISU */
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Receiving FISU\n");
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, " -> FSN %d, FIB %d, BSN %d, BIB %d\n", fsn, fib, bsn, bib);
		func_mtp_receive_fisu(mtp, bsn, bib, fsn, fib);
	}

	return 0;
}

/*
 * send bit towards layer 1.
 * bit is transmitted from flag (between frames) and from data.
 * also perform bit stuffing for data to be transmitted
 */
uint8_t mtp_send_bit(mtp_t *mtp)
{
	uint8_t bit;

	/* send flag, before frame (not transmitting) */
	if (!mtp->tx_transmitting) {
		bit = (0x7e >> mtp->tx_bit_count) & 1;
		/* start frame after flag */
		if (++mtp->tx_bit_count == 8) {
			mtp->tx_bit_count = 0;
			/* continuously send flag when power off */
			if (mtp->l2_state == MTP_L2STATE_POWER_OFF)
				return bit;
			mtp->tx_byte_count = 0;
			mtp->tx_frame_len = mtp_send_frame(mtp, mtp->tx_frame, sizeof(mtp->tx_frame));
			/* if no frame, continue with flag (not transmitting) */
			if (mtp->tx_frame_len)
				mtp->tx_transmitting = 1;
			mtp->tx_stream = 0x00;
		}
		return bit;
	}

	/* get first/next byte */
	if (mtp->tx_bit_count == 0)
		mtp->tx_byte = mtp->tx_frame[mtp->tx_byte_count];

	/* get bit of byte */
	bit = (mtp->tx_byte >> mtp->tx_bit_count) & 1;

	/* if 5 bits are '1', add '0' and don't count bits */
	if ((mtp->tx_stream & 0x1f) == 0x1f) {
		mtp->tx_stream = (mtp->tx_stream << 1);
		return 0;
	}
	mtp->tx_stream = (mtp->tx_stream << 1) | bit;

	/* count bits */
	if (++mtp->tx_bit_count == 8) {
		/* next byte or send flag (not transmitting) */
		mtp->tx_bit_count = 0;
		if (++mtp->tx_byte_count == mtp->tx_frame_len)
			mtp->tx_transmitting = 0;
	}

	return bit;
}

#define MONITOR_GOOD		0
#define MONITOR_BAD		-1
#define MONITOR_OCTET_COUNTING	-2

static void mtp_monitor(mtp_t *mtp, int bad)
{
	if (mtp->ignore_monitor)
		return;

	switch (mtp->l2_state) {
	case MTP_L2STATE_PROVING:
		if (!bad) {
			handle_event(mtp, MTP_PRIM_CORRECT_SU, NULL, 0);
			break;
		}
		/* raise error count auntil Ti has been reached */
		mtp->proving_errors++;
		if (bad == MONITOR_BAD)
			PDEBUG(DMTP2, DEBUG_NOTICE, "Proving counter raises to %d/%d due to frame error\n", mtp->proving_errors, Ti);
		else
			PDEBUG(DMTP2, DEBUG_NOTICE, "Proving counter raises to %d/%d due to octet counting\n", mtp->proving_errors, Ti);
		if (mtp->proving_errors == Ti) {
			mtp->proving_errors = 0;
			handle_event(mtp, MTP_PRIM_ABORT_PROVING, NULL, 0);
		}
		break;
	case MTP_L2STATE_ALIGNED_READY:
	case MTP_L2STATE_ALIGNED_NOT_READY:
	case MTP_L2STATE_IN_SERVICE:
	case MTP_L2STATE_PROCESSOR_OUTAGE:
		if (!bad) {
			/* reduce error count after 256 good frames */
			if (++mtp->monitor_good == D) {
				mtp->monitor_good = 0;
				if (mtp->monitor_errors > 0) {
					mtp->monitor_errors--;
					PDEBUG(DMTP2, DEBUG_NOTICE, "Link error counter reduces to %d/%d\n", mtp->monitor_errors, T);
				}
			}
		} else {
			/* raise error count auntil T has been reached */
			mtp->monitor_errors++;
			if (bad == MONITOR_BAD)
				PDEBUG(DMTP2, DEBUG_NOTICE, "Link error counter raises to %d/%d due to frame error\n", mtp->monitor_errors, T);
			else
				PDEBUG(DMTP2, DEBUG_NOTICE, "Link error counter raises to %d/%d due to octet counting\n", mtp->monitor_errors, T);
			if (mtp->monitor_errors == T) {
				mtp->monitor_errors = 0;
				handle_event(mtp, MTP_PRIM_LINK_FAILURE, NULL, 0);
			}
		}
		break;
	default:
		break;
	}
}

/*
 * a bit is received from layer 1.
 * detect flag to delimit next frame
 * remove bit stuffing
 */
void mtp_receive_bit(mtp_t *mtp, uint8_t bit)
{
	int rc;

	/* octet counting */
	if (mtp->rx_octet_counting) {
		if (++mtp->rx_octet_count == 8 * N) {
			/* octet counter hits */
			PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Octet counter hits!\n");
			mtp->rx_octet_count = 0;
			mtp_monitor(mtp, MONITOR_OCTET_COUNTING);
		}
	}
	/* put bit into rx shift register (insert at LSB and shift left) */
	mtp->rx_stream = (mtp->rx_stream << 1) | (bit & 1);
	/* we got a flag: frame ends, frame starts */
	if (mtp->rx_stream == 0x7e) {
		/* only frame between two flags */
		if (mtp->rx_byte_count) {
			// FIXME: check for 8 bit border (crc will hit anyway)
			rc = mtp_receive_frame(mtp, mtp->rx_frame, mtp->rx_byte_count);
			if (rc < 0) {
				/* bad frame */
				mtp_monitor(mtp, MONITOR_BAD);
			} else {
				/* good frame */
				mtp_monitor(mtp, MONITOR_GOOD);
			}
			if (rc == 0 && mtp->rx_octet_counting) {
				/* stop octet counting */
				PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Stop Octet counting, due to correctly received frame\n");
				mtp->rx_octet_counting = 0;
			}
			mtp->rx_flag_count = 0;
		} else {
			if (++mtp->rx_flag_count == 100)
				PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Continuously receiving HDLC flags (\"01111110\"), remote link seems to be down!\n");
		}
		mtp->rx_byte_count = 0;
		mtp->rx_bit_count = 0;
		mtp->rx_receiving = 1;
		return;
	}
	/* if we received 7 bits of '1', abort frame */
	if ((mtp->rx_stream & 0x7f) == 0x7f) {
		mtp->rx_receiving = 0;
		if (mtp->rx_octet_counting)
			return;
		/* start octet counting */
		PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Start Octet counting, due to 7 consecutive bits\n");
		mtp->rx_octet_counting = 1;
		mtp->rx_octet_count = 0;
		return;
	}
	/* if not receiving a frame (i.e. no flag received), drop it */
	if (!mtp->rx_receiving)
		return;
	/* if we received 5 bits of '1' and the 6th bit of '0', drop bit */
	if ((mtp->rx_stream & 0x3f) == 0x3e)
		return;
	/* store bit (shift towards LSB) */
	mtp->rx_byte = (mtp->rx_byte >> 1) | (bit << 7);
	if (++mtp->rx_bit_count == 8) {
		mtp->rx_bit_count = 0;
		/* if maximum length is exceeded, abort frame */
		if (mtp->rx_byte_count == (int)sizeof(mtp->rx_frame)) {
			mtp->rx_receiving = 0;
			/* start octet counting */
			PDEBUG_CHAN(DMTP2, DEBUG_DEBUG, "Start Octet counting, due to frame oversize\n");
			mtp->rx_octet_counting = 1;
			mtp->rx_octet_count = 0;
			return;
		}
		/* store byte */
		mtp->rx_frame[mtp->rx_byte_count++] = mtp->rx_byte;
	}
}

/* layer 1 wants to transmit block of data: the LSB will be sent first */
void mtp_send_block(mtp_t *mtp, uint8_t *data, int len)
{
	int i, j;
	uint8_t out = 0;

	for (i = 0; i < len; i++) {
		for (j = 0; j < 8; j++) {
			out >>= 1;
			out |= mtp_send_bit(mtp) << 7;
		}
		data[i] = out;
	}
}

/* layer 1 received block of data: the LSB was received first */
void mtp_receive_block(mtp_t *mtp, uint8_t *data, int len)
{
	int i, j;
	uint8_t in;

	for (i = 0; i < len; i++) {
		in = data[i];
		for (j = 0; j < 8; j++) {
			mtp_receive_bit(mtp, in & 1);
			in >>= 1;
		}
	}
}

