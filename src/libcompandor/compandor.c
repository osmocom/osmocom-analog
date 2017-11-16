/* Compandor to use various networks like C-Netz / NMT / AMPS / TACS
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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "../common/sample.h"
#include "compandor.h"

//#define db2level(db)			pow(10, (double)db / 20.0)

/* factor is the gain (raise and fall) after given attack/recovery time */
#define COMPRESS_ATTACK_FACTOR		1.83		/* about 1.5 after 12 dB step up */
#define COMPRESS_RECOVERY_FACTOR	0.44		/* about 0.75 after 12 dB step down */
#define EXPAND_ATTACK_FACTOR		1.145		/* about 0.57 after 6 dB step up */
#define EXPAND_RECOVERY_FACTOR		0.753		/* about 1.51 after 6 dB step down */

/* Minimum level value to keep state */
#define ENVELOPE_MIN	0.001

/* Maximum level, to prevent sqrt_tab to overflow */
#define ENVELOPE_MAX	9.990

static double sqrt_tab[10000];

/*
 * Init compandor according to ITU-T G.162 specification
 *
 * Hopefully this is correct
 *
 */
void init_compandor(compandor_t *state, double samplerate, double attack_ms, double recovery_ms, double unaffected_level)
{
	int i;

	memset(state, 0, sizeof(*state));

	state->c.peak = 1.0;
	state->c.envelope = 1.0;
	state->e.peak = 1.0;
	state->e.envelope = 1.0;
	state->c.step_up = pow(COMPRESS_ATTACK_FACTOR, 1000.0 / attack_ms / samplerate);
	state->c.step_down = pow(COMPRESS_RECOVERY_FACTOR, 1000.0 / recovery_ms / samplerate);
	state->e.step_up = pow(EXPAND_ATTACK_FACTOR, 1000.0 / attack_ms / samplerate);
	state->e.step_down = pow(EXPAND_RECOVERY_FACTOR, 1000.0 / recovery_ms / samplerate);
	state->c.unaffected = unaffected_level;
	state->e.unaffected = unaffected_level;

	// FIXME: make global, not at instance
	for (i = 0; i < 10000; i++)
		sqrt_tab[i] = sqrt(i * 0.001);
}

void compress_audio(compandor_t *state, sample_t *samples, int num)
{
	double value, peak, envelope, step_up, step_down, unaffected;
	int i;

	step_up = state->c.step_up;
	step_down = state->c.step_down;
	peak = state->c.peak;
	envelope = state->c.envelope;
	unaffected = state->c.unaffected;

//	printf("envelope=%.4f\n", envelope);
	for (i = 0; i < num; i++) {
		/* normalize sample value to unaffected level */
		value = *samples / unaffected;

		/* 'peak' is the level that raises directly with the signal
		 * level, but falls with specified recovery rate. */
		if (fabs(value) > peak)
			peak = fabs(value);
		else
			peak *= step_down;
		/* 'evelope' is the level that raises with the specified attack
		 * rate to 'peak', but falls with specified recovery rate. */
		if (peak > envelope)
			envelope *= step_up;
		else
			envelope = peak;
		if (envelope < ENVELOPE_MIN)
			envelope = ENVELOPE_MIN;
		if (envelope > ENVELOPE_MAX)
			envelope = ENVELOPE_MAX;

		value = value / sqrt_tab[(int)(envelope / 0.001)];
//if (i > 47000.0 && i < 48144)
//printf("time=%.4f envelope=%.4fdb, value=%.4f\n", (double)i/48000.0, 20*log10(envelope), value);

		*samples++ = value * unaffected;
	}
//exit(0);

	state->c.envelope = envelope;
	state->c.peak = peak;
}

void expand_audio(compandor_t *state, sample_t *samples, int num)
{
	double value, peak, envelope, step_up, step_down, unaffected;
	int i;

	step_up = state->e.step_up;
	step_down = state->e.step_down;
	peak = state->e.peak;
	envelope = state->e.envelope;
	unaffected = state->e.unaffected;

	for (i = 0; i < num; i++) {
		/* normalize sample value to 0 DB level */
		value = *samples / unaffected;

		/* for comments: see compress_audio() */
		if (fabs(value) > peak)
			peak = fabs(value);
		else
			peak *= step_down;
		if (peak > envelope)
			envelope *= step_up;
		else
			envelope = peak;
		if (envelope < ENVELOPE_MIN)
			envelope = ENVELOPE_MIN;

		value = value * envelope;

		*samples++ = value * unaffected;
	}

	state->e.envelope = envelope;
	state->e.peak = peak;
}

