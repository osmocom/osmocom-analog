#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "../libsample/sample.h"
#include "../libfilter/iir_filter.h"
#include "../libfilter/fir_filter.h"
#include "../liblogging/logging.h"

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

int num_kanal;

int main(void)
{
	iir_filter_t filter_low;
	iir_filter_t filter_high;
	fir_filter_t	*fir_low/*, *fir_high*/;
	sample_t samples[SAMPLERATE];
	double level;
	int iter = 2;
	int i;

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

	printf("testing notch filter with %d iterations, Q = 4\n", 1);

	iir_notch_init(&filter_high, 2605.0, SAMPLERATE, 1, 4);

	for (i = 0; i < 4001; i += 100) {
		gen_samples(samples, (double)i);
		iir_process(&filter_high, samples, SAMPLERATE);
		level = get_level(samples);
		printf("%s%4d Hz: %.1f dB", debug_db(level), i, level2db(level));
		if (i == 2600)
			printf(" filter frequency (2605 Hz)\n");
		else if (i == 2800)
			printf(" about 200 Hz above\n");
		else if (i == 2400)
			printf(" about 200 Hz below\n");
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

	double freq = 2000.0;
	double tb = 400.0;
	printf("testing low-pass FIR filter with %.0fHz transition bandwidth\n", tb);

	fir_low = fir_lowpass_init(SAMPLERATE, freq, tb);
	printf("Using %d taps\n", fir_low->ntaps);

	for (i = 0; i < 4001; i += 100) {
		gen_samples(samples, (double)i);
		fir_process(fir_low, samples, SAMPLERATE);
		level = get_level(samples);
		printf("%s%s%4d Hz: %.1f dB", debug_amplitude(level), debug_db(level), i, level2db(level));
		if (i == freq)
			printf(" cutoff\n");
		else
			printf("\n");
	}
	fir_exit(fir_low);

#if 0
	double freq1 = 1000.0, freq2 = 2000.0;
	tb = 100.0;
	printf("testing two-pass FIR filter\n");

	fir_high = fir_twopass_init(SAMPLERATE, freq1, freq2, tb);
	printf("Using %d taps\n", fir_high->ntaps);

	for (i = 0; i < 4001; i += 100) {
		gen_samples(samples, (double)i);
		fir_process(fir_high, samples, SAMPLERATE);
		level = get_level(samples);
		printf("%s%s%4d Hz: %.1f dB", debug_amplitude(level), debug_db(level), i, level2db(level));
		if (i == freq1 || i == freq2)
			printf(" cutoff\n");
		else
			printf("\n");
	}
	fir_exit(fir_high);
#endif

	return 0;
}

