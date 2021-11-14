/* selective call signal processing
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

#define CHAN fuenf->sender.kanal

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "../libsample/sample.h"
#include "../libdebug/debug.h"
#include "../libmobile/call.h"
#include "fuenf.h"
#include "dsp.h"

#define MAX_DISPLAY		1.4	/* something above speech level, no emphasis */
#define MAX_MODULATION		3000.0	/* maximum bandwidth of audio signal */

/* TX and RX parameters */
#define TONE_LEVEL		0.5	/* because we have two tones, also applies to digits */

/* TX parameters */
#define TX_LEN_PREAMBLE		0.600	/* duration of preamble */
#define TX_LEN_PAUSE		0.600	/* duration of pause */
#define TX_LEN_POSTAMBLE	0.070	/* duration of postamble */
#define TX_LEN_DIGIT		0.070	/* duration of paging tone */
#define TX_NUM_KANAL		10	/* number of 'Kanalbelegungston' */
#define TX_LEN_KANAL		0.250	/* duration of 'Kanalbelegungston' */
#define TX_LEN_KANAL_PAUSE	0.250	/* pause after 'Kanalbelegungston' */
#define TX_LEN_SIGNAL		5.0	/* double tone signal length */

/* RX parameters */
#define RX_MIN_LEVEL		0.1	/* level relative to TONE_LEVEL, below is silence (-20 dB) */
#define RX_MIN_PREAMBLE		800	/* duration of silence before detecting first digit (in samples) */
#define RX_DIGIT_FILTER		100.0	/* frequency to allow change of tones ( 100 Hz = 5 ms ) */
#define RX_TOL_DIGIT_FREQ	0.045	/* maximum frequency error factor allowd to detect a tone (+- 4.5%) */ 
#define RX_LEN_DIGIT_TH		80	/* time to wait for digit being stable ( 10 ms ) */
#define RX_LEN_DIGIT_MIN	400	/* minimum length in seconds allowed for a digit (- 20 ms in samples) */
#define RX_LEN_DIGIT_MAX	720	/* minimum length in seconds allowed for a digit (+ 20 ms in samples) */
#define RX_LEN_TONE_MIN		16000	/* minimum length in seconds to detect double tone (2 seconds in samples) */
#define RX_WAIT_TONE_MAX	48000	/* maximum time to wait for double tone (6 seconds in samples) */
#define RX_TOL_TONE_FREQ	5.0	/* use +-5 Hz for bandwidth, to make things simpler. (-7.4 dB @ +-5 Hz) */

static double digit_freq[DSP_NUM_DIGITS] = {
	1060.0,
	1160.0,
	1270.0,
	1400.0,
	1530.0,
	1670.0,
	1830.0,
	2000.0,
	2200.0,
	2400.0,
	2600.0, /* repeat digit */
};
#define DIGIT_FREQ_MIN 1080.0
#define DIGIT_FREQ_MAX 2600.0

#define REPEAT_DIGIT 10

/* these are the frequencies of tones to be detected */
static double tone_freq[DSP_NUM_TONES] = {
	675.0,
	825.0,
	1240.0,
	1860.0,
};

#define DSP_NUM_SIGNALS 6
static struct signals {
	enum	fuenf_funktion funktion;
	int	tone1, tone2;
} signals[DSP_NUM_SIGNALS] = {
	{ FUENF_FUNKTION_FEUER,		0, 2 },
	{ FUENF_FUNKTION_PROBE,		0, 3 },
	{ FUENF_FUNKTION_WARNUNG,	0, 1 },
	{ FUENF_FUNKTION_ABC,		2, 3 },
	{ FUENF_FUNKTION_ENTWARNUNG,	1, 3 },
	{ FUENF_FUNKTION_KATASTROPHE,	1, 2 },
};

