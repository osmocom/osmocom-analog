/* NMT frame transcoding
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
#include <inttypes.h>
#include "../common/debug.h"
#include "../common/timer.h"
#include "nmt.h"
#include "frame.h"

uint64_t nmt_encode_channel(int channel, int power)
{
	uint64_t value = 0;

	if (channel >= 200) {
		value |= 0x800;
		channel -= 200;
	}
	if (channel >= 100) {
		value |= 0x100;
		channel -= 100;
	}
	value |= channel;
	value |= power << 9;

	return value;
}

int nmt_decode_channel(uint64_t value, int *channel, int *power)
{
	if ((value & 0xff) > 99)
		return -1;

	*channel = (value & 0xff) +
		((value & 0x100) >> 8) * 100 +
		((value & 0x800) >> 11) * 200;
	*power = (value & 0x600) >> 9;

	return 0;
}

void nmt_value2digits(uint64_t value, char *digits, int num)
{
	int digit, i;

	for (i = 0; i < num; i++) {
		digit = (value >> ((num - 1 - i) << 2)) & 0xf;
		if (digit == 10)
			digits[i] = '0';
		else if (digit == 0 || digit > 10)
			digits[i] = '?';
		else
			digits[i] = digit + '0';
	}
}

uint64_t nmt_digits2value(const char *digits, int num)
{
	int digit, i;
	uint64_t value = 0;

	for (i = 0; i < num; i++) {
		value <<= 4;
		digit = *digits++;
		if (digit >= '1' && digit <= '9')
			value |= digit - '0';
		else
			value |= 10;
	}

	return value;
}

char nmt_value2digit(uint64_t value)
{
	return "D1234567890*#ABC"[value & 0x0000f];
}

uint16_t nmt_encode_area_no(uint8_t area_no)
{
	switch (area_no) {
	case 1:
		return 0x3f3;
	case 2:
		return 0x3f4;
	case 3:
		return 0x3f5;
	case 4:
		return 0x3f6;
	default:
		return 0x000;
	}
}

/* NMT Doc 450-1 4.3.2 */
static struct nmt_frame {
	const char *digits;
	enum nmt_direction direction;
	int prefix;
	const char *nr;
	const char *description;
} nmt_frame[] = {
/*	  Digits              Dir.       Prefix	Nr.	Description */
/*0*/	{ "NNNPYYHHHHHHHHHH", MTX_TO_MS, 12,	"1a",	"Calling channel indication" },
/*1*/	{ "NNNPYYHHHHHHHHHH", MTX_TO_MS, 4,	"1b",	"Combined calling and traffic channel indication" },
/*2*/	{ "NNNPYYZXXXXXXHHH", MTX_TO_MS, 12,	"2a",	"Call to mobile subscriber on calling channel" },
/*3*/	{ "NNNPYYZXXXXXXnnn", MTX_TO_MS, 12,	"2b",	"Traffic channel allocation on calling channel" },
/*4*/	{ "NNNPYYZXXXXXXHHH", MTX_TO_MS, 12,	"2c",	"Queueing information to MS with priority on calling channel" },
/*5*/	{ "NNNPYYZXXXXXXHHH", MTX_TO_MS, 12,	"2d",	"Traffic channel scanning order on calling channel" },
/*6*/	{ "NNNPYYZXXXXXXHHH", MTX_TO_MS, 12,	"2f",	"Queuing information to ordinary MS" },
/*7*/	{ "NNNPYYZXXXXXXnnn", MTX_TO_MS, 5,	"3a",	"Traffic channel allocation on traffic channel" },
/*8*/	{ "NNNPYYZXXXXXXHHH", MTX_TO_MS, 5,	"3b",	"Identity request on traffic channel" },
/*9*/	{ "NNNPYYZXXXXXXnnn", MTX_TO_MS, 9,	"3c",	"Traffic channel allocation on traffic channel, short procedure" },
/*10*/	{ "NNNPYYJJJJJJJHHH", MTX_TO_MS, 3,	"4",	"Free traffic channel indication" },
/*11*/	{ "NNNPYYZXXXXXXLLL", MTX_TO_MS, 6,	"5a",	"Line signal" },
/*12*/	{ "NNNPYYZXXXXXXLQQ", MTX_TO_MS, 6,	"5b",	"Line signal: Answer to coin-box" },
/*13*/	{ "JJJPJJJJJJJJJJJJ", MTX_TO_XX, 0,	"6",	"Idle frame" },
/*14*/	{ "NNNPYYCCCCCCCJJJ", MTX_TO_MS, 8,	"7",	"Authentication request" },
/*15*/	{ "NNNPZXXXXXXTJJJJ", MS_TO_MTX, 1,	"10a",	"Call acknowledgement from MS on calling channel (shortened frame)" },
/*16*/	{ "NNNPZXXXXXXTYKKK", MS_TO_MTX, 1,	"10b",	"Seizure from ordinary MS and identity on traffic channel" },
/*17*/	{ "NNNPZXXXXXXTYKKK", MS_TO_MTX, 6,	"10c",	"Seizure and identity from called MS on traffic channel" },
/*18*/	{ "NNNPZXXXXXXTYKKK", MS_TO_MTX, 14,	"11a",	"Roaming updating seizure and identity on traffic channel" },
/*19*/	{ "NNNPZXXXXXXTYKKK", MS_TO_MTX, 15,	"11b",	"Seizure and call achnowledgement on calling channel from MS with priority (shortened frame)" },
/*20*/	{ "NNNPZXXXXXXTYKKK", MS_TO_MTX, 11,	"12",	"Seizure from coin-box on traffic channel" },
/*21*/	{ "NNNPZXXXXXXLLLLL", MS_TO_MTX, 8,	"13a",	"Line signal" },
/*22*/	{ "NNNPZXXXXXXLLLQQ", MS_TO_MTX, 8,	"13b",	"Line signal: Answer acknowledgement from coin box" },
/*23*/	{ "NNNPZXXXXXXSSSSS", MS_TO_MTX, 7,	"14a",	"Digit signal (1st, 3rd, 5th ........digit)" },
/*24*/	{ "NNNPZXXXXXXSSSSS", MS_TO_MTX, 7,	"14b",	"Digit signal (2nd, 4th, 6th ........digit)" },
/*25*/	{ "JJJPJJJJJJJJJJJJ", XX_TO_MTX, 0,	"15",	"Idle frame" },
/*26*/	{ "NNNPRRRRRRRRRRRR", MS_TO_MTX, 12,	"16",	"Signed response" },
/*27*/	{ "NNNPYYZJJJAfffff", MTX_TO_BS, 15,	"20",	"Channel activation order" },
/*28*/	{ "NNNPYYZJJJAJJJJJ", MTX_TO_BS, 15,	"20",	"Channel activation order" },
/*29*/	{ "NNNPYYZJJJAfffff", MTX_TO_BS, 15,	"20",	"Channel activation order" },
/*30*/	{ "NNNPYYZJJJAlllff", MTX_TO_BS, 15,	"20",	"Channel activation order" },
/*31*/	{ "NNNPYYZJJJAlllJJ", MTX_TO_BS, 15,	"20",	"Channel activation order" },
/*32*/	{ "NNNPYYZJJJVJJnnn", MTX_TO_BS, 3,	"21b",	"Signal strength measurement order on data channel or idle or free marked traffic channel" },
/*33*/	{ "NNNPYYZJJJVJJnnn", MTX_TO_BS, 5,	"21c",	"Signal strength measurement order on traffic actually used" },
/*34*/	{ "NNNPYYZJJJVVVVVV", MTX_TO_BS, 14,	"22",	"Order management/maintenance order on idle channel or data channel" },
/*35*/	{ "NNNPZJJAJJJJJJJJ", BS_TO_MTX, 9,	"25",	"Channel status information" },
/*36*/	{ "NNNPZJJAJJJfllJJ", BS_TO_MTX, 9,	"25",	"Channel status information" },
/*37*/	{ "NNNPZJJAJJJJllJJ", BS_TO_MTX, 9,	"25",	"Channel status information" },
/*38*/	{ "NNNPZJJAJJJcccJJ", BS_TO_MTX, 9,	"25",	"Channel status information" },
/*39*/	{ "NNNPZJJnnnrrrrrr", BS_TO_MTX, 2,	"26",	"Signal strength measurement result" },
/*40*/	{ "NNNPZJJVVVVJJJJJ", BS_TO_MTX, 4,	"27",	"Response on other management/maintenance order on idle channel or data channel" },
/*41*/	{ "NNNPZJJVVVVJJJJJ", BS_TO_MTX, 13,	"28",	"Other maintenance information from BS" },
/*42*/	{ "NNNPYYJJJJJJJHHH", MTX_TO_MS, 10,	"30",	"Test channel indication" },
/*43*/	{ "---P------------", MTX_TO_XX, 0,	"",	"illegal (Spare)" },
/*44*/	{ "---P------------", XX_TO_MTX, 0,	"",	"illegal (Spare)" },
	{ NULL, 0, 0, NULL, NULL }
};

