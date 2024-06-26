/* NMT audio processing
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

#define CHAN nmt->sender.kanal

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "../libsample/sample.h"
#include "../liblogging/logging.h"
#include "nmt.h"
#include "transaction.h"
#include "dsp.h"

#define PI			M_PI

/* Notes on TX_PEAK_FSK level:
 *
 * This deviation is -2.2db below the speech deviation.
 *
 * At 1800 Hz the deviation shall be 4.2 kHz, so with emphasis the deviation
 * at 1000 Hz would be theoretically 2.333 kHz. This is factor 0.777 below
 * 3 kHz deviation we want at speech.
 */

/* Notes on TX_PEAK_SUPER (supervisory signal) level:
 *
 * This level has 0.3 kHz deviation at 4015 Hz.
 *
 * Same calculation as above, but now we want 0.3 kHz deviation after emphasis,
 * so we calculate what we would need at 1000 Hz in relation to 3 kHz
 * deviation.
 */

/* signaling */
#define MAX_DEVIATION		4700.0
#define MAX_MODULATION		4055.0
#define SPEECH_DEVIATION	3000.0	/* deviation of speech at 1 kHz */
#define TX_PEAK_FSK		(4200.0 / 1800.0 * 1000.0 / SPEECH_DEVIATION)
#define TX_PEAK_SUPER		(300.0 / 4015.0 * 1000.0 / SPEECH_DEVIATION)
#define BIT_RATE		1200.0
#define BIT_ADJUST		0.1	/* how much do we adjust bit clock on frequency change */
#define F0			1800.0
#define F1			1200.0
#define MAX_DISPLAY		1.4	/* something above speech level */
#define DIALTONE_HZ		425.0	/* dial tone frequency */
#define TX_PEAK_DIALTONE	1.0	/* dial tone peak FIXME: Not found in the specs! */
#define SUPER_BANDWIDTH		30.0	/* distance between two SAT tones, also bandwidth for goertzel filter */
#define SUPER_PRINT		2	/* print supervisory signal measurement every 0.5 seconds */
#define SUPER_LOST_COUNT	4	/* number of measures to loose supervisory signal */
#define SUPER_DETECT_COUNT	6	/* number of measures to detect supervisory signal */
#define MUTE_DURATION		0.280	/* a tiny bit more than two frames */

/* two supervisory tones */
static double super_freq[5] = {
	3955.0, /* 0-Signal 1 */
	3985.0, /* 0-Signal 2 */
	4015.0, /* 0-Signal 3 */
	4045.0, /* 0-Signal 4 */
	3895.0, /* noise level to check against */
};

/* table for fast sine generation */
static sample_t dsp_sine_super[65536];
static sample_t dsp_sine_dialtone[65536];

/* global init for dsp */
void dsp_init(void)
{
	int i;
	double s;

	LOGP(DDSP, LOGL_DEBUG, "Generating sine table for supervisory signal and dial tone.\n");
	for (i = 0; i < 65536; i++) {
		s = sin((double)i / 65536.0 * 2.0 * PI);
		/* supervisor sine */
		dsp_sine_super[i] = s * TX_PEAK_SUPER;
		/* dialtone sine */
		dsp_sine_dialtone[i] = s * TX_PEAK_DIALTONE;
	}

	compandor_init();
}

static int fsk_send_bit(void *inst);
static void fsk_receive_bit(void *inst, int bit, double quality, double level);

