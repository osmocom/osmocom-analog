/* Squelch functions
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

#include <string.h>
#include <math.h>
#include <stdint.h>
#include "../libdebug/debug.h"
#include "squelch.h"

#define CHAN squelch->kanal

/* How does it work:
 *
 * After init, squelch() is called with the RF level and duration of each chunk.
 * Normally quelch() returns SQUELCH_OPEN. If the RF level is below the
 * threshold level for multe_time, it returns SQUELCH_MUTE. If the RF level is
 * below the threshold level for loss_time, it returns SQUELCH_LOSS, which
 * measns that the carrier was loss.
 *
 * This is done by a counter. Whenever the RF level is below threshold, the mute
 * counter is incremented, whenever the RF level is above threshodl, the mute
 * counter is decremented. When the mute counter reaches mute_time, the mute
 * state is set and the 'mute' condition is returned. When the mute counter
 * rechers 0, the mute state is unset and the 'open' condition is returned.
 *
 * If the mute state is set, the loss counter is incremented. If the mute state
 * is not set, the loss counter is reset. When the loss counter reaches
 * loss_time, the 'loss' condition is returned.
 */

/* NOTE: SQUELCH must be calibrated !AFTER! DC bias, to get the actual noise floor */
#define SQUELCH_INIT_TIME	0.1	/* wait some time before performing squelch */
#define SQUELCH_AUTO_TIME	0.5	/* duration of squelch quelch calibration */
#define SQUELCH_AUTO_OFFSET	10.0	/* auto calibration: offset above noise floor */

void squelch_init(squelch_t *squelch, const char *kanal, double threshold_db, double mute_time, double loss_time)
{
	memset(squelch, 0, sizeof(*squelch));
	squelch->kanal = kanal;
	squelch->threshold_db = threshold_db;
	/* wait for init condition */
	squelch->init_count = 0.0;
	/* measure noise floor for auto threshold mode */
	if (threshold_db == 0.0) {
		/* automatic threshold */
		PDEBUG_CHAN(DDSP, DEBUG_INFO, "RF signal squelch: Use automatic threshold\n");
		squelch->auto_state = 1;
	} else if (!isinf(threshold_db)) {
		/* preset threshold */
		PDEBUG_CHAN(DDSP, DEBUG_INFO, "RF signal squelch: Use preset threshold of %.1f dB\n", threshold_db);
	}
	/* squelch is mute on init */
	squelch->mute_time = mute_time;
	squelch->mute_count = mute_time;
	squelch->mute_state = 1;
	/* loss condition met on init */
	squelch->loss_time = loss_time;
	squelch->loss_state = 1;
}

enum squelch_result squelch(squelch_t *squelch, double rf_level_db, double duration)
{
	/* squelch disabled */
	if (isinf(squelch->threshold_db))
		return SQUELCH_OPEN;

	/* count until start quelch processing */
	squelch->init_count += duration;
	if (squelch->init_count < SQUELCH_INIT_TIME)
		return SQUELCH_MUTE;

	/* measure noise floor and calibrate threashold_db */
	if (squelch->auto_state) {
		squelch->auto_count += duration;
		squelch->auto_level_sum += rf_level_db;
		squelch->auto_level_count++;
		if (squelch->auto_count >= SQUELCH_AUTO_TIME) {
			double noise_db, threshold_db;
			noise_db = squelch->auto_level_sum / (double) squelch->auto_level_count;
			threshold_db = noise_db + SQUELCH_AUTO_OFFSET;
			/* must be 0.1 dB smaller, so we prevent repeated debugging message with similar value */
			if (threshold_db < squelch->threshold_db - 0.1) {
				squelch->threshold_db = threshold_db;
				PDEBUG_CHAN(DDSP, DEBUG_INFO, "RF signal measurement: %.1f dB noise floor, using squelch threshold of %.1f dB\n", noise_db, threshold_db);
			}
			squelch->auto_count = 0.0;
			squelch->auto_level_count = 0;
			squelch->auto_level_sum = 0.0;
		}
	}

	/* enough RF level, so we unmute when mute_count reached 0 */
	if (rf_level_db >= squelch->threshold_db) {
		squelch->mute_count -= duration;
		if (squelch->mute_count <= 0.0) {
			if (squelch->mute_state) {
				PDEBUG_CHAN(DDSP, DEBUG_INFO, "RF signal strong: Unmuting audio (RF %.1f >= %.1f dB)\n", rf_level_db, squelch->threshold_db);
				squelch->mute_state = 0;
			}
			squelch->mute_count = 0.0;
		}
	} else {
		/* RF level too low, so we mute when mute_count reached mute_time */
		squelch->mute_count += duration;
		if (squelch->mute_count >= squelch->mute_time) {
			if (!squelch->mute_state) {
				PDEBUG_CHAN(DDSP, DEBUG_INFO, "RF signal weak: Muting audio (RF %.1f < %.1f dB)\n", rf_level_db, squelch->threshold_db);
				squelch->mute_state = 1;
			}
			squelch->mute_count = squelch->mute_time;
		}
	}

	if (squelch->mute_state) {
		/* at 'mute' condition, count and check for loss */
		squelch->loss_count += duration;
		if (squelch->loss_count >= squelch->loss_time) {
			if (!squelch->loss_state) {
				PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "RF signal loss detected after %.1f seconds\n", squelch->loss_time);
				squelch->loss_state = 1;
				return SQUELCH_LOSS;
			}
		}
		return SQUELCH_MUTE;
	} else {
		/* at unmute condition, reset loss counter */
		squelch->loss_state = 0;
		squelch->loss_count = 0.0;
		return SQUELCH_OPEN;
	}
}

