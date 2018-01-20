/* wave recording and playback functions
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include "../libsample/sample.h"
#include "../libdebug/debug.h"
#include "wave.h"

/* NOTE: No locking required for writing and reading buffer pointers, since 'int' is atomic on >=32 bit machines */

static void *record_child(void *arg)
{
	wave_rec_t *rec = (wave_rec_t *)arg;
	int to_write, to_end, len;

	while (!rec->finish || rec->buffer_writep != rec->buffer_readp) {
		/* how much data is in buffer */
		to_write = (rec->buffer_size + rec->buffer_writep - rec->buffer_readp) % rec->buffer_size;
		if (to_write == 0) {
			usleep(10000);
			continue;
		}
		/* only write up to the end of buffer */
		to_end = rec->buffer_size - rec->buffer_readp;
		if (to_end < to_write)
			to_write = to_end;
		/* write */
		errno = 0;
		len = fwrite(rec->buffer + rec->buffer_readp, 1, to_write, rec->fp);
		/* quit on error */
		if (len < 0) {
error:
			PDEBUG(DWAVE, DEBUG_ERROR, "Failed to write to recording WAVE file! (errno %d)\n", errno);
			rec->finish = 1;
			return NULL;
		}
		/* increment read pointer */
		rec->buffer_readp += len;
		if (rec->buffer_readp == rec->buffer_size)
			rec->buffer_readp = 0;
		/* quit on end of file */
		if (len != to_write)
			goto error;
	}

	return NULL;
}

static void *playback_child(void *arg)
{
	wave_play_t *play = (wave_play_t *)arg;
	int to_read, to_end, len;

	while(!play->finish) {
		/* how much space is in buffer */
		to_read = (play->buffer_size + play->buffer_readp - play->buffer_writep - 1) % play->buffer_size;
		if (to_read == 0) {
			usleep(10000);
			continue;
		}
		/* only read up to the end of buffer */
		to_end = play->buffer_size - play->buffer_writep;
		if (to_end < to_read)
			to_read = to_end;
		/* read */
		len = fread(play->buffer + play->buffer_writep, 1, to_read, play->fp);
		/* quit on error */
		if (len < 0) {
			PDEBUG(DWAVE, DEBUG_ERROR, "Failed to read from playback WAVE file! (errno %d)\n", errno);
			play->finish = 1;
			return NULL;
		}
		/* increment write pointer */
		play->buffer_writep += len;
		if (play->buffer_writep == play->buffer_size)
			play->buffer_writep = 0;
		/* quit on end of file */
		if (len != to_read) {
			play->finish = 1;
			return NULL;
		}
	}

	return NULL;
}

struct fmt {
	uint16_t	format; /* 1 = pcm, 2 = adpcm */
	uint16_t	channels; /* number of channels */
	uint32_t	sample_rate; /* sample rate */
	uint32_t	data_rate; /* data rate */
	uint16_t	bytes_sample; /* bytes per sample (all channels) */
	uint16_t	bits_sample; /* bits per sample (one channel) */
};

int wave_create_record(wave_rec_t *rec, const char *filename, int samplerate, int channels, double max_deviation)
{
	/* RIFFxxxxWAVEfmt xxxx(fmt size)dataxxxx... */
	char dummyheader[4 + 4 + 4 + 4 + 4 + sizeof(struct fmt) + 4 + 4];
	int __attribute__((__unused__)) len;
	int rc;

	memset(rec, 0, sizeof(*rec));
	rec->samplerate = samplerate;
	rec->channels = channels;
	rec->max_deviation = max_deviation;

	rec->fp = fopen(filename, "w");
	if (!rec->fp) {
		PDEBUG(DWAVE, DEBUG_ERROR, "Failed to open recording file '%s'! (errno %d)\n", filename, errno);
		return -errno;
	}

	memset(&dummyheader, 0, sizeof(dummyheader));
	len = fwrite(dummyheader, 1, sizeof(dummyheader), rec->fp);

	rec->buffer_size = samplerate * 2 * channels;
	rec->buffer = calloc(rec->buffer_size, 1);
	if (!rec->buffer) {
		PDEBUG(DWAVE, DEBUG_NOTICE, "No mem!\n");
		rc = ENOMEM;
		goto error;
	}

	rc = pthread_create(&rec->tid, NULL, record_child, rec);
	if (rc < 0) {
		PDEBUG(DWAVE, DEBUG_ERROR, "Failed to create thread to record WAVE file! (errno %d)\n", errno);
		goto error;
	}

	PDEBUG(DWAVE, DEBUG_NOTICE, "*** Writing WAVE file to %s.\n", filename);

	return 0;

error:
	if (rec->buffer) {
		free(rec->buffer);
		rec->buffer = NULL;
	}
	if (rec->fp) {
		fclose(rec->fp);
		rec->fp = NULL;
	}
	return rc;
}

