/* Goertzel functions
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "../libsample/sample.h"
#include "../libdebug/debug.h"
#include "goertzel.h"

/*
 * audio level calculation
 */

/* Return average value (rectified value)
 * The input must not have any dc offset!
 * For a perfect rectangualr wave, the result would equal the peak level.
 * For a sine wave the result would be factor (2 / PI) below peak level.
 */
double audio_mean_level(sample_t *samples, int length)
{
	double level, sk;
	int n;

	/* level calculation */
	level = 0;
	for (n = 0; n < length; n++) {
		sk = samples[n];
		if (sk < 0)
			level -= (double)sk;
		if (sk > 0)
			level += (double)sk;
	}
	level = level / (double)length;

	return level;
}

/* use hamming window */
double window[256];
int window_generated = 0;

static void gen_window(void)
{
	int i;

	for (i = 0; i < 256; i++)
		window[i] = 0.54 - 0.46 * cos(2.0 * M_PI * (double)i / 256.0);
	window_generated = 1;
}

void audio_goertzel_init(goertzel_t *goertzel, double freq, int samplerate)
{
	if (!window_generated)
		gen_window();
	memset(goertzel, 0, sizeof(*goertzel));
	goertzel->coeff = 2.0 * cos(2.0 * M_PI * freq / (double)samplerate);
}

/*
 * goertzel filter
 */

/* filter frequencies and return their levels
 *
 * samples: pointer to sample buffer
 * length: length of buffer: -7.4 dB @ +-(1 / duration) Hz and -INF @ +-(1 / duration * 2) Hz
 *  -> if duration is 10 ms, we got -7.4 dB @ +-100 Hz and -INF at +-200 Hz
 * offset: for ring buffer, start here and wrap around to 0 when length has been hit
 * coeff: array of coefficients (coeff << 15)
 * result: array of result levels (peak value of the target frequency)
 * k: number of frequencies to check
 */
void audio_goertzel(goertzel_t *goertzel, sample_t *samples, int length, int offset, double *result, int k)
{
	double sk, sk1, sk2;
	double cos2pik;
	int i, n;

	/* we do goertzel */
	for (i = 0; i < k; i++) {
		sk = 0;
		sk1 = 0;
		sk2 = 0;
		cos2pik = goertzel[i].coeff;
		/* note: after 'length' cycles, offset is restored to its initial value */
		for (n = 0; n < length; n++) {
			sk = (cos2pik * sk1) - sk2 + samples[offset++] * window[n * 256 / length];
			sk2 = sk1;
			sk1 = sk;
			if (offset == length)
				offset = 0;
		}
		/* compute level of signal */
		result[i] = sqrt(
			(sk * sk) -
			(cos2pik * sk * sk2) +
			(sk2 * sk2)
				) / (double)length * 4 / 1.08;
	}
}

