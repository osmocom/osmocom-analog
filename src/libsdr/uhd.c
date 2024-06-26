/* UHD device access
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
#include <uhd.h>
#include <uhd/usrp/usrp.h>
#include "uhd.h"
#include "../liblogging/logging.h"
#include "../liboptions/options.h"

extern int sdr_rx_overflow;

static uhd_usrp_handle		usrp = NULL;
static uhd_tx_streamer_handle	tx_streamer = NULL;
static uhd_rx_streamer_handle	rx_streamer = NULL;
static uhd_tx_metadata_handle	tx_metadata = NULL;
static uhd_rx_metadata_handle	rx_metadata = NULL;
static uhd_tune_request_t	tune_request;
static uhd_tune_result_t	tune_result;
static uhd_stream_args_t	stream_args;
static uhd_stream_cmd_t		stream_cmd;
static size_t tx_samps_per_buff, rx_samps_per_buff;
static double			samplerate;
static time_t			rx_time_secs = 0;
static double			rx_time_fract_sec = 0.0;
static time_t			tx_time_secs = 0;
static double			tx_time_fract_sec = 0.0;
static int			tx_timestamps;

int uhd_open(size_t channel, const char *_device_args, const char *_stream_args, const char *_tune_args, const char *tx_antenna, const char *rx_antenna, const char *clock_source, double tx_frequency, double rx_frequency, double lo_offset, double rate, double tx_gain, double rx_gain, double bandwidth, int timestamps)
{
	uhd_error error;
	double got_frequency, got_rate, got_gain, got_bandwidth;
	char got_antenna[64], got_clock[64];

	samplerate = rate;
	tx_timestamps = timestamps;

	LOGP(DUHD, LOGL_INFO, "Using device args \"%s\"\n", _device_args);
	LOGP(DUHD, LOGL_INFO, "Using stream args \"%s\"\n", _stream_args);
	LOGP(DUHD, LOGL_INFO, "Using tune args \"%s\"\n", _tune_args);

	/* create USRP */
	LOGP(DUHD, LOGL_INFO, "Creating USRP with args \"%s\"...\n", _device_args);
	error = uhd_usrp_make(&usrp, _device_args);
	if (error) {
		LOGP(DUHD, LOGL_ERROR, "Failed to create USRP\n");
		uhd_close();
		return -EIO;
	}

	/* clock source */
	if (clock_source && clock_source[0]) {
		if (!strcasecmp(clock_source, "list")) {
			uhd_string_vector_handle clocks;
			size_t clocks_length;
			int i;
			error = uhd_string_vector_make(&clocks);
			if (error) {
				clock_vector_error:
				LOGP(DUHD, LOGL_ERROR, "Failed to handle UHD vector, please fix!\n");
				uhd_close();
				return -EIO;
			}
			error = uhd_usrp_get_clock_sources(usrp, 0, &clocks);
			if (error) {
				LOGP(DUHD, LOGL_ERROR, "Failed to request list of clock sources!\n");
				uhd_close();
				return -EIO;
			}
			error = uhd_string_vector_size(clocks, &clocks_length);
			if (error)
				goto clock_vector_error;
			for (i = 0; i < (int)clocks_length; i++) {
				error = uhd_string_vector_at(clocks, i, got_clock, sizeof(got_clock));
				if (error)
					goto clock_vector_error;
				LOGP(DUHD, LOGL_NOTICE, "Clock source: '%s'\n", got_clock);
			}
			uhd_string_vector_free(&clocks);
			error = uhd_usrp_get_clock_source(usrp, 0, got_clock, sizeof(got_clock));
			if (error) {
				LOGP(DUHD, LOGL_ERROR, "Failed to get clock source\n");
				uhd_close();
				return -EINVAL;
			}
			LOGP(DUHD, LOGL_NOTICE, "Default clock source: '%s'\n", got_clock);
			uhd_close();
			return 1;
		}
		error = uhd_usrp_set_clock_source(usrp, clock_source, 0);
		if (error) {
			LOGP(DUHD, LOGL_ERROR, "Failed to set clock source to '%s'\n", clock_source);
			uhd_close();
			return -EIO;
		}
		error = uhd_usrp_get_clock_source(usrp, 0, got_clock, sizeof(got_clock));
		if (error) {
			LOGP(DUHD, LOGL_ERROR, "Failed to get clock source\n");
			uhd_close();
			return -EINVAL;
		}
		if (!!strcasecmp(clock_source, got_clock)) {
			LOGP(DUHD, LOGL_NOTICE, "Given clock source '%s' was accepted, but driver claims to use '%s'\n", clock_source, got_clock);
			uhd_close();
			return -EINVAL;
		}
	}

	if (tx_frequency) {
		/* antenna */
		if (tx_antenna && tx_antenna[0]) {
			if (!strcasecmp(tx_antenna, "list")) {
				uhd_string_vector_handle antennas;
				size_t antennas_length;
				int i;
				error = uhd_string_vector_make(&antennas);
				if (error) {
					tx_vector_error:
					LOGP(DUHD, LOGL_ERROR, "Failed to handle UHD vector, please fix!\n");
					uhd_close();
					return -EIO;
				}
				error = uhd_usrp_get_tx_antennas(usrp, channel, &antennas);
				if (error) {
					LOGP(DUHD, LOGL_ERROR, "Failed to request list of TX antennas!\n");
					uhd_close();
					return -EIO;
				}
				error = uhd_string_vector_size(antennas, &antennas_length);
				if (error)
					goto tx_vector_error;
				for (i = 0; i < (int)antennas_length; i++) {
					error = uhd_string_vector_at(antennas, i, got_antenna, sizeof(got_antenna));
					if (error)
						goto tx_vector_error;
					LOGP(DUHD, LOGL_NOTICE, "TX Antenna: '%s'\n", got_antenna);
				}
				uhd_string_vector_free(&antennas);
				error = uhd_usrp_get_tx_antenna(usrp, channel, got_antenna, sizeof(got_antenna));
				if (error) {
					LOGP(DUHD, LOGL_ERROR, "Failed to get TX antenna\n");
					uhd_close();
					return -EINVAL;
				}
				LOGP(DUHD, LOGL_NOTICE, "Default TX Antenna: '%s'\n", got_antenna);
				uhd_close();
				return 1;
			}
			error = uhd_usrp_set_tx_antenna(usrp, tx_antenna, channel);
			if (error) {
				LOGP(DUHD, LOGL_ERROR, "Failed to set TX antenna to '%s'\n", tx_antenna);
				uhd_close();
				return -EIO;
			}
			error = uhd_usrp_get_tx_antenna(usrp, channel, got_antenna, sizeof(got_antenna));
			if (error) {
				LOGP(DUHD, LOGL_ERROR, "Failed to get TX antenna\n");
				uhd_close();
				return -EINVAL;
			}
			if (!!strcasecmp(tx_antenna, got_antenna)) {
				LOGP(DUHD, LOGL_NOTICE, "Given TX antenna '%s' was accepted, but driver claims to use '%s'\n", tx_antenna, got_antenna);
				uhd_close();
				return -EINVAL;
			}
		}

		/* create streamers */
		error = uhd_tx_streamer_make(&tx_streamer);
		if (error) {
			LOGP(DUHD, LOGL_ERROR, "Failed to create TX streamer\n");
			uhd_close();
			return -EIO;
		}

		/* set rate */
		error = uhd_usrp_set_tx_rate(usrp, rate, channel);
		if (error) {
			LOGP(DUHD, LOGL_ERROR, "Failed to set TX rate to %.0f Hz\n", rate);
			uhd_close();
			return -EIO;
		}

		/* see what rate actually is */
		error = uhd_usrp_get_tx_rate(usrp, channel, &got_rate);
		if (error) {
			LOGP(DUHD, LOGL_ERROR, "Failed to get TX rate\n");
			uhd_close();
			return -EIO;
		}
		if (fabs(got_rate - rate) > 1.0) {
			LOGP(DUHD, LOGL_ERROR, "Given TX rate %.0f Hz is not supported, try %.0f Hz\n", rate, got_rate);
			uhd_close();
			return -EINVAL;
		}

		/* set gain */
		error = uhd_usrp_set_tx_gain(usrp, tx_gain, channel, "");
		if (error) {
			LOGP(DUHD, LOGL_ERROR, "Failed to set TX gain to %.0f\n", tx_gain);
			uhd_close();
			return -EIO;
		}

		/* see what gain actually is */
		error = uhd_usrp_get_tx_gain(usrp, channel, "", &got_gain);
		if (error) {
			LOGP(DUHD, LOGL_ERROR, "Failed to get TX gain\n");
			uhd_close();
			return -EIO;
		}
		if (fabs(got_gain - tx_gain) > 0.001) {
			LOGP(DUHD, LOGL_NOTICE, "Given TX gain %.0f is not supported, we use %.0f\n", tx_gain, got_gain);
			tx_gain = got_gain;
		}

		/* set frequency */
		memset(&tune_request, 0, sizeof(tune_request));
		tune_request.target_freq = tx_frequency;
		if (lo_offset) {
			tune_request.rf_freq_policy = UHD_TUNE_REQUEST_POLICY_MANUAL;
			tune_request.rf_freq = tx_frequency + lo_offset;
		} else
			tune_request.rf_freq_policy = UHD_TUNE_REQUEST_POLICY_AUTO;
		tune_request.dsp_freq_policy = UHD_TUNE_REQUEST_POLICY_AUTO;
		tune_request.args = options_strdup(_tune_args);
		error = uhd_usrp_set_tx_freq(usrp, &tune_request, channel, &tune_result);
		if (error) {
			LOGP(DUHD, LOGL_ERROR, "Failed to set TX frequency to %.0f Hz\n", tx_frequency);
			uhd_close();
			return -EIO;
		}

		/* see what frequency actually is */
		error = uhd_usrp_get_tx_freq(usrp, channel, &got_frequency);
		if (error) {
			LOGP(DUHD, LOGL_ERROR, "Failed to get TX frequency\n");
			uhd_close();
			return -EIO;
		}
		if (fabs(got_frequency - tx_frequency) > 100.0) {
			LOGP(DUHD, LOGL_ERROR, "Given TX frequency %.0f Hz is not supported, try %.0f Hz\n", tx_frequency, got_frequency);
			uhd_close();
			return -EINVAL;
		}

		/* set bandwidth */
		if (uhd_usrp_set_tx_bandwidth(usrp, bandwidth, channel) != 0) {
			LOGP(DUHD, LOGL_ERROR, "Failed to set TX bandwidth to %.0f Hz\n", bandwidth);
			uhd_close();
			return -EIO;
		}

		/* see what bandwidth actually is */
		error = uhd_usrp_get_tx_bandwidth(usrp, channel, &got_bandwidth);
		if (error) {
			LOGP(DUHD, LOGL_ERROR, "Failed to get TX bandwidth\n");
			uhd_close();
			return -EIO;
		}
		if (fabs(got_bandwidth - bandwidth) > 100.0) {
			LOGP(DUHD, LOGL_ERROR, "Given TX bandwidth %.0f Hz is not supported, try %.0f Hz\n", bandwidth, got_bandwidth);
			uhd_close();
			return -EINVAL;
		}

		/* set up streamer */
		memset(&stream_args, 0, sizeof(stream_args));
		stream_args.cpu_format = "fc32";
		stream_args.otw_format = "sc16";
		stream_args.args = options_strdup(_stream_args);
		stream_args.channel_list = &channel;
		stream_args.n_channels = 1;
		error = uhd_usrp_get_tx_stream(usrp, &stream_args, tx_streamer);
		if (error) {
			LOGP(DUHD, LOGL_ERROR, "Failed to set TX streamer args\n");
			uhd_close();
			return -EIO;
		}

		/* get buffer sizes */
		error = uhd_tx_streamer_max_num_samps(tx_streamer, &tx_samps_per_buff);
		if (error) {
			LOGP(DUHD, LOGL_ERROR, "Failed to get TX streamer sample buffer\n");
			uhd_close();
			return -EIO;
		}
	}

	if (rx_frequency) {
		/* antenna */
		if (rx_antenna && rx_antenna[0]) {
			if (!strcasecmp(rx_antenna, "list")) {
				uhd_string_vector_handle antennas;
				size_t antennas_length;
				int i;
				error = uhd_string_vector_make(&antennas);
				if (error) {
					rx_vector_error:
					LOGP(DUHD, LOGL_ERROR, "Failed to handle UHD vector, please fix!\n");
					uhd_close();
					return -EIO;
				}
				error = uhd_usrp_get_rx_antennas(usrp, channel, &antennas);
				if (error) {
					LOGP(DUHD, LOGL_ERROR, "Failed to request list of RX antennas!\n");
					uhd_close();
					return -EIO;
				}
				error = uhd_string_vector_size(antennas, &antennas_length);
				if (error)
					goto rx_vector_error;
				for (i = 0; i < (int)antennas_length; i++) {
					error = uhd_string_vector_at(antennas, i, got_antenna, sizeof(got_antenna));
					if (error)
						goto rx_vector_error;
					LOGP(DUHD, LOGL_NOTICE, "RX Antenna: '%s'\n", got_antenna);
				}
				uhd_string_vector_free(&antennas);
				error = uhd_usrp_get_rx_antenna(usrp, channel, got_antenna, sizeof(got_antenna));
				if (error) {
					LOGP(DUHD, LOGL_ERROR, "Failed to get RX antenna\n");
					uhd_close();
					return -EINVAL;
				}
				LOGP(DUHD, LOGL_NOTICE, "Default RX Antenna: '%s'\n", got_antenna);
				uhd_close();
				return 1;
			}
			error = uhd_usrp_set_rx_antenna(usrp, rx_antenna, channel);
			if (error) {
				LOGP(DUHD, LOGL_ERROR, "Failed to set RX antenna to '%s'\n", rx_antenna);
				uhd_close();
				return -EIO;
			}
			error = uhd_usrp_get_rx_antenna(usrp, channel, got_antenna, sizeof(got_antenna));
			if (error) {
				LOGP(DUHD, LOGL_ERROR, "Failed to get RX antenna\n");
				uhd_close();
				return -EINVAL;
			}
			if (!!strcasecmp(rx_antenna, got_antenna)) {
				LOGP(DUHD, LOGL_NOTICE, "Given RX antenna '%s' was accepted, but driver claims to use '%s'\n", rx_antenna, got_antenna);
				uhd_close();
				return -EINVAL;
			}
		}
		/* create streamers */
		error = uhd_rx_streamer_make(&rx_streamer);
		if (error) {
			LOGP(DUHD, LOGL_ERROR, "Failed to create RX streamer\n");
			uhd_close();
			return -EIO;
		}

		/* create metadata */
		error = uhd_rx_metadata_make(&rx_metadata);
		if (error) {
			LOGP(DUHD, LOGL_ERROR, "Failed to create RX metadata\n");
			uhd_close();
			return -EIO;
		}

		/* set rate */
		error = uhd_usrp_set_rx_rate(usrp, rate, channel);
		if (error) {
			LOGP(DUHD, LOGL_ERROR, "Failed to set RX rate to %.0f Hz\n", rate);
			uhd_close();
			return -EIO;
		}

		/* see what rate actually is */
		error = uhd_usrp_get_rx_rate(usrp, channel, &got_rate);
		if (error) {
			LOGP(DUHD, LOGL_ERROR, "Failed to get RX rate\n");
			uhd_close();
			return -EIO;
		}
		if (fabs(got_rate - rate) > 1.0) {
			LOGP(DUHD, LOGL_ERROR, "Given RX rate %.0f Hz is not supported, try %.0f Hz\n", rate, got_rate);
			uhd_close();
			return -EINVAL;
		}

		/* set gain */
		error = uhd_usrp_set_rx_gain(usrp, rx_gain, channel, "");
		if (error) {
			LOGP(DUHD, LOGL_ERROR, "Failed to set RX gain to %.0f\n", rx_gain);
			uhd_close();
			return -EIO;
		}

		/* see what gain actually is */
		error = uhd_usrp_get_rx_gain(usrp, channel, "", &got_gain);
		if (error) {
			LOGP(DUHD, LOGL_ERROR, "Failed to get RX gain\n");
			uhd_close();
			return -EIO;
		}
		if (fabs(got_gain - rx_gain) > 0.001) {
			LOGP(DUHD, LOGL_NOTICE, "Given RX gain %.3f is not supported, we use %.3f\n", rx_gain, got_gain);
			rx_gain = got_gain;
		}

		/* set frequency */
		memset(&tune_request, 0, sizeof(tune_request));
		tune_request.target_freq = rx_frequency;
		if (lo_offset) {
			tune_request.rf_freq_policy = UHD_TUNE_REQUEST_POLICY_MANUAL;
			tune_request.rf_freq = rx_frequency + lo_offset;
		} else
			tune_request.rf_freq_policy = UHD_TUNE_REQUEST_POLICY_AUTO;
		tune_request.dsp_freq_policy = UHD_TUNE_REQUEST_POLICY_AUTO;
		tune_request.args = options_strdup(_tune_args);
		error = uhd_usrp_set_rx_freq(usrp, &tune_request, channel, &tune_result);
		if (error) {
			LOGP(DUHD, LOGL_ERROR, "Failed to set RX frequency to %.0f Hz\n", rx_frequency);
			uhd_close();
			return -EIO;
		}

		/* see what frequency actually is */
		error = uhd_usrp_get_rx_freq(usrp, channel, &got_frequency);
		if (error) {
			LOGP(DUHD, LOGL_ERROR, "Failed to get RX frequency\n");
			uhd_close();
			return -EIO;
		}
		if (fabs(got_frequency - rx_frequency) > 100.0) {
			LOGP(DUHD, LOGL_ERROR, "Given RX frequency %.0f Hz is not supported, try %.0f Hz\n", rx_frequency, got_frequency);
			uhd_close();
			return -EINVAL;
		}

		/* set bandwidth */
		if (uhd_usrp_set_rx_bandwidth(usrp, bandwidth, channel) != 0) {
			LOGP(DUHD, LOGL_ERROR, "Failed to set RX bandwidth to %.0f Hz\n", bandwidth);
			uhd_close();
			return -EIO;
		}

		/* see what bandwidth actually is */
		error = uhd_usrp_get_rx_bandwidth(usrp, channel, &got_bandwidth);
		if (error) {
			LOGP(DUHD, LOGL_ERROR, "Failed to get RX bandwidth\n");
			uhd_close();
			return -EIO;
		}
		if (fabs(got_bandwidth - bandwidth) > 100.0) {
			LOGP(DUHD, LOGL_ERROR, "Given RX bandwidth %.0f Hz is not supported, try %.0f Hz\n", bandwidth, got_bandwidth);
			uhd_close();
			return -EINVAL;
		}

		/* set up streamer */
		memset(&stream_args, 0, sizeof(stream_args));
		stream_args.cpu_format = "fc32";
		stream_args.otw_format = "sc16";
		stream_args.args = options_strdup(_stream_args);
		stream_args.channel_list = &channel;
		stream_args.n_channels = 1;
		error = uhd_usrp_get_rx_stream(usrp, &stream_args, rx_streamer);
		if (error) {
			LOGP(DUHD, LOGL_ERROR, "Failed to set RX streamer args\n");
			uhd_close();
			return -EIO;
		}

		/* get buffer sizes */
		error = uhd_rx_streamer_max_num_samps(rx_streamer, &rx_samps_per_buff);
		if (error) {
			LOGP(DUHD, LOGL_ERROR, "Failed to get RX streamer sample buffer\n");
			uhd_close();
			return -EIO;
		}
	}

	return 0;
}

