/* Radiocom 2000 frame transcoding
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
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <inttypes.h>
#include "../liblogging/logging.h"
#include "../libhagelbarger/hagelbarger.h"
#include "frame.h"

static const char *param_hex(uint64_t value)
{
	static char result[32];
	sprintf(result, "0x%" PRIx64, value);

	return result;
}

static const char *param_voie_rel(uint64_t value)
{
	return (value) ? "Control Channel" : "Traffic Channel";
}

static const char *param_voie_sm(uint64_t value)
{
	return (value) ? "Traffic Channel" : "Control Channel";
}

const char *param_agi(uint64_t value)
{
	switch (value) {
	case 0:
		return "Prohibited control channel (no mobile allowed)";
	case 1:
		return "New registration prohibited (registered mobiles allowed)";
	case 2:
		return "Registration is reserved to test mobiles";
	case 3:
		return "Registration for nominal mobiles (home network)";
	case 4:
		return "Registration is reserved to special mobiles";
	case 5:
	case 6:
	case 7:
		return "Registration permissible for all mobile station";
	}
	return "<invalid>";
}

const char *param_aga(uint64_t value)
{
	switch (value) {
	case 0:
		return "Outgoing calls prohibited";
	case 1:
		return "Reserved (Outgoing calls prohibited)";
	case 2:
		return "Outgoing call reserved for privileged mobiles";
	case 3:
		return "Outgoing calls permissible";
	}
	return "<invalid>";
}

const char *param_power(uint64_t value)
{
	switch (value) {
	case 0:
		return "Low";
	case 1:
		return "High";
	}
	return "<invalid>";
}

const char *param_crins(uint64_t value)
{
	switch (value) {
	case 0:
		return "Finished or just registering";
	case 1:
		return "Localization impossible (queue full)";
	case 2:
		return "Mobile station temporarily disabled";
	case 3:
		return "Mobile station definitely disabled (WILL BRICK THE PHONE!)";
	case 4:
		return "Blocked localization (BS out of order)";
	case 5:
	case 6:
		return "Reserved";
	case 7:
		return "Calling subscriber unknown";
	}
	return "<invalid>";
}

const char *param_invitation(uint64_t value)
{
	switch (value) {
	case 3:
		return "to Answer";
	case 10:
		return "to Dial";
	}
	return "<unknown>";
}

static const char *param_digit(uint64_t value)
{
	static char result[32];
	switch (value) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
	case 9:
		sprintf(result, "'%c'", (int)value + '0');
		return result;
	case 10:
		return "'*'";
	case 11:
		return "'#'";
	case 12:
		return "'A'";
	case 13:
		return "'B'";
	case 14:
		return "'C'";
	case 15:
		return "'D'";
	}
	return "<invalid>";
}

static struct r2000_element {
	char element;
	const char *name;
	const char *(*decoder_rel)(uint64_t value); /* REL sends to SM */
	const char *(*decoder_sm)(uint64_t value); /* SM sends to REL */
} r2000_element[] = {
	{ 'V', "Channel Type",		param_voie_rel,	param_voie_sm },
	{ 'C', "Channel",		NULL,		NULL },
	{ 'R', "Relais",		NULL,		NULL },
	{ 'M', "Message",		NULL,		NULL },
	{ 'D', "Deport",		NULL,		NULL },
	{ 'I', "AGI",			param_agi,	param_agi },
//	{ 'A', "AGA",			param_aga,	param_aga },
	{ 'P', "power",			param_power,	param_power },
	{ 'T', "taxe",			NULL,		NULL },
	{ 't', "SM Type",		param_hex,	param_hex },
	{ 'r', "SM Relais",		NULL,		NULL },
	{ 'f', "SM Flotte",		NULL,		NULL },
	{ 'm', "SM ID",			NULL,		NULL },
	{ 'd', "Called Flotte",		NULL,		NULL },
	{ 'c', "CRINS",			param_crins,	param_crins },
	{ 'a', "Assign Channel",	NULL,		NULL },
	{ 's', "Sequence Number",	param_hex,	param_hex },
	{ 'i', "Invitation",		param_invitation,param_invitation },
	{ 'n', "NCONV",			NULL,		NULL },
	{ '0', "1st Digit",		param_digit,	param_digit },
	{ '1', "2nd Digit",		param_digit,	param_digit },
	{ '2', "3rd Digit",		param_digit,	param_digit },
	{ '3', "4th Digit",		param_digit,	param_digit },
	{ '4', "5th Digit",		param_digit,	param_digit },
	{ '5', "6th Digit",		param_digit,	param_digit },
	{ '6', "7th Digit",		param_digit,	param_digit },
	{ '7', "8th Digit",		param_digit,	param_digit },
	{ '8', "9th Digit",		param_digit,	param_digit },
	{ '9', "10th Digit",		param_digit,	param_digit },
	{ '?', "Unknown",		param_hex,	param_hex },
	{ 0, NULL, NULL, NULL }
};

