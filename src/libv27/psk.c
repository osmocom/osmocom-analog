/* Jolly's Version of PSK
 *
 * (C) 2020 by Andreas Eversberg <jolly@eversberg.eu>
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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "../liblogging/logging.h"
#include "../libsample/sample.h"
#include "psk.h"

/* bits to phase change */
double phase_change8[8] = {
	2.0 * M_PI * 1.0 / 8.0,
	2.0 * M_PI * 0.0 / 8.0,
	2.0 * M_PI * 2.0 / 8.0,
	2.0 * M_PI * 3.0 / 8.0,
	2.0 * M_PI * 6.0 / 8.0,
	2.0 * M_PI * 7.0 / 8.0,
	2.0 * M_PI * 5.0 / 8.0,
	2.0 * M_PI * 4.0 / 8.0,
};

/* phase change to bits */
uint8_t phase2bits[8] = { 1, 0, 2, 3, 7, 6, 4, 5 };

/* debug decoder */
//#define DEBUG_DECODER

/* use SIT original encoder */
//#define SIT_ENCODER

/* may be different for testing purpose */
#define TX_CARRIER	1800.0
#define RX_CARRIER	1800.0

#ifdef SIT_ENCODER
static int8_t symbols_int[8][30] = {
	{ 0xff, 0xfd, 0x00, 0x02, 0xfe, 0x02, 0x0d, 0x05, 0xf9, 0x05, 0x02, 0xd2, 0xc9, 0x23, 0x64, 0x23,
	  0xc9, 0xd2, 0x02, 0x05, 0xf9, 0x05, 0x0d, 0x02, 0xfe, 0x02, 0x00, 0xfd, 0xff, 0x00 },
	{ 0xfe, 0xff, 0x03, 0x02, 0xff, 0x07, 0x0a, 0xfa, 0xf7, 0x03, 0xef, 0xd0, 0xfe, 0x56, 0x47, 0xdb,
	  0xb4, 0xef, 0x14, 0x03, 0xff, 0x0e, 0x09, 0xfc, 0xfe, 0x01, 0xfd, 0xfd, 0x00, 0x00 },
	{ 0xff, 0x01, 0x04, 0x01, 0x01, 0x08, 0x00, 0xf2, 0xfa, 0x00, 0xe6, 0xea, 0x34, 0x57, 0x00, 0xa9,
	  0xcc, 0x16, 0x1a, 0x00, 0x06, 0x0e, 0x00, 0xf8, 0xff, 0xff, 0xfc, 0xff, 0x01, 0x00 },
	{ 0x00, 0x03, 0x03, 0xff, 0x02, 0x04, 0xf7, 0xf2, 0x01, 0xfd, 0xec, 0x11, 0x4c, 0x25, 0xb9, 0xaa,
	  0x02, 0x30, 0x11, 0xfd, 0x09, 0x06, 0xf6, 0xf9, 0x01, 0xfe, 0xfd, 0x01, 0x02, 0x00 },
	{ 0x01, 0x03, 0x00, 0xfe, 0x02, 0xfe, 0xf3, 0xfb, 0x07, 0xfb, 0xfe, 0x2e, 0x37, 0xdd, 0x9c, 0xdd,
	  0x37, 0x2e, 0xfe, 0xfb, 0x07, 0xfb, 0xf3, 0xfe, 0x02, 0xfe, 0x00, 0x03, 0x01, 0x00 },
	{ 0x02, 0x01, 0xfd, 0xfe, 0x01, 0xf9, 0xf6, 0x06, 0x09, 0xfd, 0x11, 0x30, 0x02, 0xaa, 0xb9, 0x25,
	  0x4c, 0x11, 0xec, 0xfd, 0x01, 0xf2, 0xf7, 0x04, 0x02, 0xff, 0x03, 0x03, 0x00, 0x00 },
	{ 0x01, 0xff, 0xfc, 0xff, 0xff, 0xf8, 0x00, 0x0e, 0x06, 0x00, 0x1a, 0x16, 0xcc, 0xa9, 0x00, 0x57,
	  0x34, 0xea, 0xe6, 0x00, 0xfa, 0xf2, 0x00, 0x08, 0x01, 0x01, 0x04, 0x01, 0xff, 0x00 },
	{ 0x00, 0xfd, 0xfd, 0x01, 0xfe, 0xfc, 0x09, 0x0e, 0xff, 0x03, 0x14, 0xef, 0xb4, 0xdb, 0x47, 0x56,
	  0xfe, 0xd0, 0xef, 0x03, 0xf7, 0xfa, 0x0a, 0x07, 0xff, 0x02, 0x03, 0xff, 0xfe, 0x00 },
};