/* Init transceiver instance. */
int dsp_init_sender(fuenf_t *fuenf, int samplerate, double max_deviation, double signal_deviation)
{
	int i;
	int rc;
	sample_t *spl;
	int len;

	PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Init DSP for transceiver.\n");

	/* set modulation parameters */
	sender_set_fm(&fuenf->sender, max_deviation, MAX_MODULATION, signal_deviation, MAX_DISPLAY);

	fuenf->sample_duration = 1.0 / (double)samplerate;

	/* init digit demodulator */
	rc = fm_demod_init(&fuenf->rx_digit_demod, 8000, (DIGIT_FREQ_MIN + DIGIT_FREQ_MAX) / 2.0, DIGIT_FREQ_MAX - DIGIT_FREQ_MIN);
	if (rc)
		goto error;

	/* use fourth order (2 iter) filter, since it is as fast as second order (1 iter) filter */
	iir_lowpass_init(&fuenf->rx_digit_lp, RX_DIGIT_FILTER, 8000, 2);

	/* init signal tone filters */
	for (i = 0; i < DSP_NUM_TONES; i++)
		audio_goertzel_init(&fuenf->rx_tone_goertzel[i], tone_freq[i], 8000);

	/* allocate buffer */
	len = (int)(8000.0 * (1.0 / RX_TOL_TONE_FREQ) + 0.5);
	spl = calloc(1, len * sizeof(*spl));
	if (!spl) {
		PDEBUG(DDSP, DEBUG_ERROR, "No memory!\n");
		goto error;
	}
	fuenf->rx_tone_filter_spl = spl;
	fuenf->rx_tone_filter_size = len;

	/* display values */
	fuenf->dmp_digit_level = display_measurements_add(&fuenf->sender.dispmeas, "Digit Level", "%.0f %%", DISPLAY_MEAS_LAST, DISPLAY_MEAS_LEFT, 0.0, 150.0, 100.0);
	for (i = 0; i < DSP_NUM_TONES; i++) {
		char name[64];
		sprintf(name, "%.0f Hz Level", tone_freq[i]);
		fuenf->dmp_tone_levels[i] = display_measurements_add(&fuenf->sender.dispmeas, name, "%.0f %%", DISPLAY_MEAS_LAST, DISPLAY_MEAS_LEFT, 0.0, 150.0, 100.0);
	}

	return 0;

error:
	dsp_cleanup_sender(fuenf);

	return -rc;
}

/* Cleanup transceiver instance. */
void dsp_cleanup_sender(fuenf_t *fuenf)
{
	PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Cleanup DSP for transceiver.\n");

	/* free tone buffers */
	if (fuenf->rx_tone_filter_spl)
		free(fuenf->rx_tone_filter_spl);
}

//#define DEBUG

