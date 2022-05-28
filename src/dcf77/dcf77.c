
/* implementation of DCF77 transmitter and receiver
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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <math.h>
#include "../libdebug/debug.h"
#include "dcf77.h"

double get_time(void);

#define CARRIER_FREQUENCY	77500
#define TEST_FREQUENCY		1000
#define CARRIER_BANDWIDTH	10.0
#define SAMPLE_CLOCK		1000
#define CLOCK_1S		1.0
#define CLOCK_BANDWIDTH		0.1
#define REDUCTION_FACTOR	0.15
#define REDUCTION_TH		0.575
#define TX_LEVEL		0.9

#define level2db(level)		(20 * log10(level))

static int fast_math = 0;
static float *sin_tab = NULL, *cos_tab = NULL;

const char *time_zone[4] = { "???", "CEST", "CET", "???" };
const char *week_day[8] = { "???", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
const char *month_name[13] = { "???", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

/* global init */
int dcf77_init(int _fast_math)
{
	fast_math = _fast_math;

	if (fast_math) {
		int i;

		sin_tab = calloc(65536+16384, sizeof(*sin_tab));
		if (!sin_tab) {
			fprintf(stderr, "No mem!\n");
			return -ENOMEM;
		}
		cos_tab = sin_tab + 16384;

		/* generate sine and cosine */
		for (i = 0; i < 65536+16384; i++)
			sin_tab[i] = sin(2.0 * M_PI * (double)i / 65536.0);
	}

	return 0;
}

/* global exit */
void dcf77_exit(void)
{
	if (sin_tab) {
		free(sin_tab);
		sin_tab = cos_tab = NULL;
	}
}

dcf77_t *dcf77_create(int samplerate, int use_tx, int use_rx, int test_tone)
{
	dcf77_t *dcf77 = NULL;
	dcf77_tx_t *tx;
	dcf77_rx_t *rx;

	dcf77 = calloc(1, sizeof(*dcf77));
	if (!dcf77) {
		PDEBUG(DDCF77, DEBUG_ERROR, "No mem!\n");
		return NULL;
	}
	tx = &dcf77->tx;
	rx = &dcf77->rx;

	/* measurement */
	display_wave_init(&dcf77->dispwav, (double)samplerate, "DCF77");
	display_measurements_init(&dcf77->dispmeas, samplerate, "DCF77");

	/* prepare tx */
	if (use_tx) {
		tx->enable = 1;
		if (fast_math)
			tx->phase_360 = 65536.0;
		else
			tx->phase_360 = 2.0 * M_PI;

		/* carrier generation */
		tx->carrier_phase_step = tx->phase_360 * (double)CARRIER_FREQUENCY / ((double)samplerate);
		tx->test_phase_step = tx->phase_360 * (double)TEST_FREQUENCY / ((double)samplerate);
		tx->waves_0 = CARRIER_FREQUENCY / 10;
		tx->waves_1 = CARRIER_FREQUENCY / 5;
		tx->waves_sec = CARRIER_FREQUENCY;

		tx->test_tone = test_tone;
	}

	/* prepare rx */
	if (use_rx) {
		rx->enable = 1;
		if (fast_math)
			rx->phase_360 = 65536.0;
		else
			rx->phase_360 = 2.0 * M_PI;

		/* carrier filter */
		rx->carrier_phase_step = rx->phase_360 * (double)CARRIER_FREQUENCY / ((double)samplerate);
		/* use fourth order (2 iter) filter, since it is as fast as second order (1 iter) filter */
		iir_lowpass_init(&rx->carrier_lp[0], CARRIER_BANDWIDTH, (double)samplerate, 2);
		iir_lowpass_init(&rx->carrier_lp[1], CARRIER_BANDWIDTH, (double)samplerate, 2);

		/* signal rate */
		rx->sample_step = (double)SAMPLE_CLOCK / (double)samplerate;

		/* delay buffer */
		rx->delay_size = ceil((double)SAMPLE_CLOCK * 0.1);
		rx->delay_buffer = calloc(rx->delay_size, sizeof(*rx->delay_buffer));
		if (!rx->delay_buffer) {
			PDEBUG(DDCF77, DEBUG_ERROR, "No mem!\n");
			return NULL;
		}

		/* count clock signal */
		rx->clock_count = -1;

		/* measurement parameters */
		dcf77->dmp_input_level = display_measurements_add(&dcf77->dispmeas, "Input Level", "%.0f dB", DISPLAY_MEAS_AVG, DISPLAY_MEAS_LEFT, -100.0, 0.0, -INFINITY);
		dcf77->dmp_signal_level = display_measurements_add(&dcf77->dispmeas, "Signal Level", "%.0f dB", DISPLAY_MEAS_AVG, DISPLAY_MEAS_LEFT, -100.0, 0.0, -INFINITY);
		dcf77->dmp_signal_quality = display_measurements_add(&dcf77->dispmeas, "Signal Qualtiy", "%.0f %%", DISPLAY_MEAS_LAST, DISPLAY_MEAS_LEFT, 0.0, 100.0, -INFINITY);
	}

	if (tx->enable)
		PDEBUG(DDCF77, DEBUG_INFO, "DCF77 transmitter has been created.\n");
	if (rx->enable)
		PDEBUG(DDCF77, DEBUG_INFO, "DCF77 receiver has been created.\n");

	return dcf77;
}