/* store actual number of frames for run-time range check */
static int num_frames;

const char *nmt_frame_name(int index)
{
	if (index < 0 || index >= num_frames)
		return "invalid";
	return nmt_frame[index].nr;
}

static const char *param_integer(uint64_t value, int ndigits, enum nmt_direction direction)
{
	static char result[32];
	sprintf(result, "%" PRIu64, value);

	return result;
}

static const char *param_hex(uint64_t value, int ndigits, enum nmt_direction direction)
{
	static char result[32];
	sprintf(result, "0x%" PRIx64, value);

	return result;
}

static const char *param_channel_no(uint64_t value, int ndigits, enum nmt_direction direction)
{
	static char result[32];
	int rc, channel, power;

	rc = nmt_decode_channel(value, &channel, &power);
	if (rc < 0)
		sprintf(result, "invalid(%" PRIu64 ")", value);
	else
		sprintf(result, "channel=%d power=%d", channel, power);

	return result;
}

static const char *param_country(uint64_t value, int ndigits, enum nmt_direction direction)
{
	static char result[32];

	switch (value) {
	case 0:
		return "no additional info";
	case 4:
		return "Iceland";
	case 5:
		return "Denmark";
	case 6:
		return "Sweden";
	case 7:
		return "Norway";
	case 8:
		return "Finland";
	case 9:
		return "nordic country";
	case 14:
		return "additional info";
	case 15:
		return "information to/from BS";
	default:
		sprintf(result, "%" PRIu64 " (unknown)", value);
		return result;
	}
}

