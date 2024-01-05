/* message transcoding
 *
 * (C) 2021 by Andreas Eversberg <jolly@eversberg.eu>
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
#include <errno.h>
#include <inttypes.h>
#include "../liblogging/logging.h"
#include "message.h"

static struct mpt1327_parameter_names {
	const char *name;
	const char *description;
} mpt1327_parameter_names[] = {
	{ "<constant>", "Constant" },
	{ "PFIX", "Group Prefix" },
	{ "IDENT1", "Called Party Number" },
	{ "D", "Data Call" },
	{ "CHAN", "Channel Number" },
	{ "IDENT2", "Calling Party Number" },
	{ "(N)", "Aloha Number" },
	{ "P", "Parity" },
	{ "CAT", "Category" },
	{ "TYPE", "Type" },
	{ "FUNC", "Function" },
	{ "CHAN4", "Last 4 Bits of Channel Number" },
	{ "WT", "Delay Parameter" },
	{ "RSVD", "Reserved" },
	{ "(M)", "Address Qualifier" },
	{ "QUAL", "Qualifies FUNC" },
	{ "DT", "Data" },
	{ "LEVEL", "Priority level" },
	{ "EXT", "Extended Addressing" },
	{ "FLAG1", "Flag 1" },
	{ "FLAG2", "Flag 2" },
	{ "PARAMETERS", "Parameters" },
	{ "SD", "Speech and/or Data" },
	{ "DIV", "Diversion" },
	{ "INFO", "Info" },
	{ "STATUS", "Status" },
	{ "SLOTS", "Slots for Data Message" },
	{ "POINT", "Demand Acknowledgement" },
	{ "CHECK", "Availability Check" },
	{ "E", "Emergency Call" },
	{ "AD", "Data is appended" },
	{ "DESC", "Type of Data" },
	{ "A", "B" },
	{ "B", "B" },
	{ "SPARE", "Spare" },
	{ "REVS", "Bit Reversals" },
	{ "OPER", NULL },
	{ "SYS", NULL },
	{ "CONT", NULL },
	{ "SYSDEF", NULL },
	{ "PER", NULL },
	{ "IVAL", NULL },
	{ "PON", NULL },
	{ "ID", NULL },
	{ "ADJSITE", NULL },
	{ "SOL", NULL },
	{ "LEN", NULL },
	{ "PREFIX2", NULL },
	{ "KIND", NULL },
	{ "PORT", NULL },
	{ "FAD", NULL },
	{ "INTER", NULL },
	{ "HADT", NULL },
	{ "MODEM", NULL },
	{ "O/R", NULL },
	{ "RATE", NULL },
	{ "TRANS", NULL },
	{ "RNITEL", NULL },
	{ "TNITEL", NULL },
	{ "JOB", NULL },
	{ "REASON", NULL },
	{ "ATRANS", NULL },
	{ "EFLAGS", NULL },
	{ "TASK", NULL },
	{ "ONES", NULL },
	{ "ITENUM", NULL },
	{ "USERDATA", NULL },
	{ "I/G", NULL },
	{ "MORE", NULL },
	{ "LASTBIT", NULL },
	{ "FRAGL", NULL },
	{ "RTRANS", NULL },
	{ "W/F", NULL },
	{ "P/N", NULL },
	{ "DN", NULL },
	{ "SPRE", NULL },
	{ "SX", NULL },
	{ "CAUSE", NULL },
	{ "I/T", NULL },
	{ "RESP", NULL },
	{ "TOC", NULL },
	{ "CCS", "Codeword Completion Sequence" },
	{ "LET", "Link Establishmen Time" },
	{ "PREAMBLE", NULL },
	{ "PARAMETERS1", NULL },
	{ "PARAMETERS2", NULL },
	{ "BCD11", "11 Digits encoded as BCD" },
	{ "RSA", NULL },
	{ "FCW", NULL },
	{ "SP", NULL },
	{ "EXCHANGE", NULL },
	{ "Number", NULL },
	{ "GF", NULL },
	{ "PFIXT", NULL },
	{ "IDENTT", NULL },
	{ "FORM", NULL },
	{ "PFIX2", NULL },
};

char *mpt1327_bcd = "0123456789R*#RR"; /* last digit is NULL */

