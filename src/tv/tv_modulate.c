/* television modulator
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

#include <math.h>
#include <stdint.h>
#include "../common/sample.h"
#include "tv_modulate.h"

#define WHITE_MODULATION	0.1

void tv_modulate(float *buff, int count, sample_t *bas, double amplitude)
{
	int i, ss = 0;

	for (i = 0; i < count; i++) {
		buff[ss++] = ((1.0 - bas[i]) * (1.0 - WHITE_MODULATION) + WHITE_MODULATION) * amplitude;
		buff[ss++] = 0;
	}
}
