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

enum paging_signal;

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <getopt.h>
#define __USE_GNU
#include <pthread.h>
#include <unistd.h>
#include "sample.h"
#include "fm_modulation.h"
#include "timer.h"
#include "sender.h"
#include "sdr_config.h"
#include "sdr.h"
#ifdef HAVE_UHD
#include "uhd.h"
#endif
#ifdef HAVE_SOAPY
#include "soapy.h"
#endif
#include "debug.h"

/* enable to debug buffer handling */
//#define DEBUG_BUFFER

/* enable to test without oversampling filter */
//#define DISABLE_FILTER


typedef struct sdr_thread {
	int use;
	volatile int running, exit;	/* flags to control exit of threads */
	int buffer_size;
	volatile float *buffer;
	float *buffer2;
	volatile int in, out;		/* in and out pointers (atomic, so no locking required) */
	int max_fill;			/* measure maximum buffer fill */
	double max_fill_timer;		/* timer to display/reset maximum fill */
	iir_filter_t lp[2];		/* filter for upsample/downsample IQ data */
} sdr_thread_t;

typedef struct sdr_chan {
	double		tx_frequency;	/* frequency used */
	double		rx_frequency;	/* frequency used */
	fm_mod_t	mod;		/* modulator instance */
	fm_demod_t	demod;		/* demodulator instance */
	dispmeasparam_t	*dmp_rf_level;
	dispmeasparam_t	*dmp_freq_offset;
	dispmeasparam_t	*dmp_deviation;
} sdr_chan_t;

typedef struct sdr {
	int		threads;	/* use threads */
	int		oversample;	/* oversample IQ rate */
	sdr_thread_t	thread_read,
			thread_write;
	sdr_chan_t	*chan;		/* settings for all channels */
	int		paging_channel;	/* if set, points to paging channel */
	sdr_chan_t	paging_chan;	/* settings for extra paging channel */
	int		channels;	/* number of frequencies */
	double		amplitude;	/* amplitude of each carrier */
	int		samplerate;	/* sample rate of audio data */
	int		latspl;		/* latency in audio samples */
	wave_rec_t	wave_rx_rec;
	wave_rec_t	wave_tx_rec;
	wave_play_t	wave_rx_play;
	wave_play_t	wave_tx_play;
	float		*modbuff;	/* buffer for FM transmodulation */
	sample_t	*modbuff_I;
	sample_t	*modbuff_Q;
	sample_t	*wavespl0;	/* sample buffer for wave generation */
	sample_t	*wavespl1;
} sdr_t;

