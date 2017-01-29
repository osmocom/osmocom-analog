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
#include "sample.h"
#include "wave.h"

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

	memset(rec, 0, sizeof(*rec));
	rec->samplerate = samplerate;
	rec->channels = channels;
	rec->max_deviation = max_deviation;

	rec->fp = fopen(filename, "w");
	if (!rec->fp) {
		fprintf(stderr, "Failed to open recording file '%s'! (errno %d)\n", filename, errno);
		return -errno;
	}

	memset(&dummyheader, 0, sizeof(dummyheader));
	len = fwrite(dummyheader, 1, sizeof(dummyheader), rec->fp);

	printf("*** Writing received audio to %s.\n", filename);

	return 0;
}

int wave_create_playback(wave_play_t *play, const char *filename, int samplerate, int channels, double max_deviation)
{
	uint8_t buffer[256];
	struct fmt fmt;
	int32_t size, chunk, len;
	int gotfmt = 0, gotdata = 0;
	int rc = -EINVAL;

	memset(play, 0, sizeof(*play));
	play->channels = channels;
	play->max_deviation = max_deviation;

	play->fp = fopen(filename, "r");
	if (!play->fp) {
		fprintf(stderr, "Failed to open playback file '%s'! (errno %d)\n", filename, errno);
		return -errno;
	}

	len = fread(buffer, 1, 12, play->fp);
	if (len != 12) {
		fprintf(stderr, "Failed to read RIFF header!\n");
		rc = -EIO;
		goto error;
	}
	if (!!strncmp((char *)buffer, "RIFF", 4)) {
		fprintf(stderr, "Missing RIFF header, seems that this is no WAVE file!\n");
		rc = -EINVAL;
		goto error;
	}
	size = buffer[4] + (buffer[5] << 8) + (buffer[6] << 16) + (buffer[7] << 24);
	if (!!strncmp((char *)buffer + 8, "WAVE", 4)) {
		fprintf(stderr, "Missing WAVE header, seems that this is no WAVE file!\n");
		rc = -EINVAL;
		goto error;
	}
	size -= 4;
	while (size) {
		if (size < 8) {
			fprintf(stderr, "Short read of WAVE file!\n");
			rc = -EINVAL;
			goto error;
		}
		len = fread(buffer, 1, 8, play->fp);
		if (len != 8) {
			fprintf(stderr, "Failed to read chunk of WAVE file!\n");
			rc = -EIO;
			goto error;
		}
		chunk = buffer[4] + (buffer[5] << 8) + (buffer[6] << 16) + (buffer[7] << 24);
		size -= 8 + chunk;
		if (size < 0) {
			fprintf(stderr, "WAVE error: Chunk '%c%c%c%c' overflows file size!\n", buffer[4], buffer[5], buffer[6], buffer[7]);
			rc = -EIO;
			goto error;
		}
		if (!strncmp((char *)buffer, "fmt ", 4)) {
			if (chunk < 16 || chunk > (int)sizeof(buffer)) {
				fprintf(stderr, "WAVE error: Short or corrupt 'fmt' chunk!\n");
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
				fprintf(stderr, "WAVE error: 'data' without 'fmt' chunk!\n");
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
		fprintf(stderr, "WAVE error: Missing 'data' or 'fmt' chunk!\n");
		rc = -EINVAL;
		goto error;
	}

	if (fmt.format != 1) {
		fprintf(stderr, "WAVE error: We support only PCM files!\n");
		rc = -EINVAL;
		goto error;
	}
	if (fmt.channels !=channels) {
		fprintf(stderr, "WAVE error: We expect %d cannel(s), but wave file only has %d channel(s)\n", channels, fmt.channels);
		rc = -EINVAL;
		goto error;
	}
	if ((int)fmt.sample_rate != samplerate) {
		fprintf(stderr, "WAVE error: The WAVE file's sample rate (%d) does not match our sample rate (%d)!\n", fmt.sample_rate, samplerate);
		rc = -EINVAL;
		goto error;
	}
	if ((int)fmt.data_rate != 2 * channels * samplerate) {
		fprintf(stderr, "WAVE error: The WAVE file's data rate is only %d bytes per second, but we expect %d bytes per second (2 bytes per sample * channels * samplerate)!\n", fmt.data_rate, 2 * channels * samplerate);
		rc = -EINVAL;
		goto error;
	}
	if (fmt.bytes_sample != 2 * channels) {
		fprintf(stderr, "WAVE error: The WAVE file's bytes per sample is only %d, but we expect %d bytes sample (2 bytes per sample * channels)!\n", fmt.bytes_sample, 2 * channels);
		rc = -EINVAL;
		goto error;
	}
	if (fmt.bits_sample != 16) {
		fprintf(stderr, "WAVE error: We support only 16 bit files!\n");
		rc = -EINVAL;
		goto error;
	}

	play->left = chunk / 2 / channels;

	printf("*** Replacing received audio by %s.\n", filename);

	return 0;

error:
	fclose(play->fp);
	play->fp = NULL;
	return rc;
}

int wave_read(wave_play_t *play, sample_t **samples, int length)
{
	double max_deviation = play->max_deviation;
	int16_t value; /* must be int16, so assembling bytes work */
	uint8_t buff[2 * length * play->channels];
	int __attribute__((__unused__)) len;
	int i, j, c;

	if (length > (int)play->left) {
		for (c = 0; c < play->channels; c++)
			memset(samples[c], 0, sizeof(samples[c][0] * length));
		length = play->left;
	}
	if (!length)
		return length;

	play->left -= length;
	if (!play->left)
		printf("*** Finished reading WAVE file.\n");

	/* read and correct endianness */
	len = fread(buff, 1, 2 * length * play->channels, play->fp);
	for (i = 0, j = 0; i < length; i++) {
		for (c = 0; c < play->channels; c++) {
			value = buff[j] + (buff[j + 1] << 8);
			samples[c][i] = (double)value / 32767.0 * max_deviation;
			j += 2;
		}
	}

	return length;
}

int wave_write(wave_rec_t *rec, sample_t **samples, int length)
{
	double max_deviation = rec->max_deviation;
	int32_t value;
	uint8_t buff[2 * length * rec->channels];
	int __attribute__((__unused__)) len;
	int i, j, c;

	/* write and correct endianness */
	for (i = 0, j = 0; i < length; i++) {
		for (c = 0; c < rec->channels; c++) {
			value = samples[c][i] / max_deviation * 32767.0;
			if (value > 32767)
				value = 32767;
			else if (value < -32767)
				value = -32767;
			buff[j++] = value;
			buff[j++] = value >> 8;
		}
	}
	len = fwrite(buff, 1, 2 * length * rec->channels, rec->fp);
	rec->written += length;

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

	fclose(rec->fp);
	rec->fp = NULL;

	printf("*** Received audio is written to WAVE file.\n");
}

void wave_destroy_playback(wave_play_t *play)
{
	if (!play->fp)
		return;

	fclose(play->fp);
	play->fp = NULL;
}

