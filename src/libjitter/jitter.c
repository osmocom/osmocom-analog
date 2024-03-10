/* Jitter buffering functions
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

/* How does it work:
 *
 * Storing:
 *
 * Each saved frame is sorted into the list of packages by their timestamp.
 *
 * The first packet will be stored with a timestamp offset of minimum jitter
 * window size or half of the target size, depending on the adaptive jitter
 * buffer flag.
 * 
 * Packets with the same timestamp are dropped.
 *
 * Early packts that exceed maximum jitter window size cause jitter
 * window to shift into the future.
 *
 * Late packets cause jitter window to shift into the past (allowing more
 * delay). Minimum jitter window size is added also, to prevent subsequent
 * packets from beeing late too.
 *
 * If adaptive jitter buffer is used, a delay that exceed the target size
 * is reduced to the target size.
 *
 * If ssrc changes, the buffer is reset, but not locked again.
 *
 *
 * Loading:
 *
 * jitter_offset() will return the number of samples between the jitter buffer's head and the first packet afterwards. Packets that already passed the jitter buffer's head are ignored. If no frame is ahead the jitter buffer's head, a negative value is returned.
 *
 * jitter_load() will remove and return the frame at the jitter buffer's head. Packet that already passed the jitter buffer's head are deleted. If no frame matches the jitter buffer's head, NULL is returned.
 *
 * jitter_advance() will advance the jitter buffer's head by the given number of samples.
 *
 * jitter_load_samples() will read decoded samples from jitter buffer's frames.
 * This means that that the decoder of each frame must generate samples of equal type and size.
 * If there is a gap between jitter buffer's head and the next frame, the samples are taken from the last frame.
 * The conceal function is called in this case, to extrapolate the missing samples.
 * If no conceal function is given, the last frame is repeated.
 * If there is no gap between jitter buffer's head and the next frame, the frame is decoded and the samples are taken from that frame.
 * After that the jitter buffer's head is advanced by the number of samples read.
 *
 * *TBD*
 *
 *
 * Unlocking:
 *
 * If the buffer is created or reset, the buffer is locked, so no packets are
 * stored. When the loading routine is called, the buffer is unlocked. This
 * prevents from filling the buffer before loading is performed, which would
 * cause high delay.
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "../libsample/sample.h"
#include "../liblogging/logging.h"
#include "jitter.h"

#define INITIAL_DELAY_INTERVAL	0.5
#define REPEAT_DELAY_INTERVAL	3.0

/* uncomment to enable heavy debugging */
//#define HEAVY_DEBUG
//#define VISUAL_DEBUG

static int unnamed_count = 1;

/* create jitter buffer */
int jitter_create(jitter_t *jb, const char *name, double samplerate, double target_window_duration, double max_window_duration, uint32_t window_flags)
{
	int rc = 0;

	memset(jb, 0, sizeof(*jb));

	/* optionally give a string to be show with the debug */
	if (name && *name)
		snprintf(jb->name, sizeof(jb->name) - 1, "(%s) ", name);
	else
		snprintf(jb->name, sizeof(jb->name) - 1, "(unnamed %d) ", unnamed_count++);

	jb->sample_duration = 1.0 / samplerate;
	jb->samples_20ms = samplerate / 50;
	jb->target_window_size = (int)ceil(target_window_duration / jb->sample_duration);
	jb->max_window_size = (int)ceil(max_window_duration / jb->sample_duration);
	jb->window_flags = window_flags;

	jitter_reset(jb);

	LOGP(DJITTER, LOGL_INFO, "%s Created jitter buffer. (samperate=%.0f, target_window=%.0fms, max_window=%.0fms, flag:latency=%s flag:repeat=%s)\n",
		jb->name,
		samplerate,
		(double)jb->target_window_size * jb->sample_duration * 1000.0,
		(double)jb->max_window_size * jb->sample_duration * 1000.0,
		(window_flags & JITTER_FLAG_LATENCY) ? "true" : "false",
		(window_flags & JITTER_FLAG_REPEAT) ? "true" : "false");

	return rc;
}