static void print_element(char element, uint64_t value, int dir, int debug)
{
	const char *(*decoder)(uint64_t value);
	int i;

	for (i = 0; r2000_element[i].element; i++) {
		if (r2000_element[i].element == element)
			break;
	}
	decoder = (dir == REL_TO_SM) ? r2000_element[i].decoder_rel : r2000_element[i].decoder_sm;

	if (!r2000_element[i].element)
		LOGP(DFRAME, debug, "Element '%c' %" PRIu64 " [Unknown]\n", element, value);
	else if (!decoder)
		LOGP(DFRAME, debug, "Element '%c' %" PRIu64 " [%s]\n", element, value, r2000_element[i].name);
	else
		LOGP(DFRAME, debug, "Element '%c' %" PRIu64 "=%s [%s]\n", element, value, decoder(value), r2000_element[i].name);
}

static void store_element(frame_t *frame, char element, uint64_t value)
{
	switch(element) {
	case 'V':
		frame->voie = value;
		break;
	case 'C':
		frame->channel = value;
		break;
	case 'R':
		frame->relais = value;
		break;
	case 'M':
		frame->message = value;
		break;
	case 'D':
		frame->deport = value;
		break;
	case 'I':
		frame->agi = value;
		break;
	case 'P':
		frame->sm_power = value;
		break;
	case 'T':
		frame->taxe = value;
		break;
	case 't':
		frame->sm_type = value;
		break;
	case 'r':
		frame->sm_relais = value;
		break;
	case 'f':
		frame->sm_flotte = value;
		break;
	case 'm':
		frame->sm_mor = value;
		break;
	case 'd':
		frame->sm_mop_demandee = value;
		break;
	case 'c':
		frame->crins = value;
		break;
	case 'a':
		frame->chan_assign = value;
		break;
	case 's':
		frame->sequence = value;
		break;
	case 'i':
		frame->invitation = value;
		break;
	case 'n':
		frame->nconv = value;
		break;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		frame->digit[element - '0'] = value;
		break;
	}
}

static uint64_t fetch_element(frame_t *frame, char element)
{
	switch(element) {
	case 'V':
		return frame->voie;
	case 'C':
		return frame->channel;
	case 'R':
		return frame->relais;
	case 'M':
		return frame->message;
	case 'D':
		return frame->deport;
	case 'I':
		return frame->agi;
	case 'P':
		return frame->sm_power;
	case 'T':
		return frame->taxe;
	case 't':
		return frame->sm_type;
	case 'r':
		return frame->sm_relais;
	case 'f':
		return frame->sm_flotte;
	case 'm':
		return frame->sm_mor;
	case 'd':
		return frame->sm_mop_demandee;
	case 'c':
		return frame->crins;
	case 'a':
		return frame->chan_assign;
	case 's':
		return frame->sequence;
	case 'i':
		return frame->invitation;
	case 'n':
		return frame->nconv;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		return frame->digit[element - '0'];
	}
	return 0;
}

