/* SIM card sniffer
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

#ifndef ARDUINO

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../libdebug/debug.h"
#include "sim.h"
#include "sniffer.h"

/* Layer 7 */

static void rx_icl_sdu(uint8_t *data, int length)
{
	uint8_t I, cla_ccrc, ins_aprc;
	uint16_t dlng;
	int i;

	if (length < 3) {
		PDEBUG(DSIM7, DEBUG_NOTICE, "Message too short\n");
		return;
	}

	I = *data >> 7;
	cla_ccrc = (*data++ & 0x7f);
	ins_aprc = *data++;
	dlng = *data++;
	length -= 3;

	PDEBUG(DSIM7, DEBUG_INFO, "Layer 7:\n");
	if (I == 0) {
		PDEBUG(DSIM7, DEBUG_INFO, " I = Command\n");
		PDEBUG(DSIM7, DEBUG_INFO, " CLA = 0x%02x\n", cla_ccrc);
		switch (cla_ccrc) {
		case CLA_CNTR:
			PDEBUG(DSIM7, DEBUG_INFO, "  -> CNTR (Control Class)\n");
			break;
		case CLA_STAT:
			PDEBUG(DSIM7, DEBUG_INFO, "  -> STAT (Status Class)\n");
			break;
		case CLA_WRTE:
			PDEBUG(DSIM7, DEBUG_INFO, "  -> WRTE (Write Class)\n");
			break;
		case CLA_READ:
			PDEBUG(DSIM7, DEBUG_INFO, "  -> READ (Read Class)\n");
			break;
		case CLA_EXEC:
			PDEBUG(DSIM7, DEBUG_INFO, "  -> EXEC (Execute Class)\n");
			break;
		case CLA_AUTO:
			PDEBUG(DSIM7, DEBUG_INFO, "  -> AUTO (Authentication Class)\n");
			break;
		default:
			PDEBUG(DSIM7, DEBUG_INFO, "  -> unknown class\n");
			break;
		}
		PDEBUG(DSIM7, DEBUG_INFO, " INS = 0x%02x\n", ins_aprc);
		switch (cla_ccrc) {
		case CLA_CNTR:
			switch (ins_aprc) {
			case SL_APPL:
				PDEBUG(DSIM7, DEBUG_INFO, "  -> SL-APPL (Select Application)\n");
				break;
			case CL_APPL:
				PDEBUG(DSIM7, DEBUG_INFO, "  -> CL-APPL (Close Application)\n");
				break;
			case SH_APPL:
				PDEBUG(DSIM7, DEBUG_INFO, "  -> SH-APPL (Show Application)\n");
				break;
			}
			break;
		case CLA_STAT:
			switch (ins_aprc) {
			case CHK_KON:
				PDEBUG(DSIM7, DEBUG_INFO, "  -> CHK-KCON (Consistency Check)\n");
				break;
			}
			break;
		case CLA_WRTE:
			switch (ins_aprc) {
			case WT_RUFN:
				PDEBUG(DSIM7, DEBUG_INFO, "  -> WR-RUFN (Write Rufnummernsatz)\n");
				break;
			}
			break;
		case CLA_READ:
			switch (ins_aprc) {
			case RD_EBDT:
				PDEBUG(DSIM7, DEBUG_INFO, "  -> RD-EBDT (Read Einbuchdaten)\n");
				break;
			case RD_RUFN:
				PDEBUG(DSIM7, DEBUG_INFO, "  -> RD-RUFN (Read Rufnummernsatz)\n");
				break;
			case RD_GEBZ:
				PDEBUG(DSIM7, DEBUG_INFO, "  -> RD-GEBZ (Read Gebuehrenzaehler)\n");
				break;
			}
			break;
		case CLA_EXEC:
			switch (ins_aprc) {
			case CHK_PIN:
				PDEBUG(DSIM7, DEBUG_INFO, "  -> CHK-PIN (Check PIN)\n");
				break;
			case SET_PIN:
				PDEBUG(DSIM7, DEBUG_INFO, "  -> SET-PIN (Set PIN)\n");
				break;
			case EH_GEBZ:
				PDEBUG(DSIM7, DEBUG_INFO, "  -> EH-GEBZ (Increment Gebuehrenzaehler)\n");
				break;
			case CL_GEBZ:
				PDEBUG(DSIM7, DEBUG_INFO, "  -> CL-GEBZ (Clear Gebuehrenzaehler)\n");
				break;
			}
			break;
		case CLA_AUTO:
			switch (ins_aprc) {
			case AUT_1:
				PDEBUG(DSIM7, DEBUG_INFO, "  -> AUTO-1 (Autorization)\n");
				break;
			}
			break;
		}
	} else {
		PDEBUG(DSIM7, DEBUG_INFO, " I = Response\n");
		PDEBUG(DSIM7, DEBUG_INFO, " CCRC = 0x%02x\n", cla_ccrc);
		if (cla_ccrc & CCRC_PIN_NOK)
			PDEBUG(DSIM7, DEBUG_INFO, "  -> PIN-NOT-OK\n");
		if (cla_ccrc & CCRC_AFBZ_NULL)
			PDEBUG(DSIM7, DEBUG_INFO, "  -> AFBZ = NULL\n");
		if (cla_ccrc & CCRC_APRC_VALID)
			PDEBUG(DSIM7, DEBUG_INFO, "  -> APRC valid\n");
		if (cla_ccrc & 0x08)
			PDEBUG(DSIM7, DEBUG_INFO, "  -> reserved\n");
		if (cla_ccrc & 0x10)
			PDEBUG(DSIM7, DEBUG_INFO, "  -> reserved\n");
		if (cla_ccrc & 0x20)
			PDEBUG(DSIM7, DEBUG_INFO, "  -> reserved\n");
		if (cla_ccrc & CCRC_ERROR)
			PDEBUG(DSIM7, DEBUG_INFO, "  -> GENERAL ERROR\n");
		PDEBUG(DSIM7, DEBUG_INFO, " APRC = 0x%02x\n", ins_aprc);
		if (ins_aprc & APRC_PIN_REQ)
			PDEBUG(DSIM7, DEBUG_INFO, "  -> Bit 2 = 1:PIN-Check required\n");
		else
			PDEBUG(DSIM7, DEBUG_INFO, "  -> Bit 2 = 0:PIN-Check not required\n");
		if (ins_aprc & APRC_APP_LOCKED)
			PDEBUG(DSIM7, DEBUG_INFO, "  -> Bit 3 = 1:Application locked\n");
		else
			PDEBUG(DSIM7, DEBUG_INFO, "  -> Bit 3 = 0:Application unlocked\n");
		if (ins_aprc & APRC_GEBZ_LOCK)
			PDEBUG(DSIM7, DEBUG_INFO, "  -> Bit 5 = 1:GEBZ/RUFN locked\n");
		else
			PDEBUG(DSIM7, DEBUG_INFO, "  -> Bit 5 = 0:GEBZ/RUFN unlocked\n");
		if (ins_aprc & APRC_GEBZ_FULL)
			PDEBUG(DSIM7, DEBUG_INFO, "  -> Bit 6 = 1:GEBZ full\n");
		else
			PDEBUG(DSIM7, DEBUG_INFO, "  -> Bit 6 = 0:GEBZ not full\n");
	}
	if (dlng == 255) {
		PDEBUG(DSIM7, DEBUG_NOTICE, " Unsupported length 255!\n");
		return;
	}
	PDEBUG(DSIM7, DEBUG_INFO, " DLNG = %d\n", dlng);
	if (dlng != length) {
		PDEBUG(DSIM7, DEBUG_NOTICE, " DLNG does not match message body!\n");
		return;
	}

	for (i = 0; i < length; i++) {
		PDEBUG(DSIM7, DEBUG_INFO, " DATA(%d) = 0x%02x '%c' %d\n", i, data[i], (data[i] >= 32 && data[i] <= 126) ? data[i] : '.', data[i]);
	}
}