/* reset jitter buffer */
void jitter_reset(jitter_t *jb)
{
	jitter_frame_t *jf, *temp;

	LOGP(DJITTER, LOGL_INFO, "%s Reset jitter buffer.\n", jb->name);

	/* jitter buffer locked */
	jb->unlocked = false;

	/* window becomes invalid */
	jb->window_valid = false;

	/* remove all pending frames */
	jf = jb->frame_list;
	while(jf) {
		temp = jf;
		jf = jf->next;
		free(temp);
	}
	jb->frame_list = NULL;

	/* remove current sample buffer */
	free(jb->spl_buf);
	jb->spl_buf = NULL;
	jb->spl_valid = false;
}

void jitter_destroy(jitter_t *jb)
{
	jitter_reset(jb);

	LOGP(DJITTER, LOGL_INFO, "%s Destroying jitter buffer.\n", jb->name);
}

jitter_frame_t *jitter_frame_alloc(void (*decoder)(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void *priv), void *decoder_priv, uint8_t *data, int size, uint8_t marker, uint16_t sequence, uint32_t timestamp, uint32_t ssrc)
{
	jitter_frame_t *jf;

	jf = malloc(sizeof(*jf) + size);
	if (!jf) {
		LOGP(DJITTER, LOGL_ERROR, "No memory for frame.\n");
		return NULL;
	}
	memset(jf, 0, sizeof(*jf)); // note: clear header only
	jf->decoder = decoder;
	jf->decoder_priv = decoder_priv;
	memcpy(jf->data, data, size);
	jf->size = size;
	jf->marker = marker;
	jf->sequence = sequence;
	jf->timestamp = timestamp;
	jf->ssrc = ssrc;

	return jf;
}

void jitter_frame_free(jitter_frame_t *jf)
{
	free(jf);
}

void jitter_frame_get(jitter_frame_t *jf, void (**decoder)(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void *priv), void **decoder_priv, uint8_t **data, int *size, uint8_t *marker, uint16_t *sequence, uint32_t *timestamp, uint32_t *ssrc)
{
	if (decoder)
		*decoder = jf->decoder;
	if (decoder_priv)
		*decoder_priv = jf->decoder_priv;
	if (data)
		*data = jf->data;
	if (size)
		*size = jf->size;
	if (marker)
		*marker = jf->marker;
	if (sequence)
		*sequence = jf->sequence;
	if (timestamp)
		*timestamp = jf->timestamp;
	if (ssrc)
		*ssrc = jf->ssrc;
}

/* Store frame in jitterbuffer
 *
 * Use sequence number to order frames.
 * Use timestamp to handle delay.
 */