static const char *param_number(uint64_t value, int ndigits, enum nmt_direction direction)
{
	static char result[32];

	nmt_value2digits(value, result, ndigits);
	result[ndigits] = '\0';

	return result;
}

static const char *param_ta(uint64_t value, int ndigits, enum nmt_direction direction)
{
	static char result[32];

	nmt_value2digits(value, result, ndigits);
	result[ndigits] = '\0';

	return result;
}

static const char *param_line_signal(uint64_t value, int ndigits, enum nmt_direction direction)
{
	static char result[64], *desc = "Spare";

	if (direction == MTX_TO_MS) {
		switch (value & 0xf) {
		case 0:
			desc = "Answer to coin-box";
			break;
		case 3:
			desc = "Proceed to send unencrypted digits";
			break;
		case 4:
			desc = "Acknowledge MFT converter in";
			break;
		case 5:
			desc = "Switch compandor in";
			break;
		case 6:
			desc = "Address complete";
			break;
		case 7:
			desc = "Switch compandor out";
			break;
		case 9:
			desc = "Ringing order";
			break;
		case 10:
			desc = "Acknowledge MFT converter out";
			break;
		case 11:
			desc = "Proceed to send enctrypted digits";
			break;
		case 13:
			desc = "Clearing, call transfer activated";
			break;
		case 15:
			desc = "Clearing, call transfer not activated";
			break;
		}
	} else {
		switch (value & 0xf) {
		case 1:
			desc = "Clearing, release guard";
			break;
		case 2:
			desc = "Answer acknowledgement, (coin-box)";
			break;
		case 5:
			desc = "Register recall ";
			break;
		case 7:
			desc = "MFT converter out acknowledge";
			break;
		case 8:
			desc = "MFT converter in";
			break;
		case 14:
			desc = "Answer";
			break;
		}
	}

	sprintf(result, "L(%" PRIu64 ") %s", value & 0xf, desc);
	return result;
}