void dcf77_destroy(dcf77_t *dcf77)
{
	if (dcf77) {
		dcf77_rx_t *rx = &dcf77->rx;
		free(rx->delay_buffer);
		free(dcf77);
	}

	PDEBUG(DDCF77, DEBUG_INFO, "DCF77 has been destroyed.\n");
}

/* set inital time stamp at the moment the stream starts */
void dcf77_tx_start(dcf77_t *dcf77, time_t timestamp)
{
	dcf77_tx_t *tx = &dcf77->tx;
	double now;
	time_t t;

	/* get time stamp */
	if (timestamp < 0)
		now = get_time();
	else
		now = timestamp;
	t = floor(now);
	/* current second within minute */
	tx->second = t % 60;
	/* time stamp of next minute */
	tx->timestamp = t - tx->second + 60;
	/* wave within current second */
	tx->wave = floor(fmod(now, 1.0) * (double)tx->waves_sec);
	/* silence until next second begins */
	tx->symbol = 'm'; tx->level = 0;
}

static char tx_symbol(dcf77_t *dcf77, time_t timestamp, int second)
{
	dcf77_tx_t *tx = &dcf77->tx;
	char symbol;

	/* generate frame */
	if (second == 0 || !tx->data_frame) {
		struct tm *tm;
		int isdst_next_hour, wday, zone;
		uint64_t frame = 0, p;

		timestamp += 3600;
		tm = localtime(&timestamp);
		timestamp -= 3600;
		if (!tm) {
error_tm:
			PDEBUG(DDCF77, DEBUG_ERROR, "Failed to get local time of time stamp!\n");
			return 'm';
		}
		isdst_next_hour = tm->tm_isdst;
		tm = localtime(&timestamp);
		if (!tm)
			goto error_tm;

		if (tm->tm_wday > 0)
			wday = tm->tm_wday;
		else
			wday = 7;

		if (tm->tm_isdst > 0)
			zone = 1;
		else
			zone = 2;

		PDEBUG(DDCF77, DEBUG_NOTICE, "The time transmitting: %s %s %d %02d:%02d:00 %s %02d\n", week_day[wday], month_name[tm->tm_mon + 1], tm->tm_mday, tm->tm_hour, tm->tm_min, time_zone[zone], tm->tm_year + 1900);

		if ((tm->tm_isdst > 0) != (isdst_next_hour > 0))
			frame |= (uint64_t)1 << 16;
		if (tm->tm_isdst > 0)
			frame |= (uint64_t)1 << 17;
		else
			frame |= (uint64_t)2 << 17;
		frame |= 1 << 20;

		frame |= (uint64_t)(tm->tm_min % 10) << 21;
		frame |= (uint64_t)(tm->tm_min / 10) << 25;
		p = (frame >> 21) & 0x7f;
		p = p ^ (p >> 4);
		p = p ^ (p >> 2);
		p = p ^ (p >> 1);
		frame |= (uint64_t)(p & 1) << 28;

		frame |= (uint64_t)(tm->tm_hour % 10) << 29;
		frame |= (uint64_t)(tm->tm_hour / 10) << 33;
		p = (frame >> 29) & 0x3f;
		p = p ^ (p >> 4);
		p = p ^ (p >> 2);
		p = p ^ (p >> 1);
		frame |= (uint64_t)(p & 1) << 35;

		frame |= (uint64_t)(tm->tm_mday % 10) << 36;
		frame |= (uint64_t)(tm->tm_mday / 10) << 40;
		frame |= (uint64_t)(wday) << 42;
		frame |= (uint64_t)((tm->tm_mon + 1) % 10) << 45;
		frame |= (uint64_t)((tm->tm_mon + 1) / 10) << 49;
		frame |= (uint64_t)(tm->tm_year % 10) << 50;
		frame |= (uint64_t)((tm->tm_year / 10) % 10) << 54;
		p = (frame >> 36) & 0x3fffff;
		p = p ^ (p >> 16);
		p = p ^ (p >> 8);
		p = p ^ (p >> 4);
		p = p ^ (p >> 2);
		p = p ^ (p >> 1);
		frame |= (uint64_t)(p & 1) << 58;

		tx->data_frame = frame;
	}

	if (second == 59)
		symbol = 'm';
	else symbol = ((tx->data_frame >> second) & 1) + '0';

	PDEBUG(DDSP, DEBUG_DEBUG, "Trasmitting symbol '%c' (Bit %d)\n", symbol, second);

	return symbol;
}

