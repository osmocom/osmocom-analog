/* MTS/IMTS signal processing
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

#define CHAN imts->sender.kanal

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
#include "imts.h"
#include "dsp.h"

/* uncomment to debug decoder */
//#define DEBUG_DECODER

#define PI		3.1415927

/* signaling */
#define MAX_DEVIATION		7000.0				/* signaling tone plus some extra to calibrate */
#define MAX_MODULATION		3000.0				/* FIXME */
#define SPEECH_DEVIATION	2500.0				/* deviation of speech (with emphasis) */
#define TX_PEAK_TONE		(5000.0 / SPEECH_DEVIATION)	/* signaling tone level (5khz, no emphasis) */
#define RX_MIN_AMPL		0.25				/* FIXME: Minimum level to detect tone */
/* Note that 75 is half of the distance between two tones (2000 and 2150 Hz)
 * An error of more than 50 causes too much toggeling between two tones,
 * less would take too long to detect the tone and maybe not detect it, if
 * it is too far off the expected frequency.
 */
#define RX_MIN_FREQ		50.0				/* minimum frequency error to detect tone */
#define MAX_DISPLAY		(MAX_DEVIATION / SPEECH_DEVIATION)/* as much as MAX_DEVIATION */
/* Note that FILTER_BW / SUSTAIN and QUAL_TIME sum up and should not exceed minimum tone length */
#define RX_FILTER_BW		100.0				/* amplitude filter (causes delay) */
#define RX_SUSTAIN		0.010				/* how long a tone must sustain until detected (causes delay) */
#define RX_QUAL_TIME		0.005				/* how long a quality measurement lasts after detecting a tone */

/* carrier loss detection */
#define MUTE_TIME	0.1	/* time to mute after loosing signal */
#define DELAY_TIME	0.15	/* delay, so we don't hear the noise before squelch mutes (MTS mode) */
#define LOSS_TIME	12.0	/* duration of signal loss before release (FIXME: what was the actual duration ???) */
#define SIGNAL_DETECT	0.3	/* time to have a steady signal to detect phone (used by MTS) */

#define DISPLAY_INTERVAL 0.04

/* signaling tones to be modulated */
static double tones[NUM_SIG_TONES] = {
	2150.0,		/* guard */
	2000.0,		/* idle */
	1800.0,		/* seize */
	1633.0,		/* connect */
	1336.0,		/* disconnect */
	600.0,		/* MTS 600 Hz */
	1500.0,		/* MTS 1500 Hz */
};

/* signaling tones to be demodulated (a subset of tones above) */
static double tone_response[NUM_SIG_TONES] = {
	0.71,		/* guard */
	1.08,		/* idle */
	1.01,		/* seize */
	1.04,		/* connect */
	0.71,		/* disconnect */
	0.71,		/* MTS 600 Hz */
	0.71,		/* MTS 1500 Hz */
};

/* all tone, with signaling tones first */
static const char *tone_names[] = {
	"GUARD tone",
	"IDLE tone",
	"SEIZE tone",
	"CONNECT tone",
	"DISCONNECT tone",
	"600 Hz Tone",
	"1500 Hz Tone",
	"SILENCE",
	"NOISE",
	"DIALTONE",
};

#define DIALTONE1	350.0
#define DIALTONE2	440.0

/* table for fast sine generation */
static sample_t dsp_sine_tone[65536];

/* global init for audio processing */
void dsp_init(void)
{
	int i;
	double s;

	LOGP(DDSP, LOGL_DEBUG, "Generating sine tables.\n");
	for (i = 0; i < 65536; i++) {
		s = sin((double)i / 65536.0 * 2.0 * PI);
		dsp_sine_tone[i] = s * TX_PEAK_TONE;
	}
}