static const char *param_digit(uint64_t value, int ndigits, enum nmt_direction direction)
{
	static char result[32];

	if ((value & 0xf0000) != 0x00000 && (value & 0xf0000) != 0xf0000)
		return "Invalid digit";
	if ((value & 0xf0000) != ((value & 0x0f000) << 4)
	 || (value & 0x00f00) != ((value & 0x000f0) << 4)
	 || (value & 0x000f0) != ((value & 0x0000f) << 4))
		return "Inconsistent digit";

	result[0] = nmt_value2digit(value);
	result[1] = '\0';

	return result;
}

static const char *param_supervisory(uint64_t value, int ndigits, enum nmt_direction direction)
{
	switch (value) {
	case 0:
		return "Reserved";
	case 3:
		return "Frequency 1";
	case 12:
		return "Frequency 2";
	case 9:
		return "Frequency 3";
	case 6:
		return "Frequency 4";
	default:
		return "Invalid";
	}
}

static const char *param_password(uint64_t value, int ndigits, enum nmt_direction direction)
{
	static char result[32];

	nmt_value2digits(value, result, ndigits);
	result[ndigits] = '\0';

	return result;
}

static struct nmt_parameter {
	char digit;
	const char *description;
	const char *(*decoder)(uint64_t value, int ndigits, enum nmt_direction direction);
} nmt_parameter[] = {
	{ 'N',	"Channel No.",			param_channel_no },
	{ 'n',	"TC No.",			param_channel_no },
	{ 'Y',	"Traffic area",			param_ta },
	{ 'Z',	"Mobile subscriber country",	param_country },
	{ 'X',	"Mobile subscriber No.",	param_number },
	{ 'Q',	"Tariff class",			param_integer },
	{ 'L',	"Line signal",			param_line_signal },
	{ 'S',	"Digit signals",		param_digit },
	{ 'J',	"Idle information",		param_hex },
	{ 'A',	"Channel activation",		param_hex },
	{ 'V',	"Management order",		param_hex },
	{ 'r',	"Measurement results",		param_hex },
	{ 'P',	"Prefix",			param_integer },
	{ 'f',	"Supervisory signal",		param_supervisory },
	{ 'K',	"Mobile subscriber password",	param_password },
	{ 'T',	"Area info",			param_hex },
	{ 'H',	"Additional info",		param_hex },
	{ 'C',	"Random challenge",		param_hex },
	{ 'R',	"Signed response",		param_hex },
	{ 'l',	"Limit strength evaluation",	param_hex },
	{ 'c',	"c",				param_hex },
	{ 0,	NULL }
};

/* Depending on P-value, direction and additional info, frame index (used for
 * nmt_frame[]) is recoded.
 */