static sample_t symbols[8][150];

/* indexes are rotated by 45 degrees, because the phase change during one symbol is 360+45 degrees */
static int bits2index[8] = { 2, 1, 3, 4, 7, 0, 6, 5 };
#endif

/* init psk */
int psk_mod_init(psk_mod_t *psk, void *inst, int (*send_bit)(void *inst), int samplerate, double symbolrate)
{
	double cutoff, transitionband;

	memset(psk, 0, sizeof(*psk));

	psk->send_bit = send_bit;
	psk->inst = inst;

#ifdef SIT_ENCODER
	int i, j, s;
	sample_t spl;

	if (samplerate != 48000) {
		LOGP(DDSP, LOGL_NOTICE, "Sampling rate for PSK encoder must be exactly 48000 Hz!\n");
		return -EINVAL;
	}
	if (symbolrate != 1600) {
		LOGP(DDSP, LOGL_NOTICE, "Symbol rate for PSK encoder must be exactly 1600 Hz!\n");
		return -EINVAL;
	}

	cutoff = 3300.0;
	transitionband = 200;
	psk->lp[0] = fir_lowpass_init((double)samplerate, cutoff, transitionband);
        LOGP(DDSP, LOGL_DEBUG, "Cut off frequency is at %.1f Hz and %.1f Hz.\n", TX_CARRIER + cutoff, TX_CARRIER - cutoff);

	/* interpolate symbol table from 9600 Hz to 48000 Hz */
	for (i = 0; i < 8; i++) {
		for (j = 0, s = 0; j < 30; j++) {
			spl = (double)symbols_int[i][j] / 128.0;
			symbols[i][s++] = spl;
			symbols[i][s++] = spl;
			symbols[i][s++] = spl;
			symbols[i][s++] = spl;
			symbols[i][s++] = spl;
		}
	}
#else
	if (samplerate < 48000) {
		LOGP(DDSP, LOGL_NOTICE, "Sampling rate for PSK encoder must be 48000 Hz minimum!\n");
		return -EINVAL;
	}

	/* fixme: make correct filter */
	cutoff = RX_CARRIER - 100;
	transitionband = 200;
	psk->lp[0] = fir_lowpass_init((double)samplerate, cutoff, transitionband);
	psk->lp[1] = fir_lowpass_init((double)samplerate, cutoff, transitionband);
        LOGP(DDSP, LOGL_DEBUG, "Cut off frequency is at %.1f Hz and %.1f Hz.\n", TX_CARRIER + cutoff, TX_CARRIER - cutoff);

	psk->symbols_per_sample = symbolrate / (double)samplerate;
	LOGP(DDSP, LOGL_DEBUG, "Symbol duration of %.4f symbols per sample @ %d.\n", psk->symbols_per_sample, samplerate);

	psk->carrier_phaseshift = 2.0 * M_PI * TX_CARRIER / (double)samplerate;
	LOGP(DDSP, LOGL_DEBUG, "Carrier phase shift of %.4f per sample @ %d.\n", psk->carrier_phaseshift, samplerate);
#endif

	return 0;
}

void psk_mod_exit(psk_mod_t *psk)
{
	if (psk->lp[0]) {
		fir_exit(psk->lp[0]);
		psk->lp[0] = NULL;
	}
	if (psk->lp[1]) {
		fir_exit(psk->lp[1]);
		psk->lp[1] = NULL;
	}
}