/* Init transceiver instance. */
int dsp_init_transceiver(imts_t *imts, double squelch_db, int ptt)
{
	int rc = -1;

	LOGP_CHAN(DDSP, LOGL_DEBUG, "Init DSP for Transceiver.\n");

	imts->sample_duration = 1.0 / (double)imts->sender.samplerate;

	/* init squelch */
	squelch_init(&imts->squelch, imts->sender.kanal, squelch_db, MUTE_TIME, LOSS_TIME);

	/* set modulation parameters */
	sender_set_fm(&imts->sender, MAX_DEVIATION, MAX_MODULATION, SPEECH_DEVIATION, MAX_DISPLAY);

	/* init FM demodulator for tone detection */
	if (imts->mode == MODE_IMTS) {
		imts->demod_center = (tones[TONE_GUARD] + tones[TONE_DISCONNECT]) / 2.0;
		imts->demod_bandwidth = tones[TONE_GUARD] - tones[TONE_DISCONNECT];
	} else {
		imts->demod_center = (tones[TONE_1500] + tones[TONE_600]) / 2.0;
		imts->demod_bandwidth = tones[TONE_1500] - tones[TONE_600];
	}
	rc = fm_demod_init(&imts->demod, (double)imts->sender.samplerate, imts->demod_center, imts->demod_bandwidth);
	if (rc < 0)
		goto error;
	/* use fourth order (2 iter) filter, since it is as fast as second order (1 iter) filter */
	/* NOTE: CHANGE TONE RESPONSES ABOVE, IF YOU CHANGE FILTER */
	iir_lowpass_init(&imts->demod_freq_lp, RX_FILTER_BW, (double)imts->sender.samplerate, 2);
	iir_lowpass_init(&imts->demod_ampl_lp, RX_FILTER_BW, (double)imts->sender.samplerate, 2);

	/* tones */
	imts->tone_idle_phaseshift65536 = 65536.0 / ((double)imts->sender.samplerate / tones[TONE_IDLE]);
	imts->tone_seize_phaseshift65536 = 65536.0 / ((double)imts->sender.samplerate / tones[TONE_SEIZE]);
	imts->tone_600_phaseshift65536 = 65536.0 / ((double)imts->sender.samplerate / tones[TONE_600]);
	imts->tone_1500_phaseshift65536 = 65536.0 / ((double)imts->sender.samplerate / tones[TONE_1500]);
	imts->tone_dialtone_phaseshift65536[0] = 65536.0 / ((double)imts->sender.samplerate / DIALTONE1);
	imts->tone_dialtone_phaseshift65536[1] = 65536.0 / ((double)imts->sender.samplerate / DIALTONE2);

	/* demod init */
	imts->demod_current_tone = TONE_SILENCE;
	imts->demod_sig_tone = 0;
	imts->demod_last_tone = TONE_SILENCE;

	/* delay buffer */
	if (ptt) {
		LOGP_CHAN(DDSP, LOGL_DEBUG, "Push to talk: Adding delay buffer to remove noise when signal gets lost.\n");
		imts->delay_max = (int)((double)imts->sender.samplerate * DELAY_TIME);
		imts->delay_spl = calloc(imts->delay_max, sizeof(*imts->delay_spl));
		if (!imts->delay_spl) {
			LOGP(DDSP, LOGL_ERROR, "No mem for delay buffer!\n");
			goto error;
		}
	}

	imts->dmp_tone_level = display_measurements_add(&imts->sender.dispmeas, "Tone Level", "%.1f %%", DISPLAY_MEAS_LAST, DISPLAY_MEAS_LEFT, 0.0, 150.0, 100.0);
	imts->dmp_tone_quality = display_measurements_add(&imts->sender.dispmeas, "Tone Quality", "%.1f %%", DISPLAY_MEAS_LAST, DISPLAY_MEAS_LEFT, 0.0, 150.0, 100.0);

	return 0;

error:
	dsp_cleanup_transceiver(imts);
	return rc;
}

/* Cleanup transceiver instance. */
void dsp_cleanup_transceiver(imts_t *imts)
{
	LOGP_CHAN(DDSP, LOGL_DEBUG, "Cleanup DSP for Transceiver.\n");

	fm_demod_exit(&imts->demod);
	if (imts->delay_spl) {
		free(imts->delay_spl);
		imts->delay_spl = NULL;
	}
}

