/* FSK audio processing (FSK/FFSK modem)
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "../libsample/sample.h"
#include "../libdebug/debug.h"
#include "fsk.h"

#define PI			M_PI

/* uncomment to see the modulated curve */
//#define DEBUG_MODULATOR

/* uncomment to see the shape of the filter */
//#define DEBUG_MODULATOR_SHAPE

/*
 * fsk = instance of fsk modem
 * inst = instance of user
 * send_bit() = function to be called whenever a new bit has to be sent
 * samplerate = samplerate
 * bitrate = bits per second
 * f0, f1 = two frequencies for bit 0 and bit 1
 * level = level to modulate the frequencies
 * ffsk = use FFSK modulation (each symbol ends at zero crossing)
 */
int fsk_mod_init(fsk_mod_t *fsk, void *inst, int (*send_bit)(void *inst), int samplerate, double bitrate, double f0, double f1, double level, int ffsk, int filter)
{
	double temp;
	int i;
	int rc;

	PDEBUG(DDSP, DEBUG_DEBUG, "Setup FSK for Transmitter. (F0 = %.1f, F1 = %.1f, peak = %.1f)\n", f0, f1, level);

	memset(fsk, 0, sizeof(*fsk));

	/* gen sine table with deviation */
	fsk->sin_tab = calloc(65536, sizeof(*fsk->sin_tab));
	if (!fsk->sin_tab) {
		fprintf(stderr, "No mem!\n");
		rc = -ENOMEM;
		goto error;
	}
	for (i = 0; i < 65536; i++) 
		fsk->sin_tab[i] = sin((double)i / 65536.0 * 2.0 * PI) * level;

	fsk->inst = inst;
	fsk->level = level;
	fsk->send_bit = send_bit;
	fsk->f0_deviation = (f0 - f1) / 2.0;
	fsk->f1_deviation = (f1 - f0) / 2.0;
	if (f0 < f1) {
		fsk->low_bit = 0;
		fsk->high_bit = 1;
	} else {
		fsk->low_bit = 1;
		fsk->high_bit = 0;
	}

	fsk->bits65536_per_sample = (double)bitrate / (double)samplerate * 65536.0;
	PDEBUG(DDSP, DEBUG_DEBUG, "Bitduration of %.4f bits per sample @ %d.\n", fsk->bits65536_per_sample / 65536.0, samplerate);

	fsk->phaseshift65536[0] = f0 / (double)samplerate * 65536.0;
	fsk->phaseshift65536[1] = f1 / (double)samplerate * 65536.0;
	PDEBUG(DDSP, DEBUG_DEBUG, "F0 = %.0f Hz (phaseshift65536[0] = %.4f)\n", f0, fsk->phaseshift65536[0]);
	PDEBUG(DDSP, DEBUG_DEBUG, "F1 = %.0f Hz (phaseshift65536[1] = %.4f)\n", f1, fsk->phaseshift65536[1]);

	/* use ffsk modulation, i.e. each bit has an integer number of
	 * half waves and starts/ends at zero crossing
	 */
	if (ffsk) {
		double waves;

		if (filter) {
			PDEBUG(DDSP, DEBUG_ERROR, "Cannot use FFSK with filter.\n");
			rc = -EINVAL;
			goto error;
		}

		PDEBUG(DDSP, DEBUG_DEBUG, "enable FFSK modulation mode\n");
		fsk->ffsk = 1;
		waves = (f0 / bitrate);
		if (fabs(round(waves * 2) - (waves * 2)) > 0.001) {
			fprintf(stderr, "Failed to set FFSK mode, half waves of F0 does not fit exactly into one bit, please fix!\n");
			abort();
		}
		fsk->cycles_per_bit65536[0] = waves * 65536.0;
		waves = (f1 / bitrate);
		if (fabs(round(waves * 2) - (waves * 2)) > 0.001) {
			fprintf(stderr, "Failed to set FFSK mode, half waves of F1 does not fit exactly into one bit, please fix!\n");
			abort();
		}
		fsk->cycles_per_bit65536[1] = waves * 65536.0;
	} else {
		fsk->cycles_per_bit65536[0] = f0 / bitrate * 65536.0;
		fsk->cycles_per_bit65536[1] = f1 / bitrate * 65536.0;
	}
	PDEBUG(DDSP, DEBUG_DEBUG, "F0 = %.0f Hz (cycles_per_bit65536[0] = %.4f)\n", f0, fsk->cycles_per_bit65536[0]);
	PDEBUG(DDSP, DEBUG_DEBUG, "F1 = %.0f Hz (cycles_per_bit65536[1] = %.4f)\n", f1, fsk->cycles_per_bit65536[1]);

	/* if filter is enabled, use a cosine shaped curve to change the phase each sample */
	if (filter) {
		fsk->phase_tab_0_1 = calloc(65536 + 65536, sizeof(*fsk->sin_tab));
		if (!fsk->phase_tab_0_1) {
			fprintf(stderr, "No mem!\n");
			rc = -ENOMEM;
			goto error;
		}
		fsk->phase_tab_1_0 = fsk->phase_tab_0_1 + 65536;
		for (i = 0; i < 65536; i++) {
			temp = cos((double)i / 65536.0 * PI) / 2 + 0.5; /* half cosine going from 1 to 0 */
			fsk->phase_tab_0_1[i] = temp * fsk->phaseshift65536[0] + (1.0 - temp) * fsk->phaseshift65536[1];
			fsk->phase_tab_1_0[i] = temp * fsk->phaseshift65536[1] + (1.0 - temp) * fsk->phaseshift65536[0];
#ifdef DEBUG_MODULATOR_SHAPE
			fsk->phase_tab_0_1[i] = 1.0 - temp;
			fsk->phase_tab_1_0[i] = temp;
#endif
		}

		PDEBUG(DDSP, DEBUG_DEBUG, "Enable filter to smooth FSK transmission.\n");
		fsk->filter = 1;
	}

	/* must reset, because bit states must be initialized */
	fsk_mod_reset(fsk);

	return 0;

error:
	fsk_mod_cleanup(fsk);
	return rc;
}