static struct r2000_frame {
	int dir;
	uint8_t message;
	const char *def;
	const char *name;
} r2000_frame_def[] = {
	/*                V Channel-Relais---Msg--t--HomeRel--MobieID---------misc--------Supervisory----- */
	/* messages REL->SM */
	{ REL_TO_SM,  0, "V-CCCCCCCCRRRRRRRRRMMMMMtttrrrrrrrrrmmmmmmmmmmmmmmmm-----ccc----DDDIII++---PT---", "INSCRIPTION ACK" }, /* inscription ack */
	{ REL_TO_SM,  1, "V-CCCCCCCCRRRRRRRRRMMMMM----------------------------------------DDDIII++---PT---", "IDLE" }, /* broadcast */
	{ REL_TO_SM,  2, "V-CCCCCCCCRRRRRRRRRMMMMMtttrrrrrrrrrmmmmmmmmmmmmmmmm------------DDDIII++---PT---", "PLEASE WAIT" }, /* waiting on CC */
	{ REL_TO_SM,  3, "V-CCCCCCCCRRRRRRRRRMMMMMtttrrrrrrrrrmmmmmmmmmmmmmmmmaaaaaaaa----DDDIII++---PT---", "ASSIGN INCOMING"}, /* assign incoming call */
	{ REL_TO_SM,  4, "V-CCCCCCCCRRRRRRRRRMMMMMtttrrrrrrrrrfffffffffmmmmmmmaaaaaaaa----DDDIII++---PT---", "ASSIGN INCOMING (GROUP)"}, /* assign group call */
	{ REL_TO_SM,  5, "V-CCCCCCCCRRRRRRRRRMMMMMtttrrrrrrrrrmmmmmmmmmmmmmmmmaaaaaaaa----DDDIII++---PT---", "ASSIGN OUTGOING"}, /* assign outgoing call */
	{ REL_TO_SM,  9, "V-CCCCCCCCRRRRRRRRRMMMMMtttrrrrrrrrrmmmmmmmmmmmmmmmm------------DDDIII++---PT---", "RELEASE ON CC" }, /* release call on CC */
	{ REL_TO_SM, 16, "V-CCCCCCCCRRRRRRRRRMMMMMtttrrrrrrrrrmmmmmmmmmmmmmmmm----------------------------", "IDENTITY REQ"}, /* request identity */
	{ REL_TO_SM, 17, "V-CCCCCCCCRRRRRRRRRMMMMMtttrrrrrrrrrmmmmmmmmmmmmmmmm-----nnniiii----------------", "INVITATION"}, /* invitation */
	{ REL_TO_SM, 24, "V-CCCCCCCCRRRRRRRRRMMMMMtttrrrrrrrrrmmmmmmmmmmmmmmmm----------------------------", "RELEASE ON TC"}, /* release call */
	{ REL_TO_SM, 26, "V-CCCCCCCCRRRRRRRRRMMMMMtttrrrrrrrrrmmmmmmmmmmmmmmmm----------------------------", "SUSPEND REQ"}, /* suspend after dialing */
	/* messages SM->REL */
	{ SM_TO_REL,  0, "V-CCCCCCCCRRRRRRRRRMMMMMtttrrrrrrrrrmmmmmmmmmmmmmmmm--------ssss", "INSCRIPTION REQ" }, /* inscription */
	{ SM_TO_REL,  1, "V-CCCCCCCCRRRRRRRRRMMMMMtttrrrrrrrrrmmmmmmmmmmmmmmmm--------ssss", "CALL REQ (PRIVATE)" }, /* request call */
	{ SM_TO_REL,  2, "V-CCCCCCCCRRRRRRRRRMMMMMtttrrrrrrrrrfffffffffmmmmmmmddddddddssss", "CALL REQ (GROUP)" }, /* request call */
	{ SM_TO_REL,  3, "V-CCCCCCCCRRRRRRRRRMMMMMtttrrrrrrrrrmmmmmmmmmmmmmmmm--------ssss", "CALL REQ (PUBLIC)" }, /* request call */
	{ SM_TO_REL,  6, "V-CCCCCCCCRRRRRRRRRMMMMMtttrrrrrrrrrmmmmmmmmmmmmmmmm------------", "RELEASE ON CC" }, /* release on CC */
	{ SM_TO_REL, 16, "V-CCCCCCCCRRRRRRRRRMMMMMtttrrrrrrrrrmmmmmmmmmmmmmmmm--------ssss", "IDENTITY ACK" }, /* identity response */
	{ SM_TO_REL, 17, "V-CCCCCCCCRRRRRRRRRMMMMMtttrrrrrrrrrmmmmmmmmmmmmmmmm------------", "ANSWER" }, /* answer */
	{ SM_TO_REL, 19, "V-CCCCCCCCRRRRRRRRRMMMMM1111000033332222555544447777666699998888", "DIAL 1..10" }, /* first 10 digits */
	{ SM_TO_REL, 20, "V-CCCCCCCCRRRRRRRRRMMMMM1111000033332222555544447777666699998888", "DIAL 11..20" }, /* second 10 digits */
	{ SM_TO_REL, 24, "V-CCCCCCCCRRRRRRRRRMMMMMtttrrrrrrrrrmmmmmmmmmmmmmmmm------------", "RELEASE ON TC" }, /* release call on TC */
	{ SM_TO_REL, 26, "V-CCCCCCCCRRRRRRRRRMMMMMtttrrrrrrrrrmmmmmmmmmmmmmmmm------------", "SUSPEND ACK" }, /* release after dialing */
	{ 0, 0, NULL, NULL }
};