int wave_create_playback(wave_play_t *play, const char *filename, int *samplerate_p, int *channels_p, double max_deviation)
{
	uint8_t buffer[256];
	struct fmt fmt;
	int32_t size, chunk, len;
	int gotfmt = 0, gotdata = 0;
	int rc = -EINVAL;

	memset(&fmt, 0, sizeof(fmt));
	memset(play, 0, sizeof(*play));
	play->max_deviation = max_deviation;

	play->fp = fopen(filename, "r");
	if (!play->fp) {
		PDEBUG(DWAVE, DEBUG_ERROR, "Failed to open playback file '%s'! (errno %d)\n", filename, errno);
		return -errno;
	}

	len = fread(buffer, 1, 12, play->fp);
	if (len != 12) {
		PDEBUG(DWAVE, DEBUG_ERROR, "Failed to read RIFF header!\n");
		rc = -EIO;
		goto error;
	}
	if (!!strncmp((char *)buffer, "RIFF", 4)) {
		PDEBUG(DWAVE, DEBUG_ERROR, "Missing RIFF header, seems that this is no WAVE file!\n");
		rc = -EINVAL;
		goto error;
	}
	size = buffer[4] + (buffer[5] << 8) + (buffer[6] << 16) + (buffer[7] << 24);
	if (!!strncmp((char *)buffer + 8, "WAVE", 4)) {
		PDEBUG(DWAVE, DEBUG_ERROR, "Missing WAVE header, seems that this is no WAVE file!\n");
		rc = -EINVAL;
		goto error;
	}
	size -= 4;
	while (size) {
		if (size < 8) {
			PDEBUG(DWAVE, DEBUG_ERROR, "Short read of WAVE file!\n");
			rc = -EINVAL;
			goto error;
		}
		len = fread(buffer, 1, 8, play->fp);
		if (len != 8) {
			PDEBUG(DWAVE, DEBUG_ERROR, "Failed to read chunk of WAVE file!\n");
			rc = -EIO;
			goto error;
		}
		chunk = buffer[4] + (buffer[5] << 8) + (buffer[6] << 16) + (buffer[7] << 24);
		size -= 8 + chunk;
		if (size < 0) {
			PDEBUG(DWAVE, DEBUG_ERROR, "WAVE error: Chunk '%c%c%c%c' overflows file size!\n", buffer[4], buffer[5], buffer[6], buffer[7]);
			rc = -EIO;
			goto error;
		}
		if (!strncmp((char *)buffer, "fmt ", 4)) {
			if (chunk < 16 || chunk > (int)sizeof(buffer)) {
				PDEBUG(DWAVE, DEBUG_ERROR, "WAVE error: Short or corrupt 'fmt' chunk!\n");
				rc = -EINVAL;
				goto error;
			}
			len = fread(buffer, 1, chunk, play->fp);
			fmt.format = buffer[0] + (buffer[1] << 8);
			fmt.channels = buffer[2] + (buffer[3] << 8);
			fmt.sample_rate = buffer[4] + (buffer[5] << 8) + (buffer[6] << 16) + (buffer[7] << 24);
			fmt.data_rate = buffer[8] + (buffer[9] << 8) + (buffer[10] << 16) + (buffer[11] << 24);
			fmt.bytes_sample = buffer[12] + (buffer[13] << 8);
			fmt.bits_sample = buffer[14] + (buffer[15] << 8);
			gotfmt = 1;
		} else
		if (!strncmp((char *)buffer, "data", 4)) {
			if (!gotfmt) {
				PDEBUG(DWAVE, DEBUG_ERROR, "WAVE error: 'data' without 'fmt' chunk!\n");
				rc = -EINVAL;
				goto error;
			}
			gotdata = 1;
			break;
		} else {
			while(chunk > (int)sizeof(buffer)) {
				len = fread(buffer, 1, sizeof(buffer), play->fp);
				chunk -= sizeof(buffer);
			}
			if (chunk)
				len = fread(buffer, 1, chunk, play->fp);
		}
	}

	if (!gotfmt || !gotdata) {
		PDEBUG(DWAVE, DEBUG_ERROR, "WAVE error: Missing 'data' or 'fmt' chunk!\n");
		rc = -EINVAL;
		goto error;
	}

	if (fmt.format != 1) {
		PDEBUG(DWAVE, DEBUG_ERROR, "WAVE error: We support only PCM files!\n");
		rc = -EINVAL;
		goto error;
	}
	if (*channels_p == 0)
		*channels_p = fmt.channels;
	if (fmt.channels != *channels_p) {
		PDEBUG(DWAVE, DEBUG_ERROR, "WAVE error: We expect %d cannel(s), but wave file only has %d channel(s)\n", *channels_p, fmt.channels);
		rc = -EINVAL;
		goto error;
	}
	if (*samplerate_p == 0)
		*samplerate_p = fmt.sample_rate;
	if ((int)fmt.sample_rate != *samplerate_p) {
		PDEBUG(DWAVE, DEBUG_ERROR, "WAVE error: The WAVE file's sample rate (%d) does not match our sample rate (%d)!\n", fmt.sample_rate, *samplerate_p);
		rc = -EINVAL;
		goto error;
	}
	if ((int)fmt.data_rate != 2 * *channels_p * *samplerate_p) {
		PDEBUG(DWAVE, DEBUG_ERROR, "WAVE error: The WAVE file's data rate is only %d bytes per second, but we expect %d bytes per second (2 bytes per sample * channels * samplerate)!\n", fmt.data_rate, 2 * *channels_p * *samplerate_p);
		rc = -EINVAL;
		goto error;
	}
	if (fmt.bytes_sample != 2 * *channels_p) {
		PDEBUG(DWAVE, DEBUG_ERROR, "WAVE error: The WAVE file's bytes per sample is only %d, but we expect %d bytes sample (2 bytes per sample * channels)!\n", fmt.bytes_sample, 2 * *channels_p);
		rc = -EINVAL;
		goto error;
	}
	if (fmt.bits_sample != 16) {
		PDEBUG(DWAVE, DEBUG_ERROR, "WAVE error: We support only 16 bit files!\n");
		rc = -EINVAL;
		goto error;
	}

	play->channels = *channels_p;
	play->left = chunk / 2 / *channels_p;

	play->buffer_size = *samplerate_p * 2 * *channels_p;
	play->buffer = calloc(play->buffer_size, 1);
	if (!play->buffer) {
		PDEBUG(DWAVE, DEBUG_ERROR, "No mem!\n");
		rc = -ENOMEM;
		goto error;
	}

	rc = pthread_create(&play->tid, NULL, playback_child, play);
	if (rc < 0) {
		PDEBUG(DWAVE, DEBUG_ERROR, "Failed to create thread to playback WAVE file! (errno %d)\n", errno);
		goto error;
	}

	PDEBUG(DWAVE, DEBUG_NOTICE, "*** Reading WAVE file from %s.\n", filename);

	return 0;

error:
	if (play->buffer) {
		free(play->buffer);
		play->buffer = NULL;
	}
	if (play->fp) {
		fclose(play->fp);
		play->fp = NULL;
	}
	return rc;
}