/* ICL layer */

static void rx_icl_pdu(uint8_t *data, int length)
{
	int icb_count, ext = 1;

	if (ext) {
		if (length < 1) {
			PDEBUG(DSIMI, DEBUG_NOTICE, "Message too short\n");
			return;
		}

		PDEBUG(DSIMI, DEBUG_INFO, "Interface control layer ICB1:\n");
		if (*data & ICB1_ONLINE)
			PDEBUG(DSIMI, DEBUG_INFO, " ON-LINE-BIT:         1 = On-line data\n");
		else
			PDEBUG(DSIMI, DEBUG_INFO, " ON-LINE-BIT:         0 = Off-line data\n");
		if (*data & ICB1_CONFIRM)
			PDEBUG(DSIMI, DEBUG_INFO, " CONFIRM-BIT:         1 = Confirmation\n");
		else
			PDEBUG(DSIMI, DEBUG_INFO, " CONFIRM-BIT:         0 = No meaning\n");
		if (*data & ICB1_MASTER)
			PDEBUG(DSIMI, DEBUG_INFO, " MASTER/SLAVE-BIT:    1 = Sender is master\n");
		else
			PDEBUG(DSIMI, DEBUG_INFO, " MASTER/SLAVE-BIT:    0 = Sender is slave\n");
		if (*data & ICB1_WT_EXT)
			PDEBUG(DSIMI, DEBUG_INFO, " WT-EXTENSION-BIT:    1 = Request for WT-Extension\n");
		else
			PDEBUG(DSIMI, DEBUG_INFO, " WT-EXTENSION-BIT:    0 = No request for WT-Extension\n");
		if (*data & ICB1_ABORT)
			PDEBUG(DSIMI, DEBUG_INFO, " ABORT/TERMINATE-BIT: 1 = Abort/Terminate request\n");
		else
			PDEBUG(DSIMI, DEBUG_INFO, " ABORT/TERMINATE-BIT: 0 = No meaning\n");
		if (*data & ICB1_ERROR)
			PDEBUG(DSIMI, DEBUG_INFO, " ERROR-BIT:           1 = Error\n");
		else
			PDEBUG(DSIMI, DEBUG_INFO, " ERROR-BIT:           0 = No meaning\n");
		if (*data & ICB1_CHAINING)
			PDEBUG(DSIMI, DEBUG_INFO, " CHAINING-BIT:        1 = More ICL data follows\n");
		else
			PDEBUG(DSIMI, DEBUG_INFO, " CHAINING-BIT:        0 = No more ICL data follows\n");
		if (*data & ICB_EXT)
			PDEBUG(DSIMI, DEBUG_INFO, " ICB-EXTENSION-BIT:   1 = ICB2 follows\n");
		else {
			PDEBUG(DSIMI, DEBUG_INFO, " ICB-EXTENSION-BIT:   0 = no ICB follows\n");
			ext = 0;
		}
		data++;
		length--;
	}

	if (ext) {
		if (length < 1) {
			PDEBUG(DSIMI, DEBUG_NOTICE, "Message too short\n");
			return;
		}

		PDEBUG(DSIMI, DEBUG_INFO, "Interface control layer ICB2:\n");
		if (*data & ICB2_DYNAMIC)
			PDEBUG(DSIMI, DEBUG_INFO, " DYN-BUFFER-SIZE-BIT: 1 = Buffer size %d\n", (*data & ICB2_BUFFER) * 8);
		else
			PDEBUG(DSIMI, DEBUG_INFO, " DYN-BUFFER-SIZE-BIT: 0 = No meaning\n");
		if (*data & ICB2_ISO_L2)
			PDEBUG(DSIMI, DEBUG_INFO, " ISO-7816-BLOCK-BIT:  1 = Compatible\n");
		else
			PDEBUG(DSIMI, DEBUG_INFO, " ISO-7816-BLOCK-BIT:  0 = Incompatible\n");
		if (*data & ICB2_PRIVATE)
			PDEBUG(DSIMI, DEBUG_INFO, " PRIVATE-USE-BIT:     1 = Private use layer 7 protocol\n");
		else
			PDEBUG(DSIMI, DEBUG_INFO, " PRIVATE-USE-BIT:     0 = No meaning\n");
		if (*data & ICB_EXT)
			PDEBUG(DSIMI, DEBUG_INFO, " ICB-EXTENSION-BIT:   1 = ICB3 follows\n");
		else {
			PDEBUG(DSIMI, DEBUG_INFO, " ICB-EXTENSION-BIT:   0 = no ICB follows\n");
			ext = 0;
		}
		data++;
		length--;
	}

	icb_count = 2;
	while (ext) {
		if (length < 1) {
			PDEBUG(DSIMI, DEBUG_NOTICE, "Message too short\n");
			return;
		}

		PDEBUG(DSIMI, DEBUG_INFO, "Interface control layer ICB%d:\n", ++icb_count);
		PDEBUG(DSIMI, DEBUG_INFO, " Value:               0x%02x\n", *data);
		if (!(*data & 0x80))
			ext = 0;
		data++;
		length--;
	}

	rx_icl_sdu(data, length);
}

