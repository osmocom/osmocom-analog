/* POCSAG signal processing
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

#define CHAN pocsag->sender.kanal

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "../libsample/sample.h"
#include "../libdebug/debug.h"
#include "pocsag.h"
#include "frame.h"
#include "dsp.h"

#define CODEWORD_SYNC	0x7cd215d8

#define MAX_DISPLAY	1.4	/* something above speech level, no emphasis */

static void dsp_init_ramp(pocsag_t *pocsag)
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
                pocsag->fsk_ramp_down[i] = c * pocsag->fsk_deviation * pocsag->fsk_polarity;
                pocsag->fsk_ramp_up[i] = -pocsag->fsk_ramp_down[i];
        }
}

/* Init transceiver instance. */
int dsp_init_sender(pocsag_t *pocsag, int samplerate, int baudrate, double deviation, double polarity)
{
	int rc;

	PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Init DSP for transceiver.\n");

	/* set modulation parameters */
	// NOTE: baudrate equals modulation, because we have a raised cosine ramp of beta = 0.5
	sender_set_fm(&pocsag->sender, deviation, baudrate, deviation, MAX_DISPLAY);

	pocsag->fsk_bitduration = (double)samplerate / (double)baudrate;
	pocsag->fsk_bitstep = 1.0 / pocsag->fsk_bitduration;
	PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Use %.4f samples for one bit duration @ %d.\n", pocsag->fsk_bitduration, pocsag->sender.samplerate);

	pocsag->fsk_tx_buffer_size = pocsag->fsk_bitduration * 32.0 + 10; /* 32 bit, add some extra to prevent short buffer due to rounding */
	pocsag->fsk_tx_buffer = calloc(sizeof(sample_t), pocsag->fsk_tx_buffer_size);
	if (!pocsag->fsk_tx_buffer) {
		PDEBUG_CHAN(DDSP, DEBUG_ERROR, "No memory!\n");
		rc = -ENOMEM;
		goto error;
	}

	/* create deviation and ramp */
	pocsag->fsk_deviation = 1.0; // equals what we st at sender_set_fm()
	pocsag->fsk_polarity = polarity;
	dsp_init_ramp(pocsag);

	return 0;

error:
        dsp_cleanup_sender(pocsag);

        return -rc;

}

/* Cleanup transceiver instance. */
void dsp_cleanup_sender(pocsag_t *pocsag)
{
	PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Cleanup DSP for transceiver.\n");

	if (pocsag->fsk_tx_buffer) {
		free(pocsag->fsk_tx_buffer);
		pocsag->fsk_tx_buffer = NULL;
	}
}


/* encode one codeward into samples
 * input: 32 data bits
 * output: samples
 * return number of samples */