static int decode_frame_index(const uint8_t *digits, enum nmt_direction direction, int callack)
{
	if (direction == MS_TO_MTX || direction == BS_TO_MTX || direction == XX_TO_MTX) {
		/* MS/BS TO MTX */
		switch (digits[3]) {
		case 0:
			return 25;
		case 1:
			if (callack)
				return 15;
			return 16;
		case 2:
			return 39;
		case 3:
			break;
		case 4:
			return 40;
		case 5:
			break;
		case 6:
			return 17;
		case 7:
			if (digits[11] == 0)
				return 23;
			if (digits[11] == 15)
				return 24;
			return -1;
		case 8:
			if (digits[11] == 2)
				return 22;
			return 21;
		case 9:
			switch((digits[13] << 8) + (digits[14] << 4) + digits[15]) {
			case 2:
			case 6:
				return 36;
			case 14:
				return 37;
			case 7:
			case 8:
				return 38;
			default:
				return 35;
			}
		case 10:
			break;
		case 11:
			return 20;
		case 12:
			return 26;
		case 13:
			return 41;
		case 14:
			return 18;
		case 15:
			return 19;
		}
		return 44;
	} else {
		/* MTX to MS/BS */
		switch (digits[3]) {
		case 0:
			return 13;
		case 1:
			break;
		case 2:
			break;
		case 3:
			if (digits[6] == 15)
				return 32;
			return 10;
		case 4:
			return 1;
		case 5:
			if (digits[6] == 15)
				return 33;
			switch((digits[13] << 8) + (digits[14] << 4) + digits[15]) {
			case 0x3f3:
			case 0x3f4:
			case 0x3f5:
			case 0x3f6:
			case 0x000:
				return 8;
			default:
				return 7;
			}
		case 6:
			if (digits[13] == 0)
				return 12;
			return 11;
		case 7:
			break;
		case 8:
			return 14;
		case 9:
			return 9;
		case 10:
			return 42;
		case 11:
			break;
		case 12:
			/* no subscriber */
			if (digits[6] == 0)
				return 0;
			/* battery saving */
			if (digits[6] == 14)
				return 0;
			/* info to BS (should not happen here) */
			if (digits[6] == 15)
				return 0;
			switch((digits[13] << 8) + (digits[14] << 4) + digits[15]) {
			case 0x3f3:
			case 0x3f4:
			case 0x3f5:
			case 0x3f6:
			case 0x000:
				return 2;
			case 0x3f0:
				return 6;
			case 0x3f1:
				return 4;
			case 0x3f2:
				return 5;
			default:
				return 3;
			}
		case 13:
			break;
		case 14:
			if (digits[13] != 15)
				break;
			return 34;
		case 15:
			if (digits[13] != 15)
				break;
			switch (digits[10]) {
			case 3:
				return 27;
			case 6:
			case 13:
				return 29;
			case 7:
			case 14:
				return 30;
			case 15:
				return 31;
			default:
				return 28;
			}
		}
		return 43;
	}
}

int init_frame(void)
{
	int i, j, k;
	char digit;

	/* check if all digits actually exist */
	for (i = 0; nmt_frame[i].digits; i++) {
		for (j = 0; j < 16; j++) {
			digit = nmt_frame[i].digits[j];
			if (digit == '-')
				continue;
			for (k = 0; nmt_parameter[k].digit; k++) {
				if (nmt_parameter[k].digit == digit)
					break;
			}
			if (!nmt_parameter[k].digit) {
				PDEBUG(DFRAME, DEBUG_ERROR, "Digit '%c' in message index %d does not exist, please fix!\n", digit, i);
				return -1;
			}
		}
	}
	num_frames = i;

	return 0;
}