void dcf77_encode(dcf77_t *dcf77, sample_t *samples, int length)
{
	dcf77_tx_t *tx = &dcf77->tx;
	double carrier_phase, test_phase;
	int i;

	if (!tx->enable) {
		memset(samples, 0, sizeof(*samples) * length);
		return;
	}

	carrier_phase = tx->carrier_phase;
	test_phase = tx->test_phase;
	for (i = 0; i < length; i++) {
		if (fast_math)
			samples[i] = sin_tab[(uint16_t)carrier_phase] * tx->level;
		else
			samples[i] = sin(carrier_phase) * tx->level;
		carrier_phase += tx->carrier_phase_step;
		if (carrier_phase >= tx->phase_360) {
			carrier_phase -= tx->phase_360;
			tx->wave++;
			if (tx->wave >= tx->waves_sec) {
				tx->wave -= tx->waves_sec;
				if (++tx->second == 60) {
					tx->second = 0;
					tx->timestamp += 60;
				}
				tx->symbol = tx_symbol(dcf77, tx->timestamp, tx->second);
			}
			switch (tx->symbol) {
			case '0':
				if (tx->wave < tx->waves_0)
					tx->level = TX_LEVEL * REDUCTION_FACTOR;
				else
					tx->level = TX_LEVEL;
				break;
			case '1':
				if (tx->wave < tx->waves_1)
					tx->level = TX_LEVEL * REDUCTION_FACTOR;
				else
					tx->level = TX_LEVEL;
				break;
			case 'm':
				tx->level = TX_LEVEL;
				break;
			}
			if (tx->test_tone)
				tx->level *= 0.9; /* 90 % */
		}
		if (tx->test_tone) {
			if (fast_math)
				samples[i] += sin_tab[(uint16_t)test_phase] * tx->level / 10.0; /* 10 % */
			else
				samples[i] += sin(test_phase) * tx->level / 10.0; /* 10 % */
			if (test_phase >= tx->phase_360)
				test_phase -= tx->phase_360;
			test_phase += tx->test_phase_step;
		}
	}
	tx->carrier_phase = carrier_phase;
	tx->test_phase = test_phase;
}