/* Layer 2 */

static uint8_t flip(uint8_t c)
{
	c =  ((c&0x55) << 1)  |  ((c&0xaa) >> 1); /* 67452301 */
	c =  ((c&0x33) << 2)  |  ((c&0xcc) >> 2); /* 45670123 */
	c =         (c << 4)  |         (c >> 4); /* 01234567 */

	return c;
}

void sniffer_reset(sim_sniffer_t *sim)
{
	PDEBUG(DSIM1, DEBUG_INFO, "Resetting sniffer\n");
	memset(sim, 0, sizeof(*sim));
}

static void decode_ta1(sim_sniffer_t __attribute__((unused)) *sim, uint8_t c, int count)
{
	int fi = -1, di = -1;
	double fmax = 0.0;

	switch (c >> 4) {
		case 0:
			fi = 372; fmax = 4.0;
			break;
		case 1:
			fi = 372; fmax = 5.0;
			break;
		case 2:
			fi = 558; fmax = 6.0;
			break;
		case 3:
			fi = 744; fmax = 8.0;
			break;
		case 4:
			fi = 1116; fmax = 12.0;
			break;
		case 5:
			fi = 1488; fmax = 16.0;
			break;
		case 6:
			fi = 1860; fmax = 20.0;
			break;
		case 9:
			fi = 512; fmax = 5.0;
			break;
		case 10:
			fi = 768; fmax = 7.5;
			break;
		case 11:
			fi = 1014; fmax = 10.0;
			break;
		case 12:
			fi = 1536; fmax = 15.0;
			break;
		case 13:
			fi = 2048; fmax = 20.0;
			break;
	}

	switch (c & 0xf) {
		case 1:
			di = 1;
			break;
		case 2:
			di = 2;
			break;
		case 3:
			di = 4;
			break;
		case 4:
			di = 8;
			break;
		case 5:
			di = 16;
			break;
		case 6:
			di = 32;
			break;
		case 7:
			di = 64;
			break;
		case 8:
			di = 12;
			break;
		case 9:
			di = 20;
			break;
	}

	if (fi > 0)
		PDEBUG(DSIM2, DEBUG_INFO, " TA%d Fi = %d, f(max.) = %.1f MHz\n", count, fi, fmax);
	else
		PDEBUG(DSIM2, DEBUG_INFO, " TA%d Fi = RFU\n", count);
	if (di > 0)
		PDEBUG(DSIM2, DEBUG_INFO, " TA%d Di = %d\n", count, di);
	else
		PDEBUG(DSIM2, DEBUG_INFO, " TA%d Di = RFU\n", count);
}