static struct definitions {
	int specific_only;
	enum mpt1327_codeword_dir dir;
	enum mpt1327_codeword_type type;
	char *def;
	const char *short_name;
	const char *long_name;
} definitions[] = {
	/* Filler */
	{ 1, MPT_DOWN,	MPT_FILLER,	"0 RSVD:47=00000000000000000000000000000000000000000000000 P:16", "filler", "Filler Data" },
	/* GTC Message */
	{ 0, MPT_DOWN,	MPT_GTC,	"1 PFIX:7 IDENT1:13 0 D:1 CHAN:10 IDENT2:13 (N):2 P:16", "GTC", "Go To Traffic Channel" },
	/* Category '000' Messages: Aloha Messages (Type '00') */
	{ 0, MPT_DOWN,	MPT_ALH,	"1 PFIX:7 IDENT1:13 1 CAT:3=000 TYPE:2=00 FUNC:3=000 CHAN4:4 WT:3 RSVD:2 (M):5 (N):4 P:16", "ALH", "Aloha: Any single codeword message invited" },
	{ 0, MPT_DOWN,	MPT_ALHS,	"1 PFIX:7 IDENT1:13 1 CAT:3=000 TYPE:2=00 FUNC:3=001 CHAN4:4 WT:3 RSVD:2 (M):5 (N):4 P:16", "ALHS", "Aloha: Messages invited, except RQD" },
	{ 0, MPT_DOWN,	MPT_ALHD,	"1 PFIX:7 IDENT1:13 1 CAT:3=000 TYPE:2=00 FUNC:3=010 CHAN4:4 WT:3 RSVD:2 (M):5 (N):4 P:16", "ALHD", "Aloha: Messages invited, except RQS" },
	{ 0, MPT_DOWN,	MPT_ALHE,	"1 PFIX:7 IDENT1:13 1 CAT:3=000 TYPE:2=00 FUNC:3=011 CHAN4:4 WT:3 RSVD:2 (M):5 (N):4 P:16", "ALHE", "Aloha: Emergency requests (RQE) only invited" },
	{ 0, MPT_DOWN,	MPT_ALHR,	"1 PFIX:7 IDENT1:13 1 CAT:3=000 TYPE:2=00 FUNC:3=100 CHAN4:4 WT:3 RSVD:2 (M):5 (N):4 P:16", "ALHR", "Aloha: Registration (RQR) or emergency requests (RQE) invited" },
	{ 0, MPT_DOWN,	MPT_ALHX,	"1 PFIX:7 IDENT1:13 1 CAT:3=000 TYPE:2=00 FUNC:3=101 CHAN4:4 WT:3 RSVD:2 (M):5 (N):4 P:16", "ALHX", "Aloha: Messages invited, except RQR" },
	{ 0, MPT_DOWN,	MPT_ALHF,	"1 PFIX:7 IDENT1:13 1 CAT:3=000 TYPE:2=00 FUNC:3=110 CHAN4:4 WT:3 RSVD:2 (M):5 (N):4 P:16", "ALHF", "Aloha: Fall-back mode" },
	/* Category '000' Messages: Acknowledgement Messages (Type '01') */
	{ 0, MPT_BOTH,	MPT_ACK,	"1 PFIX:7 IDENT1:13 1 CAT:3=000 TYPE:2=01 FUNC:3=000 IDENT2:13 QUAL:1 (N):4 P:16", "ACK", "Ack: General acknowledgement" },
	{ 0, MPT_BOTH,	MPT_ACKI,	"1 PFIX:7 IDENT1:13 1 CAT:3=000 TYPE:2=01 FUNC:3=001 IDENT2:13 QUAL:1 (N):4 P:16", "ACKI", "Ack: Intermediate acknowledgement, more signalling to follow" },
	{ 0, MPT_BOTH,	MPT_ACKQ,	"1 PFIX:7 IDENT1:13 1 CAT:3=000 TYPE:2=01 FUNC:3=010 IDENT2:13 QUAL:1 (N):4 P:16", "ACKQ", "Ack: Acknowledge, call queued" },
	{ 0, MPT_BOTH,	MPT_ACKX,	"1 PFIX:7 IDENT1:13 1 CAT:3=000 TYPE:2=01 FUNC:3=011 IDENT2:13 QUAL:1 (N):4 P:16", "ACKX", "Ack: Acknowledge, message rejected" },
	{ 0, MPT_BOTH,	MPT_ACKV,	"1 PFIX:7 IDENT1:13 1 CAT:3=000 TYPE:2=01 FUNC:3=100 IDENT2:13 QUAL:1 (N):4 P:16", "ACKV", "Ack: Acknowledge, called unit unavailable" },
	{ 0, MPT_BOTH,	MPT_ACKE,	"1 PFIX:7 IDENT1:13 1 CAT:3=000 TYPE:2=01 FUNC:3=101 IDENT2:13 QUAL:1 (N):4 P:16", "ACKE", "Ack: Acknowledge emergency call" },
	{ 0, MPT_BOTH,	MPT_ACKT,	"1 PFIX:7 IDENT1:13 1 CAT:3=000 TYPE:2=01 FUNC:3=110 IDENT2:13 QUAL:1 (N):4 P:16", "ACKT", "Ack: Acknowledge, try on given address" },
	{ 0, MPT_BOTH,	MPT_ACKB,	"1 PFIX:7 IDENT1:13 1 CAT:3=000 TYPE:2=01 FUNC:3=111 IDENT2:13 QUAL:1 (N):4 P:16", "ACKB", "Ack: Acknowledge, call-back, or negative acknowledgement" },
	/* Category '000' Messages: Request Messages (Type '10') */
	{ 0, MPT_UP,	MPT_RQS,	"1 PFIX:7 IDENT1:13 1 CAT:3=000 TYPE:2=10 FUNC:3=000 IDENT2:13 DT:1 LEVEL:1 EXT:1 FLAG1:1 FLAG2:1 P:16", "RQS", "Request: Request Simple call" },
	{ 0, MPT_UP,	MPT_RQSpare,	"1 PFIX:7 IDENT1:13 1 CAT:3=000 TYPE:2=10 FUNC:3=001 PARAMETERS:18 P:16", "RQSpstr", "Request: Spare. Available for customisation" },
	{ 0, MPT_UP,	MPT_RQX,	"1 PFIX:7 IDENT1:13 1 CAT:3=000 TYPE:2=10 FUNC:3=010 IDENT2:13 RSVD:5 P:16", "RQX", "Request: Request call cancel / abort transaction" },
	{ 0, MPT_UP,	MPT_RQT,	"1 PFIX:7 IDENT1:13 1 CAT:3=000 TYPE:2=10 FUNC:3=011 IDENT2:13 SD:2 DIV:1 FLAG1:1 FLAG2:1 P:16", "RQT", "Request: Request call diversion" },
	{ 0, MPT_UP,	MPT_RQE,	"1 PFIX:7 IDENT1:13 1 CAT:3=000 TYPE:2=10 FUNC:3=100 IDENT2:13 D:1 RSVD:1 EXT:1 FLAG1:1 FLAG2:1 P:16", "RQE", "Request: Request emergency call" },
	{ 0, MPT_UP,	MPT_RQR,	"1 PFIX:7 IDENT1:13 1 CAT:3=000 TYPE:2=10 FUNC:3=101 INFO:15 RSVD:3 P:16", "RQR", "Request: Request to register" },
	{ 0, MPT_UP,	MPT_RQQ,	"1 PFIX:7 IDENT1:13 1 CAT:3=000 TYPE:2=10 FUNC:3=110 IDENT2:13 STATUS:5 P:16", "RQQ", "Request: Request status transaction" },
	{ 0, MPT_UP,	MPT_RQC,	"1 PFIX:7 IDENT1:13 1 CAT:3=000 TYPE:2=10 FUNC:3=111 IDENT2:13 SLOTS:2 EXT:1 FLAG1:1 FLAG2:1 P:16", "RQC", "Request: Request to send short data message" },
	/* Category '000' Messages: Ahoy Messages (Type '10') */
	{ 0, MPT_DOWN,	MPT_AHY,	"1 PFIX:7 IDENT1:13 1 CAT:3=000 TYPE:2=10 FUNC:3=000 IDENT2:13 D:1 POINT:1 CHECK:1 E:1 AD:1 P:16", "AHY", "Ahoy: General availability check" },
	{ 0, MPT_DOWN,	MPT_AHYSpare,	"1 PFIX:7 IDENT1:13 1 CAT:3=000 TYPE:2=10 FUNC:3=001 PARAMETERS:18 P:16", "AHYSpare", "Ahoy: Spare for customisation" },
	{ 0, MPT_DOWN,	MPT_AHYX,	"1 PFIX:7 IDENT1:13 1 CAT:3=000 TYPE:2=10 FUNC:3=010 IDENT2:13 POINT:5 P:16", "AHYX", "Ahoy: Cancel alert/waiting state" },
	{ 0, MPT_DOWN,	MPT_AHYP,	"1 PFIX:7 IDENT1:13 1 CAT:3=000 TYPE:2=10 FUNC:3=101 IDENT2:13 RSVD:5 P:16", "AHYP", "Ahoy: Called Unit Presence Monitoring" },
	{ 0, MPT_DOWN,	MPT_AHYQ,	"1 PFIX:7 IDENT1:13 1 CAT:3=000 TYPE:2=10 FUNC:3=110 IDENT2:13 STATUS:5 P:16", "AHYQ", "Ahoy: Status message" },
	{ 0, MPT_DOWN,	MPT_AHYC,	"1 PFIX:7 IDENT1:13 1 CAT:3=000 TYPE:2=10 FUNC:3=111 IDENT2:13 SLOTS:2 DESC:3 P:16", "AHYC", "Ahoy: Short data invitation" },
	/* Category '000' Messages: Miscellaneous Control Messages (Type '11') */
	{ 0, MPT_DOWN,	MPT_MARK,	"1 CHAN4:4 A:1 SYS:15 1 CAT:3=000 TYPE:2=11 FUNC:3=000 B:18 P:16", "MARK", "Misc: Control channel marker" },
	{ 0, MPT_BOTH,	MPT_MAINT,	"1 PFIX:7 IDENT1:13 1 CAT:3=000 TYPE:2=11 FUNC:3=001 CHAN:10 OPER:3 RSVD:5 P:16", "MAINT", "Misc: Call maintenance message" },
	{ 0, MPT_DOWN,	MPT_CLEAR,	"1 CHAN:10 CONT:10 1 CAT:3=000 TYPE:2=11 FUNC:3=010 RSVD:4 SPARE:2 REVS:12=101010101010 P:16", "CLEAR", "Misc: Clear down from allocated channel" },
	{ 0, MPT_DOWN,	MPT_MOVE,	"1 PFIX:7 IDENT1:13 1 CAT:3=000 TYPE:2=11 FUNC:3=011 CONT:10 (M):5 RSVD:2 SPARE:1 P:16", "MOVE", "Misc: Move to specified control channel" },
	{ 0, MPT_DOWN,	MPT_BCAST0,	"1 SYSDEF:5=00000 SYS:15 1 CAT:3=000 TYPE:2=11 FUNC:3=100 CHAN:10 SPARE:2 RSVD:6 P:16", "BCAST", "Misc: Broadcast message: Announce control channel" },
	{ 0, MPT_DOWN,	MPT_BCAST1,	"1 SYSDEF:5=00001 SYS:15 1 CAT:3=000 TYPE:2=11 FUNC:3=100 CHAN:10 SPARE:2 RSVD:6 P:16", "BCAST", "Misc: Broadcast message: Withdraw control channel" },
	{ 0, MPT_DOWN,	MPT_BCAST2,	"1 SYSDEF:5=00010 SYS:15 1 CAT:3=000 TYPE:2=11 FUNC:3=100 PER:1 IVAL:5 PON:1 ID:1 RSVD:2 SPARE:8 P:16", "BCAST", "Misc: Broadcast message: Specify call maintenance parameter" },
	{ 0, MPT_DOWN,	MPT_BCAST3,	"1 SYSDEF:5=00011 SYS:15 1 CAT:3=000 TYPE:2=11 FUNC:3=100 RSVD:4 SPARE:14 P:16", "BCAST", "Misc: Broadcast message: Specify registration parameters" },
	{ 0, MPT_DOWN,	MPT_BCAST4,	"1 SYSDEF:5=00100 SYS:15 1 CAT:3=000 TYPE:2=11 FUNC:3=100 CHAN:10 SPARE:2 RSVD:2 ADJSITE:4 P:16", "BCAST", "Misc: Broadcast message: Broadcast adjected site control channel number" },
	{ 0, MPT_DOWN,	MPT_BCAST5,	"1 SYSDEF:5=00101 SYS:15 1 CAT:3=000 TYPE:2=11 FUNC:3=100 CHAN:10 SPARE:2 RSVD:2 ADJSITE:4 P:16", "BCAST", "Misc: Broadcast message: Vote now advice" },
	/* Category '001' Messages */
	{ 0, MPT_DOWN,	MPT_SAMO,	"1 PFIX:7 IDENT1:13 1 CAT:3=001 TYPE:1=0 PARAMETERS:22 P:16", "SAMO", "Sam: Outbound Single Address Message" },
	{ 0, MPT_UP,	MPT_SAMIU,	"1 PFIX:7 IDENT1:13 1 CAT:3=001 TYPE:1=0 SOL:1=1 PARAMETERS:21 P:16", "SAMIU", "Sam: Inbound Unsolicited Single Address Message" },
	{ 0, MPT_UP,	MPT_SAMIS,	"1 PARAMETERS1:20 1 CAT:3=001 TYPE:1=0 SOL:1=0 DESC:3 PARAMETERS2:18 P:16", "SAMIS", "Sam: Inbound Solicited Single Address Message" },
	{ 0, MPT_BOTH,	MPT_HEAD,	"1 PFIX:7 IDENT1:13 1 CAT:3=001 TYPE:1=1 LEN:2 PREFIX2:7 IDENT2:13 P:16", "HEAD", "Short Data Message Header" },
	/*0Category '010' Messages */
	{ 0, MPT_UP,	MPT_RQD,	"1 PFIX:7 IDENT1:13 1 CAT:3=010 KIND:1=1 PORT:3 FAD:1 IDENT2:13 INTER:1 LEVEL:1 HADT:1 E:1 MODEM:1 P:16", "RQD", "Request Standard Data Communication" },
	{ 0, MPT_DOWN,	MPT_AHYD,	"1 PFIX:7 IDENT1:13 1 CAT:3=010 KIND:1=1 PORT:3 RSVD:1 IDENT2:13 INTER:1 POINT:1 HADT:1 E:1 AD:1 P:16", "AHYD", "Availability Check for Standard Data" },
	{ 0, MPT_DOWN,	MPT_GTT,	"1 PFIX:7 IDENT1:13 1 CAT:3=010 KIND:1=0 CHAN:10 O/R:1 RATE:1 TRANS:10 P:16", "GTT", "Go To Transaction" },
	{ 0, MPT_UP,	MPT_DRUGI,	"1 PFIX:7 IDENT1:13 1 CAT:3=010 KIND:1=0 RNITEL:6 TNITEL:6 TRANS:10 P:16", "DRUGI", "Standard Data Random access, Radio Unit General Information" },
	/* Category '101' Messages */
	{ 0, MPT_BOTH,	MPT_DACKD,	"1 PFIX:7 IDENT1:13 1 CAT:3=101 KIND:1=0 JOB:4=0101 RSVD:5 REASON:3 TRANS:10 P:16", "DACKD", "Standard Data general purpose acknowlegement" },
	{ 0, MPT_DOWN,	MPT_DACK_DAL,	"1 ATRANS:10 RTRANS:10 1 CAT:3=101 KIND:1=0 JOB:4=0000 W/F:3 P/N:1 RSVD:2 DN:5 TNITEL:6 ITENUM:1 P:16", "DACK+DAL", "Standard Data Codeword + DAL" },
	{ 0, MPT_DOWN,	MPT_DACK_DALG,	"1 ATRANS:10 RTRANS:10 1 CAT:3=101 KIND:1=0 JOB:4=0001 W/F:3 P/N:1 RSVD:2 DN:5 TNITEL:6 ITENUM:1 P:16", "DACK+DALG", "Standard Data Codeword + DALG" },
	{ 0, MPT_DOWN,	MPT_DACK_DALN,	"1 ATRANS:10 RTRANS:10 1 CAT:3=101 KIND:1=0 JOB:4=0010 W/F:3 P/N:1 RSVD:2 DN:5 TNITEL:6 ITENUM:1 P:16", "DACK+DALN", "Standard Data Codeword + DALN" },
	{ 0, MPT_BOTH,	MPT_DACK_GO,	"1 ATRANS:10 RTRANS:10 1 CAT:3=101 KIND:1=0 JOB:4=0011 RSVD:3 P/N:1 RSVD:1 RNITEL:6 TNITEL:6 ITENUM:1 P:16", "DACK+'GO'", "Standard Data Codeword + 'GO'" },
	{ 0, MPT_BOTH,	MPT_DACKZ,	"1 ATRANS:10 SPRE:10 1 CAT:3=101 KIND:1=0 JOB:4=0100 SX:3 SPRE:7 CAUSE:8 P:16", "DACKZ", "Standard Data Acknowledgement for expedited data" },
	{ 0, MPT_DOWN,	MPT_DAHY,	"1 TRANS:10 RSVD:10 1 CAT:3=101 KIND:1=0 JOB:4=1000 RSVD:10 SPARE:8 P:16", "DAHY", "Standard Data General ahoy" },
	{ 0, MPT_DOWN,	MPT_DAHYZ,	"1 SPRE:10 RSVD:10 1 CAT:3=101 KIND:1=0 JOB:4=1100 SX:3 SPRE:7 CAUSE:8 P:16", "DAHYZ", "Standard Data ahoy containing expedited data" },
	{ 0, MPT_DOWN,	MPT_DAHYX,	"1 PFIX:7 IDENT1:13 1 CAT:3=101 KIND:1=0 JOB:4=1110 I/T:1 RESP:1 SPRE:3 TOC:3 TRANS:10 P:16", "DHAYX", "Standard Data ahoy containing expedited data" },
	{ 0, MPT_BOTH,	MPT_RLA,	"1 TRANS:10 RSVD:10 1 CAT:3=101 KIND:1=0 JOB:4=1111 RSVD:12 SPARE:6 P:16", "RLA", "Repeat last ACK" },
	{ 0, MPT_UP,	MPT_DRQG,	"1 TRANS:10 SPARE:7 RSVD:3 1 CAT:3=101 KIND:1=0 JOB:4=1010 RSVD:18 P:16", "DRQG", "Repeat group message" },
	{ 0, MPT_UP,	MPT_DRQZ,	"1 TRANS:10 SPRE:10 1 CAT:3=101 KIND:1=0 JOB:4=1100 SX:3 SPRE:7 CAUSE:8 P:16", "DRQZ", "Request containing expedited data" },
	{ 0, MPT_UP,	MPT_DRQX,	"1 PFIX:7 IDENT1:13 1 CAT:3=101 KIND:1=0 JOB:4=1110 SPRE:5 TOC:3 TRANS:10 P:16", "DRQX", "Request to close a transaction" },
	{ 0, MPT_BOTH,	MPT_SACK,	"1 ATRANS:10 EFLAGS:10 1 CAT:3=101 KIND:1=1 TASK:1=0 RSVD:2 EFLAGS:13 ONES:4 AD:1 ITENUM:1 P:16", "SACK", "Standard Data Selective Acknowledgement Header" },
	{ 0, MPT_BOTH,	MPT_SITH_I,	"1 TRANS:10 USERDATA:10 1 CAT:3=101 KIND:1=1 TASK:1=1 I/G:1=0 MORE:1 LASTBIT:6 FRAGL:6 TNITEL:6 ITENUM:1 P:16", "SITH", "Standard Data Address Codeword (Individual) Dataitem" },
	{ 0, MPT_DOWN,	MPT_SITH_G,	"1 TRANS:10 USERDATA:10 1 CAT:3=101 KIND:1=1 TASK:1=1 I/G:1=1 MORE:1 LASTBIT:6 FRAGL:8 RSVD:4 ITENUM:1 P:16", "SITH", "Standard Data Address Codeword (Group) Dataitem" },
	/* Startup & CCSC */
	{ 1, MPT_DOWN,	MPT_START_SYNC,	"LET:32=00000000000000000000000000000000 PREAMBLE:16=1010101010101010 1100010011010111", "Startup", "Startup sequence on CC" },
	{ 0, MPT_DOWN,	MPT_CCSC,	"0 SYS:15 CCS:16 PREAMBLE:16=1010101010101010 P:16", "CCSC/DCSC", "System Identification" },
	{ 1, MPT_DOWN,	MPT_START_SYNT,	"LET:32=00000000000000000000000000000000 PREAMBLE:16=1010101010101010 0011101100101000", "SYNT", "Startup sequence on TC" },

	/* Data codewords following ACKT(QUAL=0) address codeword */
	{ 1, MPT_DOWN,	MPT_ACKT_DT1,	"0 RSA:1 FCW:2 BCD11:44 P:16", "ACKT Data 1", "Ack: Acknowledge, try on given address; Data Word 1" },
	{ 1, MPT_DOWN,	MPT_ACKT_DT2,	"0 RSVD:10 SP:1=0 PARAMETERS:36 P:16", "ACKT Data 2", "Ack: Acknowledge, try on given address; Data Word 2" },
	{ 1, MPT_DOWN,	MPT_ACKT_DT3,	"0 RSVD:10 SP:1=1 RSVD:21 EXCHANGE:2 Number:13 P:16", "ACKT Data 3", "Ack: Acknowledge, try on given address; Data Word 3" },
	{ 1, MPT_DOWN,	MPT_ACKT_DT4,	"0 RSVD:26 GF:1 PFIXT:7 IDENTT:13 P:16", "ACKT Data 4", "Ack: Acknowledge, try on given address; Data Word 4" },
	/* Data codeword following AHY address codeword */
	{ 1, MPT_DOWN,	MPT_AHY_DT,	"0 FORM:3=000 RSVD:24 PFIX2:7 IDENT2:13 P:16", "AHY Data", "Ahoy: General availability check; Data Word" },
	/* Data codeword following AHYQ address codeword */
	{ 1, MPT_DOWN,	MPT_AHYQ_DT,	"0 RSVD:27 PFIX:7 IDENT2:13 P:16", "AHYQ Data", "Ahoy: Status message; Data Word" },
	/* Data codewords appended to SAMIS, Mode 1 */
	{ 1, MPT_UP,	MPT_SAMIS_DT,	"0 RSVD:3 BCD11:44 P:16", "SAMIS Data", "Sam: Inbound Solicited Single Address Message; Data Word" },
	/* Data codeword(s) following HEAD address codeword */
	{ 1, MPT_DOWN,	MPT_HEAD_DT,	"0 RSA:1 PARAMETERS:46 P:16", "HEAD Data", "Short Data Message Header; Data Word" },
	/* Data codeword following AHYD address codeword */
	{ 1, MPT_DOWN,	MPT_AHYD_DT,	"0 FORM:3=000 RSVD:24 PFIX2:7 IDENT2:13 P:16", "AHYD Data", "Availability Check for Standard Data; Data Word" },
	/* Data codeword following Standard Data Acknowledgement Header SACK */
	{ 1, MPT_DOWN,	MPT_SACK_DT,	"0 ONES:4 EFLAGS:40 RSVD:3 P:16", "SACK Data", "Standard Data Selective Acknowledgement; Data Word" },
};

