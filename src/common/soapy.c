/* SoapySDR device access
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
#include <errno.h>
#include <math.h>
#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include "soapy.h"
#include "debug.h"

static SoapySDRDevice *sdr = NULL;
SoapySDRStream *rxStream = NULL;
SoapySDRStream *txStream = NULL;
static int			tx_samps_per_buff, rx_samps_per_buff;
static double			samplerate;
static uint64_t			rx_count = 0;
static uint64_t			tx_count = 0;

int soapy_open(const char *device_args, double tx_frequency, double rx_frequency, double rate, double rx_gain, double tx_gain, double bandwidth)
{
	double got_frequency, got_rate, got_gain, got_bandwidth;
	size_t channel = 0;
	char *arg_string = strdup(device_args), *key, *val;
	SoapySDRKwargs args;

	samplerate = rate;

	/* create SoapySDR device */
	PDEBUG(DUHD, DEBUG_INFO, "Creating SoapySDR with args \"%s\"...\n", arg_string);
	memset(&args, 0, sizeof(args));
	while (arg_string && *arg_string) {
		key = arg_string;
		val = strchr(key, '=');
		if (!val) {
			PDEBUG(DUHD, DEBUG_ERROR, "Error parsing SDR args: No '=' after key\n");
			soapy_close();
			return -EIO;
		}
		*val++ = '\0';
		arg_string = strchr(val, ',');
		if (arg_string)
			*arg_string++ = '\0';
		SoapySDRKwargs_set(&args, key, val);
	}
	sdr = SoapySDRDevice_make(&args);
	if (!sdr) {
		PDEBUG(DUHD, DEBUG_ERROR, "Failed to create SoapySDR\n");
		soapy_close();
		return -EIO;
	}

	if (tx_frequency) {
		/* set rate */
		if (SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_TX, channel, rate) != 0) {
			PDEBUG(DUHD, DEBUG_ERROR, "Failed to set TX rate to %.0f Hz\n", rate);
			soapy_close();
			return -EIO;
		}

		/* see what rate actually is */
		got_rate = SoapySDRDevice_getSampleRate(sdr, SOAPY_SDR_TX, channel);
		if (got_rate != rate) {
			PDEBUG(DUHD, DEBUG_ERROR, "Given TX rate %.0f Hz is not supported, try %0.f Hz\n", rate, got_rate);
			soapy_close();
			return -EINVAL;
		}

		/* set gain */
		if (SoapySDRDevice_setGain(sdr, SOAPY_SDR_TX, channel, tx_gain) != 0) {
			PDEBUG(DUHD, DEBUG_ERROR, "Failed to set TX gain to %.0f\n", tx_gain);
			soapy_close();
			return -EIO;
		}

		/* see what gain actually is */
		got_gain = SoapySDRDevice_getGain(sdr, SOAPY_SDR_TX, channel);
		if (got_gain != tx_gain) {
			PDEBUG(DUHD, DEBUG_NOTICE, "Given TX gain %.0f is not supported, we use %0.f\n", tx_gain, got_gain);
			tx_gain = got_gain;
		}

		/* set frequency */
		if (SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_TX, channel, tx_frequency, NULL) != 0) {
			PDEBUG(DUHD, DEBUG_ERROR, "Failed to set TX frequency to %.0f Hz\n", tx_frequency);
			soapy_close();
			return -EIO;
		}

		/* see what frequency actually is */
		got_frequency = SoapySDRDevice_getFrequency(sdr, SOAPY_SDR_TX, channel);
		if (got_frequency != tx_frequency) {
			PDEBUG(DUHD, DEBUG_ERROR, "Given TX frequency %.0f Hz is not supported, try %0.f Hz\n", tx_frequency, got_frequency);
			soapy_close();
			return -EINVAL;
		}

		/* set bandwidth */
		if (SoapySDRDevice_setBandwidth(sdr, SOAPY_SDR_TX, channel, bandwidth) != 0) {
			PDEBUG(DUHD, DEBUG_ERROR, "Failed to set TX bandwidth to %.0f Hz\n", bandwidth);
			soapy_close();
			return -EIO;
		}

		/* see what bandwidth actually is */
		got_bandwidth = SoapySDRDevice_getBandwidth(sdr, SOAPY_SDR_TX, channel);
		if (got_bandwidth != bandwidth) {
			PDEBUG(DUHD, DEBUG_ERROR, "Given TX bandwidth %.0f Hz is not supported, try %0.f Hz\n", bandwidth, got_bandwidth);
			soapy_close();
			return -EINVAL;
		}

		/* set up streamer */
		if (SoapySDRDevice_setupStream(sdr, &txStream, SOAPY_SDR_TX, SOAPY_SDR_CF32, &channel, 1, NULL) != 0) {
			PDEBUG(DUHD, DEBUG_ERROR, "Failed to set TX streamer args\n");
			soapy_close();
			return -EIO;
		}

		/* get buffer sizes */
		tx_samps_per_buff = SoapySDRDevice_getStreamMTU(sdr, txStream);
		if (tx_samps_per_buff == 0) {
			PDEBUG(DUHD, DEBUG_ERROR, "Failed to get TX streamer sample buffer\n");
			soapy_close();
			return -EIO;
		}
	}

	if (rx_frequency) {
		/* set rate */
		if (SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_RX, channel, rate) != 0) {
			PDEBUG(DUHD, DEBUG_ERROR, "Failed to set RX rate to %.0f Hz\n", rate);
			soapy_close();
			return -EIO;
		}

		/* see what rate actually is */
		got_rate = SoapySDRDevice_getSampleRate(sdr, SOAPY_SDR_RX, channel);
		if (got_rate != rate) {
			PDEBUG(DUHD, DEBUG_ERROR, "Given RX rate %.0f Hz is not supported, try %0.f Hz\n", rate, got_rate);
			soapy_close();
			return -EINVAL;
		}

		/* set gain */
		if (SoapySDRDevice_setGain(sdr, SOAPY_SDR_RX, channel, rx_gain) != 0) {
			PDEBUG(DUHD, DEBUG_ERROR, "Failed to set RX gain to %.0f\n", rx_gain);
			soapy_close();
			return -EIO;
		}

		/* see what gain actually is */
		got_gain = SoapySDRDevice_getGain(sdr, SOAPY_SDR_RX, channel);
		if (got_gain != rx_gain) {
			PDEBUG(DUHD, DEBUG_NOTICE, "Given RX gain %.3f is not supported, we use %.3f\n", rx_gain, got_gain);
			rx_gain = got_gain;
		}

		/* set frequency */
		if (SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, channel, rx_frequency, NULL) != 0) {
			PDEBUG(DUHD, DEBUG_ERROR, "Failed to set RX frequency to %.0f Hz\n", rx_frequency);
			soapy_close();
			return -EIO;
		}

		/* see what frequency actually is */
		got_frequency = SoapySDRDevice_getFrequency(sdr, SOAPY_SDR_RX, channel);
		if (got_frequency != rx_frequency) {
			PDEBUG(DUHD, DEBUG_ERROR, "Given RX frequency %.0f Hz is not supported, try %0.f Hz\n", rx_frequency, got_frequency);
			soapy_close();
			return -EINVAL;
		}

		/* set bandwidth */
		if (SoapySDRDevice_setBandwidth(sdr, SOAPY_SDR_RX, channel, bandwidth) != 0) {
			PDEBUG(DUHD, DEBUG_ERROR, "Failed to set RX bandwidth to %.0f Hz\n", bandwidth);
			soapy_close();
			return -EIO;
		}

		/* see what bandwidth actually is */
		got_bandwidth = SoapySDRDevice_getBandwidth(sdr, SOAPY_SDR_RX, channel);
		if (got_bandwidth != bandwidth) {
			PDEBUG(DUHD, DEBUG_ERROR, "Given RX bandwidth %.0f Hz is not supported, try %0.f Hz\n", bandwidth, got_bandwidth);
			soapy_close();
			return -EINVAL;
		}

		/* set up streamer */
		if (SoapySDRDevice_setupStream(sdr, &rxStream, SOAPY_SDR_RX, SOAPY_SDR_CF32, &channel, 1, NULL) != 0) {
			PDEBUG(DUHD, DEBUG_ERROR, "Failed to set RX streamer args\n");
			soapy_close();
			return -EIO;
		}

		/* get buffer sizes */
		rx_samps_per_buff = SoapySDRDevice_getStreamMTU(sdr, rxStream);
		if (rx_samps_per_buff == 0) {
			PDEBUG(DUHD, DEBUG_ERROR, "Failed to get RX streamer sample buffer\n");
			soapy_close();
			return -EIO;
		}
	}

	return 0;
}

