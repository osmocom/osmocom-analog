/* ISO 7811 encoder/decoder
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

#include <stdint.h>
#include <string.h>
#include "iso7811.h"

/* Given is a string with or without start and end sentinel. Returned are
 * bytes containing 5 bits each. These bits shall be sent LSB first.
 * A lead-in and a start sentinel is added prior encoded string data.
 * An end sentinel, a LRC and a lead-out is added after string data.
 */
int encode_track(uint8_t *data, const char *string, int lead_in, int lead_out)
{
	int i;
	uint8_t bits, lrc;

	i = 0;
	lrc = 0;

	/* lead-in */
	for (lead_in += i; i < lead_in; i++)
		data[i] = 0;

	/* start sentinel */
	if (*string == ';')
		string++;
	bits = 0x0b;
	data[i++] = bits;
	lrc ^= bits;

	/* string */
	while (*string && *string != '?') {
		if (*string >= 0x30 && *string < 0x40)
			bits = *string - 0x30;
		else
			bits = 0;
		data[i] = bits & 0x0f;
		lrc ^= bits;
		bits ^= bits >> 2;
		bits ^= bits >> 1;
		bits &= 1;
		data[i] |= (bits ^ 1) << 4;
		string++;
		i++;
	}

	/* end sentinel */
	bits = 0x1f;
	data[i++] = bits;
	lrc ^= bits;

	/* LRC */
	data[i] = lrc & 0x0f;
	lrc ^= lrc >> 2;
	lrc ^= lrc >> 1;
	lrc &= 1;
	data[i] |= (lrc ^ 1) << 4;
	i++;

	/* lead-out */
	for (lead_out += i; i < lead_out; i++)
		data[i] = 0;

	return i;
}

/* n0nnnnnn=sssss0000 (in case of 7 digits) */
int cnetz_card(char *string, const char *number, const char *sicherung)
{
	int len;

	/* number */
	len = strlen(number);
	*string++ = *number++;
	if (len == 7)
		*string++ = '0';
	else if (len == 8)
		*string++ = *number++;
	else
		return 0;
	*string++ = *number++;
	*string++ = *number++;
	*string++ = *number++;
	*string++ = *number++;
	*string++ = *number++;
	*string++ = *number++;

	/* field seperator */
	*string++ = '=';

	/* security code */
	len = strlen(sicherung);
	if (len < 5)
		*string++ = '0';
	else
		*string++ = *sicherung++;
	if (len < 4)
		*string++ = '0';
	else
		*string++ = *sicherung++;
	if (len < 3)
		*string++ = '0';
	else
		*string++ = *sicherung++;
	if (len < 2)
		*string++ = '0';
	else
		*string++ = *sicherung++;
	*string++ = *sicherung++;
	*string++ = '0';
	*string++ = '0';
	*string++ = '0';
	*string++ = '0';

	*string++ = '\0';

	return 18;
}

/* 0:500000=000000000 */
int bsa44_service(char *string)
{
	*string++ = '0';
	*string++ = ':';
	*string++ = '5';
	*string++ = '0';
	*string++ = '0';
	*string++ = '0';
	*string++ = '0';
	*string++ = '0';
	*string++ = '=';
	*string++ = '0';
	*string++ = '0';
	*string++ = '0';
	*string++ = '0';
	*string++ = '0';
	*string++ = '0';
	*string++ = '0';
	*string++ = '0';
	*string++ = '0';
	*string++ = '\0';

	return 18;
}