static const char *get_frame_def(uint8_t message, int dir)
{
	int i;

	for (i = 0; r2000_frame_def[i].def; i++) {
		if (r2000_frame_def[i].message == message && r2000_frame_def[i].dir == dir)
			return r2000_frame_def[i].def;
	}

	return NULL;
}

const char *r2000_dir_name(int dir)
{
	return (dir == REL_TO_SM) ? "REL->SM" : "SM->REL";
}

const char *r2000_frame_name(int message, int dir)
{
	static char result[32];
	int i;

	for (i = 0; r2000_frame_def[i].def; i++) {
		if (r2000_frame_def[i].message == message && r2000_frame_def[i].dir == dir) {
			sprintf(result, "%s (%d)", r2000_frame_def[i].name, message);
			return result;
		}
	}

	sprintf(result, "UNKNOWN (%d)", message);
	return result;
}

static void display_bits(const char *def, const uint8_t *message, int num, int debug)
{
	char dispbits[num + 1];
	int i;

	if (loglevel > debug)
		return;

	/* display bits */
	if (def)
		LOGP(DFRAME, debug, "%s\n", def);
	for (i = 0; i < num; i++) {
		dispbits[i] = ((message[i / 8] >> (7 - (i & 7))) & 1) + '0';
	}
	dispbits[i] = '\0';
	LOGP(DFRAME, debug, "%s\n", dispbits);
}

static int dissassemble_frame(frame_t *frame, const uint8_t *message, int num)
{
	int i;
	const char *def;
	uint64_t value;
	int dir = (num == 80) ? REL_TO_SM : SM_TO_REL;

	memset(frame, 0, sizeof(*frame));

	frame->message = message[2] & 0x1f;
	def = get_frame_def(frame->message, dir);
	if (!def) {
		LOGP(DFRAME, LOGL_NOTICE, "Received unknown message type %d (maybe radio noise)\n", frame->message);
		display_bits(NULL, message, num, LOGL_NOTICE);
		return -EINVAL;
	}

	LOGP(DFRAME, LOGL_DEBUG, "Decoding frame %s %s\n", r2000_dir_name(dir), r2000_frame_name(frame->message, dir));

	/* disassemble elements elements */
	value = 0;
	for (i = 0; i < num; i++) {
		value = (value << 1) | ((message[i / 8] >> (7 - (i & 7))) & 1);
		if (def[i + 1] != def[i]) {
			if (def[i] != '-') {
				print_element(def[i], value, dir, LOGL_DEBUG);
				store_element(frame, def[i], value);
			}
			value = 0;
		}
	}

	display_bits(def, message, num, LOGL_DEBUG);

	return 0;
}

