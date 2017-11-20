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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "../libsample/sample.h"
#include "dtmf_decode.h"

//#define DEBUG

#define level2db(level)	(20 * log10(level))
#define db2level(db)	pow(10, (double)db / 20.0)

#define DTMF_LOW_1	697.0
#define DTMF_LOW_2	770.0
#define DTMF_LOW_3	852.0
#define DTMF_LOW_4	941.0
#define DTMF_HIGH_1	1209.0
#define DTMF_HIGH_2	1336.0
#define DTMF_HIGH_3	1477.0
#define DTMF_HIGH_4	1633.0

static const char dtmf_digit[] = "     123A456B789C*0#D";

#ifdef DEBUG
const char *_debug_amplitude(double level)
{
        static char text[42];

	strcpy(text, "                    :                    ");
	if (level > 1.0)
		level = 1.0;
	if (level < -1.0)
		level = -1.0;
	text[20 + (int)(level * 20)] = '*';

	return text;
}
#endif

int dtmf_decode_init(dtmf_dec_t *dtmf, void *priv, void (*recv_digit)(void *priv, char digit, dtmf_meas_t *meas), int samplerate, double max_amplitude, double min_amplitude)
{
	int rc;

	memset(dtmf, 0, sizeof(*dtmf));
	dtmf->priv = priv;
	dtmf->recv_digit = recv_digit;
	dtmf->samplerate = samplerate;
	dtmf->freq_tollerance = 3.0;
	dtmf->max_amplitude = max_amplitude;
	dtmf->min_amplitude = min_amplitude;
	dtmf->forward_twist = db2level(4.0);
	dtmf->reverse_twist = db2level(8.0);
	dtmf->time_detect = (int)(0.025 * (double)samplerate);
	dtmf->time_meas = (int)(0.015 * (double)samplerate);
	dtmf->time_pause = (int)(0.010 * (double)samplerate);

	/* init fm demodulator */
	rc = fm_demod_init(&dtmf->demod_low, (double)samplerate, (DTMF_LOW_1 + DTMF_LOW_4) / 2.0, DTMF_LOW_4 - DTMF_LOW_1);
	if (rc < 0)
		goto error;
	rc = fm_demod_init(&dtmf->demod_high, (double)samplerate, (DTMF_HIGH_1 + DTMF_HIGH_4) / 2.0, DTMF_HIGH_4 - DTMF_HIGH_1);
	if (rc < 0)
		goto error;

	/* use fourth order (2 iter) filter, since it is as fast as second order (1 iter) filter */
	iir_lowpass_init(&dtmf->freq_lp[0], 100.0, samplerate, 2);
	iir_lowpass_init(&dtmf->freq_lp[1], 100.0, samplerate, 2);

	return 0;

error:
	dtmf_decode_exit(dtmf);
	return rc;
}

void dtmf_decode_exit(dtmf_dec_t *dtmf)
{
	fm_demod_exit(&dtmf->demod_low);
	fm_demod_exit(&dtmf->demod_high);
}

