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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include "soapy.h"
#include "../libdebug/debug.h"
#include "../liboptions/options.h"

extern int sdr_rx_overflow;

static SoapySDRDevice *sdr = NULL;
SoapySDRStream *rxStream = NULL;
SoapySDRStream *txStream = NULL;
static int			tx_samps_per_buff, rx_samps_per_buff;
static double			samplerate;
static uint64_t			rx_count = 0;
static uint64_t			tx_count = 0;

static int parse_args(SoapySDRKwargs *args, const char *_args_string)
{
	char *args_string = options_strdup(_args_string), *key, *val;

	memset(args, 0, sizeof(*args));
	while (args_string && *args_string) {
		key = args_string;
		val = strchr(key, '=');
		if (!val) {
			PDEBUG(DSOAPY, DEBUG_ERROR, "Error parsing SDR args: No '=' after key\n");
			soapy_close();
			return -EIO;
		}
		*val++ = '\0';
		args_string = strchr(val, ',');
		if (args_string)
			*args_string++ = '\0';
		PDEBUG(DSOAPY, DEBUG_DEBUG, "SDR device args: key='%s' value='%s'\n", key, val);
		SoapySDRKwargs_set(args, key, val);
	}

	return 0;
}

int soapy_open(size_t channel, const char *_device_args, const char *_stream_args, const char *_tune_args, const char *tx_antenna, const char *rx_antenna, const char *clock_source, double tx_frequency, double rx_frequency, double lo_offset, double rate, double tx_gain, double rx_gain, double bandwidth)
{
	double got_frequency, got_rate, got_gain, got_bandwidth;
	const char *got_antenna, *got_clock;
	size_t num_channels;
	SoapySDRKwargs device_args;
	SoapySDRKwargs stream_args;
	SoapySDRKwargs tune_args;
	int rc;

	samplerate = rate;

	/* parsing ARGS */
	PDEBUG(DSOAPY, DEBUG_INFO, "Using device args \"%s\"\n", _device_args);
	rc = parse_args(&device_args, _device_args);
	if (rc < 0)
		return rc;
	PDEBUG(DSOAPY, DEBUG_INFO, "Using stream args \"%s\"\n", _stream_args);
	rc = parse_args(&stream_args, _stream_args);
	if (rc < 0)
		return rc;
	PDEBUG(DSOAPY, DEBUG_INFO, "Using tune args \"%s\"\n", _tune_args);
	rc = parse_args(&tune_args, _tune_args);
	if (rc < 0)
		return rc;

	if (lo_offset) {
		char val[32];
		snprintf(val, sizeof(val), "%.0f", lo_offset);
		val[sizeof(val) - 1] = '\0';
		SoapySDRKwargs_set(&tune_args, "OFFSET", val);
	}

	/* create SoapySDR device */
	sdr = SoapySDRDevice_make(&device_args);
	if (!sdr) {
		PDEBUG(DSOAPY, DEBUG_ERROR, "Failed to create SoapySDR\n");
		soapy_close();
		return -EIO;
	}

	/* clock source */
	if (clock_source && clock_source[0]) {
		if (!strcasecmp(clock_source, "list")) {
			char **clocks;
			size_t clocks_length;
			int i;
			clocks = SoapySDRDevice_listClockSources(sdr, &clocks_length);
			if (!clocks) {
				PDEBUG(DSOAPY, DEBUG_ERROR, "Failed to request list of clock sources!\n");
				soapy_close();
				return -EIO;
			}
			if (clocks_length) {
				for (i = 0; i < (int)clocks_length; i++)
					PDEBUG(DSOAPY, DEBUG_NOTICE, "Clock source: '%s'\n", clocks[i]);
				got_clock = SoapySDRDevice_getClockSource(sdr);
				PDEBUG(DSOAPY, DEBUG_NOTICE, "Default clock source: '%s'\n", got_clock);
			} else
				PDEBUG(DSOAPY, DEBUG_NOTICE, "There are no clock sources configurable for this device.\n");
			soapy_close();
			return 1;
		}

		if (SoapySDRDevice_setClockSource(sdr, clock_source) != 0) {
			PDEBUG(DSOAPY, DEBUG_ERROR, "Failed to set clock source to '%s'\n", clock_source);
			soapy_close();
			return -EIO;
		}
		got_clock = SoapySDRDevice_getClockSource(sdr);
		if (!!strcasecmp(clock_source, got_clock)) {
			PDEBUG(DSOAPY, DEBUG_NOTICE, "Given clock source '%s' was accepted, but driver claims to use '%s'\n", clock_source, got_clock);
			soapy_close();
			return -EINVAL;
		}
	}

	if (rx_frequency) {
		/* get number of channels and check if requested channel is in range */
		num_channels = SoapySDRDevice_getNumChannels(sdr, SOAPY_SDR_RX);
		PDEBUG(DSOAPY, DEBUG_DEBUG, "We have %d RX channel, selecting channel #%d\n", (int)num_channels, (int)channel);
		if (channel >= num_channels) {
			PDEBUG(DSOAPY, DEBUG_ERROR, "Requested channel #%d (capable of RX) does not exist. Please select channel %d..%d!\n", (int)channel, 0, (int)num_channels - 1);
			soapy_close();
			return -EIO;
		}

		/* antenna */
		if (rx_antenna && rx_antenna[0]) {
			if (!strcasecmp(rx_antenna, "list")) {
				char **antennas;
				size_t antennas_length;
				int i;
				antennas = SoapySDRDevice_listAntennas(sdr, SOAPY_SDR_RX, channel, &antennas_length);
				if (!antennas) {
					PDEBUG(DSOAPY, DEBUG_ERROR, "Failed to request list of RX antennas!\n");
					soapy_close();
					return -EIO;
				}
				for (i = 0; i < (int)antennas_length; i++)
					PDEBUG(DSOAPY, DEBUG_NOTICE, "RX Antenna: '%s'\n", antennas[i]);
				got_antenna = SoapySDRDevice_getAntenna(sdr, SOAPY_SDR_RX, channel);
				PDEBUG(DSOAPY, DEBUG_NOTICE, "Default RX Antenna: '%s'\n", got_antenna);
				soapy_close();
				return 1;
			}

			if (SoapySDRDevice_setAntenna(sdr, SOAPY_SDR_RX, channel, rx_antenna) != 0) {
				PDEBUG(DSOAPY, DEBUG_ERROR, "Failed to set RX antenna to '%s'\n", rx_antenna);
				soapy_close();
				return -EIO;
			}
			got_antenna = SoapySDRDevice_getAntenna(sdr, SOAPY_SDR_RX, channel);
			if (!!strcasecmp(rx_antenna, got_antenna)) {
				PDEBUG(DSOAPY, DEBUG_NOTICE, "Given RX antenna '%s' was accepted, but driver claims to use '%s'\n", rx_antenna, got_antenna);
				soapy_close();
				return -EINVAL;
			}
		}

		/* set rate */
		if (SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_RX, channel, rate) != 0) {
			PDEBUG(DSOAPY, DEBUG_ERROR, "Failed to set RX rate to %.0f Hz\n", rate);
			soapy_close();
			return -EIO;
		}

		/* see what rate actually is */
		got_rate = SoapySDRDevice_getSampleRate(sdr, SOAPY_SDR_RX, channel);
		if (fabs(got_rate - rate) > 1.0) {
			PDEBUG(DSOAPY, DEBUG_ERROR, "Given RX rate %.3f Hz is not supported, try %.3f Hz\n", rate, got_rate);
			soapy_close();
			return -EINVAL;
		}

		if (rx_gain) {
			/* set gain */
			if (SoapySDRDevice_setGain(sdr, SOAPY_SDR_RX, channel, rx_gain) != 0) {
				PDEBUG(DSOAPY, DEBUG_ERROR, "Failed to set RX gain to %.0f\n", rx_gain);
				soapy_close();
				return -EIO;
			}

			/* see what gain actually is */
			got_gain = SoapySDRDevice_getGain(sdr, SOAPY_SDR_RX, channel);
			if (fabs(got_gain - rx_gain) > 0.001) {
				PDEBUG(DSOAPY, DEBUG_NOTICE, "Given RX gain %.3f is not supported, we use %.3f\n", rx_gain, got_gain);
				rx_gain = got_gain;
			}
		}

		/* hack to make limesdr tune rx to tx */
		if (tx_frequency == rx_frequency)
			rx_frequency += 1.0;

		/* set frequency */
		if (SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, channel, rx_frequency, &tune_args) != 0) {
			PDEBUG(DSOAPY, DEBUG_ERROR, "Failed to set RX frequency to %.0f Hz\n", rx_frequency);
			soapy_close();
			return -EIO;
		}

		/* see what frequency actually is */
		got_frequency = SoapySDRDevice_getFrequency(sdr, SOAPY_SDR_RX, channel);
		if (fabs(got_frequency - rx_frequency) > 100.0) {
			PDEBUG(DSOAPY, DEBUG_ERROR, "Given RX frequency %.0f Hz is not supported, try %.0f Hz\n", rx_frequency, got_frequency);
			soapy_close();
			return -EINVAL;
		}

		/* set bandwidth */
		if (SoapySDRDevice_setBandwidth(sdr, SOAPY_SDR_RX, channel, bandwidth) != 0) {
			PDEBUG(DSOAPY, DEBUG_ERROR, "Failed to set RX bandwidth to %.0f Hz\n", bandwidth);
			soapy_close();
			return -EIO;
		}

		/* see what bandwidth actually is */
		got_bandwidth = SoapySDRDevice_getBandwidth(sdr, SOAPY_SDR_RX, channel);
		if (fabs(got_bandwidth - bandwidth) > 100.0) {
			PDEBUG(DSOAPY, DEBUG_ERROR, "Given RX bandwidth %.0f Hz is not supported, try %.0f Hz\n", bandwidth, got_bandwidth);
			soapy_close();
			return -EINVAL;
		}

		/* set up streamer */
