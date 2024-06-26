/* A-Netz signal processing
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

#define CHAN anetz->sender.kanal

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "../libsample/sample.h"
#include "../liblogging/logging.h"
#include <osmocom/core/timer.h>
#include "../libmobile/call.h"
#include "anetz.h"
#include "dsp.h"

#define PI		3.1415927

/* signaling */
#define MAX_DEVIATION		15000.0
#define MAX_MODULATION		4000.0
#define SPEECH_DEVIATION	10500.0	/* deviation of speech at 1 kHz */
#define TX_PEAK_TONE		(10500.0 / SPEECH_DEVIATION)	/* 10.5 kHz, no emphasis */
#define TX_PEAK_PAGE		(15000.0 / SPEECH_DEVIATION)	/* 15 kHz, no emphasis */
#define MAX_DISPLAY		(15000.0 / SPEECH_DEVIATION)	/* 15 kHz, no emphasis */
#define CHUNK_DURATION		0.010	/* 10 m = 100 Hz bandwidth (-7.6 DB @ +-100 Hz) */
#define TONE_THRESHOLD		0.05
#define QUAL_THRESHOLD		0.5

// FIXME: how long until we detect a tone?
#define TONE_DETECT_TH	8	/* chunk intervals to detect continuous tone */

/* carrier loss detection */
#define MUTE_TIME	0.1	/* time to mute after loosing signal */
#define LOSS_TIME	12.0	/* duration of signal loss before release (what was the actual duration ???) */

/* two signaling tones */
static double fsk_tones[2] = {
	2280.0,
	1750.0,
};

/* table for fast sine generation */
static sample_t dsp_sine_tone[65536];
static sample_t dsp_sine_page[65536];

/* global init for audio processing */
void dsp_init(void)
{
	int i;
	double s;

	LOGP(DDSP, LOGL_DEBUG, "Generating sine tables.\n");
	for (i = 0; i < 65536; i++) {
		s = sin((double)i / 65536.0 * 2.0 * PI);
		dsp_sine_tone[i] = s * TX_PEAK_TONE;
		dsp_sine_page[i] = s * TX_PEAK_PAGE;
	}
}

/* Init transceiver instance. */
int dsp_init_sender(anetz_t *anetz, double page_gain, int page_sequence, double squelch_db)
{
	sample_t *spl;
	int i;
	double tone;

	LOGP_CHAN(DDSP, LOGL_DEBUG, "Init DSP for 'Sender'.\n");

	/* init squelch */
	squelch_init(&anetz->squelch, anetz->sender.kanal, squelch_db, MUTE_TIME, LOSS_TIME);

	/* set modulation parameters */
	sender_set_fm(&anetz->sender, MAX_DEVIATION * page_gain, MAX_MODULATION, SPEECH_DEVIATION, MAX_DISPLAY);

	anetz->page_gain = page_gain;
	anetz->page_sequence = page_sequence;

	anetz->samples_per_chunk = anetz->sender.samplerate * CHUNK_DURATION;
	LOGP(DDSP, LOGL_DEBUG, "Using %d samples per filter chunk duration.\n", anetz->samples_per_chunk);
	spl = calloc(anetz->samples_per_chunk, sizeof(sample_t));
	if (!spl) {
		LOGP(DDSP, LOGL_ERROR, "No memory!\n");
		return -ENOMEM;
	}
	anetz->fsk_filter_spl = spl;

	anetz->tone_detected = -1;

	for (i = 0; i < 2; i++)
		audio_goertzel_init(&anetz->fsk_tone_goertzel[i], fsk_tones[i], anetz->sender.samplerate);
	tone = fsk_tones[(anetz->sender.loopback == 0) ? 0 : 1];
	anetz->tone_phaseshift65536 = 65536.0 / ((double)anetz->sender.samplerate / tone);

	anetz->dmp_tone_level = display_measurements_add(&anetz->sender.dispmeas, "Tone Level", "%.1f %%", DISPLAY_MEAS_LAST, DISPLAY_MEAS_LEFT, 0.0, 150.0, 100.0);
	anetz->dmp_tone_quality = display_measurements_add(&anetz->sender.dispmeas, "Tone Quality", "%.1f %%", DISPLAY_MEAS_LAST, DISPLAY_MEAS_LEFT, 0.0, 100.0, 100.0);

	return 0;
}

