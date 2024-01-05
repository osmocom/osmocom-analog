/* audio processing
 *
 * (C) 2021 by Andreas Eversberg <jolly@eversberg.eu>
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

#define CHAN mpt1327->sender.kanal

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "../libsample/sample.h"
#include "../libmobile/call.h"
#include "../liblogging/logging.h"
#include <osmocom/core/timer.h>
#include "mpt1327.h"
#include "dsp.h"
#include "message.h"

#define PI			M_PI

/* signaling */
#define MAX_DEVIATION		2500.0
#define MAX_MODULATION		2550.0
#define SPEECH_DEVIATION	1500.0	/* deviation of speech (no emphasis) */
#define TX_PEAK_FSK		(1500.0 / SPEECH_DEVIATION)
#define BIT_RATE		1200.0
#define BIT_ADJUST		0.1	/* how much do we adjust bit clock on frequency change */
#define F0			1800.0
#define F1			1200.0
#define MAX_DISPLAY		1.4	/* something above speech level */

/* carrier loss detection */
#define MUTE_TIME	0.1	/* time to mute after loosing signal */

void dsp_init(void)
{
}

static int fsk_send_bit(void *inst);
static void fsk_receive_bit(void *inst, int bit, double quality, double level);

/* Init FSK of transceiver */
int dsp_init_sender(mpt1327_t *mpt1327, double squelch_db)
{
	int rc;

	LOGP_CHAN(DDSP, LOGL_DEBUG, "Init DSP for Transceiver.\n");

	/* init squelch */
	squelch_init(&mpt1327->squelch, mpt1327->sender.kanal, squelch_db, MUTE_TIME, MUTE_TIME);

	/* set modulation parameters */
	sender_set_fm(&mpt1327->sender, MAX_DEVIATION, MAX_MODULATION, SPEECH_DEVIATION, MAX_DISPLAY);

	LOGP(DDSP, LOGL_DEBUG, "Using FSK level of %.3f (%.3f KHz deviation)\n", TX_PEAK_FSK, SPEECH_DEVIATION * TX_PEAK_FSK / 1e3);

	/* init fsk */
	if (fsk_mod_init(&mpt1327->fsk_mod, mpt1327, fsk_send_bit, mpt1327->sender.samplerate, BIT_RATE, F0, F1, TX_PEAK_FSK, 1, 0) < 0) {
		LOGP_CHAN(DDSP, LOGL_ERROR, "FSK init failed!\n");
		return -EINVAL;
	}
	if (fsk_demod_init(&mpt1327->fsk_demod, mpt1327, fsk_receive_bit, mpt1327->sender.samplerate, BIT_RATE, F0, F1, BIT_ADJUST) < 0) {
		LOGP_CHAN(DDSP, LOGL_ERROR, "FSK init failed!\n");
		return -EINVAL;
	}

	mpt1327->dmp_frame_level = display_measurements_add(&mpt1327->sender.dispmeas, "Frame Level", "%.1f %% (last)", DISPLAY_MEAS_LAST, DISPLAY_MEAS_LEFT, 0.0, 150.0, 100.0);
	mpt1327->dmp_frame_quality = display_measurements_add(&mpt1327->sender.dispmeas, "Frame Quality", "%.1f %% (last)", DISPLAY_MEAS_LAST, DISPLAY_MEAS_LEFT, 0.0, 100.0, 100.0);

	/* repeater */
	rc = jitter_create(&mpt1327->repeater_dejitter, "repeater", mpt1327->sender.samplerate, sizeof(sample_t), 0.050, 0.500, JITTER_FLAG_NONE);
	if (rc < 0) {
		LOGP(DDSP, LOGL_ERROR, "Failed to create and init repeater buffer!\n");
		goto error;
	}
	return 0;

error:
	dsp_cleanup_sender(mpt1327);
	return rc;
}

/* Cleanup transceiver instance. */
void dsp_cleanup_sender(mpt1327_t *mpt1327)
{
	LOGP_CHAN(DDSP, LOGL_DEBUG, "Cleanup DSP for Transceiver.\n");

	fsk_mod_cleanup(&mpt1327->fsk_mod);
	fsk_demod_cleanup(&mpt1327->fsk_demod);

	jitter_destroy(&mpt1327->repeater_dejitter);
}

