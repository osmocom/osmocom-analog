/* Audio level debugging
 *
 * (C) 2025 by Andreas Eversberg <jolly@eversberg.eu>
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
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include "../libsample/sample.h"
#include "debug.h"

#define level2db(level)		(20 * log10(level))

#define INTERVAL		0.1

static double envelope = 0;
static double power = 0;
static int counter = 0;
static double duration = 0;

void debug_audio_level(double samplerate, sample_t *samples, int num)
{
        int i;

	for (i = 0; i < num; i++) {
		if (fabs(samples[i]) > envelope)
                        envelope = fabs(samples[i]);
		power += samples[i] * samples[i];
		counter++;
		duration += 1.0 / samplerate;
		if (duration >= INTERVAL) {
			printf("ENVELOPE = %.3f FS  POWER = %.1f dBFS (RMS)\n", envelope, level2db(sqrt(power / counter)));
			counter = 0;
			duration -= INTERVAL;
			envelope = 0;
			power = 0;
		}
	}
}

void debug_audio_level_int16(double samplerate, int16_t *spl, int num)
{
	double samples[num];

	int16_to_samples_fullscale(samples, spl, num);
	debug_audio_level(samplerate, samples, num);
}