int wave_write(wave_rec_t *rec, sample_t **samples, int length)
{
	double max_deviation = rec->max_deviation;
	int32_t value;
	int __attribute__((__unused__)) len;
	int i, c;
	int to_write;

	/* on error, don't write more */
	if (rec->finish)
		return 0;

	/* how much space is in buffer */
	to_write = (rec->buffer_size + rec->buffer_readp - rec->buffer_writep - 1) % rec->buffer_size;
	to_write /= 2 * rec->channels;
	if (to_write < length)
		PDEBUG(DWAVE, DEBUG_NOTICE, "Record WAVE buffer overflow.\n");
	else
		to_write = length;
	if (to_write == 0)
		return 0;

	for (i = 0; i < to_write; i++) {
		for (c = 0; c < rec->channels; c++) {
			value = samples[c][i] / max_deviation * 32767.0;
			if (value > 32767)
				value = 32767;
			else if (value < -32767)
				value = -32767;
			rec->buffer[rec->buffer_writep] = value;
			rec->buffer_writep = (rec->buffer_writep + 1) % rec->buffer_size;
			rec->buffer[rec->buffer_writep] = value >> 8;
			rec->buffer_writep = (rec->buffer_writep + 1) % rec->buffer_size;
		}
	}
	rec->written += to_write;

	return to_write;
}