static struct mpt1327_defintion {
	int specific_only;
	enum mpt1327_codeword_dir dir;
	enum mpt1327_codeword_type type;
	const char *short_name;
	const char *long_name;
	uint64_t bits, mask;
	enum mpt1327_parameters params[64];
} mpt1327_definitions[_NUM_MPT_DEFINITIONS];

static void _CHECK_MAX_BITS(int bits, const char *name)
{
	if (bits == 64) {
		fprintf(stderr, "Message '%s' exceeds 64 bits, please fix!\n", name);
		abort();
	}
}

void init_codeword(void)
{
	uint64_t bits, mask;
	int num_bits;
	enum mpt1327_parameters params[64];
	char *param_text, *next_param, *param, *param_bits, *param_const;
	int i, j, p, b;

	if (sizeof(definitions) / sizeof(definitions[0]) != _NUM_MPT_DEFINITIONS) {
		fprintf(stderr, "definitions[] has different size than enum mpt1327_codeword_type, please fix!\n");
		abort();
	}
	if (sizeof(mpt1327_parameter_names) / sizeof(mpt1327_parameter_names[0]) != _NUM_MPT_PARAMETERS) {
		fprintf(stderr, "mpt1327_parameter_names[] has different size than enum mpt1352_parameters, please fix!\n");
		abort();
	}

	/* parse all message definitions */
	for (i = 0; i < _NUM_MPT_DEFINITIONS; i++) {
		bits = mask = 0;
		num_bits = 0;
		param_text = next_param = strdup(definitions[i].def);
		while ((param = strsep(&next_param, " "))) {
			if (param[0] >= '0' && param[0] <= '9') {
				/* param is a constant */
				while (*param) {
					if (*param < '0' || *param > '1') {
						fprintf(stderr, "Constant '%s' does not consists of '0' or '1' only, please fix!\n", param);
						abort();
					}
					_CHECK_MAX_BITS(num_bits, definitions[i].short_name);
					if ((*param++ & 1))
						bits |= 0x8000000000000000 >> num_bits;
					mask |= 0x8000000000000000 >> num_bits;
					params[num_bits] = 0;
					num_bits++;
				}
			} else {
				/* param is a parameter */
				param_bits = strchr(param, ':');
				if (!param_bits) {
					fprintf(stderr, "Param '%s' does not have a ':' to define number of bits, please fix!\n", param);
					abort();
				}
				*param_bits++ = '\0';
				/* get parameter from param text */
				for (p = 0; p < _NUM_MPT_PARAMETERS; p++) {
					if (!strcmp(mpt1327_parameter_names[p].name, param))
						break;
				}
				if (p == _NUM_MPT_PARAMETERS) {
					fprintf(stderr, "Param '%s' is not found in list of parameter names, please fix!\n", param);
					p = 0;
				}
				if (p > _NUM_MPT_PARAMETERS) {
					fprintf(stderr, "There are more parameters than definitons, please fix!\n");
					abort();
				}
				/* get constant for parameter, if given */
				param_const = strchr(param_bits, '=');
				if (param_const) {
					*param_const++ = '\0';
					if ((int)strlen(param_const) != atoi(param_bits)) {
						fprintf(stderr, "Param '%s' has %s bits, but constant '%s' does not, please fix!\n", param, param_bits, param_const);
						abort();
					}
				}
				for (b = 0; b < atoi(param_bits); b++) {
					_CHECK_MAX_BITS(num_bits, definitions[i].short_name);
					if (param_const) {
						if (param_const[b] < '0' || param_const[b] > '1') {
							fprintf(stderr, "Param '%s' has a constant '%s', but must only consist of '0' or '1', please fix!\n", param, param_const);
							abort();
						}
						if (param_const[b] == '1')
							bits |= 0x8000000000000000 >> num_bits;
						mask |= 0x8000000000000000 >> num_bits;
					}
					params[num_bits] = p;
					num_bits++;
				}
			}
		}
		free(param_text);
		if (num_bits != 64) {
			fprintf(stderr, "Message '%s' (has %d bits) is not exactly 64 bits, please fix!\n", definitions[i].short_name, num_bits);
			abort();
		}
#if 0
		printf("Message definition for '%s'\n", definitions[i].short_name);
		printf("%s\n", definitions[i].def);
		for (b = 0; b < 64; b++)
			printf("mask=%d data=%d name=%s\n", (mask >> (63 - b)) & 1, (bits >> (63 - b)) & 1, mpt1327_parameter_names[params[b]].name);
#endif
		/* check type */
		if ((int)definitions[i].type != i) {
			fprintf(stderr, "Message '%s' has type %d, but index is %d. Type and index must match, please fix!\n", definitions[i].short_name, definitions[i].type, i);
			abort();
		}
		/* store codeword definition */
		mpt1327_definitions[i].specific_only = definitions[i].specific_only;
		mpt1327_definitions[i].dir = definitions[i].dir;
		mpt1327_definitions[i].type = definitions[i].type;
		mpt1327_definitions[i].short_name = definitions[i].short_name;
		mpt1327_definitions[i].long_name = definitions[i].long_name;
		mpt1327_definitions[i].bits = bits;
		mpt1327_definitions[i].mask = mask;
		memcpy(mpt1327_definitions[i].params, params, sizeof(params));
		/* check for duplicate message types */
		for (j = 0; j < i; j++)
			if (mpt1327_definitions[j].type == definitions[i].type)
				break;
		if (j < i) {
			fprintf(stderr, "Message '%s' is duplicated (index %d and %d have same message type), please fix!\n", definitions[i].short_name, j, i);
			abort();
		}
	}
}