static void decode_ta2(sim_sniffer_t __attribute__((unused)) *sim, uint8_t c, int count)
{
	PDEBUG(DSIM2, DEBUG_INFO, " TA%d T = %d\n", count, c & 0xf);
	if (!(c & 0x10))
		PDEBUG(DSIM2, DEBUG_INFO, " TA%d : Fi and Di by TA1 shall apply.\n", count);
	else
		PDEBUG(DSIM2, DEBUG_INFO, " TA%d : Implicit values (and not Di / Di by TA1) sall apply.\n", count);
	if (!(c & 0x80))
		PDEBUG(DSIM2, DEBUG_INFO, " TA%d : Capable to change negotiable/specific mode.\n", count);
	else
		PDEBUG(DSIM2, DEBUG_INFO, " TA%d : Unable to change negotiable/specific mode.\n", count);
}

static void decode_tai(sim_sniffer_t *sim, uint8_t c, int count)
{
	if ((sim->atr_td & 0xf) != 14) {
		PDEBUG(DSIM2, DEBUG_INFO, " TA%d Value = 0x%02x\n", count, c);
		return;
	}

	if (count == 3) {
		switch (c & 0xf) {
		case 0:
			PDEBUG(DSIM2, DEBUG_INFO, " TA%d fsmin = Default\n", count);
			break;
		case 1:
		case 2:
		case 3:
			PDEBUG(DSIM2, DEBUG_INFO, " TA%d fsmin = %d MHz\n", count, c & 0xf);
			break;
		default:
			PDEBUG(DSIM2, DEBUG_INFO, " TA%d fsmin = reserved\n", count);
			break;
		}

		switch (c >> 4) {
		case 0:
		case 1:
		case 2:
		case 3:
			PDEBUG(DSIM2, DEBUG_INFO, " TA%d fsmax = reserved\n", count);
			break;
		case 5:
			PDEBUG(DSIM2, DEBUG_INFO, " TA%d fsmax = 5 MHz (Default)\n", count);
			break;
		default:
			PDEBUG(DSIM2, DEBUG_INFO, " TA%d fsmax = %d MHz\n", count, c >> 4);
			break;
		}
	} else {
		PDEBUG(DSIM2, DEBUG_INFO, " TA%d Block Waiting Time = %d\n", count, c);
	}
}

