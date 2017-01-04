/* SDR processing
 *
 * (C) 2017 by Andreas Eversberg <jolly@eversberg.eu>
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
#include <string.h>
#include <math.h>
#include "filter.h"
#include "sdr.h"
#ifdef HAVE_UHD
#include "uhd.h"
#endif
#include "debug.h"

//#define FAST_SINE

typedef struct sdr_chan {
	double tx_frequency;	/* frequency used */
	double rx_frequency;	/* frequency used */
	double offset;		/* offset to calculated center frequency */
	double tx_phase;	/* current phase of FM (used to shift and modulate ) */
	double rx_rot;		/* rotation step per sample to shift rx frequency (used to shift) */
	double rx_phase;	/* current rotation phase (used to shift) */
	double rx_last_phase;	/* last phase of FM (used to demodulate) */
	filter_lowpass_t rx_lp[2]; /* filters received IQ signal */
} sdr_chan_t;

typedef struct sdr {
	sdr_chan_t *chan;
	double spl_deviation;	/* how to convert a sample step into deviation (Hz) */
	int channels;		/* number of frequencies */
	double samplerate;	/* IQ rate */
	double amplitude;	/* amplitude of each carrier */
} sdr_t;

static const char *sdr_device_args;
static double sdr_rx_gain, sdr_tx_gain;

#ifdef FAST_SINE
static float sdr_sine[256];
#endif

int sdr_init(const char *device_args, double rx_gain, double tx_gain)
{
#ifdef FAST_SINE
	int i;

	for (i = 0; i < 256; i++) {
		sdr_sine[i] = sin(2.0*M_PI*i/256);
	}
#endif

	sdr_device_args = strdup(device_args);
	sdr_rx_gain = rx_gain;
	sdr_tx_gain = tx_gain;

	return 0;
}

void *sdr_open(const char __attribute__((__unused__)) *audiodev, double *tx_frequency, double *rx_frequency, int channels, int samplerate, double bandwidth, double sample_deviation)
{
	sdr_t *sdr;
	double center_frequency;
	int rc;
	int c;

	if (channels < 1) {
		PDEBUG(DSDR, DEBUG_ERROR, "No channel given, please fix!\n");
		abort();
	}

	sdr = calloc(sizeof(*sdr), 1);
	if (!sdr) {
		PDEBUG(DSDR, DEBUG_ERROR, "NO MEM!\n");
		goto error;
	}
	sdr->channels = channels;
	sdr->samplerate = samplerate;
	sdr->spl_deviation = sample_deviation;
	sdr->amplitude = 0.4 / (double)channels; // FIXME: actual amplitude 0.1?

	/* create list of channel states */
	sdr->chan = calloc(sizeof(*sdr->chan), channels);
	if (!sdr->chan) {
		PDEBUG(DSDR, DEBUG_ERROR, "NO MEM!\n");
		goto error;
	}
	for (c = 0; c < channels; c++) {
		PDEBUG(DSDR, DEBUG_INFO, "Frequency #%d: TX = %.6f MHz, RX = %.6f MHz\n", c, tx_frequency[c] / 1e6, rx_frequency[c] / 1e6);
		sdr->chan[c].tx_frequency = tx_frequency[c];
		sdr->chan[c].rx_frequency = rx_frequency[c];
#warning check rx frequency is in range
		filter_lowpass_init(&sdr->chan[c].rx_lp[0], bandwidth, samplerate);
		filter_lowpass_init(&sdr->chan[c].rx_lp[1], bandwidth, samplerate);
	}

	/* calculate required bandwidth (IQ rate) */
	if (channels == 1) {
		PDEBUG(DSDR, DEBUG_INFO, "Single frequency, so we use sample rate as IQ bandwidth: %.6f MHz\n", sdr->samplerate / 1e6);
		center_frequency = sdr->chan[0].tx_frequency;
	} else {
		double low_frequency = sdr->chan[0].tx_frequency, high_frequency = sdr->chan[0].tx_frequency, range;
		for (c = 1; c < channels; c++) {
			if (sdr->chan[c].tx_frequency < low_frequency)
				low_frequency = sdr->chan[c].tx_frequency;
			if (sdr->chan[c].tx_frequency > high_frequency)
				high_frequency = sdr->chan[c].tx_frequency;
		}
		range = high_frequency - low_frequency;
		PDEBUG(DSDR, DEBUG_INFO, "Range between frequencies: %.6f MHz\n", range / 1e6);
		if (range * 2 > sdr->samplerate) {
			// why that? actually i don't know. i just want to be safe....
			PDEBUG(DSDR, DEBUG_NOTICE, "The sample rate must be at least twice the range between frequencies. Please increment samplerate!\n");
			goto error;
		}
		center_frequency = (high_frequency + low_frequency) / 2.0;
	}
	PDEBUG(DSDR, DEBUG_INFO, "Using center frequency: %.6f MHz\n", center_frequency / 1e6);
	for (c = 0; c < channels; c++) {
		sdr->chan[c].offset = sdr->chan[c].tx_frequency - center_frequency;
		sdr->chan[c].rx_rot = 2 * M_PI * -sdr->chan[c].offset / sdr->samplerate;
		PDEBUG(DSDR, DEBUG_INFO, "Frequency #%d offset: %.6f MHz\n", c, sdr->chan[c].offset / 1e6);
	}
	PDEBUG(DSDR, DEBUG_INFO, "Using gain: TX %.1f dB, RX %.1f dB\n", sdr_tx_gain, sdr_rx_gain);

#ifdef HAVE_UHD
#warning hack
	rc = uhd_open(sdr_device_args, center_frequency, center_frequency - sdr->chan[0].tx_frequency + sdr->chan[0].rx_frequency, sdr->samplerate, sdr_rx_gain, sdr_tx_gain);
	if (rc)
		goto error;
#endif

	return sdr;

error:
	sdr_close(sdr);
	return NULL;
}

