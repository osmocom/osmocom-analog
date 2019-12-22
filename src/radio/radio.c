/* main function
 *
 * (C) 2018 by Andreas Eversberg <jolly@eversberg.eu>
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
#include <math.h>
#include <errno.h>
#include <pthread.h>
#include "../libsample/sample.h"
#include "../libdebug/debug.h"
#include "../libsound/sound.h"
#include "../libclipper/clipper.h"
#include "radio.h"

#define CLIP_POINT	0.85
#define DC_CUTOFF	30.0 // Wikipedia: UKW-Rundfunk
#define STEREO_BW	15000.0
#define PILOT_FREQ	19000.0
#define PILOT_BW	5.0

int radio_init(radio_t *radio, int latspl, int samplerate, const char *tx_wave_file, const char *rx_wave_file, const char *tx_audiodev, const char *rx_audiodev, enum modulation modulation, double bandwidth, double deviation, double modulation_index, double time_constant_us, double volume, int stereo, int rds, int rds2)
{
	int rc = -EINVAL;

	clipper_init(CLIP_POINT);

	memset(radio, 0, sizeof(*radio));
	radio->latspl = latspl;
	radio->volume = volume;
	radio->stereo = stereo;
	radio->rds = rds;
	radio->rds2 = rds2;
	radio->tx_wave_file = tx_wave_file;
	radio->modulation = modulation;
	radio->signal_samplerate = samplerate;
	radio->audio_bandwidth = bandwidth;

	switch (radio->modulation) {
	case MODULATION_FM:
		radio->fm_deviation = deviation;
		radio->signal_bandwidth = deviation + bandwidth;
		if (radio->stereo) {
			radio->signal_bandwidth = deviation + 53000.0;
			radio->audio_bandwidth = STEREO_BW;
		}
		if (radio->rds)
			radio->signal_bandwidth = deviation + 60000.0;
		if (radio->rds2)
			radio->signal_bandwidth = deviation + 80000.0;
		break;
	case MODULATION_AM_DSB:
	case MODULATION_AM_USB:
	case MODULATION_AM_LSB:
		/* level is 1.0, which is full amplitude */
		radio->signal_bandwidth = bandwidth;
		break;
	case MODULATION_NONE:
		PDEBUG(DRADIO, DEBUG_ERROR, "Wrong modulation, plese fix!\n");
		goto error;
	}

	if (tx_wave_file) {
		/* open wave file */
		int _samplerate = 0;
		radio->tx_audio_channels = 0;
		rc = wave_create_playback(&radio->wave_tx_play, tx_wave_file, &_samplerate, &radio->tx_audio_channels, 1.0);
		if (rc < 0) {
			PDEBUG(DRADIO, DEBUG_ERROR, "Failed to create WAVE playback instance!\n");
			goto error;
		}
		if (radio->tx_audio_channels != 1 && radio->tx_audio_channels != 2)
		{
			PDEBUG(DRADIO, DEBUG_ERROR, "WAVE file must have one or two channels!\n");
			goto error;
		}
		radio->tx_audio_samplerate = _samplerate;
		radio->tx_audio_mode = AUDIO_MODE_WAVEFILE;
	} else if (tx_audiodev) {
#ifdef HAVE_ALSA
		/* open audio device */
		radio->tx_audio_samplerate = 48000;
		radio->tx_audio_channels = (stereo) ? 2 : 1;
		radio->tx_sound = sound_open(tx_audiodev, NULL, NULL, NULL, radio->tx_audio_channels, 0.0, radio->tx_audio_samplerate, radio->latspl, 1.0, 0.0, 2.0);
		if (!radio->tx_sound) {
			rc = -EIO;
			PDEBUG(DRADIO, DEBUG_ERROR, "Failed to open sound device!\n");
			goto error;
		}
		jitter_create(&radio->tx_dejitter[0], radio->tx_audio_samplerate / 5);
		jitter_create(&radio->tx_dejitter[1], radio->tx_audio_samplerate / 5);
		radio->tx_audio_mode = AUDIO_MODE_AUDIODEV;
#else
		rc = -ENOTSUP;
		PDEBUG(DRADIO, DEBUG_ERROR, "No sound card support compiled in!\n");
		goto error;
#endif
	} else {
		int i;
		double phase;
		/* use built-in sample sound */
		radio->tx_audio_samplerate = samplerate;
		radio->tx_audio_channels = (radio->stereo) ? 2 : 1;
		radio->testtone_length = radio->tx_audio_samplerate;
		radio->testtone[0] = calloc(radio->testtone_length * 2, sizeof(sample_t));
		if (!radio->testtone[0]) {
			rc = -ENOMEM;
			PDEBUG(DRADIO, DEBUG_ERROR, "Failed to allocate test sound buffer!\n");
			goto error;
		}
		radio->testtone[1] = radio->testtone[0] + radio->testtone_length;
		/* generate tone */
		phase = 2.0 * M_PI * 1000.0 / radio->tx_audio_samplerate;
		if (radio->stereo) {
			for (i = 0; i < radio->testtone_length / 2; i++) {
				radio->testtone[0][i] = sin(i * phase);
				radio->testtone[1][i] = 0.0;
			}
			for (; i < radio->testtone_length; i++) {
				radio->testtone[0][i] = 0.0;
				radio->testtone[1][i] = sin(i * phase);
			}
		} else {
			for (i = 0; i < radio->testtone_length; i++) {
				radio->testtone[0][i] = sin(i * phase);
			}
		}
		radio->tx_audio_mode = AUDIO_MODE_TESTTONE;
	}

	if (rx_wave_file) {
		/* open wave file */
		radio->rx_audio_samplerate = 48000;
		radio->rx_audio_channels = (radio->stereo) ? 2 : 1;
		rc = wave_create_record(&radio->wave_rx_rec, rx_wave_file, radio->rx_audio_samplerate, radio->rx_audio_channels, 1.0);
		if (rc < 0) {
			PDEBUG(DRADIO, DEBUG_ERROR, "Failed to create WAVE record instance!\n");
			goto error;
		}
		radio->rx_audio_mode = AUDIO_MODE_WAVEFILE;
	} else if (rx_audiodev) {
#ifdef HAVE_ALSA
		/* open audio device */
		radio->rx_audio_samplerate = 48000;
		radio->rx_audio_channels = (stereo) ? 2 : 1;
		/* check if we use same device */
		if (radio->tx_sound && !strcmp(tx_audiodev, rx_audiodev))
			radio->rx_sound = radio->tx_sound;
		else
			radio->rx_sound = sound_open(rx_audiodev, NULL, NULL, NULL, radio->rx_audio_channels, 0.0, radio->rx_audio_samplerate, radio->latspl, 1.0, 0.0, 2.0);
		if (!radio->rx_sound) {
			rc = -EIO;
			PDEBUG(DRADIO, DEBUG_ERROR, "Failed to open sound device!\n");
			goto error;
		}
		jitter_create(&radio->rx_dejitter[0], radio->rx_audio_samplerate / 5);
		jitter_create(&radio->rx_dejitter[1], radio->rx_audio_samplerate / 5);
		radio->rx_audio_mode = AUDIO_MODE_AUDIODEV;
#else
		rc = -ENOTSUP;
		PDEBUG(DRADIO, DEBUG_ERROR, "No sound card support compiled in!\n");
		goto error;
#endif
#if 0
	} else {
		rc = -ENOTSUP;
		PDEBUG(DRADIO, DEBUG_ERROR, "No RX audio sink is selected, try \"--audio-device default\"!\n");
		goto error;
#endif
	}

	/* check if sample rate is too low */
	if (radio->tx_audio_samplerate > radio->signal_samplerate) {
		rc = -EINVAL;
		PDEBUG(DRADIO, DEBUG_ERROR, "You have selected a signal processing sample rate of %.0f. Your audio sample rate is %.0f.\n", radio->signal_samplerate, radio->tx_audio_samplerate);
		PDEBUG(DRADIO, DEBUG_ERROR, "Please select a sample rate that is higher or equal the audio sample rate!\n");
		goto error;
	}
	if (radio->rx_audio_samplerate > radio->signal_samplerate) {
		rc = -EINVAL;
		PDEBUG(DRADIO, DEBUG_ERROR, "You have selected a signal processing sample rate of %.0f. Your audio sample rate is %.0f.\n", radio->signal_samplerate, radio->rx_audio_samplerate);
		PDEBUG(DRADIO, DEBUG_ERROR, "Please select a sample rate that is higher or equal the audio sample rate!\n");
		goto error;
	}
	if (radio->signal_samplerate < radio->signal_bandwidth * 2 / 0.75) {
		rc = -EINVAL;
		PDEBUG(DRADIO, DEBUG_ERROR, "You have selected a signal processing sample rate of %.0f. Your signal's bandwidth %.0f.\n", radio->signal_samplerate, radio->signal_bandwidth);
		PDEBUG(DRADIO, DEBUG_ERROR, "Your signal processing sample rate must be at least one third greater than the signal's double bandwidth. Use at least %.0f.\n", radio->signal_bandwidth * 2.0 / 0.75);
		goto error;
	}

	iir_highpass_init(&radio->tx_dc_removal[0], DC_CUTOFF, radio->tx_audio_samplerate, 1);
	iir_highpass_init(&radio->tx_dc_removal[1], DC_CUTOFF, radio->tx_audio_samplerate, 1);

	/* stereo pilot tone phase */
	radio->pilot_phasestep = 2.0 * M_PI * PILOT_FREQ / radio->signal_samplerate;

	/* stere decoding filters */
	iir_lowpass_init(&radio->rx_lp_pilot_I, PILOT_BW, radio->signal_samplerate, 2);
        iir_lowpass_init(&radio->rx_lp_pilot_Q, PILOT_BW, radio->signal_samplerate, 2);
        iir_lowpass_init(&radio->rx_lp_sum, STEREO_BW, radio->signal_samplerate, 2);
        iir_lowpass_init(&radio->rx_lp_diff, STEREO_BW, radio->signal_samplerate, 2);

	/* init sample rate conversion, use complete bandwidth for resample filter */
	rc = init_samplerate(&radio->tx_resampler[0], radio->tx_audio_samplerate, radio->signal_samplerate, radio->tx_audio_samplerate / 2.0);
	if (rc < 0)
		goto error;
	rc = init_samplerate(&radio->tx_resampler[1], radio->tx_audio_samplerate, radio->signal_samplerate, radio->tx_audio_samplerate / 2.0);
	if (rc < 0)
		goto error;
	rc = init_samplerate(&radio->rx_resampler[0], radio->rx_audio_samplerate, radio->signal_samplerate, radio->rx_audio_samplerate / 2.0);
	if (rc < 0)
		goto error;
	rc = init_samplerate(&radio->rx_resampler[1], radio->rx_audio_samplerate, radio->signal_samplerate, radio->rx_audio_samplerate / 2.0);
	if (rc < 0)
		goto error;

	/* init filters (using signal sample rate) */
	switch (radio->modulation) {
	case MODULATION_FM:
		if (time_constant_us > 0.0) {
			radio->emphasis = 1;
			/* time constant */
			PDEBUG(DRADIO, DEBUG_INFO, "Using emphasis cut-off at %.0f Hz.\n", timeconstant2cutoff(time_constant_us));
			rc = init_emphasis(&radio->fm_emphasis[0], radio->signal_samplerate, timeconstant2cutoff(time_constant_us), DC_CUTOFF, radio->audio_bandwidth);
			if (rc < 0)
				goto error;
			rc = init_emphasis(&radio->fm_emphasis[1], radio->signal_samplerate, timeconstant2cutoff(time_constant_us), DC_CUTOFF, radio->audio_bandwidth);
			if (rc < 0)
				goto error;
		}
		rc = fm_mod_init(&radio->fm_mod, radio->signal_samplerate, 0.0, 1.0);
		if (rc < 0)
			goto error;
		rc = fm_demod_init(&radio->fm_demod, radio->signal_samplerate, 0.0, 2 * radio->signal_bandwidth);
		if (rc < 0)
			goto error;
		break;
	case MODULATION_AM_DSB:
		iir_lowpass_init(&radio->tx_am_bw_limit, radio->audio_bandwidth, radio->signal_samplerate, 1);
		/* modulation index 0.0 = no envelope, bias 1.0
		 * modulation index 1.0 = envelope +-0.5, bias 0.5
		 * modulation index 0.5 = envelope +-0.25, bias 0.75
		 */
		double gain = modulation_index / 2.0;
		double bias = 1.0 - gain;
		rc = am_mod_init(&radio->am_mod, radio->signal_samplerate, 0.0, gain, bias);
		if (rc < 0)
			goto error;
		rc = am_demod_init(&radio->am_demod, radio->signal_samplerate, 0.0, radio->signal_bandwidth, 1.0 / modulation_index);
		if (rc < 0)
			goto error;
		break;
	case MODULATION_AM_USB:
		iir_lowpass_init(&radio->tx_am_bw_limit, radio->audio_bandwidth, radio->signal_samplerate, 1);
		rc = am_mod_init(&radio->am_mod, radio->signal_samplerate, 0.0, 1.0, 0.0);
		if (rc < 0)
			goto error;
		break;
	case MODULATION_AM_LSB:
		iir_lowpass_init(&radio->tx_am_bw_limit, radio->audio_bandwidth, radio->signal_samplerate, 1);
		rc = am_mod_init(&radio->am_mod, radio->signal_samplerate, 0.0, 1.0, 0.0);
		if (rc < 0)
			goto error;
		break;
	default:
		break;
	}
	
	if (radio->tx_audio_mode)
		PDEBUG(DRADIO, DEBUG_INFO, "Bandwidth of audio source is %.0f Hz.\n", radio->tx_audio_samplerate / 2.0);
	if (radio->rx_audio_mode)
		PDEBUG(DRADIO, DEBUG_INFO, "Bandwidth of audio sink is %.0f Hz.\n", radio->rx_audio_samplerate / 2.0);
	PDEBUG(DRADIO, DEBUG_INFO, "Bandwidth of audio signal is %.0f Hz.\n", radio->audio_bandwidth);
	PDEBUG(DRADIO, DEBUG_INFO, "Bandwidth of modulated signal is %.0f Hz.\n", radio->signal_bandwidth);
	if (radio->tx_audio_mode)
		PDEBUG(DRADIO, DEBUG_INFO, "Sample rate of audio source is %.0f Hz.\n", radio->tx_audio_samplerate);
	if (radio->rx_audio_mode)
		PDEBUG(DRADIO, DEBUG_INFO, "Sample rate of audio sink is %.0f Hz.\n", radio->rx_audio_samplerate);
	PDEBUG(DRADIO, DEBUG_INFO, "Sample rate of signal is %.0f Hz.\n", radio->signal_samplerate);

	/* one or two audio channels */
	if (radio->tx_audio_channels != 1 && radio->tx_audio_channels != 2)
	{
		PDEBUG(DRADIO, DEBUG_ERROR, "Wrong number of audio channels, please fix!\n");
		goto error;
	}

	/* audio buffers: how many sample for audio (rounded down) */
	int tx_size = (int)((double)latspl / radio->tx_resampler[0].factor);
	int rx_size = (int)((double)latspl / radio->rx_resampler[0].factor);
	if (tx_size > rx_size)
		radio->audio_buffer_size = tx_size;
	else
		radio->audio_buffer_size = rx_size;
	radio->audio_buffer = calloc(radio->audio_buffer_size * 2, sizeof(*radio->audio_buffer));
	if (!radio->audio_buffer) {
		PDEBUG(DRADIO, DEBUG_ERROR, "No memory!!\n");
		rc = -ENOMEM;
		goto error;
	}

	/* signal buffers */
	radio->signal_buffer_size = latspl;
	radio->signal_buffer = calloc(radio->signal_buffer_size * 3, sizeof(*radio->signal_buffer));
	radio->signal_power_buffer = calloc(radio->signal_buffer_size, sizeof(*radio->signal_power_buffer));
	if (!radio->signal_buffer || !radio->signal_power_buffer) {
		PDEBUG(DRADIO, DEBUG_ERROR, "No memory!!\n");
		rc = -ENOMEM;
		goto error;
	}

	/* termporary I/Q/carrier buffers, used while demodulating */
	radio->I_buffer = calloc(latspl, sizeof(*radio->I_buffer));
	radio->Q_buffer = calloc(latspl, sizeof(*radio->Q_buffer));
	radio->carrier_buffer = calloc(latspl, sizeof(*radio->carrier_buffer));
	if (!radio->I_buffer || !radio->Q_buffer || !radio->carrier_buffer) {
		PDEBUG(DRADIO, DEBUG_ERROR, "No memory!!\n");
		rc = -ENOMEM;
		goto error;
	}

	return 0;