/* Init FSK of transceiver */
int dsp_init_sender(nmt_t *nmt, double deviation_factor)
{
	sample_t *spl;
	int i;

	/* attack (3ms) and recovery time (13.5ms) according to NMT specs */
	setup_compandor(&nmt->cstate, 8000, 3.0, 13.5);

	LOGP_CHAN(DDSP, LOGL_DEBUG, "Init DSP for Transceiver.\n");

	/* set modulation parameters */
	sender_set_fm(&nmt->sender, MAX_DEVIATION * deviation_factor, MAX_MODULATION * deviation_factor, SPEECH_DEVIATION * deviation_factor, MAX_DISPLAY);

	LOGP(DDSP, LOGL_DEBUG, "Using FSK level of %.3f (%.3f KHz deviation @ 1500 Hz)\n", TX_PEAK_FSK * deviation_factor, 3.5 * deviation_factor);
	LOGP(DDSP, LOGL_DEBUG, "Using Supervisory level of %.3f (%.3f KHz deviation @ 4015 Hz)\n", TX_PEAK_SUPER * deviation_factor, 0.3 * deviation_factor);

	/* init fsk */
	if (fsk_mod_init(&nmt->fsk_mod, nmt, fsk_send_bit, nmt->sender.samplerate, BIT_RATE, F0, F1, TX_PEAK_FSK, 1, 0) < 0) {
		LOGP_CHAN(DDSP, LOGL_ERROR, "FSK init failed!\n");
		return -EINVAL;
	}
	if (fsk_demod_init(&nmt->fsk_demod, nmt, fsk_receive_bit, nmt->sender.samplerate, BIT_RATE, F0, F1, BIT_ADJUST) < 0) {
		LOGP_CHAN(DDSP, LOGL_ERROR, "FSK init failed!\n");
		return -EINVAL;
	}

	/* allocate ring buffer for SAT signal detection
	 * the bandwidth of the Goertzel filter is the reciprocal of the duration
	 * we half our bandwidth, so that other supervisory signals will be canceled out completely by goertzel filter
	 */
	nmt->super_samples = (int)((double)nmt->sender.samplerate * (1.0 / (SUPER_BANDWIDTH / 2)) + 0.5);
	spl = calloc(1, nmt->super_samples * sizeof(*spl));
	if (!spl) {
		LOGP(DDSP, LOGL_ERROR, "No memory!\n");
		return -ENOMEM;
	}
	nmt->super_filter_spl = spl;

	/* count supervidory tones */
	for (i = 0; i < 5; i++) {
		audio_goertzel_init(&nmt->super_goertzel[i], super_freq[i], nmt->sender.samplerate);
		if (i < 4)
			nmt->super_phaseshift65536[i] = 65536.0 / ((double)nmt->sender.samplerate / super_freq[i]);
	}
	super_reset(nmt);

	/* dial tone */
	nmt->dial_phaseshift65536 = 65536.0 / ((double)nmt->sender.samplerate / DIALTONE_HZ);

	/* dtmf, generate tone relative to speech level */
	dtmf_encode_init(&nmt->dtmf, 8000, 1.0 / SPEECH_LEVEL);

	nmt->dmp_frame_level = display_measurements_add(&nmt->sender.dispmeas, "Frame Level", "%.1f %% (last)", DISPLAY_MEAS_LAST, DISPLAY_MEAS_LEFT, 0.0, 150.0, 100.0);
	nmt->dmp_frame_quality = display_measurements_add(&nmt->sender.dispmeas, "Frame Quality", "%.1f %% (last)", DISPLAY_MEAS_LAST, DISPLAY_MEAS_LEFT, 0.0, 100.0, 100.0);
	if (nmt->sysinfo.chan_type == CHAN_TYPE_TC || nmt->sysinfo.chan_type == CHAN_TYPE_AC_TC || nmt->sysinfo.chan_type == CHAN_TYPE_CC_TC) {
		nmt->dmp_super_level = display_measurements_add(&nmt->sender.dispmeas, "Super Level", "%.1f %%", DISPLAY_MEAS_AVG, DISPLAY_MEAS_LEFT, 0.0, 150.0, 100.0);
		nmt->dmp_super_quality = display_measurements_add(&nmt->sender.dispmeas, "Super Quality", "%.1f %%", DISPLAY_MEAS_AVG, DISPLAY_MEAS_LEFT, 0.0, 100.0, 100.0);
	}

	return 0;
}

/* Cleanup transceiver instance. */
void dsp_cleanup_sender(nmt_t *nmt)
{
	LOGP_CHAN(DDSP, LOGL_DEBUG, "Cleanup DSP for Transceiver.\n");

	fsk_mod_cleanup(&nmt->fsk_mod);
	fsk_demod_cleanup(&nmt->fsk_demod);

	if (nmt->super_filter_spl) {
		free(nmt->super_filter_spl);
		nmt->super_filter_spl = NULL;
	}
}