void sdr_close(void *inst)
{
	sdr_t *sdr = (sdr_t *)inst;

#ifdef HAVE_UHD
	uhd_close();
#endif

	if (sdr) {
		free(sdr->chan);
		free(sdr);
		sdr = NULL;
	}
}

int sdr_write(void *inst, int16_t **samples, int num, int channels)
{
	sdr_t *sdr = (sdr_t *)inst;
	float buff[num * 2];
	int c, s, ss;
	double rate, phase, amplitude, dev;
	int sent;

	if (channels != sdr->channels) {
		PDEBUG(DSDR, DEBUG_ERROR, "Invalid number of channels, please fix!\n");
		abort();
	}

	/* process all channels */
	rate = sdr->samplerate;
	amplitude = sdr->amplitude;
	memset(buff, 0, sizeof(buff));
	for (c = 0; c < channels; c++) {
		/* modulate */
		phase = sdr->chan[c].tx_phase;
		for (s = 0, ss = 0; s < num; s++) {
			/* deviation is defined by the sample value and the offset */
			dev = sdr->chan[c].offset + (double)samples[c][s] * sdr->spl_deviation;
#ifdef FAST_SINE
			phase += 256.0 * dev / rate;
			if (phase < 0.0)
				phase += 256.0;
			if (phase >= 256.0)
				phase -= 256.0;
			buff[ss++] += sdr_sine[((int)phase + 64) & 0xff] * amplitude;
			buff[ss++] += sdr_sine[(int)phase & 0xff] * amplitude;
#else
			phase += 2.0 * M_PI * dev / rate;
			if (phase < 0.0)
				phase += 2.0 * M_PI;
			if (phase >= 2.0 * M_PI)
				phase -= 2.0 * M_PI;
			buff[ss++] += cos(phase) * amplitude;
			buff[ss++] += sin(phase) * amplitude;
#endif
		}
		sdr->chan[c].tx_phase = phase;
	}

#ifdef HAVE_UHD
	sent = uhd_send(buff, num);
#endif
	if (sent < 0)
		return sent;
	
	return sent;
}

int sdr_read(void *inst, int16_t **samples, int num, int channels)
{
	sdr_t *sdr = (sdr_t *)inst;
	float buff[num * 2];
	double I[num], Q[num], i, q;
	int count;
	int c, s, ss;
	double phase, rot, last_phase, spl, dev, rate;

	rate = sdr->samplerate;

#ifdef HAVE_UHD
	count = uhd_receive(buff, num);
#endif
	if (count <= 0)
		return count;

	for (c = 0; c < channels; c++) {
		rot = sdr->chan[c].rx_rot;
		phase = sdr->chan[c].rx_phase;
		for (s = 0, ss = 0; s < count; s++) {
			phase += rot;
			i = buff[ss++];
			q = buff[ss++];
			I[s] = i * cos(phase) - q * sin(phase);
			Q[s] = i * sin(phase) + q * cos(phase);
		}
		sdr->chan[c].rx_phase = phase;
#warning eine interation von 2 führt zu müll (2. kanal gespiegeltes audio), muss man genauer mal analysieren
		filter_lowpass_process(&sdr->chan[c].rx_lp[0], I, count, 1);
		filter_lowpass_process(&sdr->chan[c].rx_lp[1], Q, count, 1);
		last_phase = sdr->chan[c].rx_last_phase;
		for (s = 0; s < count; s++) {
			phase = atan2(I[s], Q[s]);
			dev = (phase - last_phase) / 2 / M_PI;
			last_phase = phase;
			if (dev < -0.49)
				dev += 1.0;
			else if (dev > 0.49)
				dev -= 1.0;
			dev *= rate;
			spl = dev / sdr->spl_deviation;
			if (spl > 32766.0)
				spl = 32766.0;
			else if (spl < -32766.0)
				spl = -32766.0;
			samples[c][s] = spl;
		}
		sdr->chan[c].rx_last_phase = last_phase;
	}

	return count;
}

/* how many delay (in audio sample duration) do we have in the buffer */
int sdr_get_inbuffer(void __attribute__((__unused__)) *inst)
{
//	sdr_t *sdr = (sdr_t *)inst;
	int count;

#ifdef HAVE_UHD
	count = uhd_get_inbuffer();
#endif
	if (count < 0)
		return count;

	return count;
}