void *sdr_open(const char __attribute__((__unused__)) *audiodev, double *tx_frequency, double *rx_frequency, int channels, double paging_frequency, int samplerate, int latspl, double max_deviation, double max_modulation)
{
	sdr_t *sdr;
	int threads = 1, oversample = 1; /* always use threads */
	double bandwidth;
	double tx_center_frequency = 0.0, rx_center_frequency = 0.0;
	int rc;
	int c;

	PDEBUG(DSDR, DEBUG_DEBUG, "Open SDR device\n");

	if (sdr_config->samplerate != samplerate) {
		if (samplerate > sdr_config->samplerate) {
			PDEBUG(DSDR, DEBUG_ERROR, "SDR sample rate must be greater than audio sample rate!\n");
			PDEBUG(DSDR, DEBUG_ERROR, "You selected an SDR rate of %d and an audio rate of %d.\n", sdr_config->samplerate, samplerate);
			return NULL;
		}
		if ((sdr_config->samplerate % samplerate)) {
			PDEBUG(DSDR, DEBUG_ERROR, "SDR sample rate must be a multiple of audio sample rate!\n");
			PDEBUG(DSDR, DEBUG_ERROR, "You selected an SDR rate of %d and an audio rate of %d.\n", sdr_config->samplerate, samplerate);
			return NULL;
		}
		oversample = sdr_config->samplerate / samplerate;
		threads = 1;
	}

	bandwidth = 2.0 * (max_deviation + max_modulation);
	PDEBUG(DSDR, DEBUG_INFO, "Require bandwidth of 2 * (%.1f + %.1f) = %.1f\n", max_deviation / 1000, max_modulation / 1000, bandwidth / 1000);

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
	sdr->amplitude = 1.0 / (double)channels;
	sdr->samplerate = samplerate;
	sdr->latspl = latspl;
	sdr->threads = threads; /* always requried, because write may block */
	sdr->oversample = oversample;

	if (threads) {
		memset(&sdr->thread_read, 0, sizeof(sdr->thread_read));
		sdr->thread_read.buffer_size = sdr->latspl * 2 * sdr->oversample + 2;
		sdr->thread_read.buffer = calloc(sdr->thread_read.buffer_size, sizeof(*sdr->thread_read.buffer));
		if (!sdr->thread_read.buffer) {
			PDEBUG(DSDR, DEBUG_ERROR, "No mem!\n");
			goto error;
		}
		sdr->thread_read.buffer2 = calloc(sdr->thread_read.buffer_size, sizeof(*sdr->thread_read.buffer2));
		if (!sdr->thread_read.buffer2) {
			PDEBUG(DSDR, DEBUG_ERROR, "No mem!\n");
			goto error;
		}
		sdr->thread_read.in = sdr->thread_read.out = 0;
		if (oversample > 1) {
			iir_lowpass_init(&sdr->thread_read.lp[0], samplerate / 2.0, sdr_config->samplerate, 2);
			iir_lowpass_init(&sdr->thread_read.lp[1], samplerate / 2.0, sdr_config->samplerate, 2);
		}
		memset(&sdr->thread_write, 0, sizeof(sdr->thread_write));
		sdr->thread_write.buffer_size = sdr->latspl * 2 + 2;
		sdr->thread_write.buffer = calloc(sdr->thread_write.buffer_size, sizeof(*sdr->thread_write.buffer));
		if (!sdr->thread_write.buffer) {
			PDEBUG(DSDR, DEBUG_ERROR, "No mem!\n");
			goto error;
		}
		sdr->thread_write.buffer2 = calloc(sdr->thread_write.buffer_size * sdr->oversample, sizeof(*sdr->thread_write.buffer2));
		if (!sdr->thread_write.buffer2) {
			PDEBUG(DSDR, DEBUG_ERROR, "No mem!\n");
			goto error;
		}
		sdr->thread_write.in = sdr->thread_write.out = 0;
		if (oversample > 1) {
			iir_lowpass_init(&sdr->thread_write.lp[0], samplerate / 2.0, sdr_config->samplerate, 2);
			iir_lowpass_init(&sdr->thread_write.lp[1], samplerate / 2.0, sdr_config->samplerate, 2);
		}
	}

	/* alloc fm modulation buffers */
	sdr->modbuff = calloc(sdr->latspl * 2, sizeof(*sdr->modbuff));
	if (!sdr->modbuff) {
		PDEBUG(DSDR, DEBUG_ERROR, "NO MEM!\n");
		goto error;
	}
	sdr->modbuff_I = calloc(sdr->latspl, sizeof(*sdr->modbuff_I));
	if (!sdr->modbuff_I) {
		PDEBUG(DSDR, DEBUG_ERROR, "NO MEM!\n");
		goto error;
	}
	sdr->modbuff_Q = calloc(sdr->latspl, sizeof(*sdr->modbuff_Q));
	if (!sdr->modbuff_Q) {
		PDEBUG(DSDR, DEBUG_ERROR, "NO MEM!\n");
		goto error;
	}
	sdr->wavespl0 = calloc(sdr->latspl, sizeof(*sdr->wavespl0));
	if (!sdr->wavespl0) {
		PDEBUG(DSDR, DEBUG_ERROR, "NO MEM!\n");
		goto error;
	}
	sdr->wavespl1 = calloc(sdr->latspl, sizeof(*sdr->wavespl1));
	if (!sdr->wavespl1) {
		PDEBUG(DSDR, DEBUG_ERROR, "NO MEM!\n");
		goto error;
	}

	/* special case where we use a paging frequency */
	if (paging_frequency) {
		/* add extra paging channel */
		sdr->paging_channel = channels;
	}

	/* create list of channel states */
	sdr->chan = calloc(sizeof(*sdr->chan), channels + (sdr->paging_channel != 0));
	if (!sdr->chan) {
		PDEBUG(DSDR, DEBUG_ERROR, "NO MEM!\n");
		goto error;
	}

	if (tx_frequency) {
		/* calculate required bandwidth (IQ rate) */
		for (c = 0; c < channels; c++) {
			PDEBUG(DSDR, DEBUG_INFO, "Frequency #%d: TX = %.6f MHz\n", c, tx_frequency[c] / 1e6);
			sdr->chan[c].tx_frequency = tx_frequency[c];
		}
		if (sdr->paging_channel) {
			PDEBUG(DSDR, DEBUG_INFO, "Paging Frequency: TX = %.6f MHz\n", paging_frequency / 1e6);
			sdr->chan[sdr->paging_channel].tx_frequency = paging_frequency;
		}

		double tx_low_frequency = sdr->chan[0].tx_frequency, tx_high_frequency = sdr->chan[0].tx_frequency;
		for (c = 1; c < channels; c++) {
			if (sdr->chan[c].tx_frequency < tx_low_frequency)
				tx_low_frequency = sdr->chan[c].tx_frequency;
			if (sdr->chan[c].tx_frequency > tx_high_frequency)
				tx_high_frequency = sdr->chan[c].tx_frequency;
		}
		if (sdr->paging_channel) {
			if (sdr->chan[sdr->paging_channel].tx_frequency < tx_low_frequency)
				tx_low_frequency = sdr->chan[sdr->paging_channel].tx_frequency;
			if (sdr->chan[sdr->paging_channel].tx_frequency > tx_high_frequency)
				tx_high_frequency = sdr->chan[sdr->paging_channel].tx_frequency;
		}
		/* range of TX */
		double range = tx_high_frequency - tx_low_frequency;
		if (range)
			PDEBUG(DSDR, DEBUG_DEBUG, "Range between all TX Frequencies: %.6f MHz\n", range / 1e6);
		if (range * 2 > samplerate) {
			// why that? actually i don't know. i just want to be safe....
			PDEBUG(DSDR, DEBUG_NOTICE, "The sample rate must be at least twice the range between frequencies.\n");
			PDEBUG(DSDR, DEBUG_NOTICE, "The given rate is %.6f MHz, but required rate must be >= %.6f MHz\n", samplerate / 1e6, range * 2.0 / 1e6);
			PDEBUG(DSDR, DEBUG_NOTICE, "Please increase samplerate!\n");
			goto error;
		}
		tx_center_frequency = (tx_high_frequency + tx_low_frequency) / 2.0;
		PDEBUG(DSDR, DEBUG_INFO, "Using center frequency: TX %.6f MHz\n", tx_center_frequency / 1e6);
		/* set offsets to center frequency */
		for (c = 0; c < channels; c++) {
			double tx_offset;
			tx_offset = sdr->chan[c].tx_frequency - tx_center_frequency;
			PDEBUG(DSDR, DEBUG_DEBUG, "Frequency #%d: TX offset: %.6f MHz\n", c, tx_offset / 1e6);
			rc = fm_mod_init(&sdr->chan[c].mod, samplerate, tx_offset, sdr->amplitude);
			if (rc < 0)
				goto error;
		}
		if (sdr->paging_channel) {
			double tx_offset;
			tx_offset = sdr->chan[sdr->paging_channel].tx_frequency - tx_center_frequency;
			PDEBUG(DSDR, DEBUG_DEBUG, "Paging Frequency: TX offset: %.6f MHz\n", tx_offset / 1e6);
			rc = fm_mod_init(&sdr->chan[sdr->paging_channel].mod, samplerate, tx_offset, sdr->amplitude);
			if (rc < 0)
				goto error;
		}
		/* show gain */
		PDEBUG(DSDR, DEBUG_INFO, "Using gain: TX %.1f dB\n", sdr_config->tx_gain);
		/* open wave */
		if (sdr_config->write_iq_tx_wave) {
			rc = wave_create_record(&sdr->wave_tx_rec, sdr_config->write_iq_tx_wave, samplerate, 2, 1.0);
			if (rc < 0) {
				PDEBUG(DSDR, DEBUG_ERROR, "Failed to create WAVE recoding instance!\n");
				goto error;
			}
		}
		if (sdr_config->read_iq_tx_wave) {
			rc = wave_create_playback(&sdr->wave_tx_play, sdr_config->read_iq_tx_wave, samplerate, 2, 1.0);
			if (rc < 0) {
				PDEBUG(DSDR, DEBUG_ERROR, "Failed to create WAVE playback instance!\n");
				goto error;
			}
		}
	}

	if (rx_frequency) {
		for (c = 0; c < channels; c++) {
			PDEBUG(DSDR, DEBUG_INFO, "Frequency #%d: RX = %.6f MHz\n", c, rx_frequency[c] / 1e6);
			sdr->chan[c].rx_frequency = rx_frequency[c];
		}

		/* calculate required bandwidth (IQ rate) */
		double rx_low_frequency = sdr->chan[0].rx_frequency, rx_high_frequency = sdr->chan[0].rx_frequency;
		for (c = 1; c < channels; c++) {
			if (sdr->chan[c].rx_frequency < rx_low_frequency)
				rx_low_frequency = sdr->chan[c].rx_frequency;
			if (sdr->chan[c].rx_frequency > rx_high_frequency)
				rx_high_frequency = sdr->chan[c].rx_frequency;
		}
		/* range of RX */
		double range = rx_high_frequency - rx_low_frequency;
		if (range)
			PDEBUG(DSDR, DEBUG_DEBUG, "Range between all RX Frequencies: %.6f MHz\n", range / 1e6);
		if (range * 2.0 > samplerate) {
			// why that? actually i don't know. i just want to be safe....
			PDEBUG(DSDR, DEBUG_NOTICE, "The sample rate must be at least twice the range between frequencies. Please increment samplerate!\n");
			goto error;
		}
		rx_center_frequency = (rx_high_frequency + rx_low_frequency) / 2.0;
		PDEBUG(DSDR, DEBUG_INFO, "Using center frequency: RX %.6f MHz\n", rx_center_frequency / 1e6);
		/* set offsets to center frequency */
		for (c = 0; c < channels; c++) {
			double rx_offset;
			rx_offset = sdr->chan[c].rx_frequency - rx_center_frequency;
			PDEBUG(DSDR, DEBUG_DEBUG, "Frequency #%d: RX offset: %.6f MHz\n", c, rx_offset / 1e6);
			rc = fm_demod_init(&sdr->chan[c].demod, samplerate, rx_offset, bandwidth);
			if (rc < 0)
				goto error;
		}
		/* show gain */
		PDEBUG(DSDR, DEBUG_INFO, "Using gain: RX %.1f dB\n", sdr_config->rx_gain);
		/* open wave */
		if (sdr_config->write_iq_rx_wave) {
			rc = wave_create_record(&sdr->wave_rx_rec, sdr_config->write_iq_rx_wave, samplerate, 2, 1.0);
			if (rc < 0) {
				PDEBUG(DSDR, DEBUG_ERROR, "Failed to create WAVE recoding instance!\n");
				goto error;
			}
		}
		if (sdr_config->read_iq_rx_wave) {
			rc = wave_create_playback(&sdr->wave_rx_play, sdr_config->read_iq_rx_wave, samplerate, 2, 1.0);
			if (rc < 0) {
				PDEBUG(DSDR, DEBUG_ERROR, "Failed to create WAVE playback instance!\n");
				goto error;
			}
		}
		/* init measurements display */
		for (c = 0; c < channels; c++) {
			sender_t *sender = get_sender_by_empfangsfrequenz(sdr->chan[c].rx_frequency);
			if (!sender)
				continue;
			sdr->chan[c].dmp_rf_level = display_measurements_add(sender, "RF Level", "%.1f dB", DISPLAY_MEAS_AVG, DISPLAY_MEAS_LEFT, -96.0, 0.0, -INFINITY);
			sdr->chan[c].dmp_freq_offset = display_measurements_add(sender, "Freq. Offset", "%+.2f KHz", DISPLAY_MEAS_AVG, DISPLAY_MEAS_CENTER, -max_deviation / 1000.0 * 2.0, max_deviation / 1000.0 * 2.0, 0.0);
			sdr->chan[c].dmp_deviation = display_measurements_add(sender, "Deviation", "%.2f KHz", DISPLAY_MEAS_PEAK2PEAK, DISPLAY_MEAS_LEFT, 0.0, max_deviation / 1000.0 * 1.5, max_deviation / 1000.0);
		}
	}

	if (sdr_config->swap_links) {
		double temp;
		PDEBUG(DSDR, DEBUG_NOTICE, "Sapping RX and TX frequencies!\n");
		temp = rx_center_frequency;
		rx_center_frequency = tx_center_frequency;
		tx_center_frequency = temp;
	}

	display_iq_init(samplerate);
	display_spectrum_init(samplerate, rx_center_frequency);

#ifdef HAVE_UHD
	if (sdr_config->uhd) {
		rc = uhd_open(sdr_config->channel, sdr_config->device_args, sdr_config->stream_args, sdr_config->tune_args, sdr_config->tx_antenna, sdr_config->rx_antenna, tx_center_frequency, rx_center_frequency, sdr_config->samplerate, sdr_config->tx_gain, sdr_config->rx_gain, sdr_config->bandwidth, sdr_config->uhd_tx_timestamps);
		if (rc)
			goto error;
	}
#endif

#ifdef HAVE_SOAPY
	if (sdr_config->soapy) {
		rc = soapy_open(sdr_config->channel, sdr_config->device_args, sdr_config->stream_args, sdr_config->tune_args, sdr_config->tx_antenna, sdr_config->rx_antenna, tx_center_frequency, rx_center_frequency, sdr_config->samplerate, sdr_config->tx_gain, sdr_config->rx_gain, sdr_config->bandwidth);
		if (rc)
			goto error;
	}
#endif

	return sdr;

error:
	sdr_close(sdr);
	return NULL;
}

