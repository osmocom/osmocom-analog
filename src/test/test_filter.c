#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "../common/sample.h"
#include "../common/iir_filter.h"
#include "../common/debug.h"

#define level2db(level)		(20 * log10(level))
#define db2level(db)		pow(10, (double)db / 20.0)

#define SAMPLERATE	48000

static double get_level(sample_t *samples)
{
	int i;
	double envelope = 0;
	for (i = SAMPLERATE/2; i < SAMPLERATE; i++) {
		if (samples[i] > envelope)
			envelope = samples[i];
	}

	return envelope;
}

static void gen_samples(sample_t *samples, double freq)
{
	int i;
	double value;

	for (i = 0; i < SAMPLERATE; i++) {
		value = cos(2.0 * M_PI * freq / (double)SAMPLERATE * (double)i);
		samples[i] = value;
	}
}

extern void main_mobile();

int main(void)
{
	iir_filter_t filter_low;
	iir_filter_t filter_high;
	sample_t samples[SAMPLERATE];
	double level;
	int iter = 2;
	int i;

	/* this is never called, it forces the linker to add mobile functions */
	if (debuglevel == -1000) main_mobile();

	debuglevel = DEBUG_DEBUG;

	printf("testing low-pass filter with %d iterations\n", iter);

	iir_lowpass_init(&filter_low, 1000.0, SAMPLERATE, iter);

	for (i = 0; i < 4001; i += 100) {
		gen_samples(samples, (double)i);
		iir_process(&filter_low, samples, SAMPLERATE);
		level = get_level(samples);
		printf("%s%4d Hz: %.1f dB", debug_db(level), i, level2db(level));
		if (i == 1000)
			printf(" cutoff\n");
		else if (i == 2000)
			printf(" double frequency\n");
		else if (i == 4000)
			printf(" quad frequency\n");
		else
			printf("\n");
	}

	printf("testing high-pass filter with %d iterations\n", iter);

	iir_highpass_init(&filter_high, 2000.0, SAMPLERATE, iter);

	for (i = 0; i < 4001; i += 100) {
		gen_samples(samples, (double)i);
		iir_process(&filter_high, samples, SAMPLERATE);
		level = get_level(samples);
		printf("%s%4d Hz: %.1f dB", debug_db(level), i, level2db(level));
		if (i == 2000)
			printf(" cutoff\n");
		else if (i == 1000)
			printf(" half frequency\n");
		else if (i == 500)
			printf(" quarter frequency\n");
		else
			printf("\n");
	}

	printf("testing band-pass filter with %d iterations\n", iter);

	iir_lowpass_init(&filter_low, 2000.0, SAMPLERATE, iter);
	iir_highpass_init(&filter_high, 1000.0, SAMPLERATE, iter);

	for (i = 0; i < 4001; i += 100) {
		gen_samples(samples, (double)i);
		iir_process(&filter_low, samples, SAMPLERATE);
		iir_process(&filter_high, samples, SAMPLERATE);
		level = get_level(samples);
		printf("%s%4d Hz: %.1f dB", debug_db(level), i, level2db(level));
		if (i == 1000)
			printf(" cutoff high\n");
		else if (i == 2000)
			printf(" cutoff low\n");
		else
			printf("\n");
	}

	return 0;
}