static void decode_tb1(sim_sniffer_t __attribute__((unused)) *sim, uint8_t c, int count)
{
	if ((c & 0x1f) == 0)
		PDEBUG(DSIM2, DEBUG_INFO, " TB%d PI1=0: VPP not connected\n", count);
	else if ((c & 0x1f) == 5)
		PDEBUG(DSIM2, DEBUG_INFO, " TB%d PI1=5: VPP is 5 Volts (default)\n", count);
	else if ((c & 0x1f) >= 6 && (c & 0x1f) <= 25)
		PDEBUG(DSIM2, DEBUG_INFO, " TB%d PI1=%d: VPP is %d Volts\n", count, c & 0x1f, (c & 0x1f) - 1);
	else
		PDEBUG(DSIM2, DEBUG_INFO, " TB%d PI1=%d: not defined\n", count, c & 0x1f);
	PDEBUG(DSIM2, DEBUG_INFO, " TB%d II = %d\n", count, (c >> 5) & 0x3);
}

static void decode_tb2(sim_sniffer_t __attribute__((unused)) *sim, uint8_t c, int count)
{
	PDEBUG(DSIM2, DEBUG_INFO, " TB%d Value = 0x%02x\n", count, c);
}

static void decode_tbi(sim_sniffer_t *sim, uint8_t c, int count)
{
	if ((sim->atr_td & 0xf) != 14) {
		PDEBUG(DSIM2, DEBUG_INFO, " TB%d Value = 0x%02x\n", count, c);
		return;
	}

	if (count == 3) {
		PDEBUG(DSIM2, DEBUG_INFO, " TB%d Maximum block size = %d\n", count, c);
	} else {
		if (!(c & 0x01))
			PDEBUG(DSIM2, DEBUG_INFO, " TB%d XOR Checksum\n", count);
		else
			PDEBUG(DSIM2, DEBUG_INFO, " TB%d CRC Checksum\n", count);
		if (!(c & 0x02))
			PDEBUG(DSIM2, DEBUG_INFO, " TB%d 12-etu frame\n", count);
		else
			PDEBUG(DSIM2, DEBUG_INFO, " TB%d 11-etu frame\n", count);
		if (!(c & 0x04))
			PDEBUG(DSIM2, DEBUG_INFO, " TB%d No Chaining in ICL-Layer-Protocol\n", count);
		else
			PDEBUG(DSIM2, DEBUG_INFO, " TB%d Chaining in ICL-Layer-Protocol\n", count);
		if (!(c & 0x08))
			PDEBUG(DSIM2, DEBUG_INFO, " TB%d Incompatible to ISO 7816 (Character Protocol)\n", count);
		else
			PDEBUG(DSIM2, DEBUG_INFO, " TB%d Compatible to ISO 7816 (Character Protocol)\n", count);
		if (!(c & 0x10))
			PDEBUG(DSIM2, DEBUG_INFO, " TB%d No private in ICL-Layer-Protocol\n", count);
		else
			PDEBUG(DSIM2, DEBUG_INFO, " TB%d Private in ICL-Layer-Protocol\n", count);
		if (!(c & 0x20))
			PDEBUG(DSIM2, DEBUG_INFO, " TB%d No ICB-Extension in ICL-Layer-Protocol\n", count);
		else
			PDEBUG(DSIM2, DEBUG_INFO, " TB%d ICB-Extension in ICL-Layer-Protocol\n", count);
	}
}