static void *sdr_write_child(void *arg)
{
	sdr_t *sdr = (sdr_t *)arg;
	int num;
	int fill, out;
	int s, ss, o;

	while (sdr->thread_write.running) {
		/* write to SDR */
		fill = (sdr->thread_write.in - sdr->thread_write.out + sdr->thread_write.buffer_size) % sdr->thread_write.buffer_size;
		if (fill > sdr->thread_write.max_fill)
			sdr->thread_write.max_fill = fill;
		if (sdr->thread_write.max_fill_timer == 0.0)
			sdr->thread_write.max_fill_timer = get_time();
		if (get_time() - sdr->thread_write.max_fill_timer > 1.0) {
			double delay;
			delay = (double)sdr->thread_write.max_fill / 2.0 / (double)sdr->samplerate;
			sdr->thread_write.max_fill = 0;
			sdr->thread_write.max_fill_timer += 1.0;
			PDEBUG(DSDR, DEBUG_DEBUG, "write delay = %.3f ms\n", delay * 1000.0);
		}
		num = fill / 2;
		if (num) {
#ifdef DEBUG_BUFFER
			printf("Thread found %d samples in write buffer and forwards them to SDR.\n", num);
#endif
			out = sdr->thread_write.out;
			for (s = 0, ss = 0; s < num; s++) {
				for (o = 0; o < sdr->oversample; o++) {
					sdr->thread_write.buffer2[ss++] = sdr->thread_write.buffer[out];
					sdr->thread_write.buffer2[ss++] = sdr->thread_write.buffer[out + 1];
				}
				out = (out + 2) % sdr->thread_write.buffer_size;
			}
			sdr->thread_write.out = out;
#ifndef DISABLE_FILTER
			/* filter spectrum */
			if (sdr->oversample > 1) {
				iir_process_baseband(&sdr->thread_write.lp[0], sdr->thread_write.buffer2, num * sdr->oversample);
				iir_process_baseband(&sdr->thread_write.lp[1], sdr->thread_write.buffer2 + 1, num * sdr->oversample);
			}
#endif
#ifdef HAVE_UHD
			if (sdr_config->uhd)
				uhd_send(sdr->thread_write.buffer2, num * sdr->oversample);
#endif
#ifdef HAVE_SOAPY
			if (sdr_config->soapy)
				soapy_send(sdr->thread_write.buffer2, num * sdr->oversample);
#endif
		}

		/* delay some time */
		usleep(1000);
	}

	PDEBUG(DSDR, DEBUG_DEBUG, "Thread received exit!\n");
	sdr->thread_write.exit = 1;
	return NULL;
}