#ifdef SOAPY_0_7_1_OR_HIGHER
		if (!(rxStream = SoapySDRDevice_setupStream(sdr, SOAPY_SDR_RX, SOAPY_SDR_CF32, &channel, 1, &stream_args)))
#else
		if (SoapySDRDevice_setupStream(sdr, &rxStream, SOAPY_SDR_RX, SOAPY_SDR_CF32, &channel, 1, &stream_args) != 0)
#endif
		{
			PDEBUG(DSOAPY, DEBUG_ERROR, "Failed to set RX streamer args\n");
			soapy_close();
			return -EIO;
		}

		/* get buffer sizes */
		rx_samps_per_buff = SoapySDRDevice_getStreamMTU(sdr, rxStream);
		if (rx_samps_per_buff == 0) {
			PDEBUG(DSOAPY, DEBUG_ERROR, "Failed to get RX streamer sample buffer\n");
			soapy_close();
			return -EIO;
		}
	}

	if (tx_frequency) {
		/* get number of channels and check if requested channel is in range */
		num_channels = SoapySDRDevice_getNumChannels(sdr, SOAPY_SDR_TX);
		PDEBUG(DSOAPY, DEBUG_DEBUG, "We have %d TX channel, selecting channel #%d\n", (int)num_channels, (int)channel);
		if (channel >= num_channels) {
			PDEBUG(DSOAPY, DEBUG_ERROR, "Requested channel #%d (capable of TX) does not exist. Please select channel %d..%d!\n", (int)channel, 0, (int)num_channels - 1);
			soapy_close();
			return -EIO;
		}

		/* antenna */
		if (tx_antenna && tx_antenna[0]) {
			if (!strcasecmp(tx_antenna, "list")) {
				char **antennas;
				size_t antennas_length;
				int i;
				antennas = SoapySDRDevice_listAntennas(sdr, SOAPY_SDR_TX, channel, &antennas_length);
				if (!antennas) {
					PDEBUG(DSOAPY, DEBUG_ERROR, "Failed to request list of TX antennas!\n");
					soapy_close();
					return -EIO;
				}
				for (i = 0; i < (int)antennas_length; i++)
					PDEBUG(DSOAPY, DEBUG_NOTICE, "TX Antenna: '%s'\n", antennas[i]);
				got_antenna = SoapySDRDevice_getAntenna(sdr, SOAPY_SDR_TX, channel);
				PDEBUG(DSOAPY, DEBUG_NOTICE, "Default TX Antenna: '%s'\n", got_antenna);
				soapy_close();
				return 1;
			}

			if (SoapySDRDevice_setAntenna(sdr, SOAPY_SDR_TX, channel, tx_antenna) != 0) {
				PDEBUG(DSOAPY, DEBUG_ERROR, "Failed to set TX antenna to '%s'\n", tx_antenna);
				soapy_close();
				return -EIO;
			}
			got_antenna = SoapySDRDevice_getAntenna(sdr, SOAPY_SDR_TX, channel);
			if (!!strcasecmp(tx_antenna, got_antenna)) {
				PDEBUG(DSOAPY, DEBUG_NOTICE, "Given TX antenna '%s' was accepted, but driver claims to use '%s'\n", tx_antenna, got_antenna);
				soapy_close();
				return -EINVAL;
			}
		}

		/* set rate */
		if (SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_TX, channel, rate) != 0) {
			PDEBUG(DSOAPY, DEBUG_ERROR, "Failed to set TX rate to %.0f Hz\n", rate);
			soapy_close();
			return -EIO;
		}

		/* see what rate actually is */
		got_rate = SoapySDRDevice_getSampleRate(sdr, SOAPY_SDR_TX, channel);
		if (fabs(got_rate - rate) > 1.0) {
			PDEBUG(DSOAPY, DEBUG_ERROR, "Given TX rate %.3f Hz is not supported, try %.3f Hz\n", rate, got_rate);
			soapy_close();
			return -EINVAL;
		}

		if (tx_gain) {
			/* set gain */
			if (SoapySDRDevice_setGain(sdr, SOAPY_SDR_TX, channel, tx_gain) != 0) {
				PDEBUG(DSOAPY, DEBUG_ERROR, "Failed to set TX gain to %.0f\n", tx_gain);
				soapy_close();
				return -EIO;
			}

			/* see what gain actually is */
			got_gain = SoapySDRDevice_getGain(sdr, SOAPY_SDR_TX, channel);
			if (fabs(got_gain - tx_gain) > 0.001) {
				PDEBUG(DSOAPY, DEBUG_NOTICE, "Given TX gain %.3f is not supported, we use %.3f\n", tx_gain, got_gain);
				tx_gain = got_gain;
			}
		}

		/* set frequency */
		if (SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_TX, channel, tx_frequency, &tune_args) != 0) {
			PDEBUG(DSOAPY, DEBUG_ERROR, "Failed to set TX frequency to %.0f Hz\n", tx_frequency);
			soapy_close();
			return -EIO;
		}

		/* see what frequency actually is */
		got_frequency = SoapySDRDevice_getFrequency(sdr, SOAPY_SDR_TX, channel);
		if (fabs(got_frequency - tx_frequency) > 100.0) {
			PDEBUG(DSOAPY, DEBUG_ERROR, "Given TX frequency %.0f Hz is not supported, try %.0f Hz\n", tx_frequency, got_frequency);
			soapy_close();
			return -EINVAL;
		}

		/* set bandwidth */
		if (SoapySDRDevice_setBandwidth(sdr, SOAPY_SDR_TX, channel, bandwidth) != 0) {
			PDEBUG(DSOAPY, DEBUG_ERROR, "Failed to set TX bandwidth to %.0f Hz\n", bandwidth);
			soapy_close();
			return -EIO;
		}

		/* see what bandwidth actually is */
		got_bandwidth = SoapySDRDevice_getBandwidth(sdr, SOAPY_SDR_TX, channel);
		if (fabs(got_bandwidth - bandwidth) > 100.0) {
			PDEBUG(DSOAPY, DEBUG_ERROR, "Given TX bandwidth %.0f Hz is not supported, try %.0f Hz\n", bandwidth, got_bandwidth);
			soapy_close();
			return -EINVAL;
		}

		/* set up streamer */