void psk_mod(psk_mod_t *psk, sample_t *sample, int length)
{
	uint8_t bits;
	int s;
#ifdef SIT_ENCODER
	int index;

	for (s = 0; s < length; s++) {
		if (psk->spl_count == 0) {
			bits = (psk->send_bit(psk->inst) & 1) << 2;
			bits |= (psk->send_bit(psk->inst) & 1) << 1;
			bits |= (psk->send_bit(psk->inst) & 1);

#ifdef DEBUG_DECODER
			static int nextbit = 0;
			if (++nextbit == 8)
				nextbit = 0;
			bits = nextbit;
#endif

			index = psk->sym_list[psk->sym_count];
			psk->sym_count = (psk->sym_count + 1) % 5;
			psk->sym_list[psk->sym_count] = (index + bits2index[bits]) & 7;
		}

		sample[s] = symbols[psk->sym_list[psk->sym_count]][psk->spl_count];
		sample[s] += symbols[psk->sym_list[(psk->sym_count + 4) % 5]][30 + psk->spl_count];
		sample[s] += symbols[psk->sym_list[(psk->sym_count + 3) % 5]][60 + psk->spl_count];
		sample[s] += symbols[psk->sym_list[(psk->sym_count + 2) % 5]][90 + psk->spl_count];
		sample[s] += symbols[psk->sym_list[(psk->sym_count + 1) % 5]][120 + psk->spl_count];

		if (++psk->spl_count == 30)
			psk->spl_count = 0;
	}
	fir_process(psk->lp[0], sample, length);
#else
	sample_t I[length], Q[length];

	/* count symbol and get new bits for next symbol */
	for (s = 0; s < length; s++) {
		psk->symbol_pos += psk->symbols_per_sample;
		if (psk->symbol_pos >= 1.0) {
			psk->symbol_pos -= 1.0;
			/* get tree bits */
			bits = (psk->send_bit(psk->inst) & 1) << 2;
			bits |= (psk->send_bit(psk->inst) & 1) << 1;
			bits |= (psk->send_bit(psk->inst) & 1);

#ifdef DEBUG_DECODER
			static int nextbit = 0;
			if (++nextbit == 8)
				nextbit = 0;
			bits = nextbit;
#endif

			/* change phase_shift */
			psk->phase_shift += phase_change8[bits];
			if (psk->phase_shift > M_PI)
				psk->phase_shift -= 2.0 * M_PI;
		}

		I[s] = cos(psk->phase_shift);
		Q[s] = sin(psk->phase_shift);
	}

	/* filter phase_shift to limit bandwidth */
	fir_process(psk->lp[0], I, length);
	fir_process(psk->lp[1], Q, length);

	/* modulate with carrier frequency */
	for (s = 0; s < length; s++) {
		/* compensate overshooting of filter */
		*sample++ = (I[s] * cos(psk->carrier_phase) - Q[s] * sin(psk->carrier_phase)) * 0.7;
		psk->carrier_phase += psk->carrier_phaseshift;
		if (psk->carrier_phase >= 2.0 * M_PI)
			psk->carrier_phase -= 2.0 * M_PI;
	}
#endif
}

int psk_demod_init(psk_demod_t *psk, void *inst, void (*receive_bit)(void *inst, int bit), int samplerate, double symbolrate)
{
	double cutoff, transitionband;

	if (samplerate < 48000) {
		LOGP(DDSP, LOGL_NOTICE, "Sampling rate for PSK decoder must be 48000 Hz minimum!\n");
		return -EINVAL;
	}

	memset(psk, 0, sizeof(*psk));

	psk->receive_bit = receive_bit;
	psk->inst = inst;

	/* fixme: make correct filter */
//	cutoff = symbolrate / 2.0 * 1.5;
	cutoff = RX_CARRIER - 100;
	transitionband = 200;
	psk->lp[0] = fir_lowpass_init((double)samplerate, cutoff, transitionband);
	psk->lp[1] = fir_lowpass_init((double)samplerate, cutoff, transitionband);
	iir_lowpass_init(&psk->lp_error[0], 50.0, samplerate, 2);
	iir_lowpass_init(&psk->lp_error[1], 50.0, samplerate, 2);
	iir_bandpass_init(&psk->lp_clock, symbolrate, samplerate, 40);
	psk->sample_delay = (int)floor((double)samplerate / symbolrate * 0.25); /* percent of sine duration behind zero crossing */
	LOGP(DDSP, LOGL_DEBUG, "Cut off frequency is at %.1f Hz and %.1f Hz.\n", RX_CARRIER + cutoff, RX_CARRIER - cutoff);

	psk->carrier_phaseshift = 2.0 * M_PI * -RX_CARRIER / (double)samplerate;
	LOGP(DDSP, LOGL_DEBUG, "Carrier phase shift of %.4f per sample @ %d.\n", psk->carrier_phaseshift, samplerate);

	return 0;
}