static void decode_tc1(sim_sniffer_t __attribute__((unused)) *sim, uint8_t c, int count)
{
	PDEBUG(DSIM2, DEBUG_INFO, " TC%d N = %d\n", count, c);
}

static void decode_tc2(sim_sniffer_t __attribute__((unused)) *sim, uint8_t c, int count)
{
	PDEBUG(DSIM2, DEBUG_INFO, " TC%d Value = 0x%02x\n", count, c);
}

static void decode_tci(sim_sniffer_t *sim, uint8_t c, int count)
{
	if ((sim->atr_td & 0xf) != 14) {
		PDEBUG(DSIM2, DEBUG_INFO, " TC%d Value = 0x%02x\n", count, c);
		return;
	}

	PDEBUG(DSIM2, DEBUG_INFO, " TC%d Character Waiting Time = %d\n", count, c);
}

static void decode_td(sim_sniffer_t __attribute__((unused)) *sim, uint8_t c, int count)
{
	switch (c & 0xf) {
	case 0:
		PDEBUG(DSIM2, DEBUG_INFO, " TD%d T=1: Half-duplex transmission of characters (ISO 7816).\n", count);
		break;
	case 1:
		PDEBUG(DSIM2, DEBUG_INFO, " TD%d T=1: Half-duplex transmission of blocks (ISO 7816).\n", count);
		break;
	case 2:
	case 3:
		PDEBUG(DSIM2, DEBUG_INFO, " TD%d T=%d: Reserved for future full-duplex operations.\n", count, c & 0xf);
		break;
	case 4:
		PDEBUG(DSIM2, DEBUG_INFO, " TD%d T=4: Reserved for an enhanced half-duplex transmission of characters.\n", count);
		break;
	case 5:
	case 6:
	case 7:
	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
	case 13:
		PDEBUG(DSIM2, DEBUG_INFO, " TD%d T=%d: Reserved for future use by ISO/IEC JTC 1/SC 17.\n", count, c & 0xf);
		break;
	case 14:
		PDEBUG(DSIM2, DEBUG_INFO, " TD%d T=14: Refers to transmission protocols not standardized by ISO/IEC JTC 1/SC 17.\n", count);
		break;
	case 15:
		PDEBUG(DSIM2, DEBUG_INFO, " TD%d T=15: Does not refer to a transmission protocol, but only qualifies global interface bytes.\n", count);
		break;
	}
}