/* Check for SYNC bits, then collect data bits */
static void fsk_receive_bit(void *inst, int bit, double quality, double level)
{
	mpt1327_t *mpt1327 = (mpt1327_t *)inst;
	int i;

	/* normalize FSK level */
	level /= TX_PEAK_FSK;

//	printf("bit=%d quality=%.4f\n", bit, quality);
	if (!mpt1327->rx_in_sync) {
		mpt1327->rx_sync = (mpt1327->rx_sync << 1) | bit;

		/* level and quality */
		mpt1327->rx_level[mpt1327->rx_count & 0xff] = level;
		mpt1327->rx_quality[mpt1327->rx_count & 0xff] = quality;
		mpt1327->rx_count++;

		/* check if sync pattern match */
		if (mpt1327->rx_sync != mpt1327->sync_word)
			return;

		/* average level and quality */
		level = quality = 0;
		for (i = 0; i < 16; i++) {
			level += mpt1327->rx_level[(mpt1327->rx_count - 1 - i) & 0xff];
			quality += mpt1327->rx_quality[(mpt1327->rx_count - 1 - i) & 0xff];
		}
		level /= 16.0; quality /= 16.0;
//		printf("sync (level = %.2f, quality = %.2f\n", level, quality);

		/* do not accept garbage */
		if (quality < 0.65)
			return;

		/* rest sync register */
		mpt1327->rx_sync = 0;
		mpt1327->rx_in_sync = 1;
		mpt1327->rx_count = 0;

		/* mute audio from now on */
		mpt1327->rx_mute = 1;

		return;
	}

	/* read bits */
	mpt1327->rx_bits = (mpt1327->rx_bits << 1) | (bit & 1);
	mpt1327->rx_level[mpt1327->rx_count] = level;
	mpt1327->rx_quality[mpt1327->rx_count] = quality;
	if (++mpt1327->rx_count != 64)
		return;

	/* check parity */
	if (mpt1327_checkbits(mpt1327->rx_bits, NULL) != (mpt1327->rx_bits & 0xffff)) {
		LOGP(DDSP, LOGL_NOTICE, "Received corrupt codeword or noise.\n");
		mpt1327->rx_in_sync = 0;
		mpt1327->rx_mute = 0;
		return;
	}

	/* reset counter for next frame */
	mpt1327->rx_count = 0;

	/* average level and quality */
	level = quality = 0;
	for (i = 0; i < 64; i++) {
		level += mpt1327->rx_level[i];
		quality += mpt1327->rx_quality[i];
	}
	level /= 64.0; quality /= 64.0;

	/* update measurements */
	display_measurements_update(mpt1327->dmp_frame_level, level * 100.0, 0.0);
	display_measurements_update(mpt1327->dmp_frame_quality, quality * 100.0, 0.0);

	/* convert level so that received level at TX_PEAK_FSK results in 1.0 (100%) */
	mpt1327_receive_codeword(mpt1327, mpt1327->rx_bits, quality, level);
}

/* Process received audio stream from radio unit. */
void sender_receive(sender_t *sender, sample_t *samples, int length, double __attribute__((unused)) rf_level_db)
{
	mpt1327_t *mpt1327 = (mpt1327_t *) sender;
	sample_t *spl;
	int pos;
	int i;
	int was_mute = mpt1327->rx_mute; /* remember, so always mute whole chunk */
	int was_pressel_on = mpt1327->pressel_on;

	/* if channel is off, do nothing */
	if (mpt1327->dsp_mode == DSP_MODE_OFF) {
		/* measure squelch even if channel is turned off */
		if (!isinf(mpt1327->squelch.threshold_db))
			squelch(&mpt1327->squelch, rf_level_db, (double)length / (double)mpt1327->sender.samplerate);
		return;
	}

	/* fsk signal */
	fsk_demod_receive(&mpt1327->fsk_demod, samples, length);

	/* on traffic channel mute and indicate signal strength */
	if (mpt1327->dsp_mode == DSP_MODE_TRAFFIC) {
		/* process signal mute/loss, also for signalling tone */
		if (!isinf(mpt1327->squelch.threshold_db)) {
			/* use squelch to unmute and reset call timer */
			switch (squelch(&mpt1327->squelch, rf_level_db, (double)length / (double)mpt1327->sender.samplerate)) {
			case SQUELCH_LOSS:
			case SQUELCH_MUTE:
				memset(samples, 0, sizeof(*samples) * length);
				break;
			default:
				mpt1327_signal_indication(mpt1327);
			}
		} else {
			/* muting audio while pressel is off */
			if (!was_pressel_on || !mpt1327->pressel_on)
				memset(samples, 0, sizeof(*samples) * length);
		}
		/* muting audio while receiving frame */
		if (was_mute || mpt1327->rx_mute)
			memset(samples, 0, sizeof(*samples) * length);
	}

	if (mpt1327->dsp_mode == DSP_MODE_TRAFFIC) {
		/* if repeater mode, store sample in jitter buffer */
		if (mpt1327->repeater)
			jitter_save(&mpt1327->repeater_dejitter, samples, length, 0, 0, 0, 0);

		if (mpt1327->unit && mpt1327->unit->callref) {
			int count;

			count = samplerate_downsample(&mpt1327->sender.srstate, samples, length);
			spl = mpt1327->sender.rxbuf;
			pos = mpt1327->sender.rxbuf_pos;
			for (i = 0; i < count; i++) {
				spl[pos++] = samples[i];
				if (pos == 160) {
					call_up_audio(mpt1327->unit->callref, spl, 160);
					pos = 0;
				}
			}
			mpt1327->sender.rxbuf_pos = pos;
		} else
			mpt1327->sender.rxbuf_pos = 0;
	} else
		mpt1327->sender.rxbuf_pos = 0;
}