/* Generate audio stream from tone. Keep phase for next call of function. */
static int generate_tone(imts_t *imts, sample_t *samples, int length)
{
	double phaseshift[2] = {0,0}, phase[2]; // make gcc happy
	int i;

	switch (imts->tone) {
	case TONE_IDLE:
		phaseshift[0] = imts->tone_idle_phaseshift65536;
		break;
	case TONE_SEIZE:
		phaseshift[0] = imts->tone_seize_phaseshift65536;
		break;
	case TONE_600:
		phaseshift[0] = imts->tone_600_phaseshift65536;
		break;
	case TONE_1500:
		phaseshift[0] = imts->tone_1500_phaseshift65536;
		break;
	case TONE_DIALTONE:
		phaseshift[0] = imts->tone_dialtone_phaseshift65536[0];
		phaseshift[1] = imts->tone_dialtone_phaseshift65536[1];
		break;
	case TONE_SILENCE:
		break;
	default:
		LOGP_CHAN(DDSP, LOGL_ERROR, "Software error, unsupported tone, please fix!\n");
		return length;
	}

	/* don't send more than given length */
	if (imts->tone_duration && length > imts->tone_duration)
		length = imts->tone_duration;

	phase[0] = imts->tone_phase65536[0];
	phase[1] = imts->tone_phase65536[1];

	switch (imts->tone) {
	case TONE_SILENCE:
		memset(samples, 0, length * sizeof(*samples));
		phase[0] = phase[1] = 0;
		break;
	case TONE_DIALTONE:
		for (i = 0; i < length; i++) {
			*samples = dsp_sine_tone[(uint16_t)phase[0]];
			*samples += dsp_sine_tone[(uint16_t)phase[1]];
			*samples++ /= 4.0; /* not full volume */
			phase[0] += phaseshift[0];
			if (phase[0] >= 65536)
				phase[0] -= 65536;
			phase[1] += phaseshift[1];
			if (phase[1] >= 65536)
				phase[1] -= 65536;
		}
		break;
	default:
		for (i = 0; i < length; i++) {
			*samples++ = dsp_sine_tone[(uint16_t)phase[0]];
			phase[0] += phaseshift[0];
			if (phase[0] >= 65536)
				phase[0] -= 65536;
		}
	}

	imts->tone_phase65536[0] = phase[0];
	imts->tone_phase65536[1] = phase[1];

	/* if tone has been sent completely, tell IMTS call process */
	if (imts->tone_duration) {
		imts->tone_duration -= length;
		if (imts->tone_duration == 0)
			imts_tone_sent(imts, imts->tone);
	}

	return length;
}

/* Provide stream of audio toward radio unit */
void sender_send(sender_t *sender, sample_t *samples, uint8_t *power, int length)
{
	imts_t *imts = (imts_t *) sender;
	int count, input_num;

	memset(power, 1, length);

again:
	switch (imts->dsp_mode) {
	case DSP_MODE_OFF:
		memset(power, 0, length);
		memset(samples, 0, length * sizeof(*samples));
		break;
	case DSP_MODE_TONE:
		memset(power, 1, length);
		count = generate_tone(imts, samples, length);
		samples += count;
		length -= count;
		if (length)
			goto again;
		break;
	case DSP_MODE_AUDIO:
		memset(power, 1, length);
		input_num = samplerate_upsample_input_num(&sender->srstate, length);
		{
			int16_t spl[input_num];
			jitter_load_samples(&sender->dejitter, (uint8_t *)spl, input_num, sizeof(*spl), jitter_conceal_s16, NULL);
			int16_to_samples_speech(samples, spl, input_num);
		}
		samplerate_upsample(&sender->srstate, samples, input_num, samples, length);
		if (imts->pre_emphasis)
			pre_emphasis(&imts->estate, samples, length);
		break;
	}
}