void jitter_save(jitter_t *jb, jitter_frame_t *jf)
{
	jitter_frame_t **jfp;
	int32_t offset_timestamp;

	/* ignore frames until the buffer is unlocked by jitter_load() */
	if (!jb->unlocked) {
		jitter_frame_free(jf);
		return;
	}

	/* first packet (with this ssrc) sets window size to target_window_size */
	if (!jb->window_valid || jb->window_ssrc != jf->ssrc) {
		if (!jb->window_valid)
			LOGP(DJITTER, LOGL_DEBUG, "%s Initial frame after init or reset.\n", jb->name);
		else
			LOGP(DJITTER, LOGL_DEBUG, "%s SSRC changed.\n", jb->name);
		// NOTE: Reset must be called before finding the frame location below, because there will be no frame in list anymore!
		jitter_reset(jb);
		jb->unlocked = true;
		/* when using dynamic jitter buffer, we use half of the target delay */
		if ((jb->window_flags & JITTER_FLAG_LATENCY)) {
			jb->window_timestamp = jf->timestamp - (uint32_t)jb->target_window_size / 2;
		} else {
			jb->window_timestamp = jf->timestamp - (uint32_t)jb->target_window_size;
	       	}
		jb->window_valid = true;
		jb->window_ssrc = jf->ssrc;
		jb->min_delay = -1;
		jb->delay_counter = 0.0;
		jb->delay_interval = INITIAL_DELAY_INTERVAL;
	}

	/* reduce delay */
	if (jb->delay_counter >= jb->delay_interval) {
		if (jb->min_delay >= 0)
			LOGP(DJITTER, LOGL_DEBUG, "%s Statistics: target_window_delay=%.0fms max_window_delay=%.0fms  current min_delay=%.0fms\n",
				jb->name,
				(double)jb->target_window_size * jb->sample_duration * 1000.0,
				(double)jb->max_window_size * jb->sample_duration * 1000.0,
				(double)jb->min_delay * jb->sample_duration * 1000.0);
		/* delay reduction, if minimum delay is greater than target jitter window size */
		if ((jb->window_flags & JITTER_FLAG_LATENCY) && jb->min_delay > jb->target_window_size) {
			LOGP(DJITTER, LOGL_DEBUG, "%s Reducing current minimum delay of %.0fms, because maximum delay is greater than target window size of %.0fms.\n",
				jb->name,
				(double)jb->min_delay * jb->sample_duration * 1000.0,
				(double)jb->target_window_size * jb->sample_duration * 1000.0);
			/* only reduce delay to half of the target window size */
			jb->window_timestamp += jb->min_delay - jb->target_window_size / 2;

		}
		jb->delay_counter -= jb->delay_interval;
		jb->delay_interval = REPEAT_DELAY_INTERVAL;
		jb->min_delay = -1;
	}

	/* find location where to put frame into the list, depending on sequence number */
	jfp = &jb->frame_list;
	while(*jfp) {
		offset_timestamp = (int16_t)(jf->timestamp - (*jfp)->timestamp);
		/* found double entry */
		if (offset_timestamp == 0) {
			LOGP(DJITTER, LOGL_DEBUG, "%s Dropping double packet (timestamp = %u)\n", jb->name, jf->timestamp);
			jitter_frame_free(jf);
			return;
		}
		/* offset is negative, so we found the position to insert frame */
		if (offset_timestamp < 0)
			break;
		jfp = &((*jfp)->next);
	}

	offset_timestamp = jf->timestamp - jb->window_timestamp;
#ifdef HEAVY_DEBUG
	LOGP(DJITTER, LOGL_DEBUG, "%sFrame has offset of %.0fms in jitter buffer.\n", jb->name, (double)offset_timestamp * jb->sample_duration * 1000.0);
#endif

	/* measure delay */
	if (jb->min_delay < 0 || offset_timestamp < jb->min_delay)
		jb->min_delay = offset_timestamp;

	/* if frame is too early (delay ceases), shift window to the future */
	if (offset_timestamp > jb->max_window_size) {
		if ((jb->window_flags & JITTER_FLAG_LATENCY)) {
			LOGP(DJITTER, LOGL_DEBUG, "%s Frame too early: Shift jitter buffer to the future, to make the frame fit to the end. (offset_sequence(%d) > max_window_size(%d))\n", jb->name, offset_timestamp, jb->max_window_size);
			/* shift window so it fits to the end of window */
			jb->window_timestamp = jf->timestamp - jb->max_window_size;
			jb->min_delay = -1;
			jb->delay_counter = 0.0;
			jb->delay_interval = REPEAT_DELAY_INTERVAL;
		} else {
			LOGP(DJITTER, LOGL_DEBUG, "%s Frame too early: Shift jitter buffer to the future, to make the frame fit to the target delay. (offset_sequence(%d) > max_window_size(%d))\n", jb->name, offset_timestamp, jb->max_window_size);
			/* shift window so frame fits to the start of window + target delay */
			jb->window_timestamp = jf->timestamp - jb->target_window_size;
			jb->min_delay = -1;
			jb->delay_counter = 0.0;
			jb->delay_interval = REPEAT_DELAY_INTERVAL;
		}
	}

	/* is frame is too late, shift window to the past. */
	if (offset_timestamp < 0) {
		if ((jb->window_flags & JITTER_FLAG_LATENCY)) {
			LOGP(DJITTER, LOGL_DEBUG, "%s Frame too late: Shift jitter buffer to the past, and add target window size. (offset_sequence(%d) < 0)\n", jb->name, offset_timestamp);
			/* shift window so frame fits to the start of window + half of target delay */
			jb->window_timestamp = jf->timestamp - jb->target_window_size / 2;
			jb->min_delay = -1;
			jb->delay_counter = 0.0;
			jb->delay_interval = REPEAT_DELAY_INTERVAL;
		} else {
			LOGP(DJITTER, LOGL_DEBUG, "%s Frame too late: Shift jitter buffer to the past, and add half target window size. (offset_sequence(%d) < 0)\n", jb->name, offset_timestamp);
			/* shift window so frame fits to the start of window + target delay */
			jb->window_timestamp = jf->timestamp - jb->target_window_size;
			jb->min_delay = -1;
			jb->delay_counter = 0.0;
			jb->delay_interval = REPEAT_DELAY_INTERVAL;
		}
	}

	/* insert or append frame */
#ifdef HEAVY_DEBUG
	#include <time.h>
	static struct timespec tv;
        clock_gettime(CLOCK_REALTIME, &tv);
	LOGP(DJITTER, LOGL_DEBUG, "%s Store frame. %ld.%04ld\n", jb->name, tv.tv_sec, tv.tv_nsec / 1000000);
#endif
	jf->next = *jfp;
	*jfp = jf;
}

