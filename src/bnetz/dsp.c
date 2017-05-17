/* B-Netz signal processing
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

#define CHAN bnetz->sender.kanal

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "../common/sample.h"
#include "../common/debug.h"
#include "../common/timer.h"
#include "../common/call.h"
#include "bnetz.h"
#include "dsp.h"

#define PI		3.1415927

/* Notes on TX_PEAK_TONE level:
 *
 * At 2000 Hz the deviation shall be 4 kHz, so with emphasis the deviation
 * at 1000 Hz would be theoretically 2 kHz. This is factor 0.714 below
 * 2.8 kHz deviation we want at dBm0.
 */

/* signaling */
#define MAX_DEVIATION	4000.0
#define MAX_MODULATION	3000.0
#define DBM0_DEVIATION	2800.0	/* deviation of dBm0 at 1 kHz */
#define TX_PEAK_TONE	(4000.0 / 2000.0 * 1000.0 / DBM0_DEVIATION)
#define MAX_DISPLAY	1.4	/* something above dBm0 */
#define BIT_DURATION	0.010	/* bit length: 10 ms */
#define FILTER_STEP	0.001	/* step every 1 ms */
#define METERING_HZ	2900	/* metering pulse frequency */

#define TONE_DETECT_TH	70	/* 70 milliseconds to detect continuous tone */

/* carrier loss detection */
#define LOSS_INTERVAL	1000	/* filter steps (milliseconds) for one second interval */
#define LOSS_TIME	12	/* duration of signal loss before release */

/* two signaling tones */
static double fsk_bits[2] = {
	2070.0,
	1950.0,
};

/* table for fast sine generation */
static sample_t dsp_sine[65536];

/* global init for FSK */
void dsp_init(void)
{
	int i;

	PDEBUG(DDSP, DEBUG_DEBUG, "Generating sine table.\n");
	for (i = 0; i < 65536; i++) {
		dsp_sine[i] = sin((double)i / 65536.0 * 2.0 * PI) * TX_PEAK_TONE;
	}
}

/* Init transceiver instance. */
int dsp_init_sender(bnetz_t *bnetz)
{
	sample_t *spl;
	int i;

	if ((bnetz->sender.samplerate % (int)(1.0 / (double)BIT_DURATION))) {
		PDEBUG(DDSP, DEBUG_ERROR, "Samples rate must be a multiple of %d (bits per second).\n", (int)(1.0 / (double)BIT_DURATION));
		return -EINVAL;
	}
	if ((bnetz->sender.samplerate % (int)(1.0 / (double)FILTER_STEP))) {
		PDEBUG(DDSP, DEBUG_ERROR, "Samples rate must be a multiple of %d (FSK probes per second).\n", (int)(1.0 / (double)FILTER_STEP));
		return -EINVAL;
	}

	PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Init DSP for 'Sender'.\n");

	/* set modulation parameters */
	sender_set_fm(&bnetz->sender, MAX_DEVIATION, MAX_MODULATION, DBM0_DEVIATION, MAX_DISPLAY);

	audio_init_loss(&bnetz->sender.loss, LOSS_INTERVAL, bnetz->sender.loss_volume, LOSS_TIME);

	bnetz->samples_per_bit = bnetz->sender.samplerate * BIT_DURATION;
	PDEBUG(DDSP, DEBUG_DEBUG, "Using %d samples per bit duration.\n", bnetz->samples_per_bit);
	bnetz->fsk_filter_step = bnetz->sender.samplerate * FILTER_STEP;
	PDEBUG(DDSP, DEBUG_DEBUG, "Using %d samples per filter step.\n", bnetz->fsk_filter_step);
	spl = calloc(16, bnetz->samples_per_bit * sizeof(*spl));
	if (!spl) {
		PDEBUG(DDSP, DEBUG_ERROR, "No memory!\n");
		return -ENOMEM;
	}
	bnetz->telegramm_spl = spl;
	spl = calloc(1, bnetz->samples_per_bit * sizeof(*spl));
	if (!spl) {
		PDEBUG(DDSP, DEBUG_ERROR, "No memory!\n");
		return -ENOMEM;
	}
	bnetz->fsk_filter_spl = spl;
	bnetz->fsk_filter_bit = -1;

	bnetz->tone_detected = -1;

	/* count symbols */
	for (i = 0; i < 2; i++) {
		audio_goertzel_init(&bnetz->fsk_goertzel[i], fsk_bits[i], bnetz->sender.samplerate);
		bnetz->phaseshift65536[i] = 65536.0 / ((double)bnetz->sender.samplerate / fsk_bits[i]);
		PDEBUG(DDSP, DEBUG_DEBUG, "phaseshift[%d] = %.4f (must be arround 64 at 8000hz)\n", i, bnetz->phaseshift65536[i]);
	}

	return 0;
}

