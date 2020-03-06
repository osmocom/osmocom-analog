#include <stdint.h>
#include "crc16.h"

/*
 *                                      16   12   5
 * this is the CCITT CRC 16 polynomial X  + X  + X  + 1.
 * This works out to be 0x1021, but the way the algorithm works
 * lets us use 0x8408 (the reverse of the bit pattern).  The high
 * bit is always assumed to be set, thus we only use 16 bits to
 * represent the 17 bit value.
 * The low bit contains the first bit of the data on the line.
 * The low byte(bit) contains the first bit of the CRC on the line.
*/

#define POLY 0x8408

uint16_t calc_crc16(uint8_t *data_p, int length)
{
	int i;
	uint16_t data;
	uint16_t crc = 0xffff;

	while (length--) {
		data = *data_p++;
		for (i = 0; i < 8; i++) {
			if ((crc & 1) ^ (data & 1))
				crc = (crc >> 1) ^ POLY;
			else
				crc >>= 1;
			data >>= 1;
		}
	}

	crc = ~crc;

	return (crc);
}

