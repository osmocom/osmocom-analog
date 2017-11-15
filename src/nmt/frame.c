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
#include "../common/sample.h"
#include "../common/debug.h"
#include "../libhagelbarger/hagelbarger.h"
#include "nmt.h"
#include "frame.h"

uint64_t nmt_encode_channel(int nmt_system, int channel, int power)
{
	uint64_t value = 0;

	if (nmt_system == 450) {
		if (channel >= 200) {
			value |= 0x800;
			channel -= 200;
		}
		if (channel >= 100) {
			value |= 0x100;
			channel -= 100;
		}
		value |= channel % 10;
		value |= (channel / 10) << 4;
		value |= power << 9;
	} else {
		/* interleaved channels are indicated in traffic area */
		if (value >= 1024)
			value -= 1024;
		value |= channel;
		/* if channel >= 512, set upper bit */
		if (value & 0x200)
			value = value - 0x200 + 0x800;
		value |= power << 9;
	}

	return value;
}

int nmt_decode_channel(int nmt_system, uint64_t value, int *channel, int *power)
{
	if (nmt_system == 450) {
		if ((value & 0x00f) > 0x009)
			return -1;
		if ((value & 0x0f0) > 0x090)
			return -1;

		*channel = (value & 0x00f) +
			((value & 0x0f0) >> 4) * 10 +
			((value & 0x100) >> 8) * 100 +
			((value & 0x800) >> 11) * 200;
	} else {
		*channel = (value & 0x1ff) +
			((value & 0x800) >> 2);
	}
	*power = (value & 0x600) >> 9;

	return 0;
}

uint64_t nmt_encode_tc(int nmt_system, int channel, int power)
{
	uint64_t value = 0;

	if (nmt_system == 450) {
		if (channel >= 200) {
			value |= 0x800;
			channel -= 200;
		}
		if (channel >= 100) {
			value |= 0x100;
			channel -= 100;
		}
		value |= channel % 10;
		value |= (channel / 10) << 4;
		value |= power << 9;
	} else {
		value = channel;
	}

	return value;
}

static int nmt_decode_tc(int nmt_system, uint64_t value, int *channel, int *power)
{
	if (nmt_system == 450) {
		if ((value & 0x00f) > 0x009)
			return -1;
		if ((value & 0x0f0) > 0x090)
			return -1;

		*channel = (value & 0x00f) +
			((value & 0x0f0) >> 4) * 10 +
			((value & 0x100) >> 8) * 100 +
			((value & 0x800) >> 11) * 200;
		*power = (value & 0x600) >> 9;
	} else {
		*channel = value & 0x3ff;
	}

	return 0;
}

uint64_t nmt_encode_traffic_area(int nmt_system, int channel, uint8_t traffic_area)
{
	uint64_t value = 0;

	if (nmt_system == 450) {
		value = traffic_area;
	} else {
		/* upper bit is used for indication of interleaved channel */
		value = traffic_area & 0x7f;
		if (channel >= 1024)
			value |= 0x80;
	}

	return value;
}