/* receive digits and decode */
static void digit_decode(fuenf_t *fuenf, sample_t *samples, int length)
{
	sample_t frequency[length], f, a;
	sample_t I[length], Q[length];
	int i, d;
	int change, change_count;

	/* tone demodulation */
	fm_demodulate_real(&fuenf->rx_digit_demod, frequency, length, samples, I, Q);

	/* reduce bandwidth of tone detector */
	iir_process(&fuenf->rx_digit_lp, frequency, length);

	/* detect tone */
	for (i = 0; i < length; i++) {
		/* get frequency */
		f = frequency[i] + (DIGIT_FREQ_MIN + DIGIT_FREQ_MAX) / 2.0;

		/* get amplitude (a is a sqaure of the amplitude for faster math) */
		a = (I[i] * I[i] + Q[i] * Q[i]) * 2.0 * 2.0 / TONE_LEVEL / TONE_LEVEL;

#ifdef DEBUG
		if (i == 0) printf("%s %.5f   ", debug_amplitude(frequency[i] / (DIGIT_FREQ_MAX - DIGIT_FREQ_MIN) * 2.0), f);
		if (i == 0) printf("%s %.5f   ", debug_amplitude(sqrt(a)), sqrt(a));
#endif
		/* get digit that matches the frequency tolerance */
		for (d = 0; d < DSP_NUM_DIGITS; d++) {
			if (f >= digit_freq[d] * (1.0 - RX_TOL_DIGIT_FREQ) && f <= digit_freq[d] * (1.0 + RX_TOL_DIGIT_FREQ))
				break;
		}

		/* digit lound enough ? */
		if (a >= RX_MIN_LEVEL * RX_MIN_LEVEL && d < DSP_NUM_DIGITS) {
#ifdef DEBUG
			if (i == 0 && d < DSP_NUM_DIGITS) printf("digit=%d (%d == no digit detected)", d, DSP_NUM_DIGITS);
#endif
		} else
			d = -1;
#ifdef DEBUG
		if (i == 0) printf("\n");
#endif

		/* correct amplitude at cutoff frequency digit '1' and 'repeat'.*/
		if (d == 0 || d == DSP_NUM_DIGITS - 1)
			a = a * 2; /* actually 1.414 at cutoff, but a is a square, so we can use 2 */

		/* count how long this digit sustains, also report if it has changed and when */
		if (d != fuenf->rx_digit_last) {
			change = 1;
			change_count = fuenf->rx_digit_count;
			fuenf->rx_digit_last = d;
			fuenf->rx_digit_count = 0;
		} else
			change = 0;
		fuenf->rx_digit_count++;

		/* state machine to detect sequence of 5 tones */
		switch (fuenf->rx_state) {
		case RX_STATE_RESET:
			/* wait for silence */
			if (d >= 0)
				break;
			/* check if we have enought silence */
			if (fuenf->rx_digit_count == RX_MIN_PREAMBLE) {
				PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Detected silence, waiting for digits.\n");
				fuenf->rx_state = RX_STATE_IDLE;
				break;
			}
			break;
		case RX_STATE_IDLE:
			/* wait for digit */
			if (d < 0)
				break;
			PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "We have some tone, start receiving digits.\n");
			fuenf->rx_callsign_count = 0;
			fuenf->rx_callsign[fuenf->rx_callsign_count] = d;
			fuenf->rx_state = RX_STATE_DIGIT;
			break;
		case RX_STATE_DIGIT:
			/* wait for change */
			if (!change) {
				if (fuenf->rx_digit_count == RX_LEN_DIGIT_TH) {
					if (d < 0) {
						PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Not enough digits received, waiting for next transmission.\n");
						fuenf->rx_function = 0;
						fuenf->rx_function_count = 0;
						fuenf->rx_state = RX_STATE_RESET;
						break;
					}
					PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Detected digit #%d (amplitude = %.0f%%)\n", d + 1, sqrt(a) * 100.0);
					display_measurements_update(fuenf->dmp_digit_level, sqrt(a) * 100.0, 0.0);
					break;
				}
				if (fuenf->rx_digit_count == RX_LEN_DIGIT_MAX) {
					PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Detected digit too long, waiting for next transmission.\n");
					fuenf->rx_state = RX_STATE_RESET;
					break;
				}
				break;
			}
			/* if digit did not become stable (changed) during threshold */
			if (change_count < RX_LEN_DIGIT_TH) {
				/* store detected digit and wait for this one to become stable */
				fuenf->rx_callsign[fuenf->rx_callsign_count] = d;
				break;
			}
			/* if counter (when changed) was too low */
			if (change_count < RX_LEN_DIGIT_MIN) {
				PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Detected digit too short, waiting for next transmission.\n");
				fuenf->rx_state = RX_STATE_RESET;
				break;
			}
			/* increment digit and store detected digit */
			fuenf->rx_callsign_count++;
			fuenf->rx_callsign[fuenf->rx_callsign_count] = d;
			/* if 5 tones are received, decode */
			if (fuenf->rx_callsign_count == 5) {
				for (i = 0; i < 5; i++) {
					if (fuenf->rx_callsign[i] == REPEAT_DIGIT) {
						if (i == 0) {
							PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "First digit is a repeat digit, this is not allowed, waiting for next transmission.\n");
							fuenf->rx_state = RX_STATE_RESET;
							break;
						}
						fuenf->rx_callsign[i] = fuenf->rx_callsign[i - 1];
					} else
					if (fuenf->rx_callsign[i] == 9)
						fuenf->rx_callsign[i] = '0';
					else
						fuenf->rx_callsign[i] = '1' + fuenf->rx_callsign[i];
				}
				fuenf->rx_callsign[i] = '\0';
				if (i < 5)
					break;
				PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Complete call sign '%s' received, waiting for signal tone(s).\n", fuenf->rx_callsign);
				fuenf_rx_callsign(fuenf, fuenf->rx_callsign);
				fuenf->rx_function_count = 0; /* must reset, so we can detect timeout */
				fuenf->rx_state = RX_STATE_WAIT_SIGNAL;
				break;
			}
			break;
		default:
			/* tones are not decoded here */
			break;
		}
	}
}