error:
	radio_exit(radio);
	return rc;
}

void radio_exit(radio_t *radio)
{
	if (radio->audio_buffer) {
		free(radio->audio_buffer);
		radio->audio_buffer = NULL;
	}
	if (radio->signal_buffer) {
		free(radio->signal_buffer);
		radio->signal_buffer = NULL;
	}
	if (radio->signal_power_buffer) {
		free(radio->signal_power_buffer);
		radio->signal_power_buffer = NULL;
	}
	if (radio->I_buffer) {
		free(radio->I_buffer);
		radio->I_buffer = NULL;
	}
	if (radio->Q_buffer) {
		free(radio->Q_buffer);
		radio->Q_buffer = NULL;
	}
	if (radio->carrier_buffer) {
		free(radio->carrier_buffer);
		radio->carrier_buffer = NULL;
	}
	if (radio->tx_audio_mode == AUDIO_MODE_WAVEFILE) {
		wave_destroy_playback(&radio->wave_tx_play);
		radio->tx_audio_mode = AUDIO_MODE_NONE;
	}
	if (radio->rx_audio_mode == AUDIO_MODE_WAVEFILE) {
		wave_destroy_record(&radio->wave_rx_rec);
		radio->rx_audio_mode = AUDIO_MODE_NONE;
	}
#ifdef HAVE_ALSA
	if (radio->tx_sound) {
		sound_close(radio->tx_sound);
		/* if same device was used */
		if (radio->tx_sound == radio->rx_sound)
			radio->rx_sound = NULL;
		radio->tx_sound = NULL;
		radio->tx_audio_mode = AUDIO_MODE_NONE;
	}
	if (radio->rx_sound) {
		sound_close(radio->rx_sound);
		radio->rx_sound = NULL;
		radio->rx_audio_mode = AUDIO_MODE_NONE;
	}
#endif
	jitter_destroy(&radio->tx_dejitter[0]);
	jitter_destroy(&radio->tx_dejitter[1]);
	jitter_destroy(&radio->rx_dejitter[0]);
	jitter_destroy(&radio->rx_dejitter[1]);
	if (radio->tx_audio_mode == AUDIO_MODE_TESTTONE) {
		free(radio->testtone[0]);
		radio->tx_audio_mode = AUDIO_MODE_NONE;
	}
	if (radio->modulation == MODULATION_FM)
		fm_mod_exit(&radio->fm_mod);
	else
		am_mod_exit(&radio->am_mod);
}