static void decode_if(sim_sniffer_t *sim, int count)
{
	switch (count) {
	case 1:
		if (sim->atr_if_mask & 0x10)
			decode_ta1(sim, sim->atr_ta, count);
		if (sim->atr_if_mask & 0x20)
			decode_tb1(sim, sim->atr_tb, count);
		if (sim->atr_if_mask & 0x40)
			decode_tc1(sim, sim->atr_tc, count);
		if (sim->atr_if_mask & 0x80)
			decode_td(sim, sim->atr_td, count);
		break;
	case 2:
		if (sim->atr_if_mask & 0x10)
			decode_ta2(sim, sim->atr_ta, count);
		if (sim->atr_if_mask & 0x20)
			decode_tb2(sim, sim->atr_tb, count);
		if (sim->atr_if_mask & 0x40)
			decode_tc2(sim, sim->atr_tc, count);
		if (sim->atr_if_mask & 0x80)
			decode_td(sim, sim->atr_td, count);
		break;
	default:
		if (sim->atr_if_mask & 0x10)
			decode_tai(sim, sim->atr_ta, count);
		if (sim->atr_if_mask & 0x20)
			decode_tbi(sim, sim->atr_tb, count);
		if (sim->atr_if_mask & 0x40)
			decode_tci(sim, sim->atr_tc, count);
		if (sim->atr_if_mask & 0x80)
			decode_td(sim, sim->atr_td, count);
	}

	if ((sim->atr_td >> 4))
		PDEBUG(DSIM2, DEBUG_INFO, "----------------------------------------\n");
}

static void decode_hist(sim_sniffer_t __attribute__((unused)) *sim, uint8_t c, int count)
{
	PDEBUG(DSIM2, DEBUG_INFO, " History byte #%d: 0x%02x\n", count, c);
}

static void rx_atr(sim_sniffer_t *sim, uint8_t c)
{
	/* TS */
	if (sim->atr_count == 0) {
		PDEBUG(DSIM1, DEBUG_INFO, "----------------------------------------\n");
		switch (c) {
		case 0x3f:
			PDEBUG(DSIM2, DEBUG_INFO, "Reading ATR inverse bit order:\n");
			sim->inverse_order = 1;
			break;
		case 0x3b:
			PDEBUG(DSIM2, DEBUG_INFO, "Reading ATR normal bit order:\n");
			sim->inverse_order = 0;
			break;
		default:
			sniffer_reset(sim);
			return;
		}
		sim->atr_tck = c;
		sim->atr_count++;
		return;
	}

	if (sim->inverse_order)
		c = flip (c);

	sim->atr_tck ^= c;

	if (sim->atr_count == 1) {
		sim->atr_t0 = c;
		sim->atr_if_mask = c;
		sim->atr_count++;
		return;
	}

	/* get TA, if included, or skip by inc. atr_count */
	if (sim->atr_count == 2) {
		if (sim->atr_if_mask & 0x10) {
			sim->atr_ta = c;
			sim->atr_count++;
			return;
		} else
			sim->atr_count++;
	}

	/* get TB, if included, or skip by inc. atr_count */
	if (sim->atr_count == 3) {
		if (sim->atr_if_mask & 0x20) {
			sim->atr_tb = c;
			sim->atr_count++;
			return;
		} else
			sim->atr_count++;
	}

	/* get TC, if included, or skip by inc. atr_count */
	if (sim->atr_count == 4) {
		if (sim->atr_if_mask & 0x40) {
			sim->atr_tc = c;
			sim->atr_count++;
			return;
		} else
			sim->atr_count++;
	}

	/* get TD, if included, or skip by inc. atr_count */
	if (sim->atr_count == 5) {
		if (sim->atr_if_mask & 0x80) {
			sim->atr_td = c;
			/* decode content */
			decode_if(sim, sim->atr_if_count + 1);
			/* get new mask byte and start over */
			sim->atr_count = 2;
			sim->atr_if_mask = sim->atr_td;
			sim->atr_if_count++;
			return;
		} else
			sim->atr_count++;
	}

	/* decode content */
	if (sim->atr_count == 6)
		decode_if(sim, sim->atr_if_count + 1);

	/* process historical character */
	if (sim->atr_count < 6 + (sim->atr_t0 & 0xf)) {
		decode_hist(sim, c, sim->atr_count - 6 + 1);
		sim->atr_count++;
		return;
	}

	if (sim->atr_tck == 0)
		PDEBUG(DSIM2, DEBUG_INFO, " Checksum 0x%02x ok.\n", c);
	else
		PDEBUG(DSIM2, DEBUG_NOTICE, " Checksum 0x%02x error!\n", c);


	sim->l1_state = L1_STATE_RECEIVE;
	sim->block_state = BLOCK_STATE_ADDRESS;
	PDEBUG(DSIM2, DEBUG_INFO, "ATR done!\n");
}

