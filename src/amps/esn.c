/* AMPS ESN specification
 *
 * (C) 2023 by Andreas Eversberg <jolly@eversberg.eu>
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
#include <string.h>
#include <pthread.h>
#include "../libsample/sample.h"
#include "../liblogging/logging.h"
#include "../libmobile/call.h"
#include "../libmobile/cause.h"
#include <osmocom/cc/message.h>
#include "amps.h"
#include "esn.h"

const char *amps_manufacturer[256] = {
	[129] = "Oki",
	[130] = "Motorola, Inc.",
	[131] = "E.F. Johnson",
	[132] = "Hitachi",
	[133] = "Fujitsu",
	[134] = "Mitsubishi",
	[135] = "NEC America, Inc.",
	[136] = "Matsushita (Panasonic)",
	[137] = "Harris",
	[138] = "Toshiba",
	[139] = "Kokusai",
	[140] = "Clarion Company, Ltd.",
	[141] = "GoldStar Products Co.,Ltd.",
	[142] = "International Systcom (Novatel)",
	[143] = "Ericsson, Inc.",
	[144] = "Murata Machinery, Ltd.",
	[145] = "DI-BAR Electronics, Inc.",
	[146] = "Ericsson Inc. (formerly assigned to General Electric)",
	[147] = "Gateway Telephone, Inc.",
	[148] = "Robert Bosch Corporation (Blaupunkt)",
	[149] = "Universal Cellular, Inc.",
	[150] = "Alpine Electronics of America, Inc.",
	[151] = "Verma Laboratories",
	[152] = "Japan Radio Co.,Ltd.",
	[153] = "CM Communications Incorporated",
	[154] = "Sony Corporation (Japan)",
	[155] = "Tama Denki Company, Ltd.",
	[156] = "Mobira (Nokia-Kinex)",
	[157] = "Ericsson GE Mobile Communications, Inc.",
	[158] = "AT&T Technologies, Inc.",
	[159] = "QUALCOMM, Incorporated",
	[160] = "Hyundai",
	[161] = "Satellite Technology Services, Inc.",
	[162] = "Technophone Limited",
	[163] = "Yupiteru Industries Company Ltd.",
	[164] = "Hughes Network Systems",
	[165] = "TMC Company Limited (Nokia)",
	[166] = "Clarion Manufacturing Corporation of America",
	[167] = "Mansoor Electronics Limited",
	[168] = "Motorola International",
	[169] = "Otron Corporation",
	[170] = "Philips Telecom Equipment Corporation",
	[171] = "Philips Circuit Assemblies",
	[172] = "Uniden Corporation of America",
	[173] = "Uniden Corporation - Japan",
	[174] = "Shintom West Corporation of America",
	[175] = "Tottori Sanyo Electric Co. Ltd.",
	[176] = "Samsung Communications",
	[177] = "INFA Telecom Canada, Inc.",
	[178] = "Emptel Electronics Company Ltd.",
	[179] = "**Unassigned**",
	[180] = "ASCNet",
	[181] = "Yaesu USA",
	[182] = "Tecom Co. Ltd.",
	[183] = "Omni Telecommunications, Inc. (formerly assigned to Valor Electronics, Inc.)",
	[184] = "Royal Information Electronics Co. Ltd.",
	[185] = "Tele Digital Development, Inc. (formerly assigned to Cumulus Corporation.)",
	[186] = "DNC",
	[187] = "**Unassigned**",
	[188] = "Myday Technology, Ltd.",
	[189] = "NEC America, Inc.",
	[190] = "Kyocera Corporation",
	[191] = "Digital Security Controls",
	[192] = "CTCELL Digital, Inc.",
	[193] = "Matsushita Communication Industrial Corporation of America",
	[194] = "HS Electronics Corporation",
	[195] = "Motorola, Inc.",
	[196] = "Pacific Communication Sciences, Inc.",
	[197] = "Maxon Systems, Inc., (London) Ltd.",
	[198] = "Hongsheng Electronics Co., Ltd.",
	[199] = "M/ACOM",
	[200] = "CHANNLE LINK Incorporation",
	[201] = "L.G. Information & Communications (formerly assigned to Goldstar Information & Communications, Ltd.)",
	[202] = "Intel Corporation",
	[203] = "Air Communications, Inc.",
	[204] = "Ericsson GE Mobile Communications Inc.",
	[205] = "Goldtron RF PTE Ltd.",
	[206] = "Sierra Wireless Inc.",
	[207] = "Mitsubishi International Corp.",
	[208] = "JRC International, Inc.",
	[209] = "Sapura Holdings SDN. BHD.",
	[210] = "Inex Technologies, Inc.",
	[211] = "Sony Electronics (U.S.A.)",
	[212] = "Motorola, Inc.",
	[213] = "Motorola, Inc.",
	[214] = "Philips Semiconductors",
	[215] = "Carillon Corp.",
	[216] = "Nippondenso America, Inc.",
	[217] = "International Business Machines Corporation",
	[218] = "Nokia (Hong Kong)",
	[219] = "Nokia (TMC Co. Ltd.)",
	[220] = "TEMIC",
	[221] = "Northern Telecom",
	[222] = "Telrad Telecommunications Ltd.",
	[223] = "Motorola, Inc.",
	[224] = "Motorola, Inc.",
	[225] = "Telital s.r.l.",
	[226] = "Nokia (Manau, Brazil)",
	[227] = "Stanilite Pacific",
	[228] = "Philips Consumer Communications",
	[229] = "NEC America, Inc.",
	[230] = "TELLULAR Corporation",
	[231] = "Ericsson Inc.",
};

const char *esn_to_string(uint32_t esn)
{
	uint8_t mfr;
	uint32_t serial;
	static char esn_string[256];

	amps_decode_esn(esn, &mfr, &serial);

	if (amps_manufacturer[mfr])
		snprintf(esn_string, sizeof(esn_string), "0x%08x or %d-%06d (%s)", esn, mfr, serial, amps_manufacturer[mfr]);
	else
		snprintf(esn_string, sizeof(esn_string), "0x%08x or %d-%06d", esn, mfr, serial);

	return esn_string;
}