/* decode 16 digits frame */
static void disassemble_frame(frame_t *frame, const uint8_t *digits, enum nmt_direction direction, int callack)
{
	int index;
	int i, j, ndigits;
	char digit;
	uint64_t value;

	memset(frame, 0, sizeof(*frame));

	/* index of frame */
	index = decode_frame_index(digits, direction, callack);
	frame->index = index;

	/* update direction */
	direction = nmt_frame[index].direction;

	PDEBUG(DFRAME, DEBUG_DEBUG, "Decoding %s %s %s\n", nmt_dir_name(direction), nmt_frame[index].nr, nmt_frame[index].description);

	for (i = 0; i < 16; i++) {
		digit = nmt_frame[index].digits[i];
		if (digit == '-')
			continue;
		value = digits[i];
		ndigits = 1;
		for (j = i + 1; j < 16; j++) {
			if (nmt_frame[index].digits[j] != digit)
				break;
			value = (value << 4) | digits[j];
			ndigits++;
			i++;
		}
		switch (digit) {
		case 'N':
			frame->channel_no = value;
			break;
		case 'n':
			frame->tc_no = value;
			break;
		case 'Y':
			frame->traffic_area = value;
			break;
		case 'Z':
			frame->ms_country = value;
			break;
		case 'X':
			frame->ms_number = value;
			break;
		case 'Q':
			frame->tariff_class = value;
			break;
		case 'L':
			frame->line_signal = value;
			break;
		case 'S':
			frame->digit = value;
			break;
		case 'J':
			frame->idle = value;
			break;
		case 'A':
			frame->chan_act = value;
			break;
		case 'V':
			frame->meas_order = value;
			break;
		case 'r':
			frame->meas = value;
			break;
		case 'P':
			frame->prefix = value;
			break;
		case 'f':
			frame->supervisory = value;
			break;
		case 'K':
			frame->ms_password = value;
			break;
		case 'T':
			frame->area_info = value;
			break;
		case 'H':
			frame->additional_info = value;
			break;
		case 'C':
			frame->rand = value;
			break;
		case 'R':
			frame->sres = value;
			break;
		case 'l':
			frame->limit_strength_eval = value;
			break;
		case 'c':
			frame->c = value;
			break;
		default:
			PDEBUG(DFRAME, DEBUG_ERROR, "Digit '%c' does not exist, please fix!\n", digit);
			abort();
		}
		if (debuglevel <= DEBUG_DEBUG) {
			for (j = 0; nmt_parameter[j].digit; j++) {
				if (nmt_parameter[j].digit == digit) {
					PDEBUG(DFRAME, DEBUG_DEBUG, " %c: %s\n", digit, nmt_parameter[j].decoder(value, ndigits, direction));
				}
			}
		}
	}

	if (debuglevel <= DEBUG_DEBUG) {
		char debug_digits[17];

		for (i = 0; i < 16; i++)
			debug_digits[i] = "0123456789abcdef"[digits[i]];
		debug_digits[i] = '\0';
		PDEBUG(DFRAME, DEBUG_DEBUG, "%s\n", nmt_frame[index].digits);
		PDEBUG(DFRAME, DEBUG_DEBUG, "%s\n", debug_digits);
	}
}

