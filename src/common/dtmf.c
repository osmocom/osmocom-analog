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
#include "sample.h"
#include "dtmf.h"

#define PI		M_PI

static double tx_peak_dtmf_low = 0.2818 / SPEECH_LEVEL;	/* -11 dBm, relative to speech level */ 
static double tx_peak_dtmf_high	= 0.3548 / SPEECH_LEVEL;/* -9 dBm, relative to speech level */ 
#define DTMF_DURATION	0.100	/* duration in seconds */

static sample_t dsp_sine_dtmf_low[65536];
static sample_t dsp_sine_dtmf_high[65536];

void dtmf_init(dtmf_t *dtmf, int samplerate)
{
	int i;

	memset(dtmf, 0, sizeof(*dtmf));
	dtmf->samplerate = samplerate;
	dtmf->max = (int)((double)samplerate * DTMF_DURATION + 0.5);

	// FIXME: do this globally and not per instance */
	for (i = 0; i < 65536; i++) {
		dsp_sine_dtmf_low[i] = sin((double)i / 65536.0 * 2.0 * PI) * tx_peak_dtmf_low;
		dsp_sine_dtmf_high[i] = sin((double)i / 65536.0 * 2.0 * PI) * tx_peak_dtmf_high;
	}
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
	dtmf->phaseshift65536[0] = 65536.0 / ((double)dtmf->samplerate / f1);
	dtmf->phaseshift65536[1] = 65536.0 / ((double)dtmf->samplerate / f2);
}

/* Generate audio stream from DTMF tone. Keep phase for next call of function. */
void dtmf_tone(dtmf_t *dtmf, sample_t *samples, int length)
{
        double *phaseshift, *phase;
	int i, pos, max;

	/* use silence, if no tone */
	if (!dtmf->tone) {
		memset(samples, 0, length * sizeof(*samples));
		return;
	}

	phaseshift = dtmf->phaseshift65536;
	phase = dtmf->phase65536;
	pos = dtmf->pos;
	max = dtmf->max;

	for (i = 0; i < length; i++) {
		*samples++ = dsp_sine_dtmf_low[(uint16_t)phase[0]]
			   + dsp_sine_dtmf_high[(uint16_t)phase[1]];
		phase[0] += phaseshift[0];
		if (phase[0] >= 65536)
			phase[0] -= 65536;
		phase[1] += phaseshift[1];
		if (phase[1] >= 65536)
			phase[1] -= 65536;

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

