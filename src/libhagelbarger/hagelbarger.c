/* Hagelbarger (6,19) code
 *
 * A burst up to 6 encoded bits may be corrupt, to correct them.
 * After corrupt bits, a minimum of 19 bits must be correct to correct
 * another burst of corrupted bits.
 *
 * There is no parity check, so it is required to check all information
 * elements of each message. With NMT System: Messages that contain signals
 * or digits are protected by repeating the digits in the information element.
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

#include "stdint.h"

/* enable to debug the process of parity check */
//#define DEBUG_HAGEL

/* To encode NMT message: (MSB first)
 * Use input with 9 bytes, the last byte must be 0x00.
 * Use output with 18 bytes, ignore the last four (lower) bits of last byte.
 * Use length of 70.
 */
void hagelbarger_encode(const uint8_t *input, uint8_t *output, int length)
{
	uint8_t reg = 0x00, data, check;
	int i;

	for (i = 0; i < length; i++) {
		/* get data from input (MSB first) */
		data = (input[i / 8] >> (7 - (i & 7))) & 1;
		/* push data into shift register (LSB first) */
		reg = (reg << 1) | data;
		/* get data bit from register */
		data = (reg >> 6) & 1;
		/* calc check bit from register */
		check = (reg + (reg >> 3) + 1) & 1;
		/* put check bit and data bit to output (MSB first) */
		output[i / 4] = (output[i / 4] << 2) | (check << 1) | data;
	}
	/* shift last output byte all the way to MSB */
	while ((i % 4))
		output[i++ / 4] <<= 2;
}

/* To decode NMT message: (MSB first)
 * Use input with 19 bytes, the unused last 12 (lower) bits must be zero.
 * Use output with 8 bytes.
 * Use length of 64.
 */
void hagelbarger_decode(const uint8_t *input, uint8_t *output, int length)
{
	uint16_t reg_data = 0x00, reg_check = 0xff, data, check, r_parity, s_parity;
	int i, o;

	length += 10;

	for (i = 0, o = 0; i < length; i++) {
		/* get check bit from input (MSB first) */
		check = (input[i / 4] >> (7 - (i & 3) * 2)) & 1;
		/* get data bit from input (MSB first) */
		data = (input[i / 4] >> (6 - (i & 3) * 2)) & 1;
		/* push check bit into shift register (LSB first) */
		reg_check = (reg_check << 1) | check;
		/* push data bit into shift register (LSB first) */
		reg_data = (reg_data << 1) | data;
		/* calculate parity */
		r_parity = (reg_data + (reg_data >> 3) + (reg_check >> 6) + 1) & 1;
		s_parity = ((reg_data >> 3) + (reg_data >> 6) + (reg_check >> 9) + 1) & 1;
#ifdef DEBUG_HAGEL
		printf("#%d: r=%d s=%d\n", i - 10, r_parity, s_parity);
#endif
		/* flip message bit, if both parity checks fail */
		/* use 4th bit that will be shifted to 5th bit next loop */
		if (r_parity && s_parity)
			reg_data ^= 0x0008;
		/* put message bit to output (MSB first) */
		if (i >= 10) {
			output[o / 8] = (output[o / 8] << 1) | ((reg_data >> 4) & 1);
			o++;
		}
	}
	/* shift last output byte all the way to MSB */
	while ((o % 8))
		output[o++ / 8] <<= 1;
}