static void *sdr_read_child(void *arg)
{
	sdr_t *sdr = (sdr_t *)arg;
	int num, count = 0;
	int space, in;
	int s, ss;

	while (sdr->thread_read.running) {
		/* read from SDR */
		space = (sdr->thread_read.out - sdr->thread_read.in - 2 + sdr->thread_read.buffer_size) % sdr->thread_read.buffer_size;
		num = space / 2;
		if (num) {
#ifdef HAVE_UHD
			if (sdr_config->uhd)
				count = uhd_receive(sdr->thread_read.buffer2, num);
#endif
#ifdef HAVE_SOAPY
			if (sdr_config->soapy)
				count = soapy_receive(sdr->thread_read.buffer2, num);
#endif
			if (count > 0) {
#ifdef DEBUG_BUFFER
				printf("Thread read %d samples from SDR and writes them to read buffer.\n", count);
#endif
#ifndef DISABLE_FILTER
				/* filter spectrum */
				if (sdr->oversample > 1) {
					iir_process_baseband(&sdr->thread_read.lp[0], sdr->thread_read.buffer2, count);
					iir_process_baseband(&sdr->thread_read.lp[1], sdr->thread_read.buffer2 + 1, count);
				}
#endif
				in = sdr->thread_read.in;
				for (s = 0, ss = 0; s < count; s++) {
					sdr->thread_read.buffer[in++] = sdr->thread_read.buffer2[ss++];
					sdr->thread_read.buffer[in++] = sdr->thread_read.buffer2[ss++];
					in %= sdr->thread_read.buffer_size;
				}
				sdr->thread_read.in = in;
			}
		}

		/* delay some time */
		usleep(1000);
	}

	PDEBUG(DSDR, DEBUG_DEBUG, "Thread received exit!\n");
	sdr->thread_read.exit = 1;
	return NULL;
}

