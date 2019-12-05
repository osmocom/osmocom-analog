/* Sendevolumenregler to be used by B-Netz
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
 *
 */

/* This function keeps the speech level for the transmitter constant.
 *
 * B-Netz specs say that levels are amplified to speech level. The maximum gain
 * is 16 dB. The level may overshoot by 2.6 dB and must lowered to normal level
 * within 20 ms. The level should raise 4.3 dB per second until target level is
 * reached.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "../libsample/sample.h"
#include "sendevolumenregler.h"

#define db2level(db)			pow(10, (double)db / 20.0)

/*
 * Init function
 *
 * abwaerts_dbs = how many dB per second to lower the amplification, if input signal is above db0
 * aufwaerts_dbs = how many dB per second to raise the amplification, if input signal is below db0
 * maximum_db = limit of the output level above db0
 * minimum_db = below this input level, the amplification is not raised, so it stays constant.
 * db0_level = target level to be treated as 0 dB
 *
 * Hopefully this is correct
 *
 */
void init_sendevolumenregler(sendevolumenregler_t *state, double samplerate, double abwaerts_dbs, double aufwaerts_dbs, double maximum_db, double minimum_db, double db0_level)
{
	memset(state, 0, sizeof(*state));

	state->peak = 1.0;
	state->envelope = 1.0;
	state->db0_level = db0_level;
	state->step_down = pow(db2level(abwaerts_dbs), 1.0 / samplerate);
	state->step_up = pow(db2level(aufwaerts_dbs), 1.0 / samplerate);
	state->maximum_level = db2level(maximum_db);
	state->minimum_level = db2level(minimum_db);
}

/*
 * how to works:
 *
 * if value is above 'peak', raise 'peak' to value instantaniously
 * if value is below 'peak', lower 'peak' by 'step_up' (increase level)
 * if 'peak' is above 'envelope', raise 'envelope' by 'step_down' (decrease level)
 * if 'peak' is below 'envelope', lower 'envelope' to 'peak' (increase level)
 * 'envelope' will not fall below 'minimum_level' (maximum amplification)
 * if 'peak' is 'maximum_level' above 'envelope', raise 'envelope' to 'maximum_level' below 'peak'
 */
void sendevolumenregler(sendevolumenregler_t *state, sample_t *samples, int num)
{
	double value, peak, envelope, step_up, step_down, maximum_level, minimum_level, db0_level;
	int i;

	db0_level = state->db0_level;
	step_up = state->step_up;
	step_down = state->step_down;
	maximum_level = state->maximum_level;
	minimum_level = state->minimum_level;
	peak = state->peak;
	envelope = state->envelope;

	for (i = 0; i < num; i++) {
		/* normalize sample value to db0_level level */
		value = *samples / db0_level;

		/* 'peak' is the level that raises directly with the value
		 * level, but falls as specified by step_up. */
		if (fabs(value) > peak)
			peak = fabs(value);
		else
			peak /= step_up;
		/* 'evelope' is the level that raises with the specified step_down
		 * to 'peak', but falls with 'peak'. */
		if (peak > envelope)
			envelope *= step_down;
		else
			envelope = peak;
		/* no envelope below minimum level */
		if (envelope < minimum_level)
			envelope = minimum_level;
		/* raise envelope, if 'peak' exceeds maximum level */ 
		if (peak / envelope > maximum_level)
			envelope = peak / maximum_level;

		*samples++ = value / envelope * db0_level;
	}

	state->envelope = envelope;
	state->peak = peak;
}