/* receive tones and decode */
static void tone_decode(fuenf_t *fuenf, sample_t *samples, int length)
{
	double levels[DSP_NUM_TONES];
	int tone1 = -1, tone2 = -1;
	enum fuenf_funktion funktion = 0;
	int i;

	/* filter tones */
	audio_goertzel(fuenf->rx_tone_goertzel, samples, length, 0, levels, DSP_NUM_TONES);
	for (i = 0; i < DSP_NUM_TONES; i++)
		fuenf->rx_tone_levels[i] = levels[i] / TONE_LEVEL;

	/* find two frequencies */
	for (i = 0; i < DSP_NUM_TONES; i++) {
		if (fuenf->rx_tone_levels[i] < RX_MIN_LEVEL)
			continue;
		/* accpet only two ones */
		if (tone1 < 0)
			tone1 = i;
		else if (tone2 < 0)
			tone2 = i;
		else {
			/* abort, if more than two tones */
			tone1 = -1;
			tone2 = -1;
			break;
		}
	}

	/* if exactly two tones */
	if (tone2 >= 0) {
		/* select function from signal */
		for (i = 0; i < DSP_NUM_SIGNALS; i++) {
			if (tone1 == signals[i].tone1
			 && tone2 == signals[i].tone2) {
				funktion = signals[i].funktion;
				break;
			}
		}
	}

	fuenf->rx_function_count += length;

	/* state machine to detect two tones */
	switch (fuenf->rx_state) {
	case RX_STATE_WAIT_SIGNAL:
		/* wait for signal */
		if (!funktion) {
			if (fuenf->rx_function_count >= RX_WAIT_TONE_MAX) {
				PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "There is no double tone, waiting for next transmission.\n");
				fuenf->rx_state = RX_STATE_RESET;
				break;
			}
			break;
		}
		/* store signal */
		fuenf->rx_function = funktion;
		fuenf->rx_function_count = 0;
		fuenf->rx_state = RX_STATE_SIGNAL;
		break;
	case RX_STATE_SIGNAL:
		/* if signal ceases too early */
		if (funktion != fuenf->rx_function) {
			PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Signal tones ceased to early, waiting for next transmission.\n");
			fuenf->rx_state = RX_STATE_RESET;
			break;
		}
		if (fuenf->rx_function_count >= RX_LEN_TONE_MIN) {
			PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Detected tones %.0f+%.0f Hz (amplitude = %.0f%%+%.0f%%)\n", tone_freq[tone1], tone_freq[tone2], fuenf->rx_tone_levels[tone1] * 100.0, fuenf->rx_tone_levels[tone2] * 100.0);
			PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Signal tones detected, done, waiting for next transmission.\n");
			fuenf_rx_function(fuenf, fuenf->rx_function);
			fuenf->rx_state = RX_STATE_RESET;
			break;
		}
		break;
	default:
		/* digits are not decoded here */
		break;
	}
}

/* Process received audio stream from radio unit. */
void sender_receive(sender_t *sender, sample_t *samples, int length, double __attribute__((unused)) rf_level_db)
{
	fuenf_t *fuenf = (fuenf_t *) sender;

	if (fuenf->rx) {
		sample_t down[length];
		int count, i;

		/* downsample */
		memcpy(down, samples, sizeof(down)); // copy, so audio will not be corrupted at loopback
		count = samplerate_downsample(&fuenf->sender.srstate, down, length);

		/* decode digit */
		digit_decode(fuenf, down, count);

		/* decode tone */
		for (i = 0; i < count; i++) {
			/* fill buffer and decode when full */
			fuenf->rx_tone_filter_spl[fuenf->rx_tone_filter_pos] = down[i];
			if (++fuenf->rx_tone_filter_pos == fuenf->rx_tone_filter_size) {
				tone_decode(fuenf, fuenf->rx_tone_filter_spl, fuenf->rx_tone_filter_size);
				fuenf->rx_tone_filter_pos = 0;
			}
		}
		/* display levels */
		for (i = 0; i < DSP_NUM_TONES; i++)
			display_measurements_update(fuenf->dmp_tone_levels[i], fuenf->rx_tone_levels[i] * 100.0, 0.0);
	}
}