/* Cleanup transceiver instance. */
void dsp_cleanup_sender(anetz_t *anetz)
{
	LOGP_CHAN(DDSP, LOGL_DEBUG, "Cleanup DSP for 'Sender'.\n");

	if (anetz->fsk_filter_spl) {
		free(anetz->fsk_filter_spl);
		anetz->fsk_filter_spl = NULL;
	}
}

/* Count duration of tone and indicate detection/loss to protocol handler. */
static void fsk_receive_tone(anetz_t *anetz, int tone, int goodtone, double level, double quality)
{
	/* lost tone because it is not good anymore or has changed */
	if (!goodtone || tone != anetz->tone_detected) {
		if (anetz->tone_count >= TONE_DETECT_TH) {
			LOGP_CHAN(DDSP, LOGL_INFO, "Lost %.0f Hz tone after %.0f ms.\n", fsk_tones[anetz->tone_detected], 1000.0 * CHUNK_DURATION * anetz->tone_count);
			anetz_receive_tone(anetz, -1);
		}
		if (goodtone)
			anetz->tone_detected = tone;
		else
			anetz->tone_detected = -1;
		anetz->tone_count = 0;

		return;
	}

	anetz->tone_count++;

	if (anetz->tone_count == TONE_DETECT_TH) {
		LOGP_CHAN(DDSP, LOGL_INFO, "Detecting continuous %.0f Hz tone. (level = %.0f%%, quality =%.0f%%)\n", fsk_tones[anetz->tone_detected], level * 100.0, quality * 100.0);
		anetz_receive_tone(anetz, anetz->tone_detected);
	}
}

/* Filter one chunk of audio an detect tone and quality of signal. */
static void fsk_decode_chunk(anetz_t *anetz, sample_t *spl, int max)
{
	double level, result[2], quality[2];

	level = audio_mean_level(spl, max);
	/* convert mean (if level comes from a sine curve) to peak value */
	level = level * M_PI / 2.0 / TX_PEAK_TONE;

	audio_goertzel(anetz->fsk_tone_goertzel, spl, max, 0, result, 2);

	/* calculate quality of tones */
	quality[0] = result[0] / level;
	quality[1] = result[1] / level;
	/* show tones */
	display_measurements_update(anetz->dmp_tone_level, level * 100.0, 0.0);
	display_measurements_update(anetz->dmp_tone_quality, quality[1] * 100.0, 0.0);
	if ((level > TONE_THRESHOLD && quality[1] > QUAL_THRESHOLD) || anetz->sender.loopback)
		LOGP_CHAN(DDSP, LOGL_INFO, "Tone %.0f: Level=%3.0f%% Quality=%3.0f%%\n", fsk_tones[1], level * 100.0, quality[1] * 100.0);

	/* adjust level, so we get peak of sine curve */
	/* indicate detected tone */
	if (level > TONE_THRESHOLD && quality[0] > QUAL_THRESHOLD)
		fsk_receive_tone(anetz, 0, 1, level, quality[0]);
	else if (level > TONE_THRESHOLD && quality[1] > QUAL_THRESHOLD)
		fsk_receive_tone(anetz, 1, 1, level, quality[1]);
	else
		fsk_receive_tone(anetz, -1, 0, level, 0.0);
}

/* Process received audio stream from radio unit. */
void sender_receive(sender_t *sender, sample_t *samples, int length, double rf_level_db)
{
	anetz_t *anetz = (anetz_t *) sender;
	sample_t *spl;
	int max, pos;
	int i;

	/* process signal mute/loss, also for signalling tone */
	switch (squelch(&anetz->squelch, rf_level_db, (double)length / (double)anetz->sender.samplerate)) {
	case SQUELCH_LOSS:
		anetz_loss_indication(anetz, LOSS_TIME);
		/* FALLTHRU */
	case SQUELCH_MUTE:
		memset(samples, 0, sizeof(*samples) * length);
		break;
	default:
		break;
	}

	/* write received samples to decode buffer */
	max = anetz->samples_per_chunk;
	pos = anetz->fsk_filter_pos;
	spl = anetz->fsk_filter_spl;
	for (i = 0; i < length; i++) {
		spl[pos++] = samples[i];
		if (pos == max) {
			pos = 0;
			fsk_decode_chunk(anetz, spl, max);
		}
	}
	anetz->fsk_filter_pos = pos;

	/* Forward audio to network (call process). */
	if (anetz->dsp_mode == DSP_MODE_AUDIO && anetz->callref) {
		int count;

		count = samplerate_downsample(&anetz->sender.srstate, samples, length);
		spl = anetz->sender.rxbuf;
		pos = anetz->sender.rxbuf_pos;
		for (i = 0; i < count; i++) {
			spl[pos++] = samples[i];
			if (pos == 160) {
				call_up_audio(anetz->callref, spl, 160);
				pos = 0;
			}
		}
		anetz->sender.rxbuf_pos = pos;
	} else
		anetz->sender.rxbuf_pos = 0;
}

