/* GSC signal processing
 *
 * (C) 2022 by Andreas Eversberg <jolly@eversberg.eu>
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

#define CHAN gsc->sender.kanal

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <sys/param.h>
#include "../libsample/sample.h"
#include "../libdebug/debug.h"
#include "golay.h"
#include "dsp.h"

#define MAX_DISPLAY	1.4	/* something above speech level, no emphasis */
#define VOICE_BANDWIDTH	3000	/* just guessing */

static void dsp_init_ramp(gsc_t *gsc)
{
        double c;
        int i;

        PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Generating cosine shaped ramp table.\n");
        for (i = 0; i < 256; i++) {
		/* This is mathematically incorrect... */
                if (i < 64)
                        c = 1.0;
		else if (i >= 192)
                        c = -1.0;
		else
	                c = cos((double)(i - 64) / 128.0 * M_PI);
                gsc->fsk_ramp_down[i] = c * gsc->fsk_deviation * gsc->fsk_polarity;
                gsc->fsk_ramp_up[i] = -gsc->fsk_ramp_down[i];
        }
}

/* Init transceiver instance. */
int dsp_init_sender(gsc_t *gsc, int samplerate, double deviation, double polarity)
{
	int rc;

	PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Init DSP for transceiver.\n");

	/* set modulation parameters */
	// NOTE: baudrate equals modulation, because we have a raised cosine ramp of beta = 0.5
	sender_set_fm(&gsc->sender, deviation, 600.0, deviation, MAX_DISPLAY);

	gsc->fsk_bitduration = (double)samplerate / 600.0;
	gsc->fsk_bitstep = 1.0 / gsc->fsk_bitduration;
	PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Use %.4f samples for one bit duration @ %d.\n", gsc->fsk_bitduration, gsc->sender.samplerate);

	gsc->fsk_tx_buffer_size = gsc->fsk_bitduration + 10; /* 1 bit, add some extra to prevent short buffer due to rounding */
	gsc->fsk_tx_buffer = calloc(sizeof(sample_t), gsc->fsk_tx_buffer_size);
	if (!gsc->fsk_tx_buffer) {
		PDEBUG_CHAN(DDSP, DEBUG_ERROR, "No memory!\n");
		rc = -ENOMEM;
		goto error;
	}

	/* create deviation and ramp */
	gsc->fsk_deviation = 1.0; // equals what we st at sender_set_fm()
	gsc->fsk_polarity = polarity;
	dsp_init_ramp(gsc);

	return 0;

error:
        dsp_cleanup_sender(gsc);

        return -rc;

}

/* Cleanup transceiver instance. */
void dsp_cleanup_sender(gsc_t *gsc)
{
	PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Cleanup DSP for transceiver.\n");

	if (gsc->fsk_tx_buffer) {
		free(gsc->fsk_tx_buffer);
		gsc->fsk_tx_buffer = NULL;
	}
}


/* encode one bit into samples
 * input: bit
 * output: samples
 * return number of samples */
static int fsk_bit_encode(gsc_t *gsc, uint8_t bit)
{
	/* alloc samples, add 1 in case there is a rest */
	sample_t *spl;
	double phase, bitstep, devpol;
	int count;
	uint8_t lastbit;

	devpol = gsc->fsk_deviation * gsc->fsk_polarity;
	spl = gsc->fsk_tx_buffer;
	phase = gsc->fsk_tx_phase;
	lastbit = gsc->fsk_tx_lastbit;
	bitstep = gsc->fsk_bitstep * 256.0;

	if (lastbit) {
		if (bit) {
			/* stay up */
			do {
				*spl++ = devpol;
				phase += bitstep;
			} while (phase < 256.0);
			phase -= 256.0;
		} else {
			/* ramp down */
			do {
				*spl++ = gsc->fsk_ramp_down[(uint8_t)phase];
				phase += bitstep;
			} while (phase < 256.0);
			phase -= 256.0;
			lastbit = 0;
		}
	} else {
		if (bit) {
			/* ramp up */
			do {
				*spl++ = gsc->fsk_ramp_up[(uint8_t)phase];
				phase += bitstep;
			} while (phase < 256.0);
			phase -= 256.0;
			lastbit = 1;
		} else {
			/* stay down */
			do {
				*spl++ = -devpol;
				phase += bitstep;
			} while (phase < 256.0);
			phase -= 256.0;
		}
	}

	/* depending on the number of samples, return the number */
	count = ((uintptr_t)spl - (uintptr_t)gsc->fsk_tx_buffer) / sizeof(*spl);

	gsc->fsk_tx_phase = phase;
	gsc->fsk_tx_lastbit = lastbit;

	return count;
}