#ifdef SOAPY_0_7_1_OR_HIGHER
		if (!(txStream = SoapySDRDevice_setupStream(sdr, SOAPY_SDR_TX, SOAPY_SDR_CF32, &channel, 1, &stream_args)))
#else
		if (SoapySDRDevice_setupStream(sdr, &txStream, SOAPY_SDR_TX, SOAPY_SDR_CF32, &channel, 1, &stream_args) != 0)
#endif
		{
			PDEBUG(DSOAPY, DEBUG_ERROR, "Failed to set TX streamer args\n");
			soapy_close();
			return -EIO;
		}

		/* get buffer sizes */
		tx_samps_per_buff = SoapySDRDevice_getStreamMTU(sdr, txStream);
		if (tx_samps_per_buff == 0) {
			PDEBUG(DSOAPY, DEBUG_ERROR, "Failed to get TX streamer sample buffer\n");
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
		PDEBUG(DSOAPY, DEBUG_ERROR, "Failed to issue RX stream command\n");
		return -EIO;
	}

	/* enable tx stream */
	if (SoapySDRDevice_activateStream(sdr, txStream, 0, 0, 0) != 0) {
		PDEBUG(DSOAPY, DEBUG_ERROR, "Failed to issue TX stream command\n");
		return -EIO;
	}
	return 0;
}

