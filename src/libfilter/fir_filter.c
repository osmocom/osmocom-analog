/* FIR filter
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
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "../libsample/sample.h"
#include "fir_filter.h"

//#define DEBUG_TAPS

static void kernel(double *taps, int M, double cutoff, int invert)
{
	int i;
	double sum;

	for (i = 0; i <= M; i++) {
		/* gen sinc */
		if (i == M / 2)
			taps[i] = 2.0 * M_PI * cutoff;
		else
			taps[i] = sin(2.0 * M_PI * cutoff * (double)(i - M / 2))
				/ (double)(i - M / 2);
		/* blackman window */
		taps[i] *= 0.42 - 0.50 * cos(2 * M_PI * (double)(i / M))
				+ 0.08 * cos(4 * M_PI * (double)(i / M));
	}

	/* normalize */
	sum = 0;
	for (i = 0; i <= M; i++)
		sum += taps[i];
	for (i = 0; i <= M; i++)
		taps[i] /= sum;

	/* invert */
	if (invert) {
		for (i = 0; i <= M; i++)
			taps[i] = -taps[i];
		taps[M / 2] += 1.0;
	}

#ifdef DEBUG_TAPS
	puts("start");
	for (i = 0; i <= M; i++)
		puts(debug_amplitude(taps[i]));
#endif
}

static fir_filter_t *fir_init(double samplerate, double transition_bandwidth)
{
	fir_filter_t *fir;
	int M;

	/* alloc struct */
	fir = calloc(1, sizeof(*fir));
	if (!fir) {
		fprintf(stderr, "No memory creating FIR filter!\n");
		return NULL;
	}

	/* transition bandwidth */
	M = ceil(1.0 / (transition_bandwidth / samplerate));
	if ((M & 1))
		M++;

//	printf("cutoff=%.4f\n", cutoff / samplerate);
//	printf("tb=%.4f\n", transition_bandwidth / samplerate);
	fir->ntaps = M + 1;
	fir->delay = M / 2;

	/* alloc taps */
	fir->taps = calloc(fir->ntaps, sizeof(*fir->taps));
	if (!fir->taps) {
		fprintf(stderr, "No memory creating FIR filter!\n");
		fir_exit(fir);
		return NULL;
	}

	/* alloc ring buffer */
	fir->buffer = calloc(fir->ntaps, sizeof(*fir->buffer));
	if (!fir->buffer) {
		fprintf(stderr, "No memory creating FIR filter!\n");
		fir_exit(fir);
		return NULL;
	}


	return fir;
}

fir_filter_t *fir_lowpass_init(double samplerate, double cutoff, double transition_bandwidth)
{
	/* calculate kernel */
	fir_filter_t *fir =  fir_init(samplerate, transition_bandwidth);
	if (!fir)
		return NULL;
	kernel(fir->taps, fir->ntaps - 1, cutoff / samplerate, 0);
	return fir;
}

fir_filter_t *fir_highpass_init(double samplerate, double cutoff, double transition_bandwidth)
{
	fir_filter_t *fir =  fir_init(samplerate, transition_bandwidth);
	if (!fir)
		return NULL;
	kernel(fir->taps, fir->ntaps - 1, cutoff / samplerate, 1);
	return fir;
}

fir_filter_t *fir_allpass_init(double samplerate, double transition_bandwidth)
{
	fir_filter_t *fir =  fir_init(samplerate, transition_bandwidth);
	if (!fir)
		return NULL;
	fir->taps[(fir->ntaps - 1) / 2] = 1.0;
	return fir;
}

fir_filter_t *fir_twopass_init(double samplerate, double cutoff_low, double cutoff_high, double transition_bandwidth)
{
	int i;
	double sum;
	fir_filter_t *fir =  fir_init(samplerate, transition_bandwidth);
	if (!fir)
		return NULL;
	double lp_taps[fir->ntaps], hp_taps[fir->ntaps];
	kernel(lp_taps, fir->ntaps - 1, cutoff_low / samplerate, 0);
	kernel(hp_taps, fir->ntaps - 1, cutoff_high / samplerate, 1);
	sum = 0;
	printf("#warning does not work as expected\n");
	abort();
	for (i = 0; i < fir->ntaps; i++) {
		fir->taps[i] = lp_taps[i] + hp_taps[i];
		sum += fir->taps[i];
	}
	/* hp will die */
//	for (i = 0; i < fir->ntaps; i++)
	//	fir->taps[i] /= sum;
	return fir;
}

void fir_exit(fir_filter_t *fir)
{
	if (!fir)
		return;
	free(fir->taps);
	free(fir->buffer);
	free(fir);
}

void fir_process(fir_filter_t *fir, sample_t *samples, int num)
{
	int i, j;
	double y;

	for (i = 0; i < num; i++) {
		/* put sample in ring buffer */
		fir->buffer[fir->buffer_pos] = samples[i];
		if (++fir->buffer_pos == fir->ntaps)
			fir->buffer_pos = 0;

		/* convolve samples */
		y = 0;
		for (j = 0; j < fir->ntaps; j++) {
			/* convolve sample from ring buffer, starting with oldes */
			y += fir->buffer[fir->buffer_pos] * fir->taps[j];
			if (++fir->buffer_pos == fir->ntaps)
				fir->buffer_pos = 0;
		}
		samples[i] = y;
	}
}

int fir_get_delay(fir_filter_t *fir)
{
	return fir->delay;
}