/* Process received audio stream from radio unit. */
void sender_receive(sender_t __attribute__((unused)) *sender, sample_t __attribute__((unused)) *samples, int __attribute__((unused)) length, double __attribute__((unused)) rf_level_db)
{
}

/* Provide stream of audio toward radio unit */
void sender_send(sender_t *sender, sample_t *samples, uint8_t *power, int length)
{
	gsc_t *gsc = (gsc_t *) sender;
	int rc;

again:
	/* play 2 seconds of pause */
	if (gsc->wait_2_sec) {
		int tosend = MIN(length, gsc->wait_2_sec);
		memset(power, 1, tosend);
		memset(samples, 0, sizeof(samples) * tosend);
		power += tosend;
		samples += tosend;
		gsc->wait_2_sec -= tosend;
		if (gsc->wait_2_sec)
			return;
	}

	/* play wave file, if open */
	if (gsc->wave_tx_play.left) {
		int wave_num, s;
		wave_num = samplerate_upsample_input_num(&gsc->wave_tx_upsample, length);
		sample_t buffer[wave_num * 2], *wave_samples[2] = { buffer, buffer + wave_num };
		wave_read(&gsc->wave_tx_play, wave_samples, wave_num);
		if (gsc->wave_tx_channels == 2) {
			for (s = 0; s < wave_num; s++) {
				wave_samples[0][s] += wave_samples[1][s];
			}
		}
		samplerate_upsample(&gsc->wave_tx_upsample, wave_samples[0], wave_num, samples, length);
		if (!gsc->wave_tx_play.left) {
			PDEBUG_CHAN(DDSP, DEBUG_INFO, "Voice message sent.\n");
			wave_destroy_playback(&gsc->wave_tx_play);
			return;
		}
		return;
	}


	/* get FSK bits or start playing wave file */
	if (!gsc->fsk_tx_buffer_length) {
		int8_t bit = get_bit(gsc);

		/* bit == 2 means voice transmission. */
		if (bit == 2) {
			if (gsc->wave_tx_filename[0]) {
				gsc->wave_tx_samplerate = gsc->wave_tx_channels = 0;
				rc = wave_create_playback(&gsc->wave_tx_play, gsc->wave_tx_filename, &gsc->wave_tx_samplerate, &gsc->wave_tx_channels, gsc->fsk_deviation);
				if (rc < 0) {
					gsc->wave_tx_play.left = 0;
					PDEBUG_CHAN(DDSP, DEBUG_ERROR, "Failed to open wave file '%s' for voice message.\n", gsc->wave_tx_filename);
				} else {
					PDEBUG_CHAN(DDSP, DEBUG_INFO, "Sending wave file '%s' for voice message after 2 seconds.\n", gsc->wave_tx_filename);
					init_samplerate(&gsc->wave_tx_upsample, gsc->wave_tx_samplerate, gsc->sender.samplerate, VOICE_BANDWIDTH);
				}
			}
			gsc->wait_2_sec = gsc->sender.samplerate * 2.0;
			goto again;
		}

		/* no message, power is off */
		if (bit < 0) {
			memset(samples, 0, sizeof(samples) * length);
			memset(power, 0, length);
			return;
		}

		/* encode */
		gsc->fsk_tx_buffer_length = fsk_bit_encode(gsc, bit);
		gsc->fsk_tx_buffer_pos = 0;
	}

	/* send encoded bit until end of source or destination buffer is reached */
	while (length) {
		*power++ = 1;
		*samples++ = gsc->fsk_tx_buffer[gsc->fsk_tx_buffer_pos++];
		length--;
		if (gsc->fsk_tx_buffer_pos == gsc->fsk_tx_buffer_length) {
			gsc->fsk_tx_buffer_length = 0;
			break;
		}
	}

	/* do again, if destination buffer is not yet full */
	if (length)
		goto again;
}