/* Cleanup transceiver instance. */
void dsp_cleanup_sender(bnetz_t *bnetz)
{
	PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Cleanup DSP for 'Sender'.\n");

	if (bnetz->telegramm_spl) {
		free(bnetz->telegramm_spl);
		bnetz->telegramm_spl = NULL;
	}
	if (bnetz->fsk_filter_spl) {
		free(bnetz->fsk_filter_spl);
		bnetz->fsk_filter_spl = NULL;
	}
}

/* Count duration of tone and indicate detection/loss to protocol handler. */
static void fsk_receive_tone(bnetz_t *bnetz, int bit, int goodtone, double level, double quality)
{
	/* lost tone because it is not good anymore or has changed */
	if (!goodtone || bit != bnetz->tone_detected) {
		if (bnetz->tone_count >= TONE_DETECT_TH) {
			PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Lost %.0f Hz tone after %d ms.\n", fsk_bits[bnetz->tone_detected], bnetz->tone_count);
			bnetz_receive_tone(bnetz, -1);
		}
		if (goodtone)
			bnetz->tone_detected = bit;
		else
			bnetz->tone_detected = -1;
		bnetz->tone_count = 0;

		return;
	}

	bnetz->tone_count++;

	if (bnetz->tone_count >= TONE_DETECT_TH)
		audio_reset_loss(&bnetz->sender.loss);
	if (bnetz->tone_count == TONE_DETECT_TH) {
		PDEBUG_CHAN(DDSP, DEBUG_INFO, "Detecting continuous tone: %.0f:Level=%3.0f%% Quality=%3.0f%%\n", fsk_bits[bnetz->tone_detected], level * 100.0, quality * 100.0);
		bnetz_receive_tone(bnetz, bnetz->tone_detected);
	}
}

/* Collect 16 data bits (digit) and check for sync marc '01110'. */
static void fsk_receive_bit(bnetz_t *bnetz, int bit, double level, double quality)
{
	int i;

	bnetz->fsk_filter_telegramm = (bnetz->fsk_filter_telegramm << 1) | bit;
	bnetz->fsk_filter_quality[bnetz->fsk_filter_qualidx] = quality;
	bnetz->fsk_filter_level[bnetz->fsk_filter_qualidx] = level;
	if (++bnetz->fsk_filter_qualidx == 16)
		bnetz->fsk_filter_qualidx = 0;

	/* check if pattern 01110xxxxxxxxxxx matches */
	if ((bnetz->fsk_filter_telegramm & 0xf800) != 0x7000)
		return;

	/* get worst bit and average level */
	level = 0;
	for (i = 0; i < 16; i++) {
		if (bnetz->fsk_filter_quality[i] < quality)
			quality = bnetz->fsk_filter_quality[i];
		level = bnetz->fsk_filter_level[i];
	}

	/* send telegramm */
	bnetz_receive_telegramm(bnetz, bnetz->fsk_filter_telegramm, level, quality);
}

//#define DEBUG_FILTER
//#define DEBUG_QUALITY

/* Filter one chunk of audio an detect tone, quality and loss of signal.
 * The chunk is a window of 10ms. This window slides over audio stream
 * and is processed every 1ms. (one step) */