static void tone_demod(imts_t *imts, sample_t *samples, int length)
{
	sample_t frequency[length], amplitude[length];
	sample_t I[length], Q[length];
	double f, amp;
	int i, t, tone = 0; // make GCC happy

	/* demod frquency */
	fm_demodulate_real(&imts->demod, frequency, length, samples, I, Q);
	iir_process(&imts->demod_freq_lp, frequency, length);

	/* demod amplitude
	 * peak amplitude is the length of I/Q vector
	 * since we filter out the unwanted modulation product, the vector is only half of length
	 */
	for (i = 0; i < length; i++)
		amplitude[i] = sqrt(I[i] * I[i] + Q[i] * Q[i]) * 2.0;
	iir_process(&imts->demod_ampl_lp, amplitude, length);

	/* check result to detect tone or loss or change */
	for (i = 0; i < length; i++) {
		/* duration until change in tone */
		imts->demod_duration += imts->sample_duration;
		/* correct FSK amplitude */
		amplitude[i] /= TX_PEAK_TONE;
		/* see what we detect at this moment; tone is set */
		if (amplitude[i] < RX_MIN_AMPL) {
			/* silence */
			tone = TONE_SILENCE;
		} else {
			/* tone */
			f = frequency[i] + imts->demod_center;
			if (imts->mode == MODE_IMTS) {
				for (t = TONE_IDLE; t <= TONE_DISCONNECT; t++) {
					/* check for frequency */
					if (fabs(f - tones[t]) < RX_MIN_FREQ) {
						tone = t;
						break;
					}
				}
			} else {
				for (t = TONE_600; t <= TONE_1500; t++) {
					/* check for frequency */
					if (fabs(f - tones[t]) < RX_MIN_FREQ) {
						tone = t;
						break;
					}
				}
			}
			/* noise */
			if (t == NUM_SIG_TONES)
				tone = TONE_NOISE;
		}
#ifdef DEBUG_DECODER
		/* debug decoder */
		if (tone < NUM_SIG_TONES) {
			static int debug_interval = 0;
			if (++debug_interval == 30) {
				debug_interval = 0;
				printf("decoder debug: diff=%s %.0f  ", debug_amplitude((f - tones[t]) / RX_MIN_FREQ), fabs(f - tones[t]));
				amp = amplitude[i] / tone_response[t];
				printf("ampl=%s %.0f%%\n", debug_amplitude(amp), amp * 100.0);
			}
		}
#endif
		/* display level of tones, or zero at noise/silence */
		imts->display_interval +=imts->sample_duration;
		if (imts->display_interval >= DISPLAY_INTERVAL) {
			if (tone < NUM_SIG_TONES)
				amp = amplitude[i] / tone_response[tone];
			else
				amp = 0.0;
			display_measurements_update(imts->dmp_tone_level, amp * 100.0, 0.0);
			imts->display_interval -= DISPLAY_INTERVAL;
		}
		/* check for tone change */
		if (tone != imts->demod_current_tone) {
#ifdef DEBUG_DECODER
			printf("decoder debug: %s detected, waiting to sustain\n", tone_names[tone]);
#endif
			if (imts->demod_sig_tone) {
				LOGP_CHAN(DDSP, LOGL_DEBUG, "Lost %s (duration %.0f ms)\n", tone_names[imts->demod_current_tone], imts->demod_duration * 1000.0);
				imts_lost_tone(imts, imts->demod_current_tone, imts->demod_duration);
				imts->demod_sig_tone = 0;
			}
			imts->demod_current_tone = tone;
			imts->demod_sustain = 0.0;
		} else if (imts->demod_sustain < RX_SUSTAIN) {
			imts->demod_sustain += imts->sample_duration;
			/* when sustained. also tone must change to prevent flapping; lost signaling tone can be detected again */
			if (imts->demod_sustain >= RX_SUSTAIN && (imts->demod_current_tone != imts->demod_last_tone || !imts->demod_sig_tone)) {
				if (imts->demod_current_tone < NUM_SIG_TONES) {
					amp = amplitude[i] / tone_response[imts->demod_current_tone];
					imts->demod_sig_tone = 1;
					imts->demod_quality_time = 0.0;
					imts->demod_quality_count = 0;
					imts->demod_quality_value = 0.0;
				} else {
					amp = amplitude[i];
					imts->demod_sig_tone = 0;
				}
				LOGP_CHAN(DDSP, LOGL_DEBUG, "Detected %s (level %.0f%%)\n", tone_names[imts->demod_current_tone], amp * 100);
				imts_receive_tone(imts, imts->demod_current_tone, imts->demod_duration, amp);
				imts->demod_last_tone = imts->demod_current_tone;
				imts->demod_duration = imts->demod_sustain;
			} else if (imts->demod_sig_tone && imts->demod_quality_time < RX_QUAL_TIME) {
				f = frequency[i] + imts->demod_center;
				imts->demod_quality_time += imts->sample_duration;
				imts->demod_quality_count++;
				imts->demod_quality_value += fabs((f - tones[imts->demod_current_tone])) / RX_MIN_FREQ;
				if (imts->demod_quality_time >= RX_QUAL_TIME) {
					double quality = 1.0 - imts->demod_quality_value / (double)imts->demod_quality_count * 2.0;
					if (quality < 0)
						quality = 0;
					LOGP_CHAN(DDSP, LOGL_DEBUG, "Quality: %.0f%%\n", quality * 100.0);
					display_measurements_update(imts->dmp_tone_quality, quality * 100.0, 0.0);
				}
			}	
		}
	}
}

static void delay_audio(imts_t *imts, sample_t *samples, int count)
{
	sample_t *spl, s;
	int pos, max;
	int i;

	spl = imts->delay_spl;
	pos = imts->delay_pos;
	max = imts->delay_max;

	/* feed audio though delay buffer */
	for (i = 0; i < count; i++) {
		s = samples[i];
		samples[i] = spl[pos];
		spl[pos] = s;
		if (++pos == max)
			pos = 0;
	}

	imts->delay_pos = pos;
}