void nmt_value2digits(uint64_t value, char *digits, int num)
{
	int digit, i;

	for (i = 0; i < num; i++) {
		digit = (value >> ((num - 1 - i) << 2)) & 0xf;
		if (digit == 10)
			digits[i] = '0';
		else if (digit == 0)
			digits[i] = 'N';
		else if (digit > 10)
			digits[i] = digit - 10 + 'a';
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
		else if (digit == '0')
			value |= 10;
		else if (digit >= 'a' && digit <= 'f')
			value |= digit - 'a' + 10;
		else if (digit >= 'A' && digit <= 'F')
			value |= digit - 'A' + 10;
		else
			value |= 0;
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

/* convert given number to caller ID frame with given index
 * return next index */
int nmt_encode_a_number(frame_t *frame, int index, enum number_type type, const char *number, int nmt_system, int channel, int ms_power, uint8_t traffic_area)
{
	int number_offset = 0;
	int number_len = strlen(number);
	int nframes;
	uint8_t sum, ntype = 0, digit;
	int i, shift;

	/* number of frames
	 * 0..5 digits need one frame, 6..12 digits need two frames, ... */
	nframes = (number_len + 8) / 7;

	/* cycle index */
	index %= nframes;

	/* number offset for second frame is 5, and then additional 7 for the following frames */
	if (index)
		number_offset = index * 7 - 2;

	/* encode */
	frame->mt = NMT_MESSAGE_8;
	frame->channel_no = nmt_encode_channel(nmt_system, channel, ms_power);
	frame->traffic_area = nmt_encode_traffic_area(nmt_system, channel, traffic_area);
	frame->seq_number = index;
	if (index == 0) {
		/* number type */
		switch (type) {
		case TYPE_NOTAVAIL:
			ntype = 3;
			break;
		case TYPE_ANONYMOUS:
			ntype = 4;
			break;
		case TYPE_UNKNOWN:
			ntype = 0;
			break;
		case TYPE_SUBSCRIBER:
			ntype = 0;
			break;
		case TYPE_NATIONAL:
			ntype = 1;
			break;
		case TYPE_INTERNATIONAL:
			ntype = 2;
			break;
		}
		/* first 5 digits */
		frame->additional_info = ((nframes - 1) << 24) | (ntype << 20);
		shift = 16;
	} else {
		/* next digits */
		frame->additional_info = 0;
		shift = 24;
	}
	for (i = number_offset; number[i] && shift >= 0; i++, shift -= 4) {
		digit = number[i];
		if (digit >= '1' && digit <= '9')
			digit -= '0';
		else if (digit == '0')
			digit = 10;
		else
			digit = 13; /* '+' and illegal digits */
		frame->additional_info |= (digit << shift);
	}

	/* checksum */
	sum = (frame->seq_number << 4) | frame->additional_info >> 24;
	sum += (frame->additional_info >> 16);
	sum += (frame->additional_info >> 8);
	sum += frame->additional_info;
	frame->checksum = sum;

	/* return next frame index or cycle to first frame */
	if (++index == nframes)
		index = 0;
	return index;
}

/* NMT Doc 450-1 4.3.2 */
static struct nmt_frame {
	enum nmt_mt message_type;
	const char *digits;
	enum nmt_direction direction;
	int prefix;
	const char *nr;
	const char *description;
} nmt_frame[] = {
/*	  Define		Digits              Dir.       Prefix	Nr.	Description */
	{ NMT_MESSAGE_1a,	"NNNPYYHHHHHHHHHH", MTX_TO_MS, 12,	"1a",	"Calling channel indication" },
	{ NMT_MESSAGE_1a_a,	"NNNPYYHHHHHHHHHH", MTX_TO_MS, 11,	"1a'",	"Calling channel indication (for MS group A)" },
	{ NMT_MESSAGE_1a_b,	"NNNPYYHHHHHHHHHH", MTX_TO_MS, 13,	"1a''",	"Calling channel indication (for MS group B)" },
	{ NMT_MESSAGE_1b,	"NNNPYYHHHHHHHHHH", MTX_TO_MS, 4,	"1b",	"Combined calling and traffic channel indication" },
	{ NMT_MESSAGE_2a,	"NNNPYYZXXXXXXHHH", MTX_TO_MS, 12,	"2a",	"Call to mobile subscriber on calling channel" },
	{ NMT_MESSAGE_2b,	"NNNPYYZXXXXXXnnn", MTX_TO_MS, 12,	"2b",	"Traffic channel allocation on calling channel" },
	{ NMT_MESSAGE_2c,	"NNNPYYZXXXXXXHHH", MTX_TO_MS, 12,	"2c",	"Queueing information to MS with priority on calling channel" },
	{ NMT_MESSAGE_2d,	"NNNPYYZXXXXXXHHH", MTX_TO_MS, 12,	"2d",	"Traffic channel scanning order on calling channel" },
	{ NMT_MESSAGE_2e,	"NNNPYYZXXXXXXHHH", MTX_TO_MS, 4,	"2e",	"Alternative type of call to MS on combinded CC/TC" },
	{ NMT_MESSAGE_2f,	"NNNPYYZXXXXXXHHH", MTX_TO_MS, 12,	"2f",	"Queuing information to ordinary MS" },
	{ NMT_MESSAGE_3a,	"NNNPYYZXXXXXXnnn", MTX_TO_MS, 5,	"3a",	"Traffic channel allocation on traffic channel" },
	{ NMT_MESSAGE_3b,	"NNNPYYZXXXXXXHHH", MTX_TO_MS, 5,	"3b",	"Identity request on traffic channel" },
	{ NMT_MESSAGE_3c,	"NNNPYYZXXXXXXnnn", MTX_TO_MS, 9,	"3c",	"Traffic channel allocation on traffic channel, short procedure" },
	{ NMT_MESSAGE_3d,	"NNNPYYZXXXXXXnnn", MTX_TO_MS, 7,	"3d",	"Traffic channel allocation on access channel" },
	{ NMT_MESSAGE_4,	"NNNPYYJJJJJJJHHH", MTX_TO_MS, 3,	"4",	"Free traffic channel indication" },
	{ NMT_MESSAGE_4b,	"NNNPYYJJJJJJJHHH", MTX_TO_MS, 7,	"4b",	"Access channel indication" },
	{ NMT_MESSAGE_5a,	"NNNPYYZXXXXXXLLL", MTX_TO_MS, 6,	"5a",	"Line signal" },
	{ NMT_MESSAGE_5b,	"NNNPYYZXXXXXXLQQ", MTX_TO_MS, 6,	"5b",	"Line signal: Answer to coin-box" },
	{ NMT_MESSAGE_6,	"JJJPJJJJJJJJJJJJ", MTX_TO_XX, 0,	"6",	"Idle frame" },
	{ NMT_MESSAGE_7,	"NNNPYYCCCCCCCJJJ", MTX_TO_MS, 8,	"7",	"Authentication request" },
	{ NMT_MESSAGE_8,	"NNNPYYMHHHHHHHWW", MTX_TO_MS, 1,	"8",	"A-subscriber number" },
	{ NMT_MESSAGE_10a,	"NNNPZXXXXXXTJJJJ", MS_TO_MTX, 1,	"10a",	"Call acknowledgment from MS on calling channel and access on access channel (shortened frame)" },
	{ NMT_MESSAGE_10b,	"NNNPZXXXXXXTYKKK", MS_TO_MTX, 1,	"10b",	"Seizure from ordinary MS and identity on traffic channel" },
	{ NMT_MESSAGE_10c,	"NNNPZXXXXXXTYKKK", MS_TO_MTX, 6,	"10c",	"Seizure and identity from called MS on traffic channel" },
	{ NMT_MESSAGE_10d,	"NNNPZXXXXXXTJJJJ", MS_TO_MTX, 6,	"10d",	"Call acknowledgement from MS on the alternative type of call on combined CC/TC (shortened frame)" },
	{ NMT_MESSAGE_11a,	"NNNPZXXXXXXTYKKK", MS_TO_MTX, 14,	"11a",	"Roaming updating seizure and identity on traffic channel" },
	{ NMT_MESSAGE_11b,	"NNNPZXXXXXXTYKKK", MS_TO_MTX, 15,	"11b",	"Seizure and call achnowledgment on calling channel from MS with priority (shortened frame)" },
	{ NMT_MESSAGE_12,	"NNNPZXXXXXXTYKKK", MS_TO_MTX, 11,	"12",	"Seizure from coin-box on traffic channel" },
	{ NMT_MESSAGE_13a,	"NNNPZXXXXXXLLLLL", MS_TO_MTX, 8,	"13a",	"Line signal" },
	{ NMT_MESSAGE_13b,	"NNNPZXXXXXXLLLQQ", MS_TO_MTX, 8,	"13b",	"Line signal: Answer acknowledgment from coin box" },
	{ NMT_MESSAGE_14a,	"NNNPZXXXXXXSSSSS", MS_TO_MTX, 7,	"14a",	"Digit signal (1st, 3rd, 5th ........digit)" },
	{ NMT_MESSAGE_14b,	"NNNPZXXXXXXSSSSS", MS_TO_MTX, 7,	"14b",	"Digit signal (2nd, 4th, 6th ........digit)" },
	{ NMT_MESSAGE_15,	"JJJPJJJJJJJJJJJJ", XX_TO_MTX, 0,	"15",	"Idle frame" },
	{ NMT_MESSAGE_16,	"NNNPRRRRRRRRRRRR", MS_TO_MTX, 12,	"16",	"Signed response" },
	{ NMT_MESSAGE_20_1,	"NNNPYYZJJJAfffff", MTX_TO_BS, 15,	"20",	"Channel activation order" },
	{ NMT_MESSAGE_20_2,	"NNNPYYZJJJAJJJJJ", MTX_TO_BS, 15,	"20",	"Channel activation order" },
	{ NMT_MESSAGE_20_3,	"NNNPYYZJJJAfffff", MTX_TO_BS, 15,	"20",	"Channel activation order" },
	{ NMT_MESSAGE_20_4,	"NNNPYYZJJJAlllff", MTX_TO_BS, 15,	"20",	"Channel activation order" },
	{ NMT_MESSAGE_20_5,	"NNNPYYZJJJAlllJJ", MTX_TO_BS, 15,	"20",	"Channel activation order" },
	{ NMT_MESSAGE_21b,	"NNNPYYZJJJVJJnnn", MTX_TO_BS, 3,	"21b",	"Signal strength measurement order on data channel or idle or free marked traffic channel" },
	{ NMT_MESSAGE_21c,	"NNNPYYZJJJVJJnnn", MTX_TO_BS, 5,	"21c",	"Signal strength measurement order on traffic actually used" },
	{ NMT_MESSAGE_22,	"NNNPYYZJJJVVVVVV", MTX_TO_BS, 14,	"22",	"Order management/maintenance order on idle channel or data channel" },
	{ NMT_MESSAGE_25_1,	"NNNPZJJAJJJJJJJJ", BS_TO_MTX, 9,	"25",	"Channel status information" },
	{ NMT_MESSAGE_25_2,	"NNNPZJJAJJJfllJJ", BS_TO_MTX, 9,	"25",	"Channel status information" },
	{ NMT_MESSAGE_25_3,	"NNNPZJJAJJJJllJJ", BS_TO_MTX, 9,	"25",	"Channel status information" },
	{ NMT_MESSAGE_25_4,	"NNNPZJJAJJJcccJJ", BS_TO_MTX, 9,	"25",	"Channel status information" },
	{ NMT_MESSAGE_26,	"NNNPZJJnnnrrrrrr", BS_TO_MTX, 2,	"26",	"Signal strength measurement result" },
	{ NMT_MESSAGE_27,	"NNNPZJJVVVVJJJJJ", BS_TO_MTX, 4,	"27",	"Response on other management/maintenance order on idle channel or data channel" },
	{ NMT_MESSAGE_28,	"NNNPZJJVVVVJJJJJ", BS_TO_MTX, 13,	"28",	"Other maintenance information from BS" },
	{ NMT_MESSAGE_30,	"NNNPYYJJJJJJJHHH", MTX_TO_MS, 10,	"30",	"Test channel indication" },
	{ NMT_MESSAGE_UKN_MTX,	"---P------------", MTX_TO_XX, 0,	"",	"illegal (Spare)" },
	{ NMT_MESSAGE_UKN_B,	"---P------------", XX_TO_MTX, 0,	"",	"illegal (Spare)" },
	{ 0, NULL, 0, 0, NULL, NULL }
};

/* store actual number of frames for run-time range check */
static int num_frames;

const char *nmt_frame_name(enum nmt_mt mt)
{
	if ((int)mt < 0 || (int)mt >= num_frames)
		return "invalid";
	return nmt_frame[mt].nr;
}

static const char *param_integer(uint64_t value, int __attribute__((unused)) ndigits, enum nmt_direction __attribute__((unused)) direction)
{
	static char result[32];
	sprintf(result, "%" PRIu64, value);

	return result;
}

static const char *param_hex(uint64_t value, int __attribute__((unused)) ndigits, enum nmt_direction __attribute__((unused)) direction)
{
	static char result[32];
	sprintf(result, "0x%" PRIx64, value);

	return result;
}

static const char *param_channel_no_450(uint64_t value, int __attribute__((unused)) ndigits, enum nmt_direction __attribute__((unused)) direction)
{
	static char result[32];
	int rc, channel, power;

	rc = nmt_decode_channel(450, value, &channel, &power);
	if (rc < 0)
		sprintf(result, "invalid(%" PRIu64 ")", value);
	else
		sprintf(result, "channel=%d power=%d", channel, power);

	return result;
}

static const char *param_channel_no_900(uint64_t value, int __attribute__((unused)) ndigits, enum nmt_direction __attribute__((unused)) direction)
{
	static char result[32];
	int rc, channel, power;

	rc = nmt_decode_channel(900, value, &channel, &power);
	if (rc < 0)
		sprintf(result, "invalid(%" PRIu64 ")", value);
	else
		sprintf(result, "channel=%d power=%d", channel, power);

	return result;
}

static const char *param_tc_no_450(uint64_t value, int __attribute__((unused)) ndigits, enum nmt_direction __attribute__((unused)) direction)
{
	static char result[32];
	int rc, channel, power;

	rc = nmt_decode_tc(450, value, &channel, &power);
	if (rc < 0)
		sprintf(result, "invalid(%" PRIu64 ")", value);
	else
		sprintf(result, "channel=%d power=%d", channel, power);

	return result;
}

static const char *param_tc_no_900(uint64_t value, int __attribute__((unused)) ndigits, enum nmt_direction __attribute__((unused)) direction)
{
	static char result[32];
	int rc, channel;

	rc = nmt_decode_tc(900, value, &channel, NULL);
	if (rc < 0)
		sprintf(result, "invalid(%" PRIu64 ")", value);
	else
		sprintf(result, "channel=%d", channel);

	return result;
}

static const char *param_country(uint64_t value, int __attribute__((unused)) ndigits, enum nmt_direction __attribute__((unused)) direction)
{
	static char result[32];

	switch (value) {
	case 0:
		return "no additional info";
	case 1:
		return "Netherlands / Luxemburg / Malaysia / Switzerland";
	case 2:
		return "Belgium";
	case 4:
		return "Iceland / Thailand";
	case 5:
		return "Denmark";
	case 6:
		return "Sweden / Slovakia";
	case 7:
		return "Norway / Czech";
	case 8:
		return "Finland / Spain / Indonesia";
	case 9:
		return "nordic country / Austria";
	case 10:
		return "Austria";
	case 14:
		return "additional info";
	case 15:
		return "information to/from BS";
	default:
		sprintf(result, "%" PRIu64 " (unknown)", value);
		return result;
	}
}

static const char *param_number(uint64_t value, int ndigits, enum nmt_direction __attribute__((unused)) direction)
{
	static char result[32];

	nmt_value2digits(value, result, ndigits);
	result[ndigits] = '\0';

	return result;
}

static const char *param_ta_450(uint64_t value, int ndigits, enum nmt_direction __attribute__((unused)) direction)
{
	static char result[32];

	nmt_value2digits(value, result, ndigits);
	result[ndigits] = '\0';

	return result;
}

static const char *param_ta_900(uint64_t value, int __attribute__((unused)) ndigits, enum nmt_direction __attribute__((unused)) direction)
{
	static char result[32];

	if ((value & 0x80))
		sprintf(result, "%" PRIu64 " (Channel No. + 1024)", value & 0x7f);
	else
		sprintf(result, "%" PRIu64, value);

	return result;
}

static const char *param_line_signal(uint64_t value, int __attribute__((unused)) ndigits, enum nmt_direction direction)
{
	char *desc = "Spare";
	static char result[64];

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
			desc = "Proceed to send encrypted digits";
			break;
		case 12:
			desc = "Request to receive short message";
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
			desc = "Answer acknowledgment, (coin-box)";
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
		case 12:
			desc = "Acknowledge SMS request";
			break;
		case 14:
			desc = "Answer";
			break;
		}
	}

	sprintf(result, "L(%" PRIu64 ") %s", value & 0xf, desc);
	return result;
}