void psk_demod_exit(psk_demod_t *psk)
{
	if (psk->lp[0]) {
		fir_exit(psk->lp[0]);
		psk->lp[0] = NULL;
	}
	if (psk->lp[1]) {
		fir_exit(psk->lp[1]);
		psk->lp[1] = NULL;
	}
}

#ifdef DEBUG_DECODER
static void debug_phase(double phase, double amplitude, double error, double amplitude2)
{
	int x, y, xx = 100, yy = 50;
	char buffer[yy][xx + 1];
	double p;
	int i;

	if (amplitude > 1.0)
		amplitude = 1.0;
	if (amplitude < -0.0)
		amplitude = -0.0;
	if (amplitude2 > 0.5)
		amplitude2 = 0.5;
	if (amplitude2 < -0.5)
		amplitude2 = -0.5;
	amplitude2 += 0.5;

	/* clear (EOL) and fill spaces with border */
	memset(buffer, '\0', sizeof(buffer));
	memset(buffer[0], '#', xx);
	for (y = 1; y < yy - 1; y++) {
		memset(buffer[y], ' ', xx);
		buffer[y][0] = '|';
		buffer[y][xx - 1] = '|';
	}
	memset(buffer[yy - 1], '-', xx);
	buffer[0][0] = '+';
	buffer[0][xx - 1] = '+';
	buffer[yy - 1][0] = '+';
	buffer[yy - 1][xx - 1] = '+';

	/* plot target angles on buffer */
	for (i = 0, p = 0.0; i < 8; i++, p = p + M_PI / 4.0) {
		y = -(sin(p + error) * (double)yy / 1.1) + (double)yy;
		x = (cos(p + error) * (double)xx / 2.2) + (double)xx / 2.0;
		buffer[y >> 1][x] = '0' + i;
	}

	/* plot angle on buffer */
	y = -(amplitude * 1.1 * sin(phase) * (double)yy / 1.1) + (double)yy;
	x = (amplitude * 1.1 * cos(phase) * (double)xx / 2.2) + (double)xx / 2.0;
	if ((y & 1))
		buffer[y >> 1][x] = '.';
	else
		buffer[y >> 1][x] = '\'';

	/* plot amplitude on buffer */
	y = -(amplitude * (double)yy * 2.0 / 1.1) + (double)yy * 2.0 / 1.1;
	if ((y & 1))
		buffer[y >> 1][1] = '.';
	else
		buffer[y >> 1][1] = '\'';
	y = -(amplitude2 * (double)yy * 2.0 / 1.1) + (double)yy * 2.0 / 1.1;
	if ((y & 1))
		buffer[y >> 1][2] = '.';
	else
		buffer[y >> 1][2] = '\'';

	/* display on screen */
	for (y = 0; y < yy; y++)
		printf("%s\n", buffer[y]);
}
#endif