static void rx_frame(uint64_t frame)
{
	int zone;
	int minute_one, minute_ten, minute = -1;
	int hour_one, hour_ten, hour = -1;
	int day_one, day_ten, day = -1;
	int wday = -1;
	int month_one, month_ten, month = -1;
	int year_one, year_ten, year = -1;
	uint64_t p;

	PDEBUG(DFRAME, DEBUG_INFO, "Bit 0 is '0'?    : %s\n", ((frame >> 0) & 1) ? "no" : "yes");
	PDEBUG(DFRAME, DEBUG_INFO, "Bits 1..14       : 0x%04x\n", (int)(frame >> 1) & 0x3fff);
	PDEBUG(DFRAME, DEBUG_INFO, "Call Bit         : %d\n", (int)(frame >> 15) & 1);
	PDEBUG(DFRAME, DEBUG_INFO, "Change Time Zone : %s\n", ((frame >> 16) & 1) ? "yes" : "no");
	zone = ((frame >> 17) & 3);
	PDEBUG(DFRAME, DEBUG_INFO, "Time Zone        : %s\n", time_zone[zone]);
	PDEBUG(DFRAME, DEBUG_INFO, "Add Leap Second  : %s\n", ((frame >> 19) & 1) ? "yes" : "no");
	PDEBUG(DFRAME, DEBUG_INFO, "Bit 20 is '1'?   : %s\n", ((frame >> 20) & 1) ? "yes" : "no");

	minute_one = (frame >> 21 & 0xf);
	minute_ten = ((frame >> 25) & 0x7);
	p = (frame >> 21) & 0xff;
	p = p ^ (p >> 4);
	p = p ^ (p >> 2);
	p = p ^ (p >> 1);
	if (minute_one > 9 || minute_ten > 5 || (p & 1))
		PDEBUG(DFRAME, DEBUG_INFO, "Minute           : ???\n");
	else {
		minute = minute_ten * 10 + minute_one;
		PDEBUG(DFRAME, DEBUG_INFO, "Minute           : %02d\n", minute);
	}

	hour_one = (frame >> 29 & 0xf);
	hour_ten = ((frame >> 33) & 0x3);
	p = (frame >> 29) & 0x7f;
	p = p ^ (p >> 4);
	p = p ^ (p >> 2);
	p = p ^ (p >> 1);
	if (hour_one > 9 || hour_ten > 2 || (hour_ten == 2 && hour_one > 3) || (p & 1))
		PDEBUG(DFRAME, DEBUG_INFO, "Hour             : ???\n");
	else {
		hour = hour_ten * 10 + hour_one;
		PDEBUG(DFRAME, DEBUG_INFO, "Hour             : %02d\n", hour);
	}

	day_one = (frame >> 36 & 0xf);
	day_ten = ((frame >> 40) & 0x3);
	wday = (frame >> 42 & 0x7);
	month_one = (frame >> 45 & 0xf);
	month_ten = ((frame >> 49) & 0x1);
	year_one = (frame >> 50 & 0xf);
	year_ten = ((frame >> 54) & 0xf);
	p = (frame >> 36) & 0x7fffff;
	p = p ^ (p >> 16);
	p = p ^ (p >> 8);
	p = p ^ (p >> 4);
	p = p ^ (p >> 2);
	p = p ^ (p >> 1);
	if (day_one > 9 || day_ten > 3 || (day_ten == 3 && day_one > 1) || (day_ten == 0 && day_one == 0) || (p & 1))
		PDEBUG(DFRAME, DEBUG_INFO, "Day              : ???\n");
	else {
		day = day_ten * 10 + day_one;
		PDEBUG(DFRAME, DEBUG_INFO, "Day              : %d\n", day);
	}
	if (wday < 1 || wday > 7 || (p & 1)) {
		PDEBUG(DFRAME, DEBUG_INFO, "Week Day         : ???\n");
		wday = -1;
	} else
		PDEBUG(DFRAME, DEBUG_INFO, "Week Day         : %s\n", week_day[wday]);
	if (month_one > 9 || month_ten > 1 || (month_ten == 1 && month_one > 2) || (month_ten == 0 && month_one == 0) || (p & 1))
		PDEBUG(DFRAME, DEBUG_INFO, "Month            : ???\n");
	else {
		month = month_ten * 10 + month_one;
		PDEBUG(DFRAME, DEBUG_INFO, "Month            : %d\n", month);
	}
	if (year_one > 9 || year_ten > 9 || (p & 1))
		PDEBUG(DFRAME, DEBUG_INFO, "Year             : ???\n");
	else {
		year = year_ten * 10 + year_one;
		PDEBUG(DFRAME, DEBUG_INFO, "Year             : %02d\n", year);
	}

	if (minute >= 0 && hour >= 0 && day >= 0 && wday >= 0 && month >= 0 && year >= 0)
		PDEBUG(DDCF77, DEBUG_NOTICE, "The received time is: %s %s %d %02d:%02d:00 %s 20%02d\n", week_day[wday], month_name[month], day, hour, minute, time_zone[zone], year);
	else
		PDEBUG(DDCF77, DEBUG_NOTICE, "The received time is invalid!\n");
}

static void rx_symbol(dcf77_t *dcf77, char symbol)
{
	dcf77_rx_t *rx = &dcf77->rx;

	PDEBUG(DDSP, DEBUG_DEBUG, "Received symbol '%c'\n", symbol);

	if (!rx->data_receive) {
		if (symbol == 'm') {
			PDEBUG(DDSP, DEBUG_INFO, "Reception of frame has started\n");
			rx->data_receive = 1;
			rx->data_index = 0;
		}
	} else {
		if (symbol == 'm') {
			if (rx->data_index == 59) {
				rx->data_string[rx->data_index] = '\0';
				rx->data_index = 0;
				PDEBUG(DDSP, DEBUG_INFO, "Received complete frame: %s (0x%016" PRIx64 ")\n", rx->data_string, rx->data_frame);
				rx_frame(rx->data_frame);
			} else {
				PDEBUG(DDSP, DEBUG_INFO, "Short read, frame too short\n");
				rx->data_index = 0;
			}
		} else {
			if (rx->data_index == 59) {
				PDEBUG(DDSP, DEBUG_INFO, "Long read, frame too long\n");
				rx->data_receive = 0;
			} else {
				rx->data_string[rx->data_index++] = symbol;
				rx->data_frame >>= 1;
				rx->data_frame |= (uint64_t)(symbol & 1) << 58;
			}
		}
	}
}