static const char *param_digit(uint64_t value, int __attribute__((unused)) ndigits, enum nmt_direction __attribute__((unused)) direction)
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

static const char *param_supervisory(uint64_t value, int __attribute__((unused)) ndigits, enum nmt_direction __attribute__((unused)) direction)
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

static const char *param_password(uint64_t value, int ndigits, enum nmt_direction __attribute__((unused)) direction)
{
	static char result[32];

	nmt_value2digits(value, result, ndigits);
	result[ndigits] = '\0';

	return result;
}

static struct nmt_parameter {
	int system;
	char digit;
	const char *description;
	const char *(*decoder)(uint64_t value, int ndigits, enum nmt_direction direction);
} nmt_parameter[] = {
	{ 450,	'N',	"Channel No.",			param_channel_no_450 },
	{ 900,	'N',	"Channel No.",			param_channel_no_900 },
	{ 450,	'n',	"TC No.",			param_tc_no_450 },
	{ 900,	'n',	"TC No.",			param_tc_no_900 },
	{ 450,	'Y',	"Traffic area",			param_ta_450 },
	{ 900,	'Y',	"Traffic area",			param_ta_900 },
	{ 0,	'Z',	"Mobile subscriber country",	param_country },
	{ 0,	'X',	"Mobile subscriber No.",	param_number },
	{ 0,	'Q',	"Tariff class",			param_integer },
	{ 0,	'L',	"Line signal",			param_line_signal },
	{ 0,	'S',	"Digit signals",		param_digit },
	{ 0,	'J',	"Idle information",		param_hex },
	{ 0,	'A',	"Channel activation",		param_hex },
	{ 0,	'V',	"Management order",		param_hex },
	{ 0,	'r',	"Measurement results",		param_hex },
	{ 0,	'P',	"Prefix",			param_integer },
	{ 0,	'f',	"Supervisory signal",		param_supervisory },
	{ 0,	'K',	"Mobile subscriber password",	param_password },
	{ 0,	'T',	"Area info",			param_hex },
	{ 0,	'H',	"Additional info",		param_hex },
	{ 0,	'C',	"Random challenge",		param_hex },
	{ 0,	'R',	"Signed response",		param_hex },
	{ 0,	'l',	"Limit strength evaluation",	param_hex },
	{ 0,	'c',	"c",				param_hex },
	{ 0,	'M',	"Sequence Number",		param_integer },
	{ 0,	'W',	"Checksum",			param_hex },
	{ 0,	0,	NULL,				NULL }
};