/* set sequence to send */
int dsp_setup(fuenf_t *fuenf, const char *rufzeichen, enum fuenf_funktion funktion)
{
	tone_seq_t *seq = fuenf->tx_seq;
	int index = 0, tone_index;
	int i;

	fuenf->tx_seq_length = 0;

	if (strlen(rufzeichen) != 5) {
		PDEBUG_CHAN(DDSP, DEBUG_ERROR, "Given call sign has invalid length.\n");
		return -EINVAL;
	}

	PDEBUG_CHAN(DDSP, DEBUG_DEBUG, "Generating sequence for call sign '%s' and function code '%d'.\n", rufzeichen, funktion);

	/* add preamble */
	seq[index].phasestep1 = 0;
	seq[index].phasestep2 = 0;
	seq[index].duration = TX_LEN_PREAMBLE;
	index++;

	/* add tones */
	tone_index = index;
	for (i = 0; rufzeichen[i]; i++) {
		if (rufzeichen[i] < '0' || rufzeichen[i] > '9') {
			PDEBUG_CHAN(DDSP, DEBUG_ERROR, "Given call sign has invalid digit '%c'.\n", rufzeichen[i]);
			return -EINVAL;
		}
		if (rufzeichen[i] == '0')
			seq[index].phasestep1 = 2.0 * M_PI * digit_freq[9] * fuenf->sample_duration;
		else
			seq[index].phasestep1 = 2.0 * M_PI * digit_freq[rufzeichen[i] - '1'] * fuenf->sample_duration;
		/* use repeat digit, if two subsequent digits are the same */
		if (i > 0 && seq[index - 1].phasestep1 == seq[index].phasestep1) {
			seq[index].phasestep1 = 2.0 * M_PI * digit_freq[REPEAT_DIGIT] * fuenf->sample_duration;
			PDEBUG_CHAN(DDSP, DEBUG_DEBUG, " -> Adding digit '%c' as tone with %.0f Hz.\n", rufzeichen[i], digit_freq[REPEAT_DIGIT]);
		} else
			PDEBUG_CHAN(DDSP, DEBUG_DEBUG, " -> Adding digit '%c' as tone with %.0f Hz.\n", rufzeichen[i], digit_freq[rufzeichen[i] - '0']);
		seq[index].phasestep2 = 0;
		seq[index].duration = TX_LEN_DIGIT;
		index++;
	}

	if (funktion != FUENF_FUNKTION_TURBO) {
		/* add pause */
		seq[index].phasestep1 = 0;
		seq[index].phasestep2 = 0;
		seq[index].duration = TX_LEN_PAUSE;
		index++;

		/* add tones (again) */
		for (i = 0; rufzeichen[i]; i++) {
			seq[index].phasestep1 = seq[tone_index + i].phasestep1;
			seq[index].phasestep2 = 0;
			seq[index].duration = TX_LEN_DIGIT;
			index++;
		}

		/* add (second) pause */
		seq[index].phasestep1 = 0;
		seq[index].phasestep2 = 0;
		seq[index].duration = TX_LEN_PAUSE;
		index++;
	}

#ifndef DEBUG
	if (funktion == FUENF_FUNKTION_RUF) {
		PDEBUG_CHAN(DDSP, DEBUG_DEBUG, " -> Adding call signal of %.0f Hz.\n", digit_freq[REPEAT_DIGIT]);
		for (i = 0; i < TX_NUM_KANAL; i++) {
			/* add tone (double volume) */
			seq[index].phasestep1 = 2.0 * M_PI * digit_freq[REPEAT_DIGIT] * fuenf->sample_duration;
			seq[index].phasestep2 = 2.0 * M_PI * digit_freq[REPEAT_DIGIT] * fuenf->sample_duration;
			seq[index].duration = TX_LEN_KANAL;
			index++;

			/* add pause after tone */
			if (i < TX_NUM_KANAL - 1) {
				seq[index].phasestep1 = 0;
				seq[index].phasestep2 = 0;
				seq[index].duration = TX_LEN_KANAL_PAUSE;
				index++;
			}
		}

		/* add postamble */
		seq[index].phasestep1 = 0;
		seq[index].phasestep2 = 0;
		seq[index].duration = TX_LEN_POSTAMBLE;
		index++;
	} else
	if (funktion != FUENF_FUNKTION_TURBO) {
		/* add signal */
		for (i = 0; i < DSP_NUM_SIGNALS; i++) {
			if (signals[i].funktion == funktion)
				break;
		}
		PDEBUG_CHAN(DDSP, DEBUG_DEBUG, " -> Adding call signal of %.0f Hz and %.0f Hz.\n", tone_freq[signals[i].tone1], tone_freq[signals[i].tone2]);
		seq[index].phasestep1 = 2.0 * M_PI * tone_freq[signals[i].tone1] * fuenf->sample_duration;
		seq[index].phasestep2 = 2.0 * M_PI * tone_freq[signals[i].tone2] * fuenf->sample_duration;
		seq[index].duration = TX_LEN_SIGNAL;
		index++;
	}
#endif

	/* check array overflow, if it did not already crashed before */
	if (index > (int)(sizeof(fuenf->tx_seq) / sizeof(fuenf->tx_seq[0]))) {
		PDEBUG_CHAN(DDSP, DEBUG_ERROR, "Array size of tx_seq too small, please fix!\n");
		abort();
	}

	fuenf->tx_funktion = funktion;
	fuenf->tx_seq_length = index;
	fuenf->tx_seq_index = 0;
	fuenf->tx_count = 0.0;

	return index;
}