int radio_start(radio_t __attribute__((unused)) *radio)
{
	int rc = 0;

#ifdef HAVE_ALSA
	/* start rx sound */
	if (radio->rx_sound) 
		rc = sound_start(radio->rx_sound);
	/* start tx sound, if different device */
	if (radio->tx_sound && radio->tx_sound != radio->rx_sound) 
		rc = sound_start(radio->tx_sound);
#endif

	return rc;
}

int radio_tx(radio_t *radio, float *baseband, int signal_num)
{
	int i;
	int __attribute__((unused)) rc;
	int audio_num;
	sample_t *audio_samples[2];
	sample_t *signal_samples[3];
	uint8_t *signal_power;

	if (signal_num > radio->latspl) {
		PDEBUG(DRADIO, DEBUG_ERROR, "signal_num > latspl, please fix!.\n");
		abort();
	}

	/* audio buffers: how many sample for audio (rounded down) */
	audio_num = (int)((double)signal_num / radio->tx_resampler[0].factor);
	if (audio_num > radio->audio_buffer_size) {
		PDEBUG(DRADIO, DEBUG_ERROR, "audio_num > audio_buffer_size, please fix!.\n");
		abort();
	}
	audio_samples[0] = radio->audio_buffer;
	audio_samples[1] = radio->audio_buffer + radio->audio_buffer_size;

	/* signal buffers: a bit more samples to be safe */
	signal_num = (int)((double)audio_num * radio->tx_resampler[0].factor + 0.5) + 10;
	if (signal_num > radio->signal_buffer_size) {
		PDEBUG(DRADIO, DEBUG_ERROR, "signal_num > signal_buffer_size, please fix!.\n");
		abort();
	}
	signal_samples[0] = radio->signal_buffer;
	signal_samples[1] = radio->signal_buffer + radio->signal_buffer_size;
	signal_samples[2] = radio->signal_buffer + radio->signal_buffer_size * 2;
	signal_power = radio->signal_power_buffer;

	/* get audio to be sent */
	switch (radio->tx_audio_mode) {
	case AUDIO_MODE_WAVEFILE:
		wave_read(&radio->wave_tx_play, audio_samples, audio_num);
		
		if (!radio->wave_tx_play.left) {
			int rc;
			int _samplerate = 0;
			wave_destroy_playback(&radio->wave_tx_play);
			rc = wave_create_playback(&radio->wave_tx_play, radio->tx_wave_file, &_samplerate, &radio->tx_audio_channels, 1.0);
			if (rc < 0) {
				PDEBUG(DRADIO, DEBUG_ERROR, "Failed to re-open wave file.\n");
				return rc;
			}
		}
		break;
#ifdef HAVE_ALSA
	case AUDIO_MODE_AUDIODEV:
		rc = sound_read(radio->tx_sound, audio_samples, radio->audio_buffer_size, radio->tx_audio_channels, NULL);
		if (rc < 0) {
			PDEBUG(DRADIO, DEBUG_ERROR, "Failed to read from sound device (rc = %d)!\n", audio_num);
			if (rc == -EPIPE)
				PDEBUG(DRADIO, DEBUG_ERROR, "Trying to recover.\n");
			else
				return 0;
		}
		jitter_save(&radio->tx_dejitter[0], audio_samples[0], rc);
		jitter_load(&radio->tx_dejitter[0], audio_samples[0], audio_num);
		if (radio->tx_audio_channels == 2) {
			jitter_save(&radio->tx_dejitter[1], audio_samples[1], rc);
			jitter_load(&radio->tx_dejitter[1], audio_samples[1], audio_num);
		}
		break;
#endif
	case AUDIO_MODE_TESTTONE:
		for (i = 0; i < audio_num; i++) {
			audio_samples[0][i] = radio->testtone[0][radio->testtone_pos];
			audio_samples[1][i] = radio->testtone[1][radio->testtone_pos];
			radio->testtone_pos = (radio->testtone_pos + 1) % radio->testtone_length;
		}
		break;
	default:
		PDEBUG(DRADIO, DEBUG_ERROR, "Wrong audio mode, plese fix!\n");
		return -EINVAL;
	}

	/* convert mono/stereo, generate differential signal */
	if (radio->stereo && radio->tx_audio_channels == 1) {
		/* mono to stereo: sum is 90%, differential signal is 0 */
		for (i = 0; i < audio_num; i++) {
			audio_samples[0][i] = 0.9;
			audio_samples[1][i] = 0.0;
		}
	}
	if (radio->stereo && radio->tx_audio_channels == 2) {
		/* stereo: sum is 90%, diffential is 90% */
		double left, right;
		for (i = 0; i < audio_num; i++) {
			left = audio_samples[0][i];
			right = audio_samples[1][i];
			audio_samples[0][i] = (left + right) * 0.45;
			audio_samples[1][i] = (left - right) * 0.45;
		}
	}
	if (!radio->stereo && radio->tx_audio_channels == 2) {
		/* stereo to mono: sum both channel */
		for (i = 0; i < audio_num; i++)
			audio_samples[0][i] = (audio_samples[0][i] + audio_samples[1][i]) / 2.0;
	}

	/* remove DC */
	iir_process(&radio->tx_dc_removal[0], audio_samples[0], audio_num);
	if (radio->stereo)
		iir_process(&radio->tx_dc_removal[1], audio_samples[1], audio_num);

	/* gain volume */
	if (radio->volume != 1.0) {
		for (i = 0; i < audio_num; i++)
			audio_samples[0][i] *= radio->volume;
		if (radio->stereo) {
			for (i = 0; i < audio_num; i++)
				audio_samples[1][i] *= radio->volume;
		}
	}

	/* upsample */
	signal_num = samplerate_upsample(&radio->tx_resampler[0], audio_samples[0], audio_num, signal_samples[0]);
	if (radio->stereo)
		samplerate_upsample(&radio->tx_resampler[1], audio_samples[1], audio_num, signal_samples[1]);

	/* prepare baseband */
	memset(baseband, 0, sizeof(float) * 2 * signal_num);
	memset(signal_power, 1, signal_num);

	/* filter audio (remove DC, remove high frequencies, pre-emphasis)
	 * and modulate */
	switch (radio->modulation) {
	case MODULATION_FM:
		if (radio->emphasis)
			pre_emphasis(&radio->fm_emphasis[0], signal_samples[0], signal_num);
		clipper_process(signal_samples[0], signal_num);
		if (radio->stereo) {
			if (radio->emphasis)
				pre_emphasis(&radio->fm_emphasis[1], signal_samples[1], signal_num);
			clipper_process(signal_samples[1], signal_num);
			/* add pilot tone */
			double phasestep = radio->pilot_phasestep;
			double phase = radio->tx_pilot_phase;
			for (i = 0; i < signal_num; i++) {
				signal_samples[0][i] += sin(phase) * 0.1;
				signal_samples[0][i] += signal_samples[1][i] * sin(phase * 2);
				phase += phasestep;
				if (phase >= 2.0 * M_PI)
					phase -= 2.0 * M_PI;
			}
			radio->tx_pilot_phase = phase;
		}
		for (i = 0; i < signal_num; i++)
			signal_samples[0][i] *= radio->fm_deviation;
		fm_modulate_complex(&radio->fm_mod, signal_samples[0], signal_power, signal_num, baseband);
		break;
	case MODULATION_AM_DSB:
		/* also clip to prevent overshooting after audio filtering */
		clipper_process(signal_samples[0], signal_num);
		iir_process(&radio->tx_am_bw_limit, signal_samples[0], signal_num);
		am_modulate_complex(&radio->am_mod, signal_samples[0], signal_power, signal_num, baseband);
		break;
	case MODULATION_AM_USB:
	case MODULATION_AM_LSB:
		/* also clip to prevent overshooting after audio filtering */
		clipper_process(signal_samples[0], signal_num);
		iir_process(&radio->tx_am_bw_limit, signal_samples[0], signal_num);
		am_modulate_complex(&radio->am_mod, signal_samples[0], signal_power, signal_num, baseband);
		break;
	default:
		break;
	}

	return signal_num;
}