/* Depending on P-value, direction and additional info, frame index (used for
 * nmt_frame[]) is decoded.
 */
enum nmt_mt decode_frame_mt(const uint8_t *digits, enum nmt_direction direction, int callack)
{
	if (direction == MS_TO_MTX || direction == BS_TO_MTX || direction == XX_TO_MTX) {
		/* MS/BS TO MTX */
		switch (digits[3]) {
		case 0:
			return NMT_MESSAGE_15;
		case 1:
			if (callack)
				return NMT_MESSAGE_10a;
			return NMT_MESSAGE_10b;
		case 2:
			return NMT_MESSAGE_26;
		case 3:
			break;
		case 4:
			return NMT_MESSAGE_27;
		case 5:
			break;
		case 6:
			return NMT_MESSAGE_10c;
		case 7:
			if (digits[11] == 0)
				return NMT_MESSAGE_14a;
			if (digits[11] == 15)
				return NMT_MESSAGE_14b;
			break;
		case 8:
			if (digits[11] == 2)
				return NMT_MESSAGE_13b;
			return NMT_MESSAGE_13a;
		case 9:
			switch((digits[13] << 8) + (digits[14] << 4) + digits[15]) {
			case 2:
			case 6:
				return NMT_MESSAGE_25_2;
			case 14:
				return NMT_MESSAGE_25_3;
			case 7:
			case 8:
				return NMT_MESSAGE_25_4;
			default:
				return NMT_MESSAGE_25_1;
			}
		case 10:
			break;
		case 11:
			return NMT_MESSAGE_12;
		case 12:
			return NMT_MESSAGE_16;
		case 13:
			return NMT_MESSAGE_28;
		case 14:
			return NMT_MESSAGE_11a;
		case 15:
			return NMT_MESSAGE_11b;
		}
		return NMT_MESSAGE_UKN_B;
	} else {
		/* MTX to MS/BS */
		switch (digits[3]) {
		case 0:
			return NMT_MESSAGE_6;
		case 1:
			return NMT_MESSAGE_8;
		case 2:
			break;
		case 3:
			if (digits[6] == 15)
				return NMT_MESSAGE_21b;
			return NMT_MESSAGE_4;
		case 4:
			switch((digits[13] << 8) + (digits[14] << 4) + digits[15]) {
			case 0x3f3:
			case 0x3f4:
			case 0x3f5:
			case 0x3f6:
			case 0x000:
				return NMT_MESSAGE_2e;
			default:
				return NMT_MESSAGE_1b;
			}
		case 5:
			if (digits[6] == 15)
				return NMT_MESSAGE_21c;
			switch((digits[13] << 8) + (digits[14] << 4) + digits[15]) {
			case 0x3f3:
			case 0x3f4:
			case 0x3f5:
			case 0x3f6:
			case 0x000:
				return NMT_MESSAGE_3b;
			default:
				return NMT_MESSAGE_3a;
			}
		case 6:
			if (digits[13] == 0)
				return NMT_MESSAGE_5b;
			return NMT_MESSAGE_5a;
		case 7:
			switch((digits[13] << 8) + (digits[14] << 4) + digits[15]) {
			case 0x3f3:
			case 0x3f4:
			case 0x3f5:
			case 0x3f6:
			case 0x000:
				return NMT_MESSAGE_4b;
			default:
				return NMT_MESSAGE_3d;
			}
		case 8:
			return NMT_MESSAGE_7;
		case 9:
			return NMT_MESSAGE_3c;
		case 10:
			return NMT_MESSAGE_30;
		case 11:
			return NMT_MESSAGE_1a_a;
		case 12:
			/* no subscriber */
			if (digits[6] == 0)
				return NMT_MESSAGE_1a;
			/* battery saving */
			if (digits[6] == 14)
				return NMT_MESSAGE_1a;
			/* info to BS (should not happen here) */
			if (digits[6] == 15)
				return NMT_MESSAGE_1a;
			switch((digits[13] << 8) + (digits[14] << 4) + digits[15]) {
			case 0x3f3:
			case 0x3f4:
			case 0x3f5:
			case 0x3f6:
			case 0x000:
				return NMT_MESSAGE_2a;
			case 0x3f0:
				return NMT_MESSAGE_2f;
			case 0x3f1:
				return NMT_MESSAGE_2c;
			case 0x3f2:
				return NMT_MESSAGE_2d;
			default:
				return NMT_MESSAGE_2b;
			}
		case 13:
			return NMT_MESSAGE_1a_b;
		case 14:
			if (digits[13] != 15)
				break;
			return NMT_MESSAGE_22;
		case 15:
			if (digits[13] != 15)
				break;
			switch (digits[10]) {
			case 3:
				return NMT_MESSAGE_20_1;
			case 6:
			case 13:
				return NMT_MESSAGE_20_3;
			case 7:
			case 14:
				return NMT_MESSAGE_20_4;
			case 15:
				return NMT_MESSAGE_20_5;
			default:
				return NMT_MESSAGE_20_2;
			}
		}
		return NMT_MESSAGE_UKN_MTX;
	}
}