int wave_read(wave_play_t *play, sample_t **samples, int length)
{
	double max_deviation = play->max_deviation;
	int16_t value; /* must be int16, so assembling bytes work */
	int __attribute__((__unused__)) len;
	int i, c;
	int to_read;

	/* we have finished */
	if (play->left == 0) {
		to_read = 0;
read_empty:
		for (i = to_read; i < length; i++) {
			for (c = 0; c < play->channels; c++)
				samples[c][i] = 0;
		}
		return length;
	}

	/* how much do we read from buffer */
	to_read = (play->buffer_size + play->buffer_writep - play->buffer_readp) % play->buffer_size;
	to_read /= 2 * play->channels;
	if (to_read > (int)play->left)
		to_read = play->left;
	if (to_read > length)
		to_read = length;

	if (to_read == 0 && play->finish) {
		if (play->left) {
			PDEBUG(DWAVE, DEBUG_NOTICE, "*** Finished reading WAVE file. (short read)\n");
			play->left = 0;
		}
		goto read_empty;
	}

	/* read from buffer */
	for (i = 0; i < to_read; i++) {
		for (c = 0; c < play->channels; c++) {
			value = play->buffer[play->buffer_readp];
			play->buffer_readp = (play->buffer_readp + 1) % play->buffer_size;
			value |= play->buffer[play->buffer_readp] << 8;
			play->buffer_readp = (play->buffer_readp + 1) % play->buffer_size;
			samples[c][i] = (double)value / 32767.0 * max_deviation;
		}
	}
	play->left -= to_read;

	if (!play->left)
		PDEBUG(DWAVE, DEBUG_NOTICE, "*** Finished reading WAVE file.\n");

	if (to_read < length)
		goto read_empty;

	return length;
}

void wave_destroy_record(wave_rec_t *rec)
{
	uint8_t buffer[256];
	uint32_t size, wsize;
	struct fmt fmt;
	int __attribute__((__unused__)) len;

	if (!rec->fp)
		return;

	/* on error, thread has terminated */
	if (rec->finish) {
		fclose(rec->fp);
		rec->fp = NULL;
		return;
	}

	/* finish thread */
	rec->finish = 1;
	pthread_join(rec->tid, NULL);

	/* cue */
	fprintf(rec->fp, "cue %c%c%c%c%c%c%c%c", 4, 0, 0, 0, 0,0,0,0);

	/* LIST */
	fprintf(rec->fp, "LIST%c%c%c%cadtl", 4, 0, 0, 0);

	/* go to header */
	fseek(rec->fp, 0, SEEK_SET);

	size = 2 * rec->written * rec->channels;
	wsize = 4 + 8 + sizeof(fmt) + 8 + size + 8 + 4 + 8 + 4;

	/* RIFF */
	fprintf(rec->fp, "RIFF%c%c%c%c", wsize & 0xff, (wsize >> 8) & 0xff, (wsize >> 16) & 0xff, wsize >> 24);

	/* WAVE */
	fprintf(rec->fp, "WAVE");

	/* fmt */
	fprintf(rec->fp, "fmt %c%c%c%c", (uint8_t)sizeof(fmt), 0, 0, 0);
	fmt.format = 1;
	fmt.channels = rec->channels;
	fmt.sample_rate = rec->samplerate; /* samples/sec */
	fmt.data_rate = rec->samplerate * 2 * rec->channels; /* full data rate */
	fmt.bytes_sample = 2 * rec->channels; /* all channels */
	fmt.bits_sample = 16; /* one channel */
	buffer[0] = fmt.format;
	buffer[1] = fmt.format >> 8;
	buffer[2] = fmt.channels;
	buffer[3] = fmt.channels >> 8;
	buffer[4] = fmt.sample_rate;
	buffer[5] = fmt.sample_rate >> 8;
	buffer[6] = fmt.sample_rate >> 16;
	buffer[7] = fmt.sample_rate >> 24;
	buffer[8] = fmt.data_rate;
	buffer[9] = fmt.data_rate >> 8;
	buffer[10] = fmt.data_rate >> 16;
	buffer[11] = fmt.data_rate >> 24;
	buffer[12] = fmt.bytes_sample;
	buffer[13] = fmt.bytes_sample >> 8;
	buffer[14] = fmt.bits_sample;
	buffer[15] = fmt.bits_sample >> 8;
	len = fwrite(buffer, 1, sizeof(fmt), rec->fp);

	/* data */
	fprintf(rec->fp, "data%c%c%c%c", size & 0xff, (size >> 8) & 0xff, (size >> 16) & 0xff, size >> 24);

	free(rec->buffer);
	rec->buffer = NULL;
	fclose(rec->fp);
	rec->fp = NULL;

	PDEBUG(DWAVE, DEBUG_NOTICE, "*** WAVE file written.\n");
}

void wave_destroy_playback(wave_play_t *play)
{
	if (!play->fp)
		return;

	/* finish thread if not already */
	play->finish = 1;
	pthread_join(play->tid, NULL);

	free(play->buffer);
	play->buffer = NULL;
	fclose(play->fp);
	play->fp = NULL;
}