/* Check for SYNC bits, then collect data bits */
static void fsk_receive_bit(void *inst, int bit, double quality, double level)
{
	nmt_t *nmt = (nmt_t *)inst;
	uint64_t frames_elapsed;
	int i;

	/* normalize FSK level */
	level /= TX_PEAK_FSK;

	nmt->rx_bits_count++;

	if (nmt->trans && nmt->trans->dms_call)
		fsk_receive_bit_dms(nmt, bit, quality, level);

//	printf("bit=%d quality=%.4f\n", bit, quality);
	if (!nmt->rx_in_sync) {
		nmt->rx_sync = (nmt->rx_sync << 1) | bit;

		/* level and quality */
		nmt->rx_level[nmt->rx_count & 0xff] = level;
		nmt->rx_quality[nmt->rx_count & 0xff] = quality;
		nmt->rx_count++;

		/* check if pattern 1010111100010010 matches */
		if (nmt->rx_sync != 0xaf12)
			return;

		/* average level and quality */
		level = quality = 0;
		for (i = 0; i < 16; i++) {
			level += nmt->rx_level[(nmt->rx_count - 1 - i) & 0xff];
			quality += nmt->rx_quality[(nmt->rx_count - 1 - i) & 0xff];
		}
		level /= 16.0; quality /= 16.0;
//		printf("sync (level = %.2f, quality = %.2f\n", level, quality);

		/* do not accept garbage */
		if (quality < 0.65)
			return;

		/* sync time */
		nmt->rx_bits_count_last = nmt->rx_bits_count_current;
		nmt->rx_bits_count_current = nmt->rx_bits_count - 26.0;

		/* rest sync register */
		nmt->rx_sync = 0;
		nmt->rx_in_sync = 1;
		nmt->rx_count = 0;

		/* set muting of receive path */
		nmt->rx_mute = (int)((double)nmt->sender.samplerate * MUTE_DURATION);
		return;
	}

	/* read bits */
	nmt->rx_frame[nmt->rx_count] = bit + '0';
	nmt->rx_level[nmt->rx_count] = level;
	nmt->rx_quality[nmt->rx_count] = quality;
	if (++nmt->rx_count != 140)
		return;

	/* end of frame */
	nmt->rx_frame[140] = '\0';
	nmt->rx_in_sync = 0;

	/* average level and quality */
	level = quality = 0;
	for (i = 0; i < 140; i++) {
		level += nmt->rx_level[i];
		quality += nmt->rx_quality[i];
	}
	level /= 140.0; quality /= 140.0;

	/* update measurements */
	display_measurements_update(nmt->dmp_frame_level, level * 100.0, 0.0);
	display_measurements_update(nmt->dmp_frame_quality, quality * 100.0, 0.0);

	/* send telegramm */
	frames_elapsed = (nmt->rx_bits_count_current - nmt->rx_bits_count_last + 83) / 166; /* round to nearest frame */
	/* convert level so that received level at TX_PEAK_FSK results in 1.0 (100%) */
	nmt_receive_frame(nmt, nmt->rx_frame, quality, level, frames_elapsed);
}

/* compare supervisory signal against noise floor around 3895 Hz */
static void super_decode(nmt_t *nmt, sample_t *samples, int length)
{
	double result[2], level, quality;

	audio_goertzel(&nmt->super_goertzel[nmt->supervisory - 1], samples, length, 0, &result[0], 1);
	audio_goertzel(&nmt->super_goertzel[4], samples, length, 0, &result[1], 1); /* noise floor detection */

	/* normalize supervisory level */
	level = result[0] / TX_PEAK_SUPER;

	quality = (result[0] - result[1]) / result[0];
	if (quality < 0)
		quality = 0;

	if (nmt->state == STATE_ACTIVE) {
		if (++nmt->super_print == SUPER_PRINT) {
			nmt->super_print = 0;
			LOGP_CHAN(DDSP, LOGL_NOTICE, "Supervisory level %.0f%% quality %.0f%%\n", level * 100.0, quality * 100.0);
		}
		/* update measurements (if dmp_* params are NULL, we omit this) */
		display_measurements_update(nmt->dmp_super_level, level * 100.0, 0.0);
		display_measurements_update(nmt->dmp_super_quality, quality * 100.0, 0.0);
	}

	if (quality > 0.7) {
		if (nmt->super_detected == 0) {
			nmt->super_detect_count++;
			if (nmt->super_detect_count == SUPER_DETECT_COUNT) {
				nmt->super_detected = 1;
				nmt->super_detect_count = 0;
				LOGP_CHAN(DDSP, LOGL_DEBUG, "Supervisory signal detected with level=%.0f%%, quality=%.0f%%.\n", result[0] / 0.63662 / TX_PEAK_SUPER * 100.0, quality * 100.0);
				nmt_rx_super(nmt, 1, quality);
			}
		} else
			nmt->super_detect_count = 0;
	} else {
		if (nmt->super_detected == 1) {
			nmt->super_detect_count++;
			if (nmt->super_detect_count == SUPER_LOST_COUNT) {
				nmt->super_detected = 0;
				nmt->super_detect_count = 0;
				LOGP_CHAN(DDSP, LOGL_DEBUG, "Supervisory signal lost.\n");
				nmt_rx_super(nmt, 0, 0.0);
			}
		} else
			nmt->super_detect_count = 0;
	}
}