/* Cleanup transceiver instance. */
void fsk_mod_cleanup(fsk_mod_t *fsk)
{
	PDEBUG(DDSP, DEBUG_DEBUG, "Cleanup FSK for Transmitter.\n");

	if (fsk->sin_tab) {
		free(fsk->sin_tab);
		fsk->sin_tab = NULL;
	}
	if (fsk->phase_tab_0_1) {
		free(fsk->phase_tab_0_1);
		fsk->phase_tab_0_1 = NULL;
		fsk->phase_tab_1_0 = NULL;
	}
}

/* modulate bits
 *
 * If first/next bit is required, callback function send_bit() is called.
 * If there is no (more) data to be transmitted, the callback functions shall
 * return -1. In this case, this function stops and returns the number of
 * samples that have been rendered so far, if any.
 *
 * For FFSK mode, we round the phase on every bit change to the
 * next zero crossing. This prevents phase shifts due to rounding errors.
 */
int fsk_mod_send(fsk_mod_t *fsk, sample_t *sample, int length, int add)
{
	int count = 0;
	double phase, phaseshift;

	phase = fsk->tx_phase65536;

	/* get next bit */
	if (fsk->tx_bit < 0) {
next_bit:
		fsk->tx_last_bit = fsk->tx_bit;
		fsk->tx_bit = fsk->send_bit(fsk->inst);
#ifdef DEBUG_MODULATOR
		printf("bit change from %d to %d\n", fsk->tx_last_bit, fsk->tx_bit);
#endif
		if (fsk->tx_bit < 0) {
			fsk_mod_reset(fsk);
			goto done;
		}
		fsk->tx_bit &= 1;
		/* correct phase when changing bit */
		if (fsk->ffsk) {
			/* round phase to nearest zero crossing */
			if (phase > 16384.0 && phase < 49152.0)
				phase = 32768.0;
			else
				phase = 0;
#ifdef DEBUG_MODULATOR
			printf("phase %.3f bitpos=%.6f\n", phase / 65536.0, fsk->tx_bitpos65536 / 65536.0);
#endif
		}
		if (!fsk->filter) {
			/* change phase forward to the current bit position */
			phase += fsk->tx_bitpos65536 / 65536.0 * fsk->cycles_per_bit65536[fsk->tx_bit];
			if (phase >= 65536.0)
				phase -= 65536.0;
		}
	}

	/* modulate bit */
	if (!fsk->filter || fsk->tx_last_bit < 0 || fsk->tx_last_bit == fsk->tx_bit) {
		/* without filtering or when there is no bit change */
		phaseshift = fsk->phaseshift65536[fsk->tx_bit];
		while (count < length && fsk->tx_bitpos65536 < 65536.0) {
			if (add)
				sample[count++] += fsk->sin_tab[(uint16_t)phase];
			else
				sample[count++] = fsk->sin_tab[(uint16_t)phase];
#ifdef DEBUG_MODULATOR
			printf("|%s| %d\n", debug_amplitude(fsk->sin_tab[(uint16_t)phase] / fsk->level), fsk->tx_bit);
#endif
#ifdef DEBUG_MODULATOR_SHAPE
			printf("|%s| %d\n", debug_amplitude(fsk->tx_bit), fsk->tx_bit);
#endif
			phase += phaseshift;
			if (phase >= 65536.0)
				phase -= 65536.0;
			fsk->tx_bitpos65536 += fsk->bits65536_per_sample;
		}
	} else if (fsk->tx_bit > fsk->tx_last_bit) {
		/* with cosine shape filter, going from phase of 0 to phase of 1 */
		while (count < length && fsk->tx_bitpos65536 < 65536.0) {
			if (add)
				sample[count++] += fsk->sin_tab[(uint16_t)phase];
			else
				sample[count++] = fsk->sin_tab[(uint16_t)phase];
#ifdef DEBUG_MODULATOR
			printf("|%s| 0->1\n", debug_amplitude(fsk->sin_tab[(uint16_t)phase] / fsk->level));
#endif
#ifdef DEBUG_MODULATOR_SHAPE
			printf("|%s|0->1\n", debug_amplitude(fsk->phase_tab_0_1[(uint16_t)fsk->tx_bitpos65536]));
#endif
			phase += fsk->phase_tab_0_1[(uint16_t)fsk->tx_bitpos65536];
			if (phase >= 65536.0)
				phase -= 65536.0;
			fsk->tx_bitpos65536 += fsk->bits65536_per_sample;
		}
	} else {
		/* with cosine shape filter, going from phase of 1 to phase of 0 */
		while (count < length && fsk->tx_bitpos65536 < 65536.0) {
			if (add)
				sample[count++] += fsk->sin_tab[(uint16_t)phase];
			else
				sample[count++] = fsk->sin_tab[(uint16_t)phase];
#ifdef DEBUG_MODULATOR
			printf("|%s|1->0\n", debug_amplitude(fsk->sin_tab[(uint16_t)phase] / fsk->level));
#endif
#ifdef DEBUG_MODULATOR_SHAPE
			printf("|%s|1->0\n", debug_amplitude(fsk->phase_tab_1_0[(uint16_t)fsk->tx_bitpos65536]));
#endif
			phase += fsk->phase_tab_1_0[(uint16_t)fsk->tx_bitpos65536];
			if (phase >= 65536.0)
				phase -= 65536.0;
			fsk->tx_bitpos65536 += fsk->bits65536_per_sample;
		}
	}
	if (fsk->tx_bitpos65536 >= 65536.0) {
		fsk->tx_bitpos65536 -= 65536.0;
		if (!fsk->filter) {
			/* change phase back to the point when bit has changed */
			phase -= fsk->tx_bitpos65536 / 65536.0 * fsk->cycles_per_bit65536[fsk->tx_bit];
			if (phase < 0.0)
				phase += 65536.0;
		}
		goto next_bit;
	}

done:
	fsk->tx_phase65536 = phase;

	return count;
}