int init_frame(void)
{
	int i, j, k;
	char digit;

	/* check if all digits actually exist */
	for (i = 0; nmt_frame[i].digits; i++) {
		/* check mesage type */
		if ((int)nmt_frame[i].message_type != i) {
			PDEBUG(DFRAME, DEBUG_ERROR, "Message type at message index #%d does not have a value of %d, but has %d, please fix!\n", i, i + 1, nmt_frame[i].message_type);
			return -1;
		}
		/* check IEs */
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
static void disassemble_frame(int nmt_system, frame_t *frame, const uint8_t *digits, enum nmt_direction direction, int callack)
{
	enum nmt_mt mt;
	int i, j, ndigits;
	char digit;
	uint64_t value;

	memset(frame, 0, sizeof(*frame));

	/* message type of frame */
	mt = decode_frame_mt(digits, direction, callack);
	frame->mt = mt;

	/* update direction */
	direction = nmt_frame[mt].direction;

	PDEBUG(DFRAME, DEBUG_DEBUG, "Decoding %s %s %s\n", nmt_dir_name(direction), nmt_frame[mt].nr, nmt_frame[mt].description);

	for (i = 0; i < 16; i++) {
		digit = nmt_frame[mt].digits[i];
		if (digit == '-')
			continue;
		value = digits[i];
		ndigits = 1;
		for (j = i + 1; j < 16; j++) {
			if (nmt_frame[mt].digits[j] != digit)
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
		case 'M':
			frame->seq_number = value;
			break;
		case 'W':
			frame->checksum = value;
			break;
		default:
			PDEBUG(DFRAME, DEBUG_ERROR, "Digit '%c' does not exist, please fix!\n", digit);
			abort();
		}
		if (debuglevel <= DEBUG_DEBUG) {
			for (j = 0; nmt_parameter[j].digit; j++) {
				if (nmt_parameter[j].system != 0 && nmt_parameter[j].system != nmt_system)
					continue;
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
		PDEBUG(DFRAME, DEBUG_DEBUG, "%s\n", nmt_frame[mt].digits);
		PDEBUG(DFRAME, DEBUG_DEBUG, "%s\n", debug_digits);
	}
}

/* encode 16 digits frame */
static void assemble_frame(int nmt_system, frame_t *frame, uint8_t *digits, int debug)
{
	enum nmt_mt mt;
	int i, j;
	char digit;
	uint64_t value;
	enum nmt_direction direction;

	mt = frame->mt;

	if ((int)mt >= num_frames) {
		PDEBUG(DFRAME, DEBUG_ERROR, "Frame mt %d out of range (0..%d), please fix!\n", mt, num_frames - 1);
		abort();
	}

	/* set prefix of frame */
	frame->prefix = nmt_frame[mt].prefix;

	/* retrieve direction */
	direction = nmt_frame[mt].direction;

	if (debug)
		PDEBUG(DFRAME, DEBUG_DEBUG, "Coding %s %s %s\n", nmt_dir_name(direction), nmt_frame[mt].nr, nmt_frame[mt].description);

	for (i = 15; i >= 0; i--) {
		digit = nmt_frame[mt].digits[i];
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
		case 'M':
			value = frame->seq_number;
			break;
		case 'W':
			value = frame->checksum;
			break;
		default:
			PDEBUG(DFRAME, DEBUG_ERROR, "Digit '%c' does not exist, please fix!\n", digit);
			abort();
		}

		digits[i] = (value & 0xf);
		value >>= 4;
		for (j = i - 1; j >= 0; j--) {
			if (nmt_frame[mt].digits[j] != digit)
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
			digit = nmt_frame[mt].digits[i];
			if (digit == '-')
				continue;
			value = digits[i];
			ndigits = 1;
			for (j = i + 1; j < 16; j++) {
				if (nmt_frame[mt].digits[j] != digit)
					break;
				value = (value << 4) | digits[j];
				ndigits++;
				i++;
			}
			for (j = 0; nmt_parameter[j].digit; j++) {
				if (nmt_parameter[j].system != 0 && nmt_parameter[j].system != nmt_system)
					continue;
				if (nmt_parameter[j].digit == digit) {
					PDEBUG(DFRAME, DEBUG_DEBUG, " %c: %s\n", digit, nmt_parameter[j].decoder(value, ndigits, direction));
				}
			}
		}

		for (i = 0; i < 16; i++)
			debug_digits[i] = "0123456789abcdef"[digits[i]];
		debug_digits[i] = '\0';
		PDEBUG(DFRAME, DEBUG_DEBUG, "%s\n", nmt_frame[mt].digits);
		PDEBUG(DFRAME, DEBUG_DEBUG, "%s\n", debug_digits);
	}
}

/* encode frame to bits
 * debug can be turned on or off
 */
const char *encode_frame(int nmt_system, frame_t *frame, int debug)
{
	uint8_t digits[16], message[9], code[18];
	static char bits[166];
	int i;

	assemble_frame(nmt_system, frame, digits, debug);

	/* hagelbarger code */
	message[8] = 0x00;
	for (i = 0; i < 8; i++)
		message[i] = (digits[i * 2] << 4) | digits[i * 2 + 1];
	hagelbarger_encode(message, code, 70);
	memcpy(bits, "10101010101010111100010010", 26);
	for (i = 0; i < 140; i++)
		bits[i + 26] = ((code[i / 8] >> (7 - (i & 7))) & 1) + '0';

	return bits;
}

/* decode bits to frame */
int decode_frame(int nmt_system, frame_t *frame, const char *bits, enum nmt_direction direction, int callack)
{
	uint8_t digits[16], message[8], code[19];
	int i;

	/* hagelbarger code */
	memset(code, 0x00, sizeof(code));
	for (i = 0; i < 140; i++)
		code[i / 8] |= (bits[i] & 1) << (7 - (i & 7));
	hagelbarger_decode(code, message, 64);
	for (i = 0; i < 8; i++) {
		digits[i * 2] = message[i] >> 4;
		digits[i * 2 + 1] = message[i] & 0x0f;
	}

	disassemble_frame(nmt_system, frame, digits, direction, callack);

	return 0;
}