/* Reset supervisory detection states, so ongoing tone will be detected again. */
void super_reset(nmt_t *nmt)
{
	LOGP_CHAN(DDSP, LOGL_DEBUG, "Supervisory detector reset.\n");
	nmt->super_detected = 0;
	nmt->super_detect_count = 0;
}

/* Process received audio stream from radio unit. */
void sender_receive(sender_t *sender, sample_t *samples, int length, double __attribute__((unused)) rf_level_db)
{
	nmt_t *nmt = (nmt_t *) sender;
	sample_t *spl;
	int max, pos;
	int i;

	/* write received samples to decode buffer */
	max = nmt->super_samples;
	spl = nmt->super_filter_spl;
	pos = nmt->super_filter_pos;
	for (i = 0; i < length; i++) {
		spl[pos++] = samples[i];
		if (pos == max) {
			pos = 0;
			if (nmt->supervisory)
				super_decode(nmt, spl, max);
		}
	}
	nmt->super_filter_pos = pos;

	/* fsk signal */
	fsk_demod_receive(&nmt->fsk_demod, samples, length);

	/* muting audio while receiving frame */
	for (i = 0; i < length; i++) {
		if (nmt->rx_mute && !nmt->sender.loopback) {
			samples[i] = 0;
			nmt->rx_mute--;
		}
	}

	if ((nmt->dsp_mode == DSP_MODE_AUDIO || nmt->dsp_mode == DSP_MODE_DTMF)
	 && nmt->trans && nmt->trans->callref) {
		int len, count;

		len = samplerate_downsample(&nmt->sender.srstate, samples, length);
		if (nmt->compandor)
			expand_audio(&nmt->cstate, samples, len);
		if (nmt->dsp_mode == DSP_MODE_DTMF) {
			/* encode and fill with silence after finish */
			count = dtmf_encode(&nmt->dtmf, samples, len);
			if (count < len)
				memset(samples + count, 0, sizeof(*samples) * (len - count));
		}
		spl = nmt->sender.rxbuf;
		pos = nmt->sender.rxbuf_pos;
		for (i = 0; i < len; i++) {
			spl[pos++] = samples[i];
			if (pos == 160) {
				call_up_audio(nmt->trans->callref, spl, 160);
				pos = 0;
			}
		}
		nmt->sender.rxbuf_pos = pos;
	} else
		nmt->sender.rxbuf_pos = 0;
}

static int fsk_send_bit(void *inst)
{
	nmt_t *nmt = (nmt_t *)inst;
	const char *frame;

	/* send frame bit (prio) */
	if (nmt->dsp_mode == DSP_MODE_FRAME) {
		if (!nmt->tx_frame_length || nmt->tx_frame_pos == nmt->tx_frame_length) {
			/* request frame */
			frame = nmt_get_frame(nmt);
			if (!frame) {
				nmt->tx_frame_length = 0;
				LOGP_CHAN(DDSP, LOGL_DEBUG, "Stop sending frames.\n");
				return -1;
			}
			memcpy(nmt->tx_frame, frame, 166);
			nmt->tx_frame_length = 166;
			nmt->tx_frame_pos = 0;
		}

		return nmt->tx_frame[nmt->tx_frame_pos++];
	}

	/* send dms bit */
	return dms_send_bit(nmt);
}

/* Generate audio stream with supervisory signal. Keep phase for next call of function. */
static void super_encode(nmt_t *nmt, sample_t *samples, int length)
{
        double phaseshift, phase;
	int i;

	phaseshift = nmt->super_phaseshift65536[nmt->supervisory - 1];
	phase = nmt->super_phase65536;

	for (i = 0; i < length; i++) {
		*samples++ += dsp_sine_super[(uint16_t)phase];
		phase += phaseshift;
		if (phase >= 65536)
			phase -= 65536;
	}

	nmt->super_phase65536 = phase;
}