/* start streaming */
int sdr_start(void *inst)
{
	sdr_t *sdr = (sdr_t *)inst;
	int rc = -EINVAL;

#ifdef HAVE_UHD
	if (sdr_config->uhd)
		rc = uhd_start();
#endif
#ifdef HAVE_SOAPY
	if (sdr_config->soapy)
		rc = soapy_start();
#endif
	if (rc < 0)
		return rc;

	if (sdr->threads) {
		int rc;
		pthread_t tid;
		char tname[64];

		PDEBUG(DSDR, DEBUG_DEBUG, "Create threads!\n");
		sdr->thread_write.running = 1;
		sdr->thread_write.exit = 0;
		rc = pthread_create(&tid, NULL, sdr_write_child, inst);
		if (rc < 0) {
			sdr->thread_write.running = 0;
			PDEBUG(DSDR, DEBUG_ERROR, "Failed to create thread!\n");
			return rc;
		}
		pthread_getname_np(tid, tname, sizeof(tname));
		strncat(tname, "-sdr_tx", sizeof(tname));
		tname[sizeof(tname) - 1] = '\0';
		pthread_setname_np(tid, tname);
		sdr->thread_read.running = 1;
		sdr->thread_read.exit = 0;
		rc = pthread_create(&tid, NULL, sdr_read_child, inst);
		if (rc < 0) {
			sdr->thread_read.running = 0;
			PDEBUG(DSDR, DEBUG_ERROR, "Failed to create thread!\n");
			return rc;
		}
		pthread_getname_np(tid, tname, sizeof(tname));
		strncat(tname, "-sdr_rx", sizeof(tname));
		tname[sizeof(tname) - 1] = '\0';
		pthread_setname_np(tid, tname);
	}

	return 0;
}