/* calculate check bits, ispired by olle@toolcrypt.org (snable) */
uint16_t mpt1327_checkbits(uint64_t bits, uint16_t *parityp)
{
	uint16_t check = 0x0000, parity = 0;
	int bit;
	int b;

	/* calculate check at upper 15 bits */
	for (b = 0; b < 48; b++) {
		bit = (bits >> (63 - b)) & 1;
		parity ^= bit;
		if (bit != (check >> 15))
			check ^= 0x6815;
		check <<= 1;
	}

	/* invert lowest check bit (of 15 upper bits) */
	check ^= 0x0002;

	/* finish parity and append as lest bit (bit 0) */
	for (b = 1; b < 16; b++)
		parity ^= (check >> b) & 1;
	check ^= parity;

	if (parityp)
		*parityp = parity;
	return check;
}

static void debug_codeword(const char *prefix, int i, uint64_t bits, int enc)
{
	uint64_t value;
	char text[1024];
	int column;
	int b;

	if (loglevel > LOGL_INFO)
		return;

	switch (mpt1327_definitions[i].type) {
	case MPT_START_SYNC:
	case MPT_CCSC:
	case MPT_START_SYNT:
	case MPT_ALH:
	case MPT_ALHS:
	case MPT_ALHD:
	case MPT_ALHE:
	case MPT_ALHR:
	case MPT_ALHX:
	case MPT_ALHF:
	case MPT_BCAST0:
	case MPT_BCAST1:
	case MPT_BCAST2:
	case MPT_BCAST3:
	case MPT_BCAST4:
	case MPT_BCAST5:
		if (enc && loglevel > LOGL_DEBUG)
			return;
	default:
		;
	}

	LOGP(DFRAME, LOGL_INFO, "%s Codeword %s: %s\n", prefix, mpt1327_definitions[i].short_name, mpt1327_definitions[i].long_name);
	column = 0;
	for (b = 0; b < 64; b++) {
		/* if we have first parameter or we swith to next parameter */
		if (b == 0 || mpt1327_definitions[i].params[b] != mpt1327_definitions[i].params[b - 1]) {
			value = 0;
			if (b != 0)
				text[column++] = ' ';
			if (mpt1327_definitions[i].params[b]) {
				strcpy(text + column, mpt1327_parameter_names[mpt1327_definitions[i].params[b]].name);
				column += strlen(mpt1327_parameter_names[mpt1327_definitions[i].params[b]].name);
				text[column++] = '=';
			}
		}
		value = (value << 1) | ((bits >> (63 - b)) & 1);
		text[column++] = ((bits >> (63 - b)) & 1) + '0';
#if 0
		if (b == 63 || mpt1327_definitions[i].params[b] != mpt1327_definitions[i].params[b + 1]) {
			sprintf(text + column, "(%" PRIu64 ")", value);
			column += strlen(text + column);
		}
#endif
	}
	text[column] = '\0';
	LOGP(DFRAME, LOGL_INFO, "%s\n", text);
}

