/* DTMF coder
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
#include "../libsample/sample.h"
#include "dtmf_encode.h"

#define PI		M_PI

#define PEAK_DTMF_LOW	0.2818	/* -11 dBm, relative to 0 dBm level */ 
#define PEAK_DTMF_HIGH	0.3548	/* -9 dBm, relative to 0 dBm level */ 

void dtmf_encode_init(dtmf_enc_t *dtmf, int samplerate, double dBm_level)
{
	int i;

	memset(dtmf, 0, sizeof(*dtmf));
	dtmf->samplerate = samplerate;

	for (i = 0; i < 65536; i++) {
		dtmf->sine_low[i] = sin((double)i / 65536.0 * 2.0 * PI) * PEAK_DTMF_LOW * dBm_level;
		dtmf->sine_high[i] = sin((double)i / 65536.0 * 2.0 * PI) * PEAK_DTMF_HIGH * dBm_level;
	}
}

/* set dtmf tone */
int dtmf_encode_set_tone(dtmf_enc_t *dtmf, char tone, double on_duration, double off_duration)
{
	double f1, f2;

	switch(tone) {
		case '1': f1 = 697.0; f2 = 1209.0; break;
		case '2': f1 = 697.0; f2 = 1336.0; break;
		case '3': f1 = 697.0; f2 = 1477.0; break;
	case'a':case 'A': f1 = 697.0; f2 = 1633.0; break;
		case '4': f1 = 770.0; f2 = 1209.0; break;
		case '5': f1 = 770.0; f2 = 1336.0; break;
		case '6': f1 = 770.0; f2 = 1477.0; break;
	case'b':case 'B': f1 = 770.0; f2 = 1633.0; break;
		case '7': f1 = 852.0; f2 = 1209.0; break;
		case '8': f1 = 852.0; f2 = 1336.0; break;
		case '9': f1 = 852.0; f2 = 1477.0; break;
	case'c':case 'C': f1 = 852.0; f2 = 1633.0; break;
		case '*': f1 = 941.0; f2 = 1209.0; break;
		case '0': f1 = 941.0; f2 = 1336.0; break;
		case '#': f1 = 941.0; f2 = 1477.0; break;
	case'd':case 'D': f1 = 941.0; f2 = 1633.0; break;
	default:
		dtmf->tone = 0;
		return -1;
	}
	dtmf->tone = tone;
	dtmf->pos = 0;
	dtmf->on = (int)((double)dtmf->samplerate * on_duration);
	dtmf->off = dtmf->on + (int)((double)dtmf->samplerate * off_duration);
	dtmf->phaseshift65536[0] = 65536.0 / ((double)dtmf->samplerate / f1);
	dtmf->phaseshift65536[1] = 65536.0 / ((double)dtmf->samplerate / f2);
	dtmf->phase65536[0] = 0.0;
	dtmf->phase65536[1] = 0.0;

	return 0;
}

/* Generate audio stream from DTMF tone.
 * Keep phase for next call of function.
 * Stop, if tone has finished and return only the samples that were used.
 */
int dtmf_encode(dtmf_enc_t *dtmf, sample_t *samples, int length)
{
        double *phaseshift, *phase;
	sample_t *sine_low, *sine_high;
	int count = 0;
	int i;

	/* if no tone */
	if (!dtmf->tone)
		return 0;

	sine_low = dtmf->sine_low;
	sine_high = dtmf->sine_high;
	phaseshift = dtmf->phaseshift65536;
	phase = dtmf->phase65536;

	for (i = 0; i < length; i++) {
		*samples++ = sine_low[(uint16_t)phase[0]]
			   + sine_high[(uint16_t)phase[1]];
		phase[0] += phaseshift[0];
		if (phase[0] >= 65536.0)
			phase[0] -= 65536.0;
		phase[1] += phaseshift[1];
		if (phase[1] >= 65536.0)
			phase[1] -= 65536.0;

		dtmf->pos++;
		/* tone ends */
		if (dtmf->pos == dtmf->on) {
			phaseshift[0] = 0.0;
			phaseshift[1] = 0.0;
			phase[0] = 0.0;
			phase[1] = 0.0;
		}
		/* pause ends */
		if (dtmf->pos == dtmf->off) {
			dtmf->tone = 0;
			break;
		}
	}
	count += i;

	return count;
}