int radio_rx(radio_t *radio, float *baseband, int signal_num)
{
	int i;
	int audio_num;
	sample_t *samples[3];
	double p;

	if (signal_num > radio->latspl) {
		PDEBUG(DRADIO, DEBUG_ERROR, "signal_num > latspl, please fix!.\n");
		abort();
	}

	if (signal_num > radio->signal_buffer_size) {
		PDEBUG(DRADIO, DEBUG_ERROR, "signal_num > signal_buffer_size, please fix!.\n");
		abort();
	}
	samples[0] = radio->signal_buffer;
	samples[1] = radio->signal_buffer + radio->signal_buffer_size;
	samples[2] = radio->signal_buffer + radio->signal_buffer_size * 2;

	switch (radio->modulation) {
	case MODULATION_FM:
		fm_demodulate_complex(&radio->fm_demod, samples[0], signal_num, baseband, radio->I_buffer, radio->Q_buffer);
		for (i = 0; i < signal_num; i++)
			samples[0][i] /= radio->fm_deviation;
		if (radio->stereo) {
			/* filter pilot tone */
			p = radio->rx_pilot_phase; /* don't increment in radio structure, will be done later */
			for (i = 0; i < signal_num; i++) {
				samples[1][i] = samples[0][i] * cos(p); /* I */
				samples[2][i] = samples[0][i] * sin(p); /* Q */
				p += radio->pilot_phasestep;
				if (p >= 2.0 * M_PI)
				p -= 2.0 * M_PI;
			}
			iir_process(&radio->rx_lp_pilot_I, samples[1], signal_num);
			iir_process(&radio->rx_lp_pilot_Q, samples[2], signal_num);
			/* mix pilot tone (double phase) with differential signal */
			for (i = 0; i < signal_num; i++) {
				p = atan2(samples[2][i], samples[1][i]);
				/* subtract measured phase difference (use double amplitude, because we filter later) */
			        samples[1][i] = samples[0][i] * sin((radio->rx_pilot_phase - p) * 2.0) * 2.0;
				radio->rx_pilot_phase += radio->pilot_phasestep;
				if (radio->rx_pilot_phase >= 2.0 * M_PI)
				radio->rx_pilot_phase -= 2.0 * M_PI;
			}
			/* filter to match bandwidth */
			iir_process(&radio->rx_lp_sum, samples[0], signal_num);
			iir_process(&radio->rx_lp_diff, samples[1], signal_num);
		}
		if (radio->emphasis) {
			dc_filter(&radio->fm_emphasis[0], samples[0], signal_num);
			de_emphasis(&radio->fm_emphasis[0], samples[0], signal_num);
			if (radio->stereo) {
				dc_filter(&radio->fm_emphasis[1], samples[1], signal_num);
				de_emphasis(&radio->fm_emphasis[1], samples[1], signal_num);
			}
		}
		break;
	case MODULATION_AM_DSB:
		am_demodulate_complex(&radio->am_demod, samples[0], signal_num, baseband, radio->I_buffer, radio->Q_buffer, radio->carrier_buffer);
		break;
	case MODULATION_AM_USB:
	case MODULATION_AM_LSB:
		am_demodulate_complex(&radio->am_demod, samples[0], signal_num, baseband, radio->I_buffer, radio->Q_buffer, radio->carrier_buffer);
		break;
	default:
		break;
	}

	/* downsample */
	audio_num = samplerate_downsample(&radio->rx_resampler[0], samples[0], signal_num);
	if (radio->stereo)
		samplerate_downsample(&radio->rx_resampler[1], samples[1], signal_num);

	/* dampen volume */
	if (radio->volume != 1.0) {
		for (i = 0; i < audio_num; i++)
			samples[0][i] /= radio->volume;
		if (radio->stereo) {
			for (i = 0; i < audio_num; i++)
				samples[1][i] /= radio->volume;
		}
	}

	/* convert mono/stereo, (from differential signal) */
	if (radio->stereo && radio->rx_audio_channels == 1) {
		/* stereo to mono */
		for (i = 0; i < audio_num; i++) {
			samples[0][i] = (samples[0][i] + samples[1][i]) / 2.0;
		}
	}
	if (radio->stereo && radio->rx_audio_channels == 2) {
		/* stereo from differential */
		double sum, diff;
		for (i = 0; i < audio_num; i++) {
			sum = samples[0][i];
			diff = samples[1][i];
			samples[0][i] = sum + diff / 2.0;
			samples[1][i] = sum - diff / 2.0;
		}
	}
	if (!radio->stereo && radio->rx_audio_channels == 2) {
		/* mono to stereo: clone channel */
		for (i = 0; i < audio_num; i++)
			samples[1][i] = samples[0][i];
	}

	/* store received audio */
	switch (radio->rx_audio_mode) {
	case AUDIO_MODE_WAVEFILE:
		wave_write(&radio->wave_rx_rec, samples, audio_num);
		break;
#ifdef HAVE_ALSA
	case AUDIO_MODE_AUDIODEV:
		jitter_save(&radio->rx_dejitter[0], samples[0], audio_num);
		if (radio->rx_audio_channels == 2)
			jitter_save(&radio->rx_dejitter[1], samples[1], audio_num);
		audio_num = sound_get_tosend(radio->rx_sound, radio->signal_buffer_size);
		jitter_load(&radio->rx_dejitter[0], samples[0], audio_num);
		if (radio->rx_audio_channels == 2)
			jitter_load(&radio->rx_dejitter[1], samples[1], audio_num);
		audio_num = sound_write(radio->rx_sound, samples, NULL, audio_num, NULL, NULL, radio->rx_audio_channels);
		if (audio_num < 0) {
			PDEBUG(DRADIO, DEBUG_ERROR, "Failed to write to sound device (rc = %d)!\n", audio_num);
			if (audio_num == -EPIPE)
				PDEBUG(DRADIO, DEBUG_ERROR, "Trying to recover.\n");
			else
				return 0;
		}
		break;
#endif
	default:
		PDEBUG(DRADIO, DEBUG_ERROR, "Wrong audio mode, plese fix!\n");
		return -EINVAL;
	}

	return signal_num;
}

