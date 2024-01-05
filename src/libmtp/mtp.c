/* MTP common functions
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

#define CHAN mtp->name

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <osmocom/core/timer.h>
#include "../liblogging/logging.h"
#include "mtp.h"

static void mtp_t1(void *data)
{
        mtp_t *mtp = data;

	mtp_send(mtp, MTP_PRIM_T1_TIMEOUT, 0, NULL, 0);
}

static void mtp_t2(void *data)
{
        mtp_t *mtp = data;

	mtp_send(mtp, MTP_PRIM_T2_TIMEOUT, 0, NULL, 0);
}

static void mtp_t3(void *data)
{
        mtp_t *mtp = data;

	mtp_send(mtp, MTP_PRIM_T3_TIMEOUT, 0, NULL, 0);
}

static void mtp_t4(void *data)
{
        mtp_t *mtp = data;

	mtp_send(mtp, MTP_PRIM_T4_TIMEOUT, 0, NULL, 0);
}

int mtp_init(mtp_t *mtp, const char *name, void *inst, void (*mtp_receive)(void *inst, enum mtp_prim prim, uint8_t slc, uint8_t *data, int len), int bitrate, int ignore_monitor, uint8_t sio, uint16_t local_pc, uint16_t remote_pc)
{
	memset(mtp, 0, sizeof(*mtp));

	mtp->name = name;
	mtp->inst = inst;
	mtp->mtp_receive = mtp_receive;
	if (bitrate != 64000 && bitrate != 4800) {
		fprintf(stderr, "Wrong bit rate %d, please fix!\n", bitrate);
		abort();
	}
	mtp->bitrate = bitrate;
	mtp->ignore_monitor = ignore_monitor;
	mtp->sio = sio;
	mtp->local_pc = local_pc;
	mtp->remote_pc = remote_pc;
	osmo_timer_setup(&mtp->t1, mtp_t1, mtp);
	osmo_timer_setup(&mtp->t2, mtp_t2, mtp);
	osmo_timer_setup(&mtp->t3, mtp_t3, mtp);
	osmo_timer_setup(&mtp->t4, mtp_t4, mtp);

	return 0;
}

void mtp_exit(mtp_t *mtp)
{
	if (!mtp)
		return;

	osmo_timer_del(&mtp->t1);
	osmo_timer_del(&mtp->t2);
	osmo_timer_del(&mtp->t3);
	osmo_timer_del(&mtp->t4);

	mtp_flush(mtp);
}

void mtp_flush(mtp_t *mtp)
{
	struct mtp_msg *temp;

	while (mtp->tx_queue) {
		temp = mtp->tx_queue;
		mtp->tx_queue = mtp->tx_queue->next;
		free(temp);
	}
}