/* transmit call tone or pause, return 0, if no sequence */
static int encode(fuenf_t *fuenf, sample_t *samples, int length)
{
	tone_seq_t *seq;
	int count = 0;
	double value;

	/* no sequence */
	if (!fuenf->tx_seq_length)
		return 0;

	seq = &fuenf->tx_seq[fuenf->tx_seq_index];

	/* generate wave */
	while (count < length && fuenf->tx_count < seq->duration) {
		value = 0;
		/* reset phase when not sending sine wave */
		if (seq->phasestep1) {
			value += sin(fuenf->tx_phase1);
			fuenf->tx_phase1 += seq->phasestep1;
		} else
			fuenf->tx_phase1 = 0.0;
		if (seq->phasestep2) {
			value += sin(fuenf->tx_phase2);
			fuenf->tx_phase2 += seq->phasestep2;
		} else
			fuenf->tx_phase2 = 0.0;
		fuenf->tx_count += fuenf->sample_duration;
		*samples++ = value * TONE_LEVEL;
		count++;
	}

	/* transition to next segment */
	if (fuenf->tx_count >= seq->duration) {
		fuenf->tx_count -= seq->duration;
		if (++fuenf->tx_seq_index == fuenf->tx_seq_length) {
			fuenf->tx_seq_length = 0;
			fuenf_tx_done(fuenf);
		}

	}

	return count;
}

/* Provide stream of audio toward radio unit */
void sender_send(sender_t *sender, sample_t *samples, uint8_t *power, int length)
{
	fuenf_t *fuenf = (fuenf_t *) sender;
	sample_t *orig_samples = samples;
	int orig_length = length;
	int count;
	sample_t *spl;
	int pos;
	int i;

	/* speak through */
	if (fuenf->state == FUENF_STATE_DURCHSAGE && fuenf->callref) {
		jitter_load(&fuenf->sender.dejitter, samples, length);
		memset(power, 1, length);
	} else {
		/* send if something has to be sent. else turn transmitter off */
		while ((count = encode(fuenf, samples, length))) {
			memset(power, 1, count);
			samples += count;
			power += count;
			length -= count;
		}
		if (length) {
			memset(samples, 0, sizeof(samples) * length);
			memset(power, 0, length);
		}
	}

	/* Also forward audio to network (call process). */
	if (fuenf->callref) {
		sample_t copy_samples[orig_length];
// should we always echo back what we talk through???
#if 0
		if (fuenf->state == FUENF_STATE_DURCHSAGE)
			memset(copy_samples, 0, sizeof(copy_samples));
		else
#endif
			memcpy(copy_samples, orig_samples, sizeof(copy_samples));
		count = samplerate_downsample(&fuenf->sender.srstate, copy_samples, orig_length);
		spl = fuenf->sender.rxbuf;
		pos = fuenf->sender.rxbuf_pos;
		for (i = 0; i < count; i++) {
			spl[pos++] = copy_samples[i];
			if (pos == 160) {
				call_up_audio(fuenf->callref, spl, 160);
				pos = 0;
			}
		}
		fuenf->sender.rxbuf_pos = pos;
	} else
		fuenf->sender.rxbuf_pos = 0;

}