void dtmf_decode_filter(dtmf_dec_t *dtmf, sample_t *samples, int length, sample_t *frequency_low, sample_t *frequency_high, sample_t *amplitude_low, sample_t *amplitude_high)
{
	sample_t I_low[length], Q_low[length];
	sample_t I_high[length], Q_high[length];
	int i;

	fm_demodulate_real(&dtmf->demod_low, frequency_low, length, samples, I_low, Q_low);
	fm_demodulate_real(&dtmf->demod_high, frequency_high, length, samples, I_high, Q_high);
	/* peak amplitude is the length of I/Q vector
	 * since we filter out the unwanted modulation product, the vector is only half of length */
	for (i = 0; i < length; i++) {
		amplitude_low[i] = sqrt(I_low[i] * I_low[i] + Q_low[i] * Q_low[i]) * 2.0;
		amplitude_high[i] = sqrt(I_high[i] * I_high[i] + Q_high[i] * Q_high[i]) * 2.0;
	}
	iir_process(&dtmf->freq_lp[0], frequency_low, length);
	iir_process(&dtmf->freq_lp[1], frequency_high, length);
}
void dtmf_decode(dtmf_dec_t *dtmf, sample_t *samples, int length)
{
	sample_t frequency_low[length], amplitude_low[length];
	sample_t frequency_high[length], amplitude_high[length];
	double tollerance, min_amplitude, max_amplitude, forward_twist, reverse_twist, f1, f2;
	int time_detect, time_meas, time_pause;
	int low = 0, high = 0;
	char detected, digit;
	int count;
	int aplitude_ok, twist_ok;
	int i;

	tollerance = dtmf->freq_tollerance;
	min_amplitude = dtmf->min_amplitude;
	max_amplitude = dtmf->max_amplitude;
	forward_twist = dtmf->forward_twist;
	reverse_twist = dtmf->reverse_twist;
	time_detect = dtmf->time_detect;
	time_meas = dtmf->time_meas;
	time_pause = dtmf->time_pause;
	detected = dtmf->detected;
	count = dtmf->count;

	/* FM/AM demod */
	dtmf_decode_filter(dtmf, samples, length, frequency_low, frequency_high, amplitude_low, amplitude_high);

	for (i = 0; i < length; i++) {
#ifdef DEBUG
		printf("%s %.5f\n", _debug_amplitude(samples[i]/2.0), samples[i]/2.0);
#endif
		/* get frequency of low frequencies, correct amplitude drop at cutoff point */
		f1 = frequency_low[i] + (DTMF_LOW_1 + DTMF_LOW_4) / 2.0;
		if (f1 >= DTMF_LOW_1 - tollerance && f1 <= DTMF_LOW_1 + tollerance) {
			/* cutoff point */
			amplitude_low[i] /= 0.7071;
			low = 1;
			f1 -= DTMF_LOW_1;
		} else
		if (f1 >= DTMF_LOW_2 - tollerance && f1 <= DTMF_LOW_2 + tollerance) {
			amplitude_low[i] /= 1.0734;
			low = 2;
			f1 -= DTMF_LOW_2;
		} else
		if (f1 >= DTMF_LOW_3 - tollerance && f1 <= DTMF_LOW_3 + tollerance) {
			amplitude_low[i] /= 1.0389;
			low = 3;
			f1 -= DTMF_LOW_3;
		} else
		if (f1 >= DTMF_LOW_4 - tollerance && f1 <= DTMF_LOW_4 + tollerance) {
			/* cutoff point */
			amplitude_low[i] /= 0.7071;
			low = 4;
			f1 -= DTMF_LOW_4;
		} else
			low = 0;
		/* get frequency of high frequencies, correct amplitude drop at cutoff point */
		f2 = frequency_high[i] + (DTMF_HIGH_1 + DTMF_HIGH_4) / 2.0;
		if (f2 >= DTMF_HIGH_1 - tollerance && f2 <= DTMF_HIGH_1 + tollerance) {
			/* cutoff point */
			amplitude_high[i] /= 0.7071;
			high = 1;
			f2 -= DTMF_HIGH_1;
		} else
		if (f2 >= DTMF_HIGH_2 - tollerance && f2 <= DTMF_HIGH_2 + tollerance) {
			amplitude_high[i] /= 1.0731;
			high = 2;
			f2 -= DTMF_HIGH_2;
		} else
		if (f2 >= DTMF_HIGH_3 - tollerance && f2 <= DTMF_HIGH_3 + tollerance) {
			amplitude_high[i] /= 1.0372;
			high = 3;
			f2 -= DTMF_HIGH_3;
		} else
		if (f2 >= DTMF_HIGH_4 - tollerance && f2 <= DTMF_HIGH_4 + tollerance) {
			/* cutoff point */
			amplitude_high[i] /= 0.7071;
			high = 4;
			f2 -= DTMF_HIGH_4;
		} else
			high = 0;
		digit = 0;
		aplitude_ok = 0;
		twist_ok = 0;
		if (low && high) {
			digit = dtmf_digit[low*4+high];
			/* check for limits */
			if (amplitude_low[i] <= max_amplitude && amplitude_low[i] >= min_amplitude && amplitude_high[i] <= max_amplitude && amplitude_high[i] >= min_amplitude) {
				aplitude_ok = 1;
#ifdef DEBUG
				printf("%.5f %.5f %.1f\n", amplitude_low[i], amplitude_high[i], level2db(amplitude_high[i] / amplitude_low[i]));
#endif
				if (amplitude_high[i] / amplitude_low[i] <= forward_twist && amplitude_low[i] / amplitude_high[i] <= reverse_twist)
					twist_ok = 1;
			}
		}

		if (!detected) {
			if (digit && aplitude_ok && twist_ok) {
				if (count == 0) {
					memset(&dtmf->meas, 0, sizeof(dtmf->meas));
				}
				if (count >= time_meas) {
					dtmf->meas.frequency_low += f1;
					dtmf->meas.frequency_high += f2;
					dtmf->meas.amplitude_low += amplitude_low[i];
					dtmf->meas.amplitude_high += amplitude_high[i];
					dtmf->meas.count++;
				}
				count++;
				if (count >= time_detect) {
					detected = digit;
					dtmf->meas.frequency_low /= dtmf->meas.count;
					dtmf->meas.frequency_high /= dtmf->meas.count;
					dtmf->meas.amplitude_low /= dtmf->meas.count;
					dtmf->meas.amplitude_high /= dtmf->meas.count;
					dtmf->meas.count = 1;
					dtmf->recv_digit(dtmf->priv, digit, &dtmf->meas);
				}
			} else
				count = 0;
		} else {
			if (!digit || digit != detected || !aplitude_ok || !twist_ok) {
				count++;
				if (count >= time_pause) {
					detected = 0;
#ifdef DEBUG
					printf("lost!\n");
#endif
				}
			} else
				count = 0;
		}
#ifdef DEBUG
		if (digit)
			printf("DTMF tone='%c' diff frequency=%.1f %.1f amplitude=%.1f %.1f dB (%s) twist=%.1f dB (%s)\n", digit, f1, f2, level2db(amplitude_low[i]), level2db(amplitude_high[i]), (aplitude_ok) ? "OK" : "nok", level2db(amplitude_high[i] / amplitude_low[i]), (twist_ok) ? "OK" : "nok");
#endif

		dtmf->detected = detected;
		dtmf->count = count;
	}
}