/* encode 16 digits frame */
static void assemble_frame(frame_t *frame, uint8_t *digits, int debug)
{
	int index;
	int i, j;
	char digit;
	uint64_t value;
	enum nmt_direction direction;

	index = frame->index;

	if (index >= num_frames) {
		PDEBUG(DFRAME, DEBUG_ERROR, "Frame index %d out of range (0..%d), please fix!\n", index, num_frames - 1);
		abort();
	}

	/* set prefix of frame */
	frame->prefix = nmt_frame[index].prefix;

	/* retrieve direction */
	direction = nmt_frame[index].direction;

	if (debug)
		PDEBUG(DFRAME, DEBUG_DEBUG, "Coding %s %s %s\n", nmt_dir_name(direction), nmt_frame[index].nr, nmt_frame[index].description);

	for (i = 15; i >= 0; i--) {
		digit = nmt_frame[index].digits[i];
		if (digit == '-') {
			digits[i] = 0;
			continue;
		}
		switch (digit) {
		case 'N':
			value = frame->channel_no;
			break;
		case 'n':
			value = frame->tc_no;
			break;
		case 'Y':
			value = frame->traffic_area;
			break;
		case 'Z':
			value = frame->ms_country;
			break;
		case 'X':
			value = frame->ms_number;
			break;
		case 'Q':
			value = frame->tariff_class;
			break;
		case 'L':
			value = frame->line_signal;
			break;
		case 'S':
			value = frame->digit;
			break;
		case 'J':
			value = frame->idle;
			break;
		case 'A':
			value = frame->chan_act;
			break;
		case 'V':
			value = frame->meas_order;
			break;
		case 'r':
			value = frame->meas;
			break;
		case 'P':
			value = frame->prefix;
			break;
		case 'f':
			value = frame->supervisory;
			break;
		case 'K':
			value = frame->ms_password;
			break;
		case 'T':
			value = frame->area_info;
			break;
		case 'H':
			value = frame->additional_info;
			break;
		case 'C':
			value = frame->rand;
			break;
		case 'R':
			value = frame->sres;
			break;
		case 'l':
			value = frame->limit_strength_eval;
			break;
		case 'c':
			value = frame->c;
			break;
		default:
			PDEBUG(DFRAME, DEBUG_ERROR, "Digit '%c' does not exist, please fix!\n", digit);
			abort();
		}

		digits[i] = (value & 0xf);
		value >>= 4;
		for (j = i - 1; j >= 0; j--) {
			if (nmt_frame[index].digits[j] != digit)
				break;
			digits[j] = (value & 0xf);
			value >>= 4;
			i--;
		}
	}
	if (debug && debuglevel <= DEBUG_DEBUG) {
		char debug_digits[17];
		int ndigits;

		for (i = 0; i < 16; i++) {
			digit = nmt_frame[index].digits[i];
			if (digit == '-')
				continue;
			value = digits[i];
			ndigits = 1;
			for (j = i + 1; j < 16; j++) {
				if (nmt_frame[index].digits[j] != digit)
					break;
				value = (value << 4) | digits[j];
				ndigits++;
				i++;
			}
			for (j = 0; nmt_parameter[j].digit; j++) {
				if (nmt_parameter[j].digit == digit) {
					PDEBUG(DFRAME, DEBUG_DEBUG, " %c: %s\n", digit, nmt_parameter[j].decoder(value, ndigits, direction));
				}
			}
		}

		for (i = 0; i < 16; i++)
			debug_digits[i] = "0123456789abcdef"[digits[i]];
		debug_digits[i] = '\0';
		PDEBUG(DFRAME, DEBUG_DEBUG, "%s\n", nmt_frame[index].digits);
		PDEBUG(DFRAME, DEBUG_DEBUG, "%s\n", debug_digits);
	}
}

/* encode digits of a frame to 166 bits */
static void encode_digits(const uint8_t *digits, char *bits)
{
	uint8_t x[64];
	int i;
	uint8_t digit;

	/* copy bit sync and frame sync */
	memcpy(bits, "10101010101010111100010010", 26);
	bits += 26;

	for (i = 0; i < 16; i++) {
		digit = *digits++;
		x[(i << 2) + 0] = (digit >> 3) & 1;
		x[(i << 2) + 1] = (digit >> 2) & 1;
		x[(i << 2) + 2] = (digit >> 1) & 1;
		x[(i << 2) + 3] = digit & 1;
	}

	/* parity check bits */
	for (i = 0; i < 3; i++)
		bits[(i << 1)] = '1' - x[i];
	for (i = 3; i < 64; i++)
		bits[(i << 1)] = '1' - (x[i] ^ x[i - 3]);
	for (i = 64; i < 67; i++)
		bits[(i << 1)] = '1' - x[i - 3];
	for (i = 67; i < 70; i++)
		bits[(i << 1)] = '1';

	/* information bits */
	for (i = 0; i < 6; i++)
		bits[(i << 1) + 1] = '0';
	for (i = 6; i < 70; i++)
		bits[(i << 1) + 1] = x[i - 6] + '0';
}

/* debug parity check */
//#define DEBUG_DECODE