/* start streaming */
int soapy_start(void)
{
	/* enable rx stream */
	if (SoapySDRDevice_activateStream(sdr, rxStream, 0, 0, 0) != 0) {
		PDEBUG(DUHD, DEBUG_ERROR, "Failed to issue RX stream command\n");
		return -EIO;
	}
	return 0;
}

void soapy_close(void)
{
	PDEBUG(DUHD, DEBUG_DEBUG, "Clean up UHD\n");
	if (txStream) {
		SoapySDRDevice_closeStream(sdr, txStream);
		txStream = NULL;
	}
	if (rxStream) {
		SoapySDRDevice_deactivateStream(sdr, rxStream, 0, 0);
		SoapySDRDevice_closeStream(sdr, rxStream);
		rxStream = NULL;
	}
	if (sdr) {
		SoapySDRDevice_unmake(sdr);
		sdr = NULL;
	}
}

int soapy_send(float *buff, int num)
{
    	const void *buffs_ptr[1];
	int chunk;
	int sent = 0, count;
	int flags = 0;

	while (num) {
		chunk = num;
		if (chunk > tx_samps_per_buff)
			chunk = tx_samps_per_buff;
		/* create tx metadata */
		buffs_ptr[0] = buff;
		count = SoapySDRDevice_writeStream(sdr, txStream, buffs_ptr, chunk, &flags, 0, 0);
		if (count <= 0)
			break;

		/* increment tx counter */
		tx_count += count;

		sent += count;
		buff += count * 2;
		num -= count;
	}

	return sent;
}