/* reset transmitter state, so we get a clean start */
void fsk_mod_reset(fsk_mod_t *fsk)
{
	fsk->tx_phase65536 = 0.0;
	fsk->tx_bitpos65536 = 0.0;
	fsk->tx_bit = -1;
	fsk->tx_last_bit = -1;
}

/*
 * fsk = instance of fsk modem
 * inst = instance of user
 * receive_bit() = function to be called whenever a new bit was received
 * samplerate = samplerate
 * bitrate = bits per second
 * f0, f1 = two frequencies for bit 0 and bit 1
 * bitadjust = how much to adjust the sample clock when a bitchange was detected. (0 = nothing, don't use this, 0.5 full adjustment)
 */
int fsk_demod_init(fsk_demod_t *fsk, void *inst, void (*receive_bit)(void *inst, int bit, double quality, double level), int samplerate, double bitrate, double f0, double f1, double bitadjust)
{
	double bandwidth;
	int rc;

	PDEBUG(DDSP, DEBUG_DEBUG, "Setup FSK for Receiver. (F0 = %.1f, F1 = %.1f)\n", f0, f1);

	memset(fsk, 0, sizeof(*fsk));

	fsk->inst = inst;
	fsk->rx_bit = -1;
	fsk->rx_bitadjust = bitadjust;
	fsk->receive_bit = receive_bit;
	fsk->f0_deviation = (f0 - f1) / 2.0;
	fsk->f1_deviation = (f1 - f0) / 2.0;
	if (f0 < f1) {
		fsk->low_bit = 0;
		fsk->high_bit = 1;
	} else {
		fsk->low_bit = 1;
		fsk->high_bit = 0;
	}

	/* calculate bandwidth */
	bandwidth = fabs(f0 - f1) * 2.0;

	/* init fm demodulator */
	rc = fm_demod_init(&fsk->demod, (double)samplerate, (f0 + f1) / 2.0, bandwidth);
	if (rc < 0)
		goto error;

	fsk->bits_per_sample = (double)bitrate / (double)samplerate;
	PDEBUG(DDSP, DEBUG_DEBUG, "Bitduration of %.4f bits per sample @ %d.\n", fsk->bits_per_sample, samplerate);

	return 0;

error:
	fsk_demod_cleanup(fsk);
	return rc;
}