/* get offset to next chunk, return -1, if there is no */
int32_t jitter_offset(jitter_t *jb)
{
	jitter_frame_t *jf;
	int16_t offset_timestamp = 0;

	/* now unlock jitter buffer */
	jb->unlocked = true;

	/* get timestamp of chunk that is not in the past */
	while ((jf = jb->frame_list)) {
		offset_timestamp = jf->timestamp - jb->window_timestamp;
		if (offset_timestamp >= 0)
			break;
	}

	return (jf) ? offset_timestamp : -1;
}

/* get next data chunk from jitterbuffer */
jitter_frame_t *jitter_load(jitter_t *jb)
{
	jitter_frame_t *jf;
	int32_t offset_timestamp;

#ifdef HEAVY_DEBUG
	static struct timespec tv;
        clock_gettime(CLOCK_REALTIME, &tv);
	LOGP(DJITTER, LOGL_DEBUG, "%s Load frame. %ld.%04ld\n", jb->name, tv.tv_sec, tv.tv_nsec / 1000000);
#endif

	/* now unlock jitter buffer */
	jb->unlocked = true;

	/* get current chunk, free all chunks that are in the past */
	while ((jf = jb->frame_list)) {
		offset_timestamp = jf->timestamp - jb->window_timestamp;
		if (offset_timestamp >= 0)
			break;
		/* detach and free */
		jb->frame_list = jf->next;
		jitter_frame_free(jf);
	}

	/* next frame in the future */
	if (jf && jf->timestamp != jb->window_timestamp)
		return NULL;

	/* detach, and return */
	if (jf)
		jb->frame_list = jf->next;
	return jf;
}

/* advance time stamp of jitter buffer */
void jitter_advance(jitter_t *jb, uint32_t offset)
{
	if (!jb->window_valid)
		return;

	jb->window_timestamp += offset;

	/* increment timer to check delay */
	jb->delay_counter += jb->sample_duration * (double)offset;
}

/* load samples from jitter buffer
 * store in spl_buf until all copied
 * conceal, if frame is missing
 * ceate silence, if no spl_buf exists in the first place */