/* decode digits from 140 bits (not including sync) */
// FIXME: do real convolutional decoding
static int decode_digits(uint8_t *digits, const char *bits, int callack)
{
	uint8_t x[64];
	int i, short_frame = 0;

	/* information bits */
	for (i = 0; i < 6; i++) {
		if (bits[(i << 1) + 1] != '0') {
#ifdef DEBUG_DECODE
			PDEBUG(DFRAME, DEBUG_DEBUG, "Frame bad at information bit #%d.\n", i);
#endif
			return -1;
		}
	}
	for (i = 6; i < 70; i++)
		x[i - 6] = bits[(i << 1) + 1] - '0';

	/* create digits */
	for (i = 0; i < 16; i++) {
		digits[i] = ((x[(i << 2) + 0] & 1) << 3)
			  | ((x[(i << 2) + 1] & 1) << 2)
			  | ((x[(i << 2) + 2] & 1) << 1)
			  | (x[(i << 2) + 3] & 1);
	}

	/* check for short frame */
	if (callack && (digits[3] == 1 || digits[3] == 15)) {
		digits[13] = 0;
		digits[14] = 0;
		digits[15] = 0;
		short_frame = 1;
#ifdef DEBUG_DECODE
		PDEBUG(DFRAME, DEBUG_DEBUG, "Received shortened frame\n");
#endif
	}

	/* parity check bits */
	for (i = 0; i < 3; i++) {
		if (bits[(i << 1)] != '1' - x[i]) {
#ifdef DEBUG_DECODE
			PDEBUG(DFRAME, DEBUG_DEBUG, "Frame bad at parity bit #%d.\n", i);
#endif
			return -1;
		}
	}
#if 0
#warning this check does not work, since we get error even at bit 50
	for (i = 3; i < ((short_frame) ? 52 : 64); i++) {
		if (bits[(i << 1)] != '1' - (x[i] ^ x[i - 3])) {
			/* According to NMT Doc 450-3, bits after Y(114) shall
			 * be omitted for short frame. It would make more sense
			 * to stop after Y(116), so only the last three digits
			 * are omitted and not the last bit of digit13 also.
			 * Tests have showed that it we receive correctly up to
			 * Y(116), but we ignore an error, if only up to Y(114) 
			 * is received.
			 */
			if (short_frame && i == 51) {
				PDEBUG(DFRAME, DEBUG_DEBUG, "Frame bad after bit Y(114), ignoring.\n");
				digits[13] = 0;
				break;
			}
#ifdef DEBUG_DECODE
			PDEBUG(DFRAME, DEBUG_DEBUG, "Frame bad at parity bit #%d.\n", i);
#endif
			return -1;
		}
	}
#endif
	/* Tests showed corrupt frame at parity #50 (i == 50)
	 * We just ignore whatever we receive after 48 bits of checksum.
	 * This is not the correct approach, but in case of a corrupt
	 * frame 10.a, we would drop it, if it failes later checks.
	 */
	for (i = 3; i < ((short_frame) ? 48 : 64); i++) {
		if (bits[(i << 1)] != '1' - (x[i] ^ x[i - 3])) {
#ifdef DEBUG_DECODE
			PDEBUG(DFRAME, DEBUG_DEBUG, "Frame bad at parity bit #%d.\n", i);
#endif
			return -1;
		}
	}
	if (short_frame)
		return 0;
	for (i = 64; i < 67; i++) {
		if (bits[(i << 1)] != '1' - x[i - 3]) {
#ifdef DEBUG_DECODE
			PDEBUG(DFRAME, DEBUG_DEBUG, "Frame bad at parity bit #%d.\n", i);
#endif
			return -1;
		}
	}
	for (i = 67; i < 70; i++) {
		if (bits[(i << 1)] != '1') {
#ifdef DEBUG_DECODE
			PDEBUG(DFRAME, DEBUG_DEBUG, "Frame bad at parity bit #%d.\n", i);
#endif
			return -1;
		}
	}

	return 0;
}

/* encode frame to bits
 * debug can be turned on or off
 */
const char *encode_frame(frame_t *frame, int debug)
{
	uint8_t digits[16];
	static char bits[166];

	assemble_frame(frame, digits, debug);
	encode_digits(digits, bits);

	return bits;
}

/* decode bits to frame */
int decode_frame(frame_t *frame, const char *bits, enum nmt_direction direction, int callack)
{
	uint8_t digits[16];
	int rc;

	rc = decode_digits(digits, bits, callack);
	if (rc < 0)
		return rc;
	disassemble_frame(frame, digits, direction, callack);

	return 0;
}