static inline void fsk_decode_step(bnetz_t *bnetz, int pos)
{
	double level, result[2], softbit, quality;
	int max;
	sample_t *spl;
	int bit;

	max = bnetz->samples_per_bit;
	spl = bnetz->fsk_filter_spl;

	level = audio_level(spl, max);

	if (audio_detect_loss(&bnetz->sender.loss, level))
		bnetz_loss_indication(bnetz);

	audio_goertzel(bnetz->fsk_goertzel, spl, max, pos, result, 2);

	/* calculate soft bit from both frequencies */
	softbit = (result[1] / level - result[0] / level + 1.0) / 2.0;
	/* scale it, since both filters overlap by some percent */
#define MIN_QUALITY 0.08
	softbit = (softbit - MIN_QUALITY) / (0.850 - MIN_QUALITY - MIN_QUALITY);
	if (softbit > 1)
		softbit = 1;
	if (softbit < 0)
		softbit = 0;
#ifdef DEBUG_FILTER
	printf("|%s", debug_amplitude(result[0]/level));
	printf("|%s| low=%.3f high=%.3f level=%d\n", debug_amplitude(result[1]/level), result[0]/level, result[1]/level, (int)level);
#endif
	if (softbit > 0.5)
		bit = 1;
	else
		bit = 0;

//	quality = result[bit] / level;
	if (softbit > 0.5)
		quality = softbit * 2.0 - 1.0;
	else
		quality = 1.0 - softbit * 2.0;

	// FIXME: better threshold
	/* adjust level, so we get peak of sine curve */
	if (level / 0.63 > 0.05 && (softbit > 0.75 || softbit < 0.25)) {
		fsk_receive_tone(bnetz, bit, 1, level / 0.63662 / TX_PEAK_TONE, quality);
	} else
		fsk_receive_tone(bnetz, bit, 0, level / 0.63662 / TX_PEAK_TONE, quality);

	if (bnetz->fsk_filter_bit != bit) {
		/* if we have a bit change, reset sample counter to one half bit duration */
		bnetz->fsk_filter_bit = bit;
		bnetz->fsk_filter_sample = 5;
	} else if (--bnetz->fsk_filter_sample == 0) {
		/* if sample counter bit reaches 0, we reset sample counter to one bit duration */
#ifdef DEBUG_QUALITY
		printf("|%s| quality=%.2f ", debug_amplitude(softbit), quality);
		printf("|%s|\n", debug_amplitude(quality);
#endif
		/* adjust level, so we get peak of sine curve */
		fsk_receive_bit(bnetz, bit, level / 0.63662 / TX_PEAK_TONE, quality);
		bnetz->fsk_filter_sample = 10;
	}
}

/* Process received audio stream from radio unit. */
void sender_receive(sender_t *sender, sample_t *samples, int length)
{
	bnetz_t *bnetz = (bnetz_t *) sender;
	sample_t *spl;
	int max, pos, step;
	int i;

	/* write received samples to decode buffer */
	max = bnetz->samples_per_bit;
	pos = bnetz->fsk_filter_pos;
	step = bnetz->fsk_filter_step;
	spl = bnetz->fsk_filter_spl;
	for (i = 0; i < length; i++) {
		spl[pos++] = samples[i];
		if (pos == max)
			pos = 0;
		/* if filter step has been reched */
		if (!(pos % step)) {
			fsk_decode_step(bnetz, pos);
		}
	}
	bnetz->fsk_filter_pos = pos;

	if (bnetz->dsp_mode == DSP_MODE_AUDIO && bnetz->callref) {
		int count;

		count = samplerate_downsample(&bnetz->sender.srstate, samples, length);
		spl = bnetz->sender.rxbuf;
		pos = bnetz->sender.rxbuf_pos;
		for (i = 0; i < count; i++) {
			spl[pos++] = samples[i];
			if (pos == 160) {
				call_tx_audio(bnetz->callref, spl, 160);
				pos = 0;
			}
		}
		bnetz->sender.rxbuf_pos = pos;
	} else
		bnetz->sender.rxbuf_pos = 0;
}

static void fsk_tone(bnetz_t *bnetz, sample_t *samples, int length, int tone)
{
	double phaseshift, phase;
	int i;

	phase = bnetz->phase65536;
	phaseshift = bnetz->phaseshift65536[tone];

	for (i = 0; i < length; i++) {
		*samples++ = dsp_sine[(uint16_t)phase];
		phase += phaseshift;
		if (phase >= 65536)
			phase -= 65536;
	}

	bnetz->phase65536 = phase;
}

static int fsk_telegramm(bnetz_t *bnetz, sample_t *samples, int length)
{
	sample_t *spl;
	const char *telegramm;
	int i, j;
	double phaseshift, phase;
	int count, max;

next_telegramm:
	if (!bnetz->telegramm) {
		/* request telegramm */
//		PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Request new 'Telegramm'.\n");
		telegramm = bnetz_get_telegramm(bnetz);
		if (!telegramm) {
			PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Stop sending 'Telegramm'.\n");
			return length;
		}
		bnetz->telegramm = 1;
		bnetz->telegramm_pos = 0;
		spl = bnetz->telegramm_spl;
		/* render telegramm */
		phase = bnetz->phase65536;
		for (i = 0; i < 16; i++) {
			phaseshift = bnetz->phaseshift65536[telegramm[i] == '1'];
			for (j = 0; j < bnetz->samples_per_bit; j++) {
				*spl++ = dsp_sine[(uint16_t)phase];
				phase += phaseshift;
				if (phase >= 65536)
					phase -= 65536;
			}
		}
		bnetz->phase65536 = phase;
	}

	/* send audio from telegramm */
	max = bnetz->samples_per_bit * 16;
	count = max - bnetz->telegramm_pos;
	if (count > length)
		count = length;
	spl = bnetz->telegramm_spl + bnetz->telegramm_pos;
	for (i = 0; i < count; i++)
		*samples++ = *spl++;
	length -= count;
	bnetz->telegramm_pos += count;
	/* check for end of telegramm */
	if (bnetz->telegramm_pos == max) {
		bnetz->telegramm = 0;
		/* we need more ? */
		if (length)
			goto next_telegramm;
	}

	return length;
}

/* Provide stream of audio toward radio unit */
void sender_send(sender_t *sender, sample_t *samples, int length)
{
	bnetz_t *bnetz = (bnetz_t *) sender;
	int len;

again:
	switch (bnetz->dsp_mode) {
	case DSP_MODE_SILENCE:
		memset(samples, 0, length * sizeof(*samples));
		break;
	case DSP_MODE_AUDIO:
		jitter_load(&bnetz->sender.dejitter, samples, length);
		break;
	case DSP_MODE_0:
		fsk_tone(bnetz, samples, length, 0);
		break;
	case DSP_MODE_1:
		fsk_tone(bnetz, samples, length, 1);
		break;
	case DSP_MODE_TELEGRAMM:
		/* Encode telegramm into audio stream. If telegramms have
		 * stopped, process again for rest of stream. */
		len = fsk_telegramm(bnetz, samples, length);
		if (len) {
			samples += length - len;
			length = len;
			goto again;
		}
		break;
	}
}

const char *bnetz_dsp_mode_name(enum dsp_mode mode)
{
        static char invalid[16];

	switch (mode) {
	case DSP_MODE_SILENCE:
		return "SILENCE";
	case DSP_MODE_AUDIO:
		return "AUDIO";
	case DSP_MODE_0:
		return "TONE 0";
	case DSP_MODE_1:
		return "TONE 1";
	case DSP_MODE_TELEGRAMM:
		return "TELEGRAMM";
	}

	sprintf(invalid, "invalid(%d)", mode);
	return invalid;
}

void bnetz_set_dsp_mode(bnetz_t *bnetz, enum dsp_mode mode)
{
	/* reset telegramm */
	if (mode == DSP_MODE_TELEGRAMM && bnetz->dsp_mode != mode)
		bnetz->telegramm = 0;
	
	PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "DSP mode %s -> %s\n", bnetz_dsp_mode_name(bnetz->dsp_mode), bnetz_dsp_mode_name(mode));
	bnetz->dsp_mode = mode;
}

