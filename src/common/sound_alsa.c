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
#include "debug.h"
#include "sound.h"

typedef struct sound {
	snd_pcm_t *phandle, *chandle;
	int pchannels, cchannels;
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
	if (rrate != samplerate) {
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

void *sound_open(const char *device, int samplerate)
{
	sound_t *sound;
	int rc;

	sound = calloc(1, sizeof(sound_t));
	if (!sound) {
		PDEBUG(DSOUND, DEBUG_ERROR, "Failed to alloc memory!\n");
		return NULL;
	}

	rc = snd_pcm_open(&sound->phandle, device, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
	if (rc < 0) {
		PDEBUG(DSOUND, DEBUG_ERROR, "Failed to open '%s' for playback! (%s)\n", device, snd_strerror(rc));
		goto error;
	}

	rc = snd_pcm_open(&sound->chandle, device, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
	if (rc < 0) {
		PDEBUG(DSOUND, DEBUG_ERROR, "Failed to open '%s' for capture! (%s)\n", device, snd_strerror(rc));
		goto error;
	}

	rc = set_hw_params(sound->phandle, samplerate, &sound->pchannels);
	if (rc < 0) {
		PDEBUG(DSOUND, DEBUG_ERROR, "Failed to set playback hw params\n");
		goto error;
	}
	PDEBUG(DSOUND, DEBUG_DEBUG, "Playback with %d channels.\n", sound->pchannels);

	rc = set_hw_params(sound->chandle, samplerate, &sound->cchannels);
	if (rc < 0) {
		PDEBUG(DSOUND, DEBUG_ERROR, "Failed to set capture hw params\n");
		goto error;
	}
	PDEBUG(DSOUND, DEBUG_DEBUG, "Capture with %d channels.\n", sound->cchannels);

	rc = snd_pcm_prepare(sound->phandle);
	if (rc < 0) {
		PDEBUG(DSOUND, DEBUG_ERROR, "cannot prepare audio interface for use (%s)\n", snd_strerror(rc));
		goto error;
	}

	rc = snd_pcm_prepare(sound->chandle);
	if (rc < 0) {
		PDEBUG(DSOUND, DEBUG_ERROR, "cannot prepare audio interface for use (%s)\n", snd_strerror(rc));
		goto error;
	}

	/* trigger capturing */
	int16_t buff[2];
	snd_pcm_readi(sound->chandle, buff, 1);

	return sound;

error:
	sound_close(sound);
	return NULL;
}

void sound_close(void *inst)
{
	sound_t *sound = (sound_t *)inst;

	if (sound->phandle > 0)
		snd_pcm_close(sound->phandle);
	if (sound->chandle > 0)
		snd_pcm_close(sound->chandle);
	free(sound);
}

int sound_write(void *inst, int16_t *samples_left, int16_t *samples_right, int num)
{
	sound_t *sound = (sound_t *)inst;
	int16_t buff[num << 1], *samples;
	int rc;
	int i, ii;

	if (sound->pchannels == 2) {

		for (i = 0, ii = 0; i < num; i++) {
			buff[ii++] = *samples_right++;
			buff[ii++] = *samples_left++;
		}
		samples = buff;
	} else
		samples = samples_left;

	rc = snd_pcm_writei(sound->phandle, samples, num);
	if (rc < 0) {
		PDEBUG(DSOUND, DEBUG_ERROR, "failed to write audio to interface (%s)\n", snd_strerror(rc));
		if (rc == -EPIPE)
			snd_pcm_prepare(sound->phandle);
		return rc;
	}

	if (rc != num)
		PDEBUG(DSOUND, DEBUG_ERROR, "short write to audio interface, written %d bytes, got %d bytes\n", num, rc);

	return rc;
}

int sound_read(void *inst, int16_t *samples_left, int16_t *samples_right, int num)
{
	sound_t *sound = (sound_t *)inst;
	int16_t buff[num << 1];
	int in, rc;
	int i, ii;

	/* get samples in rx buffer */
	in = snd_pcm_avail(sound->chandle);
	/* if less than 2 frames available, try next time */
	if (in < 2)
		return 0;
	/* read one frame less than in buffer, because snd_pcm_readi() seems
	 * to corrupt last frame */
	in--;
	if (in > num)
		in = num;

	if (sound->cchannels == 2)
		rc = snd_pcm_readi(sound->chandle, buff, in);
	else
		rc = snd_pcm_readi(sound->chandle, samples_left, in);

	if (rc < 0) {
		if (errno == EAGAIN)
			return 0;
		PDEBUG(DSOUND, DEBUG_ERROR, "failed to read audio from interface (%s)\n", snd_strerror(rc));
		/* recover read */
		if (rc == -EPIPE)
			snd_pcm_prepare(sound->chandle);
		return rc;
	}

	if (sound->cchannels == 2) {
		for (i = 0, ii = 0; i < rc; i++) {
			*samples_right++ = buff[ii++];
			*samples_left++ = buff[ii++];
		}
	} else
		memcpy(samples_right, samples_left, num << 1);

	return rc;
}

/* 
 * get playback buffer fill
 *
 * return number of frames */
int sound_get_inbuffer(void *inst)
{
	sound_t *sound = (sound_t *)inst;
	int rc;
	snd_pcm_sframes_t delay;

	rc = snd_pcm_delay(sound->phandle, &delay);
	if (rc < 0) {
		if (rc == -32)
			PDEBUG(DSOUND, DEBUG_ERROR, "Buffer underrun: Please use higher latency and enable real time scheduling\n");
		else
			PDEBUG(DSOUND, DEBUG_ERROR, "failed to get delay from interface (%s)\n", snd_strerror(rc));
		if (rc == -EPIPE)
			snd_pcm_prepare(sound->phandle);
		return rc;
	}

	return delay;
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