/* Set 4 paging frequencies */
void dsp_set_paging(anetz_t *anetz, double *freq)
{
	int i;

	for (i = 0; i < 4; i++) {
		anetz->paging_phaseshift65536[i] = 65536.0 / ((double)anetz->sender.samplerate / freq[i]);
		anetz->paging_phase65536[i] = 0;
	}
}

/* Generate audio stream of 4 simultanious paging tones. Keep phase for next call of function.
 * Use TX_PEAK_PAGE*page_gain for all tones, which gives peak of 1/4th for each individual tone. */
static void fsk_paging_tone(anetz_t *anetz, sample_t *samples, int length)
{
	double *phaseshift, *phase;
	int i;
	double sample;

	phaseshift = anetz->paging_phaseshift65536;
	phase = anetz->paging_phase65536;

	for (i = 0; i < length; i++) {
		sample	= dsp_sine_page[(uint16_t)phase[0]]
			+ dsp_sine_page[(uint16_t)phase[1]]
			+ dsp_sine_page[(uint16_t)phase[2]]
			+ dsp_sine_page[(uint16_t)phase[3]];
		*samples++ = sample / 4.0 * anetz->page_gain;
		phase[0] += phaseshift[0];
		phase[1] += phaseshift[1];
		phase[2] += phaseshift[2];
		phase[3] += phaseshift[3];
		if (phase[0] >= 65536) phase[0] -= 65536;
		if (phase[1] >= 65536) phase[1] -= 65536;
		if (phase[2] >= 65536) phase[2] -= 65536;
		if (phase[3] >= 65536) phase[3] -= 65536;
	}
}

/* Generate audio stream of 4 sequenced paging tones. Keep phase for next call
 * of function.
 *
 * Use TX_PEAK_PAGE for each tone, that is four times higher per tone.
 *
 * Click removal when changing tones that have individual phase:
 * When tone changes to next tone, a transition of 2ms is performed. The last
 * tone is faded out and the new tone faded in.
 */
static void fsk_paging_tone_sequence(anetz_t *anetz, sample_t *samples, int length, int numspl)
{
	double *phaseshift, *phase;
	int tone, count, transition;

	phaseshift = anetz->paging_phaseshift65536;
	phase = anetz->paging_phase65536;
	tone = anetz->paging_tone;
	count = anetz->paging_count;
	transition = anetz->paging_transition;

	while (length) {
		/* use tone, but during transition of tones, keep phase 0 degrees (high level) until next tone reaches 0 degrees (high level) */
		if (!transition)
			*samples++ = dsp_sine_page[(uint16_t)phase[tone]] * anetz->page_gain;
		else {
			/* fade between old an new tone */
			*samples++
				= (double)dsp_sine_page[(uint16_t)phase[(tone - 1) & 3]] * (double)(transition - count) / (double)transition / 2.0 * anetz->page_gain
				+ (double)dsp_sine_page[(uint16_t)phase[tone]] * (double)count / (double)transition / 2.0 * anetz->page_gain;
		}
		phase[0] += phaseshift[0];
		phase[1] += phaseshift[1];
		phase[2] += phaseshift[2];
		phase[3] += phaseshift[3];
		if (phase[0] >= 65536) phase[0] -= 65536;
		if (phase[1] >= 65536) phase[1] -= 65536;
		if (phase[2] >= 65536) phase[2] -= 65536;
		if (phase[3] >= 65536) phase[3] -= 65536;
		count++;
		if (transition && count == transition) {
			transition = 0;
			/* reset counter again, when transition ends */
			count = 0;
		}
		if (count >= numspl) {
			/* start transition to next tone (lasts 2 ms) */
			transition = anetz->sender.samplerate / 500;
			/* reset counter here, when transition starts */
			count = 0;
			if (++tone == 4)
				tone = 0;
		}
		length--;
	}

	anetz->paging_tone = tone;
	anetz->paging_count = count;
	anetz->paging_transition = transition;
}