void sdr_close(void *inst)
{
	sdr_t *sdr = (sdr_t *)inst;

	PDEBUG(DSDR, DEBUG_DEBUG, "Close SDR device\n");

	if (sdr->threads) {
		if (sdr->thread_write.running) {
			PDEBUG(DSDR, DEBUG_DEBUG, "Thread sending exit!\n");
			sdr->thread_write.running = 0;
			while (sdr->thread_write.exit == 0)
				usleep(1000);
		}
		if (sdr->thread_read.running) {
			PDEBUG(DSDR, DEBUG_DEBUG, "Thread sending exit!\n");
			sdr->thread_read.running = 0;
			while (sdr->thread_read.exit == 0)
				usleep(1000);
		}
	}

	if (sdr->thread_read.buffer)
		free((void *)sdr->thread_read.buffer);
	if (sdr->thread_read.buffer2)
		free((void *)sdr->thread_read.buffer2);
	if (sdr->thread_write.buffer)
		free((void *)sdr->thread_write.buffer);
	if (sdr->thread_write.buffer2)
		free((void *)sdr->thread_write.buffer2);

#ifdef HAVE_UHD
	if (sdr_config->uhd)
		uhd_close();
#endif

#ifdef HAVE_SOAPY
	if (sdr_config->soapy)
		soapy_close();
#endif

	if (sdr) {
		free(sdr->modbuff);
		free(sdr->modbuff_I);
		free(sdr->modbuff_Q);
		free(sdr->wavespl0);
		free(sdr->wavespl1);
		wave_destroy_record(&sdr->wave_rx_rec);
		wave_destroy_record(&sdr->wave_tx_rec);
		wave_destroy_playback(&sdr->wave_rx_play);
		wave_destroy_playback(&sdr->wave_tx_play);
		if (sdr->chan) {
			int c;

			for (c = 0; c < sdr->channels; c++) {
				fm_mod_exit(&sdr->chan[c].mod);
				fm_demod_exit(&sdr->chan[c].demod);
			}
			if (sdr->paging_channel)
				fm_mod_exit(&sdr->chan[sdr->paging_channel].mod);
			free(sdr->chan);
		}
		free(sdr);
		sdr = NULL;
	}
}