static int assemble_frame(frame_t *frame, uint8_t *message, int num, int debug)
{
	int i;
	const char *def;
	uint64_t value = 0; // make GCC happy
	char element;
	int dir = (num == 80) ? REL_TO_SM : SM_TO_REL;

	def = get_frame_def(frame->message, dir);
	if (!def) {
		LOGP(DFRAME, LOGL_ERROR, "Cannot assemble unknown message type %d, please define/fix!\n", frame->message);
		abort();
	}
	memset(message, 0, (num + 7) / 8);

	if (debug)
		LOGP(DFRAME, LOGL_DEBUG, "Ccoding frame %s %s\n", r2000_dir_name(dir), r2000_frame_name(frame->message, dir));

	/* assemble elements elements */
	element = 0;
	for (i = num - 1; i >= 0; i--) {
		if (element != def[i]) {
			element = def[i];
			switch (def[i]) {
			case '-':
				value = 0;
				break;
			case '+':
				value = 0xffffffffffffffff;
				break;
			default:
				value = fetch_element(frame, element);
			}
		}
		message[i / 8] |= (value & 1) << (7 - (i & 7));
		value >>= 1;
	}

	if (debug) {
		for (i = 0; i < num; i++) {
			if (def[i + 1] != def[i] && def[i] != '-' && def[i] != '+') {
				value = fetch_element(frame, def[i]);
					print_element(def[i], value, dir, LOGL_DEBUG);
			}
		}

		display_bits(def, message, num, LOGL_DEBUG);
	}

	return 0;
}

/* encode frame to bits
 */
const char *encode_frame(frame_t *frame, int debug)
{
	uint8_t message[11], code[23];
	static char bits[32 + 176 + 1];
	int i;

	assemble_frame(frame, message, 80, debug);

	/* hagelbarger code */
	hagelbarger_encode(message, code, 88);
	memcpy(bits, "10101010101010101010111100010010", 32);
	for (i = 0; i < 176; i++)
		bits[i + 32] = ((code[i / 8] >> (7 - (i & 7))) & 1) + '0';
	bits[208] = '\0';

	return bits;
}

//#define GEGENPROBE

/* decode bits to frame */
int decode_frame(frame_t *frame, const char *bits)
{
	uint8_t message[11], code[23];
	int i, num = strlen(bits);

#ifdef GEGENPROBE
	printf("bits as received=%s\n", bits);
#endif
	/* hagelbarger code */
	memset(code, 0x00, sizeof(code));
	for (i = 0; i < num; i++)
		code[i / 8] |= (bits[i] & 1) << (7 - (i & 7));
	hagelbarger_decode(code, message, num / 2 - 6);

#if 0
	for (i = 0; i < num / 2; i++) {
		printf("%d", (message[i / 8] >> (7 - (i & 7))) & 1);
		if ((i & 7) == 7)
			printf(" = 0x%02x\n", message[i / 8]);
	}
#endif

#ifdef GEGENPROBE
	hagelbarger_encode(message, code, num / 2);
	printf("bits after re-encoding=");
	for (i = 0; i < num; i++)
		printf("%d", (code[i / 8] >> (7 - (i & 7))) & 1);
	printf("\n");
#endif

	return dissassemble_frame(frame, message, num / 2 - 8);
}

