/* C-Netz audio spectrum inversion (Sprachverschleierung)
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

#include <stdint.h>
#include <math.h>
#include "scrambler.h"

#define PI		M_PI

/* FTZ 171 TR 60 Clause 6.2
 * Carrier frequency, that is the spectrum that is mirrored */
#define CARRIER_HZ	3300.0
#define FILTER_BELOW	300.0
#define FILTER_TURNS	2

/* FTZ 171 TR 60 Clause 6.3
 * How much must the carrier frequency be lower than a 1000 HZ tone that passes the inversion.
 * The filter must be tuned to get that loss. */
#define TEST_1000HZ_DB	55.0

/* sine wave for carrier to modulate to */
static double carrier[256];

void scrambler_init(void)
{
	int i;

	for (i = 0; i < 256; i++) {
		carrier[i] = sin((double)i / 256.0 * 2 * PI);
	}
}

void scrambler_setup(scrambler_t *scrambler, int samplerate)
{
	filter_lowpass_init(&scrambler->lp, CARRIER_HZ - FILTER_BELOW, samplerate, FILTER_TURNS);
	scrambler->carrier_phaseshift256 = 256.0 / ((double)samplerate / CARRIER_HZ);
}

/* Modulate samples to carriere that is twice the mirror frequency.
 * Then we got spectrum above carrier and mirrored spectrum below carrier.
 * Afterwards we cut off carrier frequency and frequencies above carrier.
 */
void scrambler(scrambler_t *scrambler, int16_t *samples, int length)
{
	double spl[length];
	int32_t sample;
	double phaseshift, phase;
	int i;

	phaseshift = scrambler->carrier_phaseshift256;
	phase = scrambler->carrier_phase256;

	for (i = 0; i < length; i++) {
		/* modulate samples to carrier */
		spl[i] = (double)samples[i] / 32768.0 * carrier[((uint8_t)phase) & 0xff];
		phase += phaseshift;
		if (phase >= 256.0)
			phase -= 256.0;
	}

	scrambler->carrier_phase256 = phase;

	/* cut off carrier frequency and modulation above carrier frequency */
	filter_process(&scrambler->lp, spl, length);

	for (i = 0; i < length; i++) {
		/* store result */
		sample = spl[i] * 2.0 * 32768.0;
		if (sample > 32767)
			sample = 32767;
		else if (sample < -32768)
			sample = -32768;
		*samples++ = sample;
	}
}


