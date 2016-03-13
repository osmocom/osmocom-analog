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
#include "dtmf.h"

#define PI		M_PI

#define TX_PEAK_DTMF	7000   /* single dtmf tone peak (note this is half to total peak) */ 
#define DTMF_DURATION	0.100   /* duration in seconds */

int dsp_sine_dtmf[256];

void dtmf_init(dtmf_t *dtmf, int samplerate)
{
	int i;

	memset(dtmf, 0, sizeof(*dtmf));
	dtmf->samplerate = samplerate;
	dtmf->max = (int)((double)samplerate * DTMF_DURATION + 0.5);

	// FIXME: do this globally and not per instance */
	for (i = 0; i < 256; i++)
		dsp_sine_dtmf[i] = (int)(sin((double)i / 256.0 * 2.0 * PI) * TX_PEAK_DTMF);
}

/* set dtmf tone */
void dtmf_set_tone(dtmf_t *dtmf, char tone)
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
		return;
	}
	dtmf->tone = tone;
	dtmf->pos = 0;
	dtmf->phaseshift256[0] = 256.0 / ((double)dtmf->samplerate / f1);
	dtmf->phaseshift256[1] = 256.0 / ((double)dtmf->samplerate / f2);
}

/* Generate audio stream from DTMF tone. Keep phase for next call of function. */
void dtmf_tone(dtmf_t *dtmf, int16_t *samples, int length)
{
        double *phaseshift, *phase;
	int i, pos, max;

	/* use silence, if no tone */
	if (!dtmf->tone) {
		memset(samples, 0, length * sizeof(*samples));
		return;
	}

	phaseshift = dtmf->phaseshift256;
	phase = dtmf->phase256;
	pos = dtmf->pos;
	max = dtmf->max;

	for (i = 0; i < length; i++) {
		*samples++ = dsp_sine_dtmf[((uint8_t)phase[0]) & 0xff]
			   + dsp_sine_dtmf[((uint8_t)phase[1]) & 0xff];
		phase[0] += phaseshift[0];
		if (phase[0] >= 256)
			phase[0] -= 256;
		phase[1] += phaseshift[1];
		if (phase[1] >= 256)
			phase[1] -= 256;

		/* tone ends */
		if (++pos == max) {
			dtmf->tone = 0;
			break;
		}
	}
	length -= i;

	dtmf->pos = pos;

	/* if tone ends, fill rest with silence */
	if (length)
		memset(samples, 0, length * sizeof(*samples));
}

