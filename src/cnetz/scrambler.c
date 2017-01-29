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
#include "../common/sample.h"
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
static double carrier[65536];

void scrambler_init(void)
{
	int i;

	for (i = 0; i < 65536; i++) {
		/* our amplitude must be doubled, since we have one spectrum above and one below carrier */
		carrier[i] = sin((double)i / 65536.0 * 2 * PI) * 2.0;
	}
}

void scrambler_setup(scrambler_t *scrambler, int samplerate)
{
	filter_lowpass_init(&scrambler->lp, CARRIER_HZ - FILTER_BELOW, samplerate, FILTER_TURNS);
	scrambler->carrier_phaseshift65536 = 65536.0 / ((double)samplerate / CARRIER_HZ);
}

/* Modulate samples to carriere that is twice the mirror frequency.
 * Then we got spectrum above carrier and mirrored spectrum below carrier.
 * Afterwards we cut off carrier frequency and frequencies above carrier.
 */
void scrambler(scrambler_t *scrambler, sample_t *samples, int length)
{
	double phaseshift, phase;
	int i;

	phaseshift = scrambler->carrier_phaseshift65536;
	phase = scrambler->carrier_phase65536;

	for (i = 0; i < length; i++) {
		/* modulate samples to carrier */
		samples[i] *= carrier[(uint16_t)phase];
		phase += phaseshift;
		if (phase >= 65536.0)
			phase -= 65536.0;
	}

	scrambler->carrier_phase65536 = phase;

	/* cut off carrier frequency and modulation above carrier frequency */
	filter_process(&scrambler->lp, samples, length);
}