int sdr_write(void *inst, sample_t **samples, uint8_t **power, int num, enum paging_signal __attribute__((unused)) *paging_signal, int *on, int channels)
{
	sdr_t *sdr = (sdr_t *)inst;
	float *buff = NULL;
	int c, s, ss;
	int sent = 0;

	if (num > sdr->latspl) {
		fprintf(stderr, "exceeding maximum size given by sdr_latspl, please fix!\n");
		abort();
	}
	if (channels != sdr->channels && channels != 0) {
		PDEBUG(DSDR, DEBUG_ERROR, "Invalid number of channels, please fix!\n");
		abort();
	}

	/* process all channels */
	if (channels) {
		buff = sdr->modbuff;
		memset(buff, 0, sizeof(*buff) * num * 2);
		for (c = 0; c < channels; c++) {
			/* switch to paging channel, if requested */
			if (on[c] && sdr->paging_channel)
				fm_modulate_complex(&sdr->chan[sdr->paging_channel].mod, samples[c], power[c], num, buff);
			else
				fm_modulate_complex(&sdr->chan[c].mod, samples[c], power[c], num, buff);
		}
	} else {
		buff = (float *)samples;
	}

	if (sdr->wave_tx_rec.fp) {
		sample_t *spl_list[2] = { sdr->wavespl0, sdr->wavespl1 };
		for (s = 0, ss = 0; s < num; s++) {
			spl_list[0][s] = buff[ss++];
			spl_list[1][s] = buff[ss++];
		}
		wave_write(&sdr->wave_tx_rec, spl_list, num);
	}
	if (sdr->wave_tx_play.fp) {
		sample_t *spl_list[2] = { sdr->wavespl0, sdr->wavespl1 };
		wave_read(&sdr->wave_tx_play, spl_list, num);
		for (s = 0, ss = 0; s < num; s++) {
			buff[ss++] = spl_list[0][s];
			buff[ss++] = spl_list[1][s];
		}
	}

	if (sdr->threads) {
		/* store data towards SDR in ring buffer */
		int space, in;

		space = (sdr->thread_write.out - sdr->thread_write.in - 2 + sdr->thread_write.buffer_size) % sdr->thread_write.buffer_size;
		if (space < num * 2) {
			PDEBUG(DSDR, DEBUG_ERROR, "Write SDR buffer overflow!\n");
			num = space / 2;
		}
#ifdef DEBUG_BUFFER
		printf("Writing %d samples to write buffer.\n", num);
#endif
		in = sdr->thread_write.in;
		for (s = 0, ss = 0; s < num; s++) {
			sdr->thread_write.buffer[in++] = buff[ss++];
			sdr->thread_write.buffer[in++] = buff[ss++];
			in %= sdr->thread_write.buffer_size;
		}
		sdr->thread_write.in = in;
		sent = num;
	} else {
#ifdef HAVE_UHD
		if (sdr_config->uhd)
			sent = uhd_send(buff, num);
#endif
#ifdef HAVE_SOAPY
		if (sdr_config->soapy)
			sent = soapy_send(buff, num);
#endif
		if (sent < 0)
			return sent;
	}
	
	return sent;
}

