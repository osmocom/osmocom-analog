/* Eurosignal signal processing
 *
 * (C) 2019 by Andreas Eversberg <jolly@eversberg.eu>
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

#define CHAN euro->sender.kanal

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "../libsample/sample.h"
#include "../libdebug/debug.h"
#include "eurosignal.h"
#include "dsp.h"

#define PI		3.1415927

/* signaling */
#define MAX_DEVIATION	5000.0	/* FIXME */
#define MAX_MODULATION	2000.0
#define TONE_DEVIATION	5000.0	/* FIXME */
#define TONE_INDEX	0.92
#define MAX_DISPLAY	1.4	/* something above tone level */

#define DIGIT_DURATION	0.1	/* duration of digit */
#define PAUSE_DURATION	0.22	/* duration of pause */

#define FREQUENCY_MIN	313.3
#define FREQUENCY_MAX	1153.1
#define FREQUENCY_TOL	15.0	/* tolerance of frequency */
#define DIGIT_DETECT	200	/* time for a tone to sustain (in samples) */
#define TIMEOUT_DETECT	4000	/* time for a tone to sustain (in samples) */

static struct dsp_digits {
	char	digit;
	char	*name;
	double	frequency;
	double	phaseshift65536;
} dsp_digits[] = {
	{ 'I',	"Idle",		1153.1,	0 },
	{ 'R',	"Repeat",	1062.9,	0 },
	{ '0',	"Digit 0",	 979.8,	0 },
	{ '1',	"Digit 1",	 903.1,	0 },
	{ '2',	"Digit 2",	 832.5,	0 },
	{ '3',	"Digit 3",	 767.4,	0 },
	{ '4',	"Digit 4",	 707.4,	0 },
	{ '5',	"Digit 5",	 652.0,	0 },
	{ '6',	"Digit 6",	 601.0,	0 },
	{ '7',	"Digit 7",	 554.0,	0 },
	{ '8',	"Digit 8",	 510.7,	0 },
	{ '9',	"Digit 9",	 470.8,	0 },
	{ 'A',	"Digit 10",	 433.9,	0 },
	{ 'B',	"Digit 11",	 400.0,	0 },
	{ 'C',	"Digit 12",	 368.7,	0 },
	{ 'D',	"Digit 13",	 339.9,	0 },
	{ 'E',	"Digit 14",	 313.3,	0 },
	{ '\0',	NULL,		 0.0,	0 },
};

static const char *digit_to_name(char digit)
{
	int i;

	for (i = 0; dsp_digits[i].digit; i++) {
		if (dsp_digits[i].digit == digit)
			return dsp_digits[i].name;
	}

	return "<none>";
}

static double digit_to_phaseshift65536(char digit)
{
	int i;

	for (i = 0; dsp_digits[i].digit; i++) {
		if (dsp_digits[i].digit == digit)
			return dsp_digits[i].phaseshift65536;
	}

	return 0.0;
}

static double dsp_tone[65536];

/* global init for FSK */
void dsp_init(int samplerate)
{
	int i;

	PDEBUG(DDSP, DEBUG_DEBUG, "Generating phase shiftings for tones.\n");
	for (i = 0; dsp_digits[i].digit; i++)
		dsp_digits[i].phaseshift65536 = 65536.0 / ((double)samplerate / dsp_digits[i].frequency);

	PDEBUG(DDSP, DEBUG_DEBUG, "Generating sine table for tones.\n");
	for (i = 0; i < 65536; i++)
		dsp_tone[i] = sin((double)i / 65536.0 * 2.0 * PI);
}

/* Init transceiver instance. */
int dsp_init_sender(euro_t *euro, int samplerate, int fm)
{
	int rc = 0;

	PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Init DSP for 'Sender'.\n");

	/* set modulation parameters */
	if (fm)
		sender_set_fm(&euro->sender, MAX_DEVIATION, MAX_MODULATION, TONE_DEVIATION, MAX_DISPLAY);
	else
		sender_set_am(&euro->sender, MAX_MODULATION, 1.0, MAX_DISPLAY, TONE_INDEX);

	euro->sample_duration = 1.0 / (double)samplerate;

	/* initial phase shift */
	euro->tx_phaseshift65536 = digit_to_phaseshift65536('I');

	/* init demodulator */
	rc = fm_demod_init(&euro->rx_demod, 8000, (FREQUENCY_MIN + FREQUENCY_MAX) / 2.0, FREQUENCY_MAX - FREQUENCY_MIN);
	if (rc)
		goto error;

	/* use fourth order (2 iter) filter, since it is as fast as second order (1 iter) filter */
	iir_lowpass_init(&euro->rx_lp, 25.0, 8000, 2);

	euro->dmp_tone_level = display_measurements_add(&euro->sender.dispmeas, "Tone Level", "%.1f %%", DISPLAY_MEAS_LAST, DISPLAY_MEAS_LEFT, 0.0, 150.0, 100.0);
	//euro->dmp_tone_quality = display_measurements_add(&euro->sender.dispmeas, "Tone Quality", "%.1f %%", DISPLAY_MEAS_LAST, DISPLAY_MEAS_LEFT, 0.0, 100.0, 100.0);

	return 0;

error:
	dsp_cleanup_sender(euro);

	return -rc;
}

/* Cleanup transceiver instance. */
void dsp_cleanup_sender(euro_t *euro)
{
	PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Cleanup DSP for 'Sender'.\n");

	/* cleanup demodulator */
	fm_demod_exit(&euro->rx_demod);
}

//#define DEBUG