/* start streaming */
int uhd_start(void)
{
	uhd_error error;

	/* enable rx stream */
	memset(&stream_cmd, 0, sizeof(stream_cmd));
	stream_cmd.stream_mode = UHD_STREAM_MODE_START_CONTINUOUS;
	stream_cmd.stream_now = true;
	error = uhd_rx_streamer_issue_stream_cmd(rx_streamer, &stream_cmd);
	if (error) {
		LOGP(DUHD, LOGL_ERROR, "Failed to issue RX stream command\n");
		return -EIO;
	}
	return 0;
}

void uhd_close(void)
{
	LOGP(DUHD, LOGL_DEBUG, "Clean up UHD\n");
	if (tx_metadata)
        	uhd_tx_metadata_free(&tx_metadata);
	if (rx_metadata)
        	uhd_rx_metadata_free(&rx_metadata);
	if (tx_streamer)
	        uhd_tx_streamer_free(&tx_streamer);
	if (rx_streamer)
	        uhd_rx_streamer_free(&rx_streamer);
	if (usrp)
	        uhd_usrp_free(&usrp);
}

int uhd_send(float *buff, int num)
{
    	const void *buffs_ptr[1];
	int chunk;
	size_t sent = 0, count;
	uhd_error error;

	while (num) {
		chunk = num;
		if (chunk > (int)tx_samps_per_buff)
			chunk = (int)tx_samps_per_buff;
		/* create tx metadata */
		if (tx_timestamps)
			error = uhd_tx_metadata_make(&tx_metadata, true, tx_time_secs, tx_time_fract_sec, false, false);
		else
			error = uhd_tx_metadata_make(&tx_metadata, false, 0, 0.0, false, false);
		if (error)
			LOGP(DUHD, LOGL_ERROR, "Failed to create TX metadata\n");
		buffs_ptr[0] = buff;
		count = 0;
		error = uhd_tx_streamer_send(tx_streamer, buffs_ptr, chunk, &tx_metadata, 1.0, &count);
		if (error) {
			LOGP(DUHD, LOGL_ERROR, "Failed to write to TX streamer\n");
			break;
		}
		if (count == 0)
			break;

		/* increment time stamp */
		tx_time_fract_sec += (double)count / samplerate;
		if (tx_time_fract_sec >= 1.0) {
			tx_time_secs++;
			tx_time_fract_sec -= 1.0;
		}
//printf("adv=%.3f\n", ((double)tx_time_secs + tx_time_fract_sec) - ((double)rx_time_secs + rx_time_fract_sec));

		sent += count;
		buff += count * 2;
		num -= count;
	}

	return sent;
}

