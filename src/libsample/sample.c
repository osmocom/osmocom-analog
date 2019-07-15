/* Sample definition
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

#include <stdint.h>
#include "sample.h"

/*
 * A regular voice conversation takes place at this factor below the full range
 * of 16 bits signed value:
 */
static double int_16_speech_level = SPEECH_LEVEL * 0.7079; /* 16 dBm below dBm0, which is about 3dBm below full 16 bit range */

/* A sample_t is a value that has virtually infinite precision but will also
 * support high numbers. 'double' or 'float' types are sufficient.
 *
 * When using sample_t inside signal processing of each base station, the
 * level of +- 1 is relative to the normal speech evenlope.
 *
 * When converting sample_t to int16_t, the level of +- 1 is reduced by factor.
 * This way the speech may be louder before clipping happens.
 *
 * When using sample_t to modulate (SDR or sound card), the level is changed,
 * so it represents the frequency deviation in Hz. The deviation of speech
 * envelope is network dependent.
 */

void samples_to_int16(int16_t *spl, sample_t *samples, int length)
{
	int32_t value;

	while (length--) {
		value = *samples++ * int_16_speech_level * 32768.0;
		if (value > 32767.0)
			*spl++ = 32767;
		else if (value < -32767.0)
			*spl++ = -32767;
		else
			*spl++ = (uint16_t)value;
	}
}

void int16_to_samples(sample_t *samples, int16_t *spl, int length)
{
	while (length--) {
		*samples++ = (double)(*spl++) / 32767.0 / int_16_speech_level;
	}
}