void soapy_close(void)
{
	PDEBUG(DSOAPY, DEBUG_DEBUG, "Clean up SoapySDR\n");
	if (txStream) {
		SoapySDRDevice_deactivateStream(sdr, txStream, 0, 0);
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
		count = SoapySDRDevice_writeStream(sdr, txStream, buffs_ptr, chunk, &flags, 0, 1000000);
		if (count <= 0) {
			PDEBUG(DUHD, DEBUG_ERROR, "Failed to write to TX streamer (error=%d)\n", count);
			break;
		}

		sent += count;
		buff += count * 2;
		num -= count;
	}
	/* increment tx counter */
	tx_count += sent;

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
			sdr_rx_overflow = 1;
			break;
		}
		/* read RX stream */
		buffs_ptr[0] = buff;
		count = SoapySDRDevice_readStream(sdr, rxStream, buffs_ptr, rx_samps_per_buff, &flags, &timeNs, 0);
		if (count > 0) {
			/* commit received data to buffer */
			got += count;
			buff += count * 2;
			max -= count;
		} else {
			/* got nothing this time */
			break;
		}
	}
	/* update current rx time */
	rx_count += got;

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
	if (tosend > latspl) {
// It is normal that we have underruns, prior initial filling of buffer.
// FIXME: better solution to detect underrun
//		PDEBUG(DSOAPY, DEBUG_ERROR, "SDR TX underrun!\n");
		tosend = 0;
		tx_count = rx_count;
	}
	if (tosend < 0)
		tosend = 0;

	return tosend;
}