static void rx_char(sim_sniffer_t *sim, uint8_t c)
{
	if (sim->inverse_order)
		c = flip(c);

	sim->block_checksum ^= c;

	switch (sim->block_state) {
	case BLOCK_STATE_ADDRESS:
		if ((c >> 4) != 1 && (c & 0xf) != 1) {
			/* start over if we do not get a valid message start */
			sniffer_reset(sim);
			sniffer_rx(sim, c);
			return;
		}
		PDEBUG(DSIM1, DEBUG_INFO, "----------------------------------------\n");
		sim->block_address = c;
		sim->block_state = BLOCK_STATE_CONTROL;
		sim->block_checksum = c;
		return;
	case BLOCK_STATE_CONTROL:
		sim->block_control = c;
		sim->block_state = BLOCK_STATE_LENGTH;
		return;
	case BLOCK_STATE_LENGTH:
		sim->block_length = c;
		sim->block_count = 0;
		sim->block_state = BLOCK_STATE_DATA;
		return;
	case BLOCK_STATE_DATA:
		if (sim->block_count < sim->block_length) {
			sim->block_data[sim->block_count++] = c;
			return;
		}
		PDEBUG(DSIM2, DEBUG_INFO, "Layer 2:\n");
		PDEBUG(DSIM2, DEBUG_INFO, " source %d -> to %d\n", sim->block_address >> 4, sim->block_address & 0xf);
		if ((sim->block_control & 0x11) == 0x00)
			PDEBUG(DSIM2, DEBUG_INFO, " control I: N(S)=%d N(R)=%d\n", (sim->block_control >> 1) & 7, sim->block_control >> 5);
		else if ((sim->block_control & 0x1f) == 0x09)
			PDEBUG(DSIM2, DEBUG_INFO, " control REJ: N(R)=%d\n", sim->block_control >> 5);
		else if (sim->block_control == 0xef)
			PDEBUG(DSIM2, DEBUG_INFO, " control RES");
		else
			PDEBUG(DSIM2, DEBUG_INFO, " control unknown 0x%02x\n", sim->block_control);
		PDEBUG(DSIM2, DEBUG_INFO, " length %d\n", sim->block_length);
		if (sim->block_checksum == 0)
			rx_icl_pdu(sim->block_data, sim->block_length);
		else
			PDEBUG(DSIM2, DEBUG_NOTICE, "Received message with checksum error!\n");
		sim->block_state = BLOCK_STATE_ADDRESS;
	}
}

void sniffer_rx(sim_sniffer_t *sim, uint8_t c)
{

	PDEBUG(DSIM1, DEBUG_DEBUG, "Serial RX '0x%02x'\n", c);

	switch (sim->l1_state) {
	case L1_STATE_RESET:
		if (c != 0x3f && c != 0x3b) {
			PDEBUG(DSIM1, DEBUG_INFO, "Received garbage '0x%02x' while waiting for ATR\n", c);
			break;
		}
		sim->l1_state = L1_STATE_ATR;
		sim->atr_count = 0;
		/* fall through */
	case L1_STATE_ATR:
		rx_atr(sim, c);
		break;
	case L1_STATE_RECEIVE:
		rx_char(sim, c);
		break;
	default:
		break;
	}
}

void sniffer_timeout(sim_sniffer_t *sim)
{
	switch (sim->l1_state) {
	case L1_STATE_RESET:
	case L1_STATE_ATR:
		if (sim->l1_state == L1_STATE_ATR && sim->atr_count)
			PDEBUG(DSIM1, DEBUG_NOTICE, "Timeout while receiving ATR!\n");
		sim->l1_state = L1_STATE_ATR;
		sim->atr_count = 0;
		break;
	case L1_STATE_RECEIVE:
		if (sim->block_state != BLOCK_STATE_ADDRESS)
			PDEBUG(DSIM1, DEBUG_NOTICE, "Timeout while receiving message!\n");
		sim->block_state = BLOCK_STATE_ADDRESS;
		break;
	default:
		break;
	}
}

#endif /* ARDUINO */