/* Generate audio stream from dial tone. Keep phase for next call of function. */
static void dial_tone(nmt_t *nmt, sample_t *samples, int length)
{
        double phaseshift, phase;
	int i;

	phaseshift = nmt->dial_phaseshift65536;
	phase = nmt->dial_phase65536;

	for (i = 0; i < length; i++) {
		*samples++ = dsp_sine_dialtone[(uint16_t)phase];
		phase += phaseshift;
		if (phase >= 65536)
			phase -= 65536;
	}

	nmt->dial_phase65536 = phase;
}

/* Provide stream of audio toward radio unit */
void sender_send(sender_t *sender, sample_t *samples, uint8_t *power, int length)
{
	nmt_t *nmt = (nmt_t *) sender;
	int count, input_num;

	memset(power, 1, length);

again:
	switch (nmt->dsp_mode) {
	case DSP_MODE_AUDIO:
	case DSP_MODE_DTMF:
		input_num = samplerate_upsample_input_num(&sender->srstate, length);
		{
			int16_t spl[input_num];
			jitter_load_samples(&sender->dejitter, (uint8_t *)spl, input_num, sizeof(*spl), jitter_conceal_s16, NULL);
			int16_to_samples_speech(samples, spl, input_num);
		}
		if (nmt->compandor)
			compress_audio(&nmt->cstate, samples, input_num);
		samplerate_upsample(&sender->srstate, samples, input_num, samples, length);
		/* send after dejitter, so audio is flushed */
		if (nmt->dms.tx_frame_valid) {
			fsk_mod_send(&nmt->fsk_mod, samples, length, 0);
			break;
		}
		if (nmt->supervisory)
			super_encode(nmt, samples, length);
		break;
	case DSP_MODE_DIALTONE:
		dial_tone(nmt, samples, length);
		break;
	case DSP_MODE_SILENCE:
		memset(samples, 0, length * sizeof(*samples));
		break;
	case DSP_MODE_FRAME:
		/* Encode frame into audio stream. If frames have
		 * stopped, process again for rest of stream. */
		count = fsk_mod_send(&nmt->fsk_mod, samples, length, 0);
		/* special case: add supervisory signal to frame at loop test */
		if (nmt->sender.loopback && nmt->supervisory)
			super_encode(nmt, samples, count);
		samples += count;
		length -= count;
		if (length)
			goto again;
		break;
	}
}

static const char *nmt_dsp_mode_name(enum dsp_mode mode)
{
        static char invalid[16];

	switch (mode) {
	case DSP_MODE_SILENCE:
		return "SILENCE";
	case DSP_MODE_DIALTONE:
		return "DIALTONE";
	case DSP_MODE_AUDIO:
		return "AUDIO";
	case DSP_MODE_FRAME:
		return "FRAME";
	case DSP_MODE_DTMF:
		return "DTMF";
	}

	sprintf(invalid, "invalid(%d)", mode);
	return invalid;
}

void nmt_set_dsp_mode(nmt_t *nmt, enum dsp_mode mode)
{
	/* reset frame */
	if (mode == DSP_MODE_FRAME && nmt->dsp_mode != mode) {
		fsk_mod_reset(&nmt->fsk_mod);
		nmt->tx_frame_length = 0;
	}

	LOGP_CHAN(DDSP, LOGL_DEBUG, "DSP mode %s -> %s\n", nmt_dsp_mode_name(nmt->dsp_mode), nmt_dsp_mode_name(mode));
	if ((mode == DSP_MODE_AUDIO || mode == DSP_MODE_DTMF) && (nmt->dsp_mode != DSP_MODE_AUDIO && nmt->dsp_mode != DSP_MODE_DTMF))
		jitter_reset(&nmt->sender.dejitter);

	nmt->dsp_mode = mode;
}

/* Receive audio from call instance. */
void call_down_audio(void *decoder, void *decoder_priv, int callref, uint16_t sequence, uint8_t marker, uint32_t timestamp, uint32_t ssrc, uint8_t *payload, int payload_len)
{
	transaction_t *trans;
	nmt_t *nmt;

	trans = get_transaction_by_callref(callref);
	if (!trans)
		return;
	nmt = trans->nmt;
	if (!nmt)
		return;

	if (nmt->dsp_mode == DSP_MODE_AUDIO || nmt->dsp_mode == DSP_MODE_DTMF) {
		jitter_frame_t *jf;
		jf = jitter_frame_alloc(decoder, decoder_priv, payload, payload_len, marker, sequence, timestamp, ssrc);
		if (jf)
			jitter_save(&nmt->sender.dejitter, jf);
	}
}

void call_down_clock(void) {}