static void tone_decode(euro_t *euro, sample_t *samples, int length)
{
	sample_t frequency[length], f;
	sample_t I[length], Q[length];
	int i, d;
	char digit;

	/* tone demodulation */
        fm_demodulate_real(&euro->rx_demod, frequency, length, samples, I, Q);

	/* reduce bandwidth of tone detector */
	iir_process(&euro->rx_lp, frequency, length);

	/* detect tone */
	for (i = 0; i < length; i++) {
		/* get frequency */
		f = frequency[i] + (FREQUENCY_MIN + FREQUENCY_MAX) / 2.0;
#ifdef DEBUG
		if (i == 0) printf("%s %.5f   ", debug_amplitude(frequency[i] / (FREQUENCY_MAX - FREQUENCY_MIN) * 2.0), f);
#endif
		for (d = 0; dsp_digits[d].digit; d++) {
			if (f >= dsp_digits[d].frequency - FREQUENCY_TOL && f <= dsp_digits[d].frequency + FREQUENCY_TOL)
				break;
		}
#ifdef DEBUG
		if (i == 0) printf("%c\n", dsp_digits[d].digit);
#endif

		/* change detection and collect digits */
		digit = dsp_digits[d].digit;
		if (digit != euro->rx_digit_last) {
			euro->rx_digit_last = digit;
			euro->rx_digit_count = 0;
		}
		euro->rx_digit_count++;
		switch (digit) {
		case 'I':
			/* pause tone */
			if (euro->rx_digit_count == DIGIT_DETECT) {
				PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Detected Idle tone, starting.\n");
				euro->rx_digit_receiving = 1;
				euro->rx_digit_index = 0;
				euro->rx_timeout_count = 0;
			}
			break;
		case '\0':
			/* we are not yet receiving digits */
			if (!euro->rx_digit_receiving)
				break;
			if (euro->rx_digit_count == DIGIT_DETECT) {
				/* out of range tone */
				PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Detected tone out of range, aborting.\n");
				euro->rx_digit_receiving = 0;
			}
			break;
		default:
			/* we are not yet receiving digits */
			if (!euro->rx_digit_receiving)
				break;
			/* got digit */
			if (euro->rx_digit_count == DIGIT_DETECT) {
				double level;
				level = sqrt(I[i] * I[i] + Q[i] * Q[i]) * 2;
				PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Detected digit '%s' (level = %.0f%%)\n", digit_to_name(digit), level * 100.0);
				display_measurements_update(euro->dmp_tone_level, level * 100.0, 0.0);
				euro->rx_digits[euro->rx_digit_index] = digit;
				euro->rx_digit_index++;
				euro->rx_timeout_count = 0;
				if (euro->rx_digit_index == 6 || euro->rx_digits[0] == 'R') {
					euro->rx_digits[euro->rx_digit_index] = '\0';
					euro_receive_id(euro, euro->rx_digits);
					euro->rx_digit_receiving = 0;
				}
				break;
			}
		}

		/* abort if tone sustains too long or next tone will not become steady */
		if (euro->rx_digit_receiving && euro->rx_digit_index) {
			euro->rx_timeout_count++;
			if (euro->rx_timeout_count == TIMEOUT_DETECT) {
				PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Timeout receiving, aborting.\n");
				euro->rx_digit_receiving = 0;
			}
		}

		euro->rx_digit_last = digit;
	}
}

/* Process received audio stream from radio unit. */
void sender_receive(sender_t *sender, sample_t *samples, int length, double __attribute__((unused)) rf_level_db)
{
	euro_t *euro = (euro_t *) sender;
	sample_t down[length];
	int count;

	/* downsample and decode */
	memcpy(down, samples, sizeof(down)); // copy, so audio will not be corrupted at loopback
	count = samplerate_downsample(&euro->sender.srstate, down, length);

	if (euro->rx)
		tone_decode(euro, down, count);
}

/* Generate tone of paging digits. */
static void tone_send(euro_t *euro, sample_t *samples, int length)
{
	int i;

	for (i = 0; i < length; i++) {
		if (!euro->tx_digits[0]) {
			if (euro->tx_time >= PAUSE_DURATION) {
				euro->tx_time -= PAUSE_DURATION;
				euro_get_id(euro, euro->tx_digits);
				euro->tx_digit_index = 0;
				PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Sending digit '%s'\n", digit_to_name(euro->tx_digits[0]));
				euro->tx_phaseshift65536 = digit_to_phaseshift65536(euro->tx_digits[0]);
			}
		} else {
			if (euro->tx_time >= DIGIT_DURATION) {
				euro->tx_time -= DIGIT_DURATION;
				if (++euro->tx_digit_index == 6) {
					PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Sending Idle tone'\n");
					euro->tx_digits[0] = '\0';
					euro->tx_phaseshift65536 = digit_to_phaseshift65536('I');
				} else {
					PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Sending digit '%s'\n", digit_to_name(euro->tx_digits[euro->tx_digit_index]));
					euro->tx_phaseshift65536 = digit_to_phaseshift65536(euro->tx_digits[euro->tx_digit_index]);
				}
			}
		}

		*samples++ = dsp_tone[(uint16_t)euro->tx_phase];
		euro->tx_phase += euro->tx_phaseshift65536;
		if (euro->tx_phase >= 65536.0)
			euro->tx_phase -= 65536.0;
		euro->tx_time += euro->sample_duration;
	}
}

/* Provide stream of audio toward radio unit */
void sender_send(sender_t *sender, sample_t *samples, uint8_t *power, int length)
{
	euro_t *euro = (euro_t *) sender;

	if (euro->tx) {
		memset(power, 1, length);
		tone_send(euro, samples, length);
	} else {
		memset(power, 0, length);
		memset(samples, 0, sizeof(*samples) * length);
	}
}

