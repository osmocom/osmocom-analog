/* Compander to use various networks like C-Netz / NMT / AMPS
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
#include <string.h>
#include <math.h>
#include "compander.h"

/* this is the 0 DB level that stays 0 DB after compression / espansion */
#define ZERO_DB_LEVEL	16384.0

/* what factor shall the gain raise and fall after given attack/recovery time */
#define ATTACK_FACTOR	1.5
#define RECOVERY_FACTOR	0.75

/* Minimum level value to keep state */
#define ENVELOP_MIN	0.001

static double sqrt_tab[10000];

/*
 * Generate companding tables according to NMT specification
 *
 * Hopefully this is correct
 *
 */
void init_compander(compander_t *state, int samplerate, double attack_ms, double recovery_ms)
{
	int i;

	memset(state, 0, sizeof(*state));

	state->envelop_e = 1.0;
	state->envelop_c = 1.0;
	/* ITU-T G.162: 1.5 times the steady state after attack_ms */
	state->step_up = pow(ATTACK_FACTOR, 1000.0 / attack_ms / (double)samplerate);

	/* ITU-T G.162: 0.75 times the steady state after recovery_ms */
	state->step_down = pow(RECOVERY_FACTOR, 1000.0 / recovery_ms / (double)samplerate);

	// FIXME: make global, not at instance
	for (i = 0; i < 10000; i++)
		sqrt_tab[i] = sqrt(i * 0.001);
}

void compress_audio(compander_t *state, int16_t *samples, int num)
{
	int32_t sample;
	double value, envelop, step_up, step_down;
	int i;

	step_up = state->step_up;
	step_down = state->step_down;
	envelop = state->envelop_c;

//	printf("envelop=%.4f\n", envelop);
	for (i = 0; i < num; i++) {
		/* normalize sample value to 0 DB level */
		value = (double)(*samples) / ZERO_DB_LEVEL;

		if (fabs(value) > envelop)
			envelop *= step_up;
		else
			envelop *= step_down;
		if (envelop < ENVELOP_MIN)
			envelop = ENVELOP_MIN;

		value = value / sqrt_tab[(int)(envelop / 0.001)];

		/* convert back from 0 DB level to sample value */
		sample = (int)(value * ZERO_DB_LEVEL);
		if (sample > 32767)
			sample = 32767;
		else if (sample < -32768)
			sample = -32768;
		*samples++ = sample;
	}

	state->envelop_c = envelop;
}

void expand_audio(compander_t *state, int16_t *samples, int num)
{
	int32_t sample;
	double value, envelop, step_up, step_down;
	int i;

	step_up = state->step_up;
	step_down = state->step_down;
	envelop = state->envelop_e;

	for (i = 0; i < num; i++) {
		/* normalize sample value to 0 DB level */
		value = (double)(*samples) / ZERO_DB_LEVEL;

		if (fabs(value) > envelop)
			envelop *= step_up;
		else
			envelop *= step_down;
		if (envelop < ENVELOP_MIN)
			envelop = ENVELOP_MIN;

		value = value * envelop;

		/* convert back from 0 DB level to sample value */
		sample = (int)(value * ZERO_DB_LEVEL);
		if (sample > 32767)
			sample = 32767;
		else if (sample < -32768)
			sample = -32768;
		*samples++ = sample;
	}

	state->envelop_e = envelop;
}

