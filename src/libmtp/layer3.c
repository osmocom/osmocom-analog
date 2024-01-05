/* Jolly's implementation of MTP layer 3
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
 * This is only a minimal implementation to make a C-Netz-BTS working.
 */

#define CHAN mtp->name

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <osmocom/core/timer.h>
#include <osmocom/core/utils.h>
#include "../liblogging/logging.h"
#include "mtp.h"

/* message from layer 4 */
int mtp_send(mtp_t *mtp, enum mtp_prim prim, uint8_t slc, uint8_t *data, int len)
{
	uint8_t buffer[len + 4];

	if (prim == MTP_PRIM_DATA) {
		LOGP_CHAN(DMTP3, LOGL_DEBUG, "Send frame to remote: SIO=0x%02x DPC=%d OPC=%d SLC=%d %s\n", mtp->sio, mtp->remote_pc, mtp->local_pc, slc, osmo_hexdump(data, len));
		/* add header */
		buffer[0] = mtp->remote_pc;
		buffer[1] = (mtp->remote_pc >> 8) & 0x3f;
		buffer[1] |= (mtp->local_pc << 6) & 0xc0;
		buffer[2] = mtp->local_pc >> 2;
		buffer[3] = (mtp->local_pc >> 10) & 0x0f;
		buffer[3] |= slc << 4;

		/* add payload */
		if (len)
			memcpy(buffer + 4, data, len);
		data = buffer;
		len += 4;
	}

	/* transmit */
	return mtp_l3l2(mtp, prim, mtp->sio, data, len);
}

/* message from layer 2 */
void mtp_l2l3(mtp_t *mtp, enum mtp_prim prim, uint8_t sio, uint8_t *data, int len)
{
	uint16_t dpc, opc;
	uint8_t slc = 0;

	if (prim == MTP_PRIM_DATA) {
		if (len < 4) {
			LOGP_CHAN(DMTP3, LOGL_NOTICE, "Short frame from layer 2 (len=%d)\n", len);
			return;
		}

		/* parse header */
		dpc = data[0];
		dpc |= (data[1] << 8) & 0x3f00;
		opc = data[1] >> 6;
		opc |= data[2] << 2;
		opc |= (data[3] << 10) & 0x3c00;
		slc = data[3] >> 4;
		data += 4;
		len -= 4;

		LOGP_CHAN(DMTP3, LOGL_DEBUG, "Received frame from remote: SIO=0x%02x DPC=%d OPC=%d SLC=%d %s\n", sio, dpc, opc, slc, osmo_hexdump(data, len));

		if (dpc != mtp->local_pc || opc != mtp->remote_pc) {
			LOGP_CHAN(DMTP3, LOGL_NOTICE, "Received message with wrong point codes: %d->%d but expecting %d->%d\n", opc, dpc, mtp->remote_pc, mtp->local_pc);
			return;
		}
		if ((sio & 0x0f) == 0x0 && len >= 1) {
			LOGP_CHAN(DMTP3, LOGL_NOTICE, "MGMT message received: SLC=%d H0=%d H1=%d %s\n", slc, data[0] & 0xf, data[0] >> 4, osmo_hexdump(data + 1, len - 1));
			return;
		}
		if (sio != mtp->sio) {
			LOGP_CHAN(DMTP3, LOGL_NOTICE, "Received message with wrong SIO: 0x%02x but expecting 0x%02x\n", sio, mtp->sio);
			return;
		}
	}

	/* receive */
	mtp->mtp_receive(mtp->inst, prim, slc, data, len);
}