static int fsk_send_bit(void *inst)
{
	mpt1327_t *mpt1327 = (mpt1327_t *)inst;

	/* send frame bit (prio) */
	if (!mpt1327->tx_bit_num || mpt1327->tx_count == mpt1327->tx_bit_num) {
		/* request frame */
		mpt1327->tx_bit_num = mpt1327_send_codeword(mpt1327, &mpt1327->tx_bits);
		if (mpt1327->tx_bit_num == 0) {
			return -1;
		}
		mpt1327->tx_count = 0;
	}
	return (mpt1327->tx_bits >> (63 - mpt1327->tx_count++)) & 1;

	return -1;
}

/* Provide stream of audio toward radio unit */
void sender_send(sender_t *sender, sample_t *samples, uint8_t *power, int length)
{
	mpt1327_t *mpt1327 = (mpt1327_t *) sender;
	int input_num;

	if (mpt1327->dsp_mode == DSP_MODE_OFF) {
		memset(power, 0, length);
		memset(samples, 0, sizeof(*samples) * length);
		return;
	}

	memset(power, 1, length);

	if (mpt1327->dsp_mode == DSP_MODE_TRAFFIC) {
		input_num = samplerate_upsample_input_num(&sender->srstate, length);
		jitter_load(&sender->dejitter, samples, input_num);
		samplerate_upsample(&sender->srstate, samples, input_num, samples, length);
		/* if repeater mode, sum samples from jitter buffer to samples */
		if (mpt1327->repeater) {
			sample_t uplink[length];
			int i;
			jitter_load(&mpt1327->repeater_dejitter, uplink, length);
			for (i = 0; i < length; i++)
				samples[i] += uplink[i];
		}
	} else
		memset(samples, 0, sizeof(*samples) * length);

	/* If there is something to modulate (pending TX frame),
	 * overwrite audio with FSK audio. */
	fsk_mod_send(&mpt1327->fsk_mod, samples, length, 0);
}

const char *mpt1327_dsp_mode_name(enum dsp_mode mode)
{
        static char invalid[16];

	switch (mode) {
	case DSP_MODE_OFF:
		return "OFF";
	case DSP_MODE_TRAFFIC:
		return "TRAFFIC";
	case DSP_MODE_CONTROL:
		return "CONTROL";
	}

	sprintf(invalid, "invalid(%d)", mode);
	return invalid;
}

void mpt1327_set_dsp_mode(mpt1327_t *mpt1327, enum dsp_mode mode, int repeater)
{
	//NOTE: DO NOT RESET FRAME, because mode may change before frame has been sent!

	if (mode == DSP_MODE_CONTROL)
		mpt1327->sync_word = 0xc4d7;
	if (mode == DSP_MODE_TRAFFIC)
		mpt1327->sync_word = 0x3b28;

	if (repeater)
		jitter_reset(&mpt1327->repeater_dejitter);
	mpt1327->repeater = repeater;

	LOGP_CHAN(DDSP, LOGL_DEBUG, "DSP mode %s -> %s\n", mpt1327_dsp_mode_name(mpt1327->dsp_mode), mpt1327_dsp_mode_name(mode));
	mpt1327->dsp_mode = mode;
}

void mpt1327_reset_sync(mpt1327_t *mpt1327)
{
	mpt1327->rx_in_sync = 0;
	mpt1327->rx_sync = 0;
	mpt1327->rx_mute = 0;
}