/* Process received audio stream from radio unit. */
void sender_receive(sender_t *sender, sample_t *samples, int length, double rf_level_db)
{
	imts_t *imts = (imts_t *) sender;

	/* no processing if off */
	if (imts->dsp_mode == DSP_MODE_OFF)
		return;

	/* process signal mute/loss, also for signalling tone */
	switch (squelch(&imts->squelch, rf_level_db, (double)length * imts->sample_duration)) {
	case SQUELCH_LOSS:
		imts_loss_indication(imts, LOSS_TIME);
		/* FALLTHRU */
	case SQUELCH_MUTE:
		if (imts->mode == MODE_MTS && !imts->is_mute) {
			LOGP_CHAN(DDSP, LOGL_INFO, "Low RF level, muting.\n");
			memset(imts->delay_spl, 0, sizeof(*samples) * imts->delay_max);
			imts->is_mute = 1;
		}
		memset(samples, 0, sizeof(*samples) * length);
		/* signal is gone */
		imts->rf_signal = 0;
		break;
	default:
		if (imts->is_mute) {
			LOGP_CHAN(DDSP, LOGL_INFO, "High RF level, unmuting.\n");
			imts->is_mute = 0;
		}
		/* detect signal, if it is steady for a while */
		if (imts->rf_signal < SIGNAL_DETECT * (double)imts->sender.samplerate) {
			/* only do this, if signal has not been detected yet */
			imts->rf_signal += length;
			if (imts->rf_signal >= SIGNAL_DETECT * (double)imts->sender.samplerate)
				imts_signal_indication(imts);
		}
		break;
	}

	/* FM/AM demod */
	tone_demod(imts, samples, length);

	/* delay audio to prevent noise before squelch mutes (don't do that for signaling tones) */
	if (imts->delay_spl)
		delay_audio(imts, samples, length);

	/* Forward audio to network (call process). */
	if (imts->dsp_mode == DSP_MODE_AUDIO && imts->callref) {
		sample_t *spl;
		int pos;
		int count;
		int i;

		if (imts->de_emphasis) {
			dc_filter(&imts->estate, samples, length);
			de_emphasis(&imts->estate, samples, length);
		}
		count = samplerate_downsample(&imts->sender.srstate, samples, length);
		spl = imts->sender.rxbuf;
		pos = imts->sender.rxbuf_pos;
		for (i = 0; i < count; i++) {
			spl[pos++] = samples[i];
			if (pos == 160) {
				call_up_audio(imts->callref, spl, 160);
				pos = 0;
			}
		}
		imts->sender.rxbuf_pos = pos;
	} else
		imts->sender.rxbuf_pos = 0;

}

const char *imts_dsp_mode_name(enum dsp_mode mode)
{
        static char invalid[16];

	switch (mode) {
	case DSP_MODE_OFF:
		return "TRX OFF";
	case DSP_MODE_TONE:
		return "TONE";
	case DSP_MODE_AUDIO:
		return "AUDIO";
	}

	sprintf(invalid, "invalid(%d)", mode);
	return invalid;
}

void imts_set_dsp_mode(imts_t *imts, enum dsp_mode mode, int tone, double duration, int reset_demod)
{
	/* reset demod */
	if (reset_demod) {
		imts->demod_current_tone = TONE_SILENCE;
		imts->demod_sig_tone = 0;
		imts->demod_last_tone = TONE_SILENCE;
		imts->demod_duration = 0.0;
	}

	if (mode == DSP_MODE_AUDIO && imts->dsp_mode != mode)
		jitter_reset(&imts->sender.dejitter);
	if (imts->dsp_mode != mode) {
		LOGP_CHAN(DDSP, LOGL_DEBUG, "DSP mode %s -> %s\n", imts_dsp_mode_name(imts->dsp_mode), imts_dsp_mode_name(mode));
		imts->dsp_mode = mode;
	}

	if (mode == DSP_MODE_TONE) {
		if (duration)
			LOGP_CHAN(DDSP, LOGL_DEBUG, "Start sending %s for %.3f seconds.\n", tone_names[tone], duration);
		else
			LOGP_CHAN(DDSP, LOGL_DEBUG, "Start sending %s continuously.\n", tone_names[tone]);
		imts->tone = tone;
		imts->tone_duration = duration * (double)imts->sender.samplerate;
	}
}

/* Receive audio from call instance. */
void call_down_audio(void *decoder, void *decoder_priv, int callref, uint16_t sequence, uint8_t marker, uint32_t timestamp, uint32_t ssrc, uint8_t *payload, int payload_len)
{
	sender_t *sender;
	imts_t *imts;

	for (sender = sender_head; sender; sender = sender->next) {
		imts = (imts_t *) sender;
		if (imts->callref == callref)
			break;
	}
	if (!sender)
		return;

	if (imts->dsp_mode == DSP_MODE_AUDIO) {
		jitter_frame_t *jf;
		jf = jitter_frame_alloc(decoder, decoder_priv, payload, payload_len, marker, sequence, timestamp, ssrc);
		if (jf)
			jitter_save(&imts->sender.dejitter, jf);
	}
}

void call_down_clock(void) {}