/* Cleanup transceiver instance. */
void fsk_demod_cleanup(fsk_demod_t *fsk)
{
	PDEBUG(DDSP, DEBUG_DEBUG, "Cleanup FSK for Receiver.\n");

	fm_demod_exit(&fsk->demod);
}

//#define DEBUG_MODULATOR
//#define DEBUG_FILTER

/* Demodulates bits
 *
 * If bit is received, callback function send_bit() is called.
 *
 * We sample each bit 0.5 bits after polarity change.
 *
 * If we have a bit change, adjust sample counter towards one half bit duration.
 * We may have noise, so the bit change may be wrong or not at the correct place.
 * This can cause bit slips.
 * Therefore we change the sample counter only slightly, so bit slips may not
 * happen so quickly.
 */
void fsk_demod_receive(fsk_demod_t *fsk, sample_t *sample, int length)
{
	sample_t I[length], Q[length], frequency[length], f;
	int i;
	int bit;
	double level, quality;

	/* demod samples to offset around center frequency */
	fm_demodulate_real(&fsk->demod, frequency, length, sample, I, Q);

	for (i = 0; i < length; i++) {
		f = frequency[i];
		if (f < 0)
			bit = fsk->low_bit;
		else
			bit = fsk->high_bit;
#ifdef DEBUG_FILTER
			printf("|%s| %.3f\n", debug_amplitude(f / fabs(fsk->f0_deviation) / 2), f / fabs(fsk->f0_deviation));
#endif
	

		if (fsk->rx_bit != bit) {
#ifdef DEBUG_FILTER
			puts("bit change");
#endif
			fsk->rx_bit = bit;
			if (fsk->rx_bitpos < 0.5) {
				fsk->rx_bitpos += fsk->rx_bitadjust;
				if (fsk->rx_bitpos > 0.5)
					fsk->rx_bitpos = 0.5;
			} else
			if (fsk->rx_bitpos > 0.5) {
				fsk->rx_bitpos -= fsk->rx_bitadjust;
				if (fsk->rx_bitpos < 0.5)
					fsk->rx_bitpos = 0.5;
			}
			/* if we have a pulse before we sampled a bit after last pulse */
			if (fsk->rx_change) {
				/* peak level is the length of I/Q vector
				 * since we filter out the unwanted modulation product, the vector is only half of length */
				level = sqrt(I[i] * I[i] + Q[i] * Q[i]) * 2.0;
#ifdef DEBUG_FILTER
				printf("prematurely bit change (level=%.3f)\n", level);
#endif
				/* quality is 0.0, because a prematurely level change is caused by noise and has nothing to measure. */
				fsk->receive_bit(fsk->inst, fsk->rx_bit, 0.0, level);
			}
			fsk->rx_change = 1;
		}
		/* if bit counter reaches 1, we subtract 1 and sample the bit */
		if (fsk->rx_bitpos >= 1.0) {
			/* peak level is the length of I/Q vector
			 * since we filter out the unwanted modulation product, the vector is only half of length */
			level = sqrt(I[i] * I[i] + Q[i] * Q[i]) * 2.0;
			/* quality is defined on how accurat the target frequency it hit
			 * if it is hit close to the center or close to double deviation from center, quality is close to 0 */
			if (bit == 0)
				quality = 1.0 - fabs((f - fsk->f0_deviation) / fsk->f0_deviation);
			else
				quality = 1.0 - fabs((f - fsk->f1_deviation) / fsk->f1_deviation);
			if (quality < 0)
				quality = 0;
#ifdef DEBUG_FILTER
			printf("sample (level=%.3f, quality=%.3f)\n", level, quality);
#endif
			fsk->receive_bit(fsk->inst, bit, quality, level);
			fsk->rx_bitpos -= 1.0;
			fsk->rx_change = 0;
		}
		fsk->rx_bitpos += fsk->bits_per_sample;
	}
}