void psk_demod(psk_demod_t *psk, sample_t *sample, int length)
{
	sample_t I[length], Q[length], i;
	sample_t Ip[length], Qp[length];
	double phases[length];
	sample_t amplitudes[length], amplitudes2[length];
	double phaseshift, phase, phase_error, angle_error;
	uint16_t phase_error_int, offset;
	int s, ss;
	uint8_t sector, rotation, bits;

	/* demodulate phase from carrier */
	phaseshift = psk->carrier_phaseshift;
	phase = psk->carrier_phase;
	for (s = 0, ss = 0; s < length; s++) {
		phase += phaseshift;
		i = sample[ss++];
		if (phase < 0)
			phase += 2.0 * M_PI;
		else if (phase >= 2.0 * M_PI)
			phase -= 2.0 * M_PI;
		I[s] = i * cos(phase);
		Q[s] = i * sin(phase);
	}
	psk->carrier_phase = phase;
	fir_process(psk->lp[0], I, length);
	fir_process(psk->lp[1], Q, length);

	/* get phase error */
	for (s = 0, ss = 0; s < length; s++) {
		phases[s] = atan2(Q[s], I[s]);
		amplitudes[s] = sqrt(Q[s] * Q[s] + I[s] * I[s]) * 2.0;
		amplitudes2[s] = amplitudes[s];
		Ip[s] = amplitudes[s] * cos(phases[s] * 8.0) * 2.0;
		Qp[s] = amplitudes[s] * sin(phases[s] * 8.0) * 2.0;
	}
	iir_process(&psk->lp_error[0], Ip, length);
	iir_process(&psk->lp_error[1], Qp, length);

	/* filter amplitude to get symbol clock */
	/* NOTE: the filter biases the amplitude, so that we have positive and negative peaks.
	   positive peak is the sample point */
	iir_process(&psk->lp_clock, amplitudes2, length);

	for (s = 0; s < length; s++) {
		/* calculate change of phase error angle within one sample */
		phase_error_int = (int)floor(atan2(Qp[s], Ip[s]) / (2.0 * M_PI) * 65536.0);
		offset = phase_error_int - psk->last_phase_error;
		psk->last_phase_error = phase_error_int;

		/* apply change to current phase error value */
		psk->phase_error += (int16_t)offset;
		if (psk->phase_error >= 65536 * 8)
			psk->phase_error -= 65536 * 8;
		if (psk->phase_error < 0)
			psk->phase_error += 65536 * 8;

		phase_error = (double)psk->phase_error / 65536.0 * (2.0 * M_PI) / 8.0;

		/* if we have reached a zero crossing of the amplitude signal, wait for sample point */
		if (psk->sample_timer && --psk->sample_timer == 0) {
			/* sample point reached */
			phase = fmod(phases[s] - phase_error + 4.0 * M_PI, 2.0 * M_PI);
			if (phase < 2.0 * M_PI / 16.0 * 1.0)
				sector = 0;
			else if (phase < 2.0 * M_PI / 16.0 * 3.0)
				sector = 1;
			else if (phase < 2.0 * M_PI / 16.0 * 5.0)
				sector = 2;
			else if (phase < 2.0 * M_PI / 16.0 * 7.0)
				sector = 3;
			else if (phase < 2.0 * M_PI / 16.0 * 9.0)
				sector = 4;
			else if (phase < 2.0 * M_PI / 16.0 * 11.0)
				sector = 5;
			else if (phase < 2.0 * M_PI / 16.0 * 13.0)
				sector = 6;
			else if (phase < 2.0 * M_PI / 16.0 * 15.0)
				sector = 7;
			else
				sector = 0;
			angle_error = fmod(phase / 2.0 / M_PI * 8.0, 1.0);
			if (angle_error > 0.5)
				angle_error -= 1.0;

			rotation = (sector - psk->last_sector) & 7; // might be negative, so we use AND!
			bits = phase2bits[rotation];
#ifdef DEBUG_DECODER
			printf("sector=%d last_sector=%d rotation=%d bits=%d angle_error=%.2f\n", sector, psk->last_sector, rotation, bits, angle_error);
#endif
			psk->last_sector = sector;
			/* report bits */
#ifndef DEBUG_DECODER
			psk->receive_bit(psk->inst, bits >> 2);
			psk->receive_bit(psk->inst, (bits >> 1) & 1);
			psk->receive_bit(psk->inst, bits & 1);
#endif
		}
		if (psk->last_amplitude <= 0.0 && amplitudes2[s] > 0.0)
			psk->sample_timer = psk->sample_delay;
		psk->last_amplitude = amplitudes2[s];

#ifdef DEBUG_DECODER
		static int when = 0;
		if (++when > 10000) {
			printf("\0337\033[H");
			/* display amplitude between 0.0 and 1.0, aplitude2 between -0.5 and 0.5 */
			debug_phase(phases[s], amplitudes[s], phase_error, amplitudes2[s]);
			printf("phase2 = %.4f offset = %d, error = %d (error & 0xffff = %d)\n", phase_error, offset, psk->phase_error, psk->phase_error & 0xffff);
			printf("\033[0;39m\0338"); fflush(stdout);
			usleep(50000);
		}
#endif
	}
}