/* Generate audio stream from tone. Keep phase for next call of function. */
static void fsk_tone(anetz_t *anetz, sample_t *samples, int length)
{
	double phaseshift, phase;
	int i;

	phaseshift = anetz->tone_phaseshift65536;
	phase = anetz->tone_phase65536;

	for (i = 0; i < length; i++) {
		*samples++ = dsp_sine_tone[(uint16_t)phase];
		phase += phaseshift;
		if (phase >= 65536)
			phase -= 65536;
	}

	anetz->tone_phase65536 = phase;
}

/* Provide stream of audio toward radio unit */
void sender_send(sender_t *sender, sample_t *samples, uint8_t *power, int length)
{
	anetz_t *anetz = (anetz_t *) sender;
	int input_num;

	memset(power, 1, length);

	switch (anetz->dsp_mode) {
	case DSP_MODE_SILENCE:
		memset(samples, 0, length * sizeof(*samples));
		break;
	case DSP_MODE_AUDIO:
		input_num = samplerate_upsample_input_num(&sender->srstate, length);
		{
			int16_t spl[input_num];
			jitter_load_samples(&sender->dejitter, (uint8_t *)spl, input_num, sizeof(*spl), jitter_conceal_s16, NULL);
			int16_to_samples_speech(samples, spl, input_num);
		}
		samplerate_upsample(&sender->srstate, samples, input_num, samples, length);
		break;
	case DSP_MODE_TONE:
		fsk_tone(anetz, samples, length);
		break;
	case DSP_MODE_PAGING:
		if (anetz->page_sequence)
			fsk_paging_tone_sequence(anetz, samples, length, anetz->page_sequence * anetz->sender.samplerate / 1000);
		else
			fsk_paging_tone(anetz, samples, length);
		break;
	}
}

static const char *anetz_dsp_mode_name(enum dsp_mode mode)
{
        static char invalid[16];

	switch (mode) {
	case DSP_MODE_SILENCE:
		return "SILENCE";
	case DSP_MODE_AUDIO:
		return "AUDIO";
	case DSP_MODE_TONE:
		return "TONE";
	case DSP_MODE_PAGING:
		return "PAGING";
	}

	sprintf(invalid, "invalid(%d)", mode);
	return invalid;
}

void anetz_set_dsp_mode(anetz_t *anetz, enum dsp_mode mode, int detect_reset)
{
	LOGP_CHAN(DDSP, LOGL_DEBUG, "DSP mode %s -> %s\n", anetz_dsp_mode_name(anetz->dsp_mode), anetz_dsp_mode_name(mode));
	if (mode == DSP_MODE_AUDIO && anetz->dsp_mode != mode)
		jitter_reset(&anetz->sender.dejitter);

	anetz->dsp_mode = mode;
	/* reset sequence paging */
	anetz->paging_tone = 0;
	anetz->paging_count = 0;
	anetz->paging_transition = 0;
	/* reset tone detector */
	if (detect_reset)
		anetz->tone_detected = -1;
}

/* Receive audio from call instance. */
void call_down_audio(void *decoder, void *decoder_priv, int callref, uint16_t sequence, uint8_t marker, uint32_t timestamp, uint32_t ssrc, uint8_t *payload, int payload_len)
{
	sender_t *sender;
	anetz_t *anetz;

	for (sender = sender_head; sender; sender = sender->next) {
		anetz = (anetz_t *) sender;
		if (anetz->callref == callref)
			break;
	}
	if (!sender)
		return;

	if (anetz->dsp_mode == DSP_MODE_AUDIO) {
		jitter_frame_t *jf;
		jf = jitter_frame_alloc(decoder, decoder_priv, payload, payload_len, marker, sequence, timestamp, ssrc);
		if (jf)
			jitter_save(&anetz->sender.dejitter, jf);
	}
}

void call_down_clock(void) {}