int sdr_read(void *inst, sample_t **samples, int num, int channels, double *rf_level_db)
{
	sdr_t *sdr = (sdr_t *)inst;
	float *buff = NULL;
	int count = 0;
	int c, s, ss;

	if (num > sdr->latspl) {
		fprintf(stderr, "exceeding maximum size given by sdr_latspl, please fix!\n");
		abort();
	}

	if (channels) {
		buff = sdr->modbuff;
	} else {
		buff = (float *)samples;
	}

	if (sdr->threads) {
		/* load data from SDR out of ring buffer */
		int fill, out;

		fill = (sdr->thread_read.in - sdr->thread_read.out + sdr->thread_read.buffer_size) % sdr->thread_read.buffer_size;
		if (fill > sdr->thread_read.max_fill)
			sdr->thread_read.max_fill = fill;
		if (sdr->thread_read.max_fill_timer == 0.0)
			sdr->thread_read.max_fill_timer = get_time();
		if (get_time() - sdr->thread_read.max_fill_timer > 1.0) {
			double delay;
			delay = (double)sdr->thread_read.max_fill / 2.0 / (double)sdr_config->samplerate;
			sdr->thread_read.max_fill = 0;
			sdr->thread_read.max_fill_timer += 1.0;
			PDEBUG(DSDR, DEBUG_DEBUG, "read delay = %.3f ms\n", delay * 1000.0);
		}
		if (fill / 2 / sdr->oversample < num)
			num = fill / 2 / sdr->oversample;
#ifdef DEBUG_BUFFER
		printf("Reading %d samples from read buffer.\n", num);
#endif
		out = sdr->thread_read.out;
		for (s = 0, ss = 0; s < num; s++) {
			buff[ss++] = sdr->thread_read.buffer[out];
			buff[ss++] = sdr->thread_read.buffer[out + 1];
			out = (out + 2 * sdr->oversample) % sdr->thread_read.buffer_size;
		}
		sdr->thread_read.out = out;
		count = num;
	} else {
#ifdef HAVE_UHD
		if (sdr_config->uhd)
			count = uhd_receive(buff, num);
#endif
#ifdef HAVE_SOAPY
		if (sdr_config->soapy)
			count = soapy_receive(buff, num);
#endif
		if (count <= 0)
			return count;
	}

	if (sdr->wave_rx_rec.fp) {
		sample_t *spl_list[2] = { sdr->wavespl0, sdr->wavespl1 };
		for (s = 0, ss = 0; s < count; s++) {
			spl_list[0][s] = buff[ss++];
			spl_list[1][s] = buff[ss++];
		}
		wave_write(&sdr->wave_rx_rec, spl_list, count);
	}
	if (sdr->wave_rx_play.fp) {
		sample_t *spl_list[2] = { sdr->wavespl0, sdr->wavespl1 };
		wave_read(&sdr->wave_rx_play, spl_list, count);
		for (s = 0, ss = 0; s < count; s++) {
			buff[ss++] = spl_list[0][s];
			buff[ss++] = spl_list[1][s];
		}
	}
	display_iq(buff, count);
	display_spectrum(buff, count);

	if (channels) {
		for (c = 0; c < channels; c++) {
			fm_demodulate_complex(&sdr->chan[c].demod, samples[c], count, buff, sdr->modbuff_I, sdr->modbuff_Q);
			sender_t *sender = get_sender_by_empfangsfrequenz(sdr->chan[c].rx_frequency);
			if (!sender || !count)
				continue;
			double min, max, avg;
			avg = 0.0;
			for (s = 0; s < count; s++) {
				/* average the square length of vector */
				avg += sdr->modbuff_I[s] * sdr->modbuff_I[s] + sdr->modbuff_Q[s] * sdr->modbuff_Q[s];
			}
			avg = sqrt(avg /(double)count); /* RMS */
			avg = log10(avg) * 20;
			display_measurements_update(sdr->chan[c].dmp_rf_level, avg, 0.0);
			rf_level_db[c] = avg;
			min = 0.0;
			max = 0.0;
			avg = 0.0;
			for (s = 0; s < count; s++) {
				avg += samples[c][s];
				if (s == 0 || samples[c][s] > max)
					max = samples[c][s];
				if (s == 0 || samples[c][s] < min)
					min = samples[c][s];
			}
			avg /= (double)count;
			display_measurements_update(sdr->chan[c].dmp_freq_offset, avg / 1000.0, 0.0);
			/* use half min and max, because we want the deviation above/below (+-) center frequency. */
			display_measurements_update(sdr->chan[c].dmp_deviation, min / 2.0 / 1000.0, max / 2.0 / 1000.0);
		}
	}

	return count;
}

/* how much do we need to send (in audio sample duration) to get the target delay (latspl) */
int sdr_get_tosend(void *inst, int latspl)
{
	sdr_t *sdr = (sdr_t *)inst;
	int count = 0;

#ifdef HAVE_UHD
	if (sdr_config->uhd)
		count = uhd_get_tosend(latspl * sdr->oversample);
#endif
#ifdef HAVE_SOAPY
	if (sdr_config->soapy)
		count = soapy_get_tosend(latspl * sdr->oversample);
#endif
	if (count < 0)
		return count;
	count /= sdr->oversample;

	if (sdr->threads) {
		/* substract what we have in write buffer, because this is not jent sent to the SDR */
		int fill;

		fill = (sdr->thread_write.in - sdr->thread_write.out + sdr->thread_write.buffer_size) % sdr->thread_write.buffer_size;
		count -= fill / 2;
		if (count < 0)
			count = 0;
	}

	return count;
}