uint64_t mpt1327_encode_codeword(mpt1327_codeword_t *codeword)
{
	uint64_t params[_NUM_MPT_PARAMETERS];
	uint64_t bits;
	int i, b;

	/* check all codeword definitions */
	for (i = 0; i < _NUM_MPT_DEFINITIONS; i++) {
		if (mpt1327_definitions[i].type == codeword->type)
			break;
	}
	if (i == _NUM_MPT_DEFINITIONS) {
		fprintf(stderr, "Codeword not found for type %d, please fix!\n", codeword->type);
		abort();
	}

	/* fill parameters */
	memcpy(params, codeword->params, sizeof(params));
	bits = 0;
	for (b = 63; b >= 0; b--) {
		if ((params[mpt1327_definitions[i].params[b]] & 1))
			bits |= (0x8000000000000000 >> b);
		params[mpt1327_definitions[i].params[b]] >>= 1;
	}

	/* set constants */
	bits = (bits & ~mpt1327_definitions[i].mask) | mpt1327_definitions[i].bits;

	/* calculate MPT_CCS (See MTP1327 Appendix 3) */
	if (codeword->type == MPT_CCSC) {
		uint64_t ccs = 0xaaaac4d400000000 | ((codeword->params[MPT_SYS] & 0x7fff) << 17);
		uint16_t parity;
assumption_wrong:
		ccs = (ccs & 0xffffffffffff0000) | mpt1327_checkbits(ccs, &parity);
		if (parity == 0) {
			ccs |= 0x10000;
			goto assumption_wrong;
		}
		bits = (bits & 0xffff0000ffffffff) | (((ccs ^ 0x2) & 0x1fffe) << 31);
	}

	/* add parity, if not forced by definition */
	if (!(mpt1327_definitions[i].mask & 0xffff))
		bits = (bits & 0xffffffffffff0000) | mpt1327_checkbits(bits, NULL);

	debug_codeword("Transmitting", i, bits, 1);

	return bits;
}