void jitter_load_samples(jitter_t *jb, uint8_t *spl, int len, size_t sample_size, void (*conceal)(uint8_t *spl, int len, void *priv), void *conceal_priv)
{
	jitter_frame_t *jf;
	int32_t offset;
	void (*decoder)(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void *priv);
	void *decoder_priv;
	uint8_t *payload;
	int payload_len;
	int tocopy;

#ifdef VISUAL_DEBUG
	int32_t offset_timestamp;
	char debug[jb->max_window_size + 32];
	int last = 0;
	memset(debug, ' ', sizeof(debug));
	for (jf = jb->frame_list; jf; jf = jf->next) {
		offset_timestamp = jf->timestamp - jb->window_timestamp;
		if (offset_timestamp < 0)
			continue;
		offset_timestamp = (int)((double)offset_timestamp * jb->sample_duration * 1000.0);
		debug[offset_timestamp] = '0' + jf->sequence % 10;
		last = offset_timestamp + 1;
	}
	debug[last] = '\0';
	LOGP(DJITTER, LOGL_DEBUG, "%s:%s\n", jb->name, debug);
#endif

next_chunk:
	/* nothing more to return */
	if (!len)
		return;

copy_chunk:
	/* consume from buffer, if valid */
	if (jb->spl_buf && jb->spl_valid) {
		tocopy = jb->spl_len - jb->spl_pos;
		if (tocopy > len)
			tocopy = len;
		/* advance jitter buffer */
		jitter_advance(jb, tocopy);
		memcpy(spl, jb->spl_buf + jb->spl_pos * sample_size, tocopy * sample_size);
		spl += tocopy * sample_size;
		len -= tocopy;
		jb->spl_pos += tocopy;
		if (jb->spl_pos == jb->spl_len) {
			jb->spl_pos = 0;
			jb->spl_valid = false;
		}
		goto next_chunk;
	}

	/* get offset to next frame in jitter buffer */
	offset = jitter_offset(jb);
	/* jitter buffer is empty, so we must conceal all samples we have */
	if (offset < 0)
		offset = len;
	/* if we have an offset, we need to conceal the samples */
	if (offset > 0) {
		/* only process as much samples as need */
		if (offset > len)
			offset = len;
		/* advance jitter buffer */
		jitter_advance(jb, offset);
		/* if there is no buffer, allocate 20ms, filled with 0 */
		if (!jb->spl_buf) {
			jb->spl_len = jb->samples_20ms;
			jb->spl_buf = calloc(jb->spl_len, sample_size);
		}
		/* do until all samples are processed */
		while (offset) {
			tocopy = jb->spl_len - jb->spl_pos;
			if (tocopy > offset)
				tocopy = offset;
			if (conceal)
				conceal(jb->spl_buf + jb->spl_pos * sample_size, tocopy, conceal_priv);
			memcpy(spl, jb->spl_buf + jb->spl_pos * sample_size, tocopy * sample_size);
			spl += tocopy * sample_size;
			len -= tocopy;
			jb->spl_pos += tocopy;
			if (jb->spl_pos == jb->spl_len)
				jb->spl_pos = 0;
			offset -= tocopy;
		}
		goto next_chunk;
	}

	/* load from jitter buffer (it should work, because offset equals 0 */
	jf = jitter_load(jb);
	if (!jf) {
		LOGP(DJITTER, LOGL_ERROR, "%s Failed to get frame from jitter buffer, please fix!\n", jb->name);
		jitter_reset(jb);
		return;
	}
	/* get data from frame */
	jitter_frame_get(jf, &decoder, &decoder_priv, &payload, &payload_len, NULL, NULL, NULL, NULL);
	/* free previous buffer */
	free(jb->spl_buf);
	jb->spl_buf = NULL;
	jb->spl_pos = 0;
	/* decode */
	if (decoder) {
		decoder(payload, payload_len, &jb->spl_buf, &jb->spl_len, decoder_priv);
		if (!jb->spl_buf) {
			jitter_frame_free(jf);
			return;
		}
	} else {
		/* no decoder, so just copy as it is */
		jb->spl_buf = malloc(payload_len);
		if (!jb->spl_buf) {
			jitter_frame_free(jf);
			return;
		}
		memcpy(jb->spl_buf, payload, payload_len);
		jb->spl_len = payload_len;
	}
	jb->spl_len /= sample_size;
	jb->spl_valid = true;
	/* free jiter frame */
	jitter_frame_free(jf);
	goto copy_chunk;
}

void jitter_conceal_s16(uint8_t *_spl, int len, void __attribute__((unused)) *priv)
{
	int16_t *spl = (int16_t *)_spl;

	while (len) {
		*spl++ /= 1.5;
		len--;
	}
}

