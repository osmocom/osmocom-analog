/* Loss detection
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
#include "../common/debug.h"
#include "loss.h"

/* initialize detector
 *
 * interval: number of detector calls for one interval of one second
 * threshold: intervals may differ by this factor, to be declared as similar
 *            0 to disable, e.g. 1.3 for 30 percent change
 */
void audio_init_loss(loss_t *loss, int interval, double threshold, int seconds)
{
	memset(loss, 0, sizeof(*loss));

	loss->interval = interval;
	loss->threshold = threshold;
	loss->interval_num = seconds;
}

/* call this when tones/telegrams are detected */
void audio_reset_loss(loss_t *loss)
{
	if (loss->interval_count > 0) {
		PDEBUG(DDSP, DEBUG_DEBUG, "Signal is recovered (loss is gone).\n");
		loss->interval_count = 0;
	}
	loss->level = 0;
	loss->level_count = 0;
}

#define LOSS_MAX_DIFF 	1.1	/* 10 % difference */

/* call this for every interval */
int audio_detect_loss(loss_t *loss, double level)
{
	double diff;
			
	/* disabled */
	if (loss->threshold == 0.0)
		return 0;

	/* calculate a total level to detect loss */
	loss->level += level;

	if (++loss->level_count < loss->interval) 
		return 0;

	/* normalize level */
	loss->level = loss->level / loss->level_count;

	PDEBUG(DDSP, DEBUG_DEBUG, "Noise level = %.0f%%\n", loss->level * 100);

	diff = loss->level / loss->level_last;
	if (diff < 1.0)
		diff = 1.0 / diff;
	loss->level_last = loss->level;
	loss->level = 0;
	loss->level_count = 0;
	if (diff < LOSS_MAX_DIFF && loss->level_last > loss->threshold) {
		loss->interval_count++;
		PDEBUG(DDSP, DEBUG_DEBUG, "Detected signal loss %d for intervals level change %.0f%% (below %.0f%%).\n", loss->interval_count, diff * 100 - 100, LOSS_MAX_DIFF * 100 - 100);
	} else if (loss->interval_count > 0) {
		audio_reset_loss(loss);
	}

	if (loss->interval_count == loss->interval_num)
		return 1;
	return 0;
}