int mpt1327_decode_codeword(mpt1327_codeword_t *codeword, int specific, enum mpt1327_codeword_dir dir, uint64_t bits)
{
	int i, b;

	memset(codeword, 0, sizeof(*codeword));
	codeword->dir = dir;

	/* check all codeword definitions */
	for (i = 0; i < _NUM_MPT_DEFINITIONS; i++) {
		/* skip if direction does not match */
		if (dir != mpt1327_definitions[i].dir && mpt1327_definitions[i].dir != MPT_BOTH)
			continue;
		if (specific >= 0) {
			/* select where type matches */
			if (mpt1327_definitions[i].type == (unsigned int)specific)
				break;
		} else {
			/* ignore message definitions that require specifiying codeword type */
			if (mpt1327_definitions[i].specific_only)
				continue;
			/* select where masked bits match */
			if (mpt1327_definitions[i].bits == (bits & mpt1327_definitions[i].mask))
				break;
		}
	}
	if (i == _NUM_MPT_DEFINITIONS) {
		char debug[256];
		LOGP(DFRAME, LOGL_NOTICE, "Received unknown codeword or loopback from transmitter side.\n");
		for (b = 0; b < 64; b++)
			debug[b] = ((bits >> (63 - b)) & 1) + '0';
		debug[b] = '\0';
		LOGP(DFRAME, LOGL_DEBUG, "%s\n", debug);
		return -EINVAL;
	}
	codeword->type = mpt1327_definitions[i].type;
	codeword->short_name = mpt1327_definitions[i].short_name;
	codeword->long_name = mpt1327_definitions[i].long_name;

	/* fill parameters */
	for (b = 0; b < 64; b++) {
		codeword->params[mpt1327_definitions[i].params[b]] <<= 1;
		if ((bits & (0x8000000000000000 >> b)))
			codeword->params[mpt1327_definitions[i].params[b]] |= 1;
	}

	debug_codeword("Receiving", i, bits, 0);

	return 0;
}