/* read what we got, return 0, if buffer is empty, otherwise return the number of samples */
int soapy_receive(float *buff, int max)
{
    	void *buffs_ptr[1];
	int got = 0, count;
	long long timeNs;
	int flags = 0;

	while (1) {
		if (max < rx_samps_per_buff) {
			/* no more space this time */
			PDEBUG(DUHD, DEBUG_ERROR, "SDR RX overflow!\n");
			break;
		}
		/* read RX stream */
		buffs_ptr[0] = buff;
		count = SoapySDRDevice_readStream(sdr, rxStream, buffs_ptr, rx_samps_per_buff, &flags, &timeNs, 0);
		if (count > 0) {
			/* update current rx time */
			rx_count += count;
			/* commit received data to buffer */
			got += count;
			buff += count * 2;
			max -= count;
		} else {
			/* got nothing this time */
			break;
		}
	}

	return got;
}

/* estimate number of samples that can be sent */
int soapy_get_tosend(int latspl)
{
	int tosend;

	/* we need the rx time stamp to determine how much data is already sent in advance */
	if (rx_count == 0)
		return 0;

	/* if we have not yet sent any data, we set initial tx time stamp */
	if (tx_count == 0)
		tx_count = rx_count;

	/* we check how advance our transmitted time stamp is */
	tosend = latspl - (tx_count - rx_count);
	/* in case of underrun: */
	if (tosend < 0) {
		PDEBUG(DUHD, DEBUG_ERROR, "SDR TX underrun!\n");
		tosend = 0;
	}

	return tosend;
}