static int fsk_block_encode(pocsag_t *pocsag, uint32_t word)
{
	/* alloc samples, add 1 in case there is a rest */
	sample_t *spl;
	double phase, bitstep, devpol;
	int i, count;
	uint8_t lastbit;

	devpol = pocsag->fsk_deviation * pocsag->fsk_polarity;
	spl = pocsag->fsk_tx_buffer;
	phase = pocsag->fsk_tx_phase;
	lastbit = pocsag->fsk_tx_lastbit;
	bitstep = pocsag->fsk_bitstep * 256.0;

	/* add 32 bits */
	for (i = 0; i < 32; i++) {
		if (lastbit) {
			if ((word & 0x80000000)) {
				/* stay up */
				do {
					*spl++ = devpol;
					phase += bitstep;
				} while (phase < 256.0);
				phase -= 256.0;
			} else {
				/* ramp down */
				do {
					*spl++ = pocsag->fsk_ramp_down[(uint8_t)phase];
					phase += bitstep;
				} while (phase < 256.0);
				phase -= 256.0;
				lastbit = 0;
			}
		} else {
			if ((word & 0x80000000)) {
				/* ramp up */
				do {
					*spl++ = pocsag->fsk_ramp_up[(uint8_t)phase];
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
		word <<= 1;
	}

	/* depending on the number of samples, return the number */
	count = ((uintptr_t)spl - (uintptr_t)pocsag->fsk_tx_buffer) / sizeof(*spl);

	pocsag->fsk_tx_phase = phase;
	pocsag->fsk_tx_lastbit = lastbit;

	return count;
}

static void fsk_block_decode(pocsag_t *pocsag, uint8_t bit)
{
	if (!pocsag->fsk_rx_sync) {
		pocsag->fsk_rx_word = (pocsag->fsk_rx_word << 1) | bit;
		if (pocsag->fsk_rx_word == CODEWORD_SYNC) {
			put_codeword(pocsag, pocsag->fsk_rx_word, -1, -1);
			pocsag->fsk_rx_sync = 16;
			pocsag->fsk_rx_index = 0;
		} else
		if (pocsag->fsk_rx_word == (uint32_t)(~CODEWORD_SYNC))
			PDEBUG_CHAN(DDSP, DEBUG_NOTICE, "Received inverted sync, caused by wrong polarity or by radio noise. Verify correct polarity!\n");
	} else {
		pocsag->fsk_rx_word = (pocsag->fsk_rx_word << 1) | bit;
		if (++pocsag->fsk_rx_index == 32) {
			pocsag->fsk_rx_index = 0;
			put_codeword(pocsag, pocsag->fsk_rx_word, (16 - pocsag->fsk_rx_sync) >> 1, pocsag->fsk_rx_sync & 1);
			--pocsag->fsk_rx_sync;
		}
	}
}

static void fsk_decode(pocsag_t *pocsag, sample_t *spl, int length)
{
	double phase, bitstep, polarity;
	int i;
	uint8_t lastbit;

	polarity = pocsag->fsk_polarity;
	phase = pocsag->fsk_rx_phase;
	lastbit = pocsag->fsk_rx_lastbit;
	bitstep = pocsag->fsk_bitstep;

	for (i = 0; i < length; i++) {
		if (*spl++ * polarity > 0.0) {
			if (lastbit) {
				/* stay up */
				phase += bitstep;
				if (phase >= 1.0) {
					phase -= 1.0;
					fsk_block_decode(pocsag, 1);
				}
			} else {
				/* ramp up */
				phase = -0.5;
				fsk_block_decode(pocsag, 1);
				lastbit = 1;
			}
		} else {
			if (lastbit) {
				/* ramp down */
				phase = -0.5;
				fsk_block_decode(pocsag, 0);
				lastbit = 0;
			} else {
				/* stay down */
				phase += bitstep;
				if (phase >= 1.0) {
					phase -= 1.0;
					fsk_block_decode(pocsag, 0);
				}
			}
		}
	}

	pocsag->fsk_rx_phase = phase;
	pocsag->fsk_rx_lastbit = lastbit;
}

/* Process received audio stream from radio unit. */
void sender_receive(sender_t *sender, sample_t *samples, int length, double __attribute__((unused)) rf_level_db)
{
	pocsag_t *pocsag = (pocsag_t *) sender;

	if (pocsag->rx)
		fsk_decode(pocsag, samples, length);
}

/* Provide stream of audio toward radio unit */
void sender_send(sender_t *sender, sample_t *samples, uint8_t *power, int length)
{
	pocsag_t *pocsag = (pocsag_t *) sender;

again:
	/* get word */
	if (!pocsag->fsk_tx_buffer_length) {
		int64_t word = get_codeword(pocsag);

		/* no message, power is off */
		if (word < 0) {
			memset(samples, 0, sizeof(samples) * length);
			memset(power, 0, length);
			return;
		}

		/* encode */
		pocsag->fsk_tx_buffer_length = fsk_block_encode(pocsag, word);
		pocsag->fsk_tx_buffer_pos = 0;
	}

	/* send encoded word until end of source or destination buffer is reaced */
	while (length) {
		*power++ = 1;
		*samples++ = pocsag->fsk_tx_buffer[pocsag->fsk_tx_buffer_pos++];
		length--;
		if (pocsag->fsk_tx_buffer_pos == pocsag->fsk_tx_buffer_length) {
			pocsag->fsk_tx_buffer_length = 0;
			break;
		}
	}

	/* do again, if destination buffer is not yet full */
	if (length)
		goto again;
}