//#define DEBUG_SAMPLE

void dcf77_decode(dcf77_t *dcf77, sample_t *samples, int length)
{
	dcf77_rx_t *rx = &dcf77->rx;
	sample_t I[length], Q[length];
	double phase, level, delayed_level, reduction, quality;
	int i;

	display_wave(&dcf77->dispwav, samples, length, 1.0);

	if (!rx->enable)
		return;

	/* rotate spectrum */
	phase = rx->carrier_phase;
	for (i = 0; i < length; i++) {
		/* mix with carrier frequency */
		if (fast_math) {
			I[i] = cos_tab[(uint16_t)phase] * samples[i];
			Q[i] = sin_tab[(uint16_t)phase] * samples[i];
		} else {
			I[i] = cos(phase) * samples[i];
			Q[i] = sin(phase) * samples[i];
		}
		phase += rx->carrier_phase_step;
		if (phase >= rx->phase_360)
			phase -= rx->phase_360;
	}
	rx->carrier_phase = phase;

	level = sqrt(I[0] * I[0] + Q[0] * Q[0]);
	if (level > 0.0) // don't average with level of 0.0 (-inf dB)
		display_measurements_update(dcf77->dmp_input_level, level2db(level), 0.0);

	/* filter carrier */
	iir_process(&rx->carrier_lp[0], I, length);
	iir_process(&rx->carrier_lp[1], Q, length);

	for (i = 0; i < length; i++) {
		rx->sample_counter += rx->sample_step;
		if (rx->sample_counter >= 1.0) {
			rx->sample_counter -= 1.0;
			/* level */
			level = sqrt(I[i] * I[i] + Q[i] * Q[i]);
			if (level > 0.0) // don't average with level of 0.0 (-inf dB)
				display_measurements_update(dcf77->dmp_signal_level, level2db(level), 0.0);

#ifdef DEBUG_SAMPLE
			printf("%s amplitude= %.6f\n", debug_amplitude(level/rx->value_level), level/rx->value_level);
#endif

			/* delay sample */
			delayed_level = rx->delay_buffer[rx->delay_index];
			rx->delay_buffer[rx->delay_index] = level;
			if (++rx->delay_index == rx->delay_size)
				rx->delay_index = 0;

			if (rx->clock_count < 0 || rx->clock_count > 900) {
				if (level / delayed_level < REDUCTION_TH)
					rx->clock_count = 0;
			}
			if (rx->clock_count >= 0) {
				if (rx->clock_count == 0) {
#ifdef DEBUG_SAMPLE
					puts("got clock");
#endif
					rx->value_level = delayed_level;
				}
				if (rx->clock_count == 50) {
#ifdef DEBUG_SAMPLE
					puts("*short*");
#endif
					rx->value_short = level;
					reduction = rx->value_short / rx->value_level;
					if (reduction < REDUCTION_TH) {
#ifdef DEBUG_SAMPLE
						printf("reduction is %.3f\n", reduction);
#endif
						if (reduction < REDUCTION_FACTOR)
							reduction = REDUCTION_FACTOR;
						quality = 1.0 - (reduction - REDUCTION_FACTOR) / (REDUCTION_TH - REDUCTION_FACTOR);
						display_measurements_update(dcf77->dmp_signal_quality, quality * 100.0, 0.0);
					}
				}
				if (rx->clock_count == 150) {
#ifdef DEBUG_SAMPLE
					puts("*long*");
#endif
					rx->value_long = level;
					if (rx->value_long / rx->value_level < REDUCTION_TH)
						rx_symbol(dcf77, '1');
					else
						rx_symbol(dcf77, '0');
				}
				if (rx->clock_count == 1100) {
#ifdef DEBUG_SAMPLE
					puts("*missing clock*");
#endif
					rx->clock_count = -1;
					rx_symbol(dcf77, 'm');
				}
			}
			if (rx->clock_count >= 0)
				rx->clock_count++;


		}
	}
}



