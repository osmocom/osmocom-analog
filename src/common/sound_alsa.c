/* Sound device access
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

#include <stdlib.h>
#include <stdint.h>
#include <alsa/asoundlib.h>
#include "sample.h"
#include "debug.h"
#include "sender.h"

typedef struct sound {
	snd_pcm_t *phandle, *chandle;
	int pchannels, cchannels;
	double spl_deviation;		/* how much deviation is one sample step */
	double paging_phaseshift;	/* phase to shift every sample */
	double paging_phase;	 	/* current phase */
} sound_t;

static int set_hw_params(snd_pcm_t *handle, int samplerate, int *channels)
{
	snd_pcm_hw_params_t *hw_params = NULL;
	int rc;
	unsigned int rrate;

	rc = snd_pcm_hw_params_malloc(&hw_params);
	if (rc < 0) {
		PDEBUG(DSOUND, DEBUG_ERROR, "Failed to allocate hw_params! (%s)\n", snd_strerror(rc));
		goto error;
	}

	rc = snd_pcm_hw_params_any(handle, hw_params);
	if (rc < 0) {
		PDEBUG(DSOUND, DEBUG_ERROR, "cannot initialize hardware parameter structure (%s)\n", snd_strerror(rc));
		goto error;
	}

	rc = snd_pcm_hw_params_set_rate_resample(handle, hw_params, 0);
	if (rc < 0) {
		PDEBUG(DSOUND, DEBUG_ERROR, "cannot set real hardware rate (%s)\n", snd_strerror(rc));
		goto error;
	}

	rc = snd_pcm_hw_params_set_access (handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (rc < 0) {
		PDEBUG(DSOUND, DEBUG_ERROR, "cannot set access to interleaved (%s)\n", snd_strerror(rc));
		goto error;
	}

	rc = snd_pcm_hw_params_set_format(handle, hw_params, SND_PCM_FORMAT_S16);
	if (rc < 0) {
		PDEBUG(DSOUND, DEBUG_ERROR, "cannot set sample format (%s)\n", snd_strerror(rc));
		goto error;
	}

	rrate = samplerate;
	rc = snd_pcm_hw_params_set_rate_near(handle, hw_params, &rrate, 0);
	if (rc < 0) {
		PDEBUG(DSOUND, DEBUG_ERROR, "cannot set sample rate (%s)\n", snd_strerror(rc));
		goto error;
	}
	if ((int)rrate != samplerate) {
		PDEBUG(DSOUND, DEBUG_ERROR, "Rate doesn't match (requested %dHz, get %dHz)\n", samplerate, rrate);
		rc = -EIO;
		goto error;
	}

	*channels = 1;
	rc = snd_pcm_hw_params_set_channels(handle, hw_params, *channels);
	if (rc < 0) {
		*channels = 2;
		rc = snd_pcm_hw_params_set_channels(handle, hw_params, *channels);
		if (rc < 0) {
			PDEBUG(DSOUND, DEBUG_ERROR, "cannot set channel count to 1 nor 2 (%s)\n", snd_strerror(rc));
			goto error;
		}
	}

	rc = snd_pcm_hw_params(handle, hw_params);
	if (rc < 0) {
		PDEBUG(DSOUND, DEBUG_ERROR, "cannot set parameters (%s)\n", snd_strerror(rc));
		goto error;
	}

	snd_pcm_hw_params_free(hw_params);

	return 0;

error:
	if (hw_params) {
		snd_pcm_hw_params_free(hw_params);
	}

	return rc;
}

static int sound_prepare(sound_t *sound)
{
	int rc;

	rc = snd_pcm_prepare(sound->phandle);
	if (rc < 0) {
		PDEBUG(DSOUND, DEBUG_ERROR, "cannot prepare audio interface for use (%s)\n", snd_strerror(rc));
		return rc;
	}

	rc = snd_pcm_prepare(sound->chandle);
	if (rc < 0) {
		PDEBUG(DSOUND, DEBUG_ERROR, "cannot prepare audio interface for use (%s)\n", snd_strerror(rc));
		return rc;
	}

	return 0;
}

void *sound_open(const char *audiodev, double __attribute__((unused)) *tx_frequency, double __attribute__((unused)) *rx_frequency, int channels, double __attribute__((unused)) paging_frequency, int samplerate, double max_deviation, double __attribute__((unused)) max_modulation)
{
	sound_t *sound;
	int rc;

	if (channels < 1 || channels > 2) {
		PDEBUG(DSOUND, DEBUG_ERROR, "Cannot use more than two channels with the same sound card!\n");
		return NULL;
	}

	sound = calloc(1, sizeof(sound_t));
	if (!sound) {
		PDEBUG(DSOUND, DEBUG_ERROR, "Failed to alloc memory!\n");
		return NULL;
	}

	sound->spl_deviation = max_deviation / 32767.0;
	sound->paging_phaseshift = 1.0 / ((double)samplerate / 1000.0);

	rc = snd_pcm_open(&sound->phandle, audiodev, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
	if (rc < 0) {
		PDEBUG(DSOUND, DEBUG_ERROR, "Failed to open '%s' for playback! (%s)\n", audiodev, snd_strerror(rc));
		goto error;
	}

	rc = snd_pcm_open(&sound->chandle, audiodev, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
	if (rc < 0) {
		PDEBUG(DSOUND, DEBUG_ERROR, "Failed to open '%s' for capture! (%s)\n", audiodev, snd_strerror(rc));
		goto error;
	}

	rc = set_hw_params(sound->phandle, samplerate, &sound->pchannels);
	if (rc < 0) {
		PDEBUG(DSOUND, DEBUG_ERROR, "Failed to set playback hw params\n");
		goto error;
	}
	if (sound->pchannels < channels) {
		PDEBUG(DSOUND, DEBUG_ERROR, "Sound card only supports %d channel for playback.\n", sound->pchannels);
		goto error;
	}
	PDEBUG(DSOUND, DEBUG_DEBUG, "Playback with %d channels.\n", sound->pchannels);

	rc = set_hw_params(sound->chandle, samplerate, &sound->cchannels);
	if (rc < 0) {
		PDEBUG(DSOUND, DEBUG_ERROR, "Failed to set capture hw params\n");
		goto error;
	}
	if (sound->cchannels < channels) {
		PDEBUG(DSOUND, DEBUG_ERROR, "Sound card only supports %d channel for capture.\n", sound->cchannels);
		goto error;
	}
	PDEBUG(DSOUND, DEBUG_DEBUG, "Capture with %d channels.\n", sound->cchannels);

	rc = sound_prepare(sound);
	if (rc < 0)
		goto error;

	return sound;

error:
	sound_close(sound);
	return NULL;
}

/* start streaming */
int sound_start(void *inst)
{
	sound_t *sound = (sound_t *)inst;
	int16_t buff[2];

	/* trigger capturing */
	snd_pcm_readi(sound->chandle, buff, 1);

	return 0;
}

void sound_close(void *inst)
{
	sound_t *sound = (sound_t *)inst;

	if (sound->phandle != NULL)
		snd_pcm_close(sound->phandle);
	if (sound->chandle != NULL)
		snd_pcm_close(sound->chandle);
	free(sound);
}

static void gen_paging_tone(sound_t *sound, int16_t *samples, int length, enum paging_signal paging_signal, int on)
{
	double phaseshift, phase;
	int i;

	switch (paging_signal) {
	case PAGING_SIGNAL_NOTONE:
		/* no tone if paging signal is on */
		on = !on;
		// fall through
	case PAGING_SIGNAL_TONE:
		/* tone if paging signal is on */
		if (on) {
			phaseshift = sound->paging_phaseshift;
			phase = sound->paging_phase;
			for (i = 0; i < length; i++) {
				if (phase < 0.5)
					*samples++ = 30000;
				else
					*samples++ = -30000;
				phase += phaseshift;
				if (phase >= 1.0)
					phase -= 1.0;
			}
			sound->paging_phase = phase;
		} else
			memset(samples, 0, length << 1);
		break;
	case PAGING_SIGNAL_NEGATIVE:
		/* negative signal if paging signal is on */
		on = !on;
		// fall through
	case PAGING_SIGNAL_POSITIVE:
		/* positive signal if paging signal is on */
		if (on)
			memset(samples, 127, length << 1);
		else
			memset(samples, 128, length << 1);
		break;
	case PAGING_SIGNAL_NONE:
		break;
	}
}

int sound_write(void *inst, sample_t **samples, uint8_t __attribute__((unused)) **power, int num, enum paging_signal *paging_signal, int *on, int channels)
{
	sound_t *sound = (sound_t *)inst;
	double spl_deviation = sound->spl_deviation;
	int32_t value;
	int16_t buff[num << 1];
	int rc;
	int i, ii;

	if (sound->pchannels == 2) {
		/* two channels */
		if (paging_signal && on && paging_signal[0] != PAGING_SIGNAL_NONE) {
			int16_t paging[num << 1];
			gen_paging_tone(sound, paging, num, paging_signal[0], on[0]);
			for (i = 0, ii = 0; i < num; i++) {
				value = samples[0][i] / spl_deviation;
				if (value > 32767)
					value = 32767;
				else if (value < -32767)
					value = -32767;
				buff[ii++] = value;
				buff[ii++] = paging[i];
			}
		} else if (channels == 2) {
			for (i = 0, ii = 0; i < num; i++) {
				value = samples[0][i] / spl_deviation;
				if (value > 32767)
					value = 32767;
				else if (value < -32767)
					value = -32767;
				buff[ii++] = value;
				value = samples[1][i] / spl_deviation;
				if (value > 32767)
					value = 32767;
				else if (value < -32767)
					value = -32767;
				buff[ii++] = value;
			}
		} else {
			for (i = 0, ii = 0; i < num; i++) {
				value = samples[0][i] / spl_deviation;
				if (value > 32767)
					value = 32767;
				else if (value < -32767)
					value = -32767;
				buff[ii++] = value;
				buff[ii++] = value;
			}
		}
	} else {
		/* one channel */
		for (i = 0, ii = 0; i < num; i++) {
			value = samples[0][i] / spl_deviation;
			if (value > 32767)
				value = 32767;
			else if (value < -32767)
				value = -32767;
			buff[ii++] = value;
		}
	}
	rc = snd_pcm_writei(sound->phandle, buff, num);

	if (rc < 0) {
		PDEBUG(DSOUND, DEBUG_ERROR, "failed to write audio to interface (%s)\n", snd_strerror(rc));
		if (rc == -EPIPE) {
			sound_prepare(sound);
			sound_start(sound);
		}
		return rc;
	}

	if (rc != num)
		PDEBUG(DSOUND, DEBUG_ERROR, "short write to audio interface, written %d bytes, got %d bytes\n", num, rc);

	return rc;
}

#define KEEP_FRAMES	8	/* minimum frames not to read, due to bug in ALSA */

int sound_read(void *inst, sample_t **samples, int num, int channels)
{
	sound_t *sound = (sound_t *)inst;
	double spl_deviation = sound->spl_deviation;
	int16_t buff[num << 1];
	int32_t spl;
	int in, rc;
	int i, ii;

	/* get samples in rx buffer */
	in = snd_pcm_avail(sound->chandle);
	/* if not more than KEEP_FRAMES frames available, try next time */
	if (in <= KEEP_FRAMES)
		return 0;
	/* read some frames less than in buffer, because snd_pcm_readi() seems
	 * to corrupt last frames */
	in -= KEEP_FRAMES;
	if (in > num)
		in = num;

	rc = snd_pcm_readi(sound->chandle, buff, in);

	if (rc < 0) {
		if (errno == EAGAIN)
			return 0;
		PDEBUG(DSOUND, DEBUG_ERROR, "failed to read audio from interface (%s)\n", snd_strerror(rc));
		/* recover read */
		if (rc == -EPIPE) {
			sound_prepare(sound);
			sound_start(sound);
		}
		return rc;
	}

	if (sound->cchannels == 2) {
		if (channels < 2) {
			for (i = 0, ii = 0; i < rc; i++) {
				spl = buff[ii++];
				spl += buff[ii++];
				samples[0][i] = (double)spl * spl_deviation;
			}
		} else {
			for (i = 0, ii = 0; i < rc; i++) {
				samples[0][i] = (double)buff[ii++] * spl_deviation;
				samples[1][i] = (double)buff[ii++] * spl_deviation;
			}
		}
	} else {
		for (i = 0, ii = 0; i < rc; i++) {
			samples[0][i] = (double)buff[ii++] * spl_deviation;
		}
	}

	return rc;
}

/* 
 * get playback buffer space
 *
 * return number of samples to be sent */
int sound_get_tosend(void *inst, int latspl)
{
	sound_t *sound = (sound_t *)inst;
	int rc;
	snd_pcm_sframes_t delay;
	int tosend;

	rc = snd_pcm_delay(sound->phandle, &delay);
	if (rc < 0) {
		if (rc == -32)
			PDEBUG(DSOUND, DEBUG_ERROR, "Buffer underrun: Please use higher latency and enable real time scheduling\n");
		else
			PDEBUG(DSOUND, DEBUG_ERROR, "failed to get delay from interface (%s)\n", snd_strerror(rc));
		if (rc == -EPIPE) {
			sound_prepare(sound);
			sound_start(sound);
		}
		return rc;
	}

	tosend = latspl - delay;
	return tosend;
}

int sound_is_stereo_capture(void *inst)
{
	sound_t *sound = (sound_t *)inst;

	if (sound->cchannels == 2)
		return 1;
	return 0;
}

int sound_is_stereo_playback(void *inst)
{
	sound_t *sound = (sound_t *)inst;

	if (sound->pchannels == 2)
		return 1;
	return 0;
}