/* read what we got, return 0, if buffer is empty, otherwise return the number of samples */
int uhd_receive(float *buff, int max)
{
    	void *buffs_ptr[1];
	size_t got = 0, count;
	uhd_error error;
	bool has_time_spec;
	int rc;

	while (1) {
		if (max < (int)rx_samps_per_buff) {
			/* no more space this time */
			sdr_rx_overflow = 1;
			break;
		}
		/* read RX stream */
		buffs_ptr[0] = buff;
		count = 0;
		error = uhd_rx_streamer_recv(rx_streamer, buffs_ptr, rx_samps_per_buff, &rx_metadata, 0.0, false, &count);
		if (error) {
			LOGP(DUHD, LOGL_ERROR, "Failed to read from UHD device.\n");
			break;
		}
		if (count) {
			if (tx_timestamps) {
				/* get time stamp of received RX packet */
				rc = uhd_rx_metadata_has_time_spec(rx_metadata, &has_time_spec);
				if (rc == 0 && has_time_spec)
					rc = uhd_rx_metadata_time_spec(rx_metadata, &rx_time_secs, &rx_time_fract_sec);
				if (rc < 0 || !has_time_spec) {
					LOGP(DSOAPY, LOGL_ERROR, "SDR RX: No time stamps available. This may cuse little gaps and problems with time slot based networks, like C-Netz.\n");
					tx_timestamps = 0;
				}
			}
			if (!tx_timestamps) {
				/* increment time stamp */
				rx_time_fract_sec += (double)count / samplerate;
				if (rx_time_fract_sec >= 1.0) {
					rx_time_secs++;
					rx_time_fract_sec -= 1.0;
				}
			}
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
int uhd_get_tosend(int buffer_size)
{
	double advance;
	int tosend;

	/* we need the rx time stamp to determine how much data is already sent in advance */
	if (rx_time_secs == 0 && rx_time_fract_sec == 0.0)
		return 0;

	/* if we have not yet sent any data, we set initial tx time stamp */
	if (tx_time_secs == 0 && tx_time_fract_sec == 0.0) {
		tx_time_secs = rx_time_secs;
		tx_time_fract_sec = rx_time_fract_sec;
		if (tx_timestamps) {
			tx_time_fract_sec += (double)buffer_size / samplerate;
			if (tx_time_fract_sec >= 1.0) {
				tx_time_fract_sec -= 1.0;
				tx_time_secs++;
			}
		}
	}

	/* we check how advance our transmitted time stamp is */
	advance = ((double)tx_time_secs + tx_time_fract_sec) - ((double)rx_time_secs + rx_time_fract_sec);
	/* in case of underrun: */
	if (advance < 0) {
		LOGP(DSOAPY, LOGL_ERROR, "SDR TX underrun, seems we are too slow. Use lower SDR sample rate.\n");
		advance = 0;
	}
	tosend = buffer_size - (int)(advance * samplerate);
	if (tosend < 0)
		tosend = 0;

	return tosend;
}

