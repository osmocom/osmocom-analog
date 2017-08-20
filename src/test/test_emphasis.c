#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "../common/sample.h"
#include "../common/iir_filter.h"
#include "../common/emphasis.h"
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
	emphasis_t estate;
	double cut_off = CUT_OFF_EMPHASIS_DEFAULT;
	sample_t samples[SAMPLERATE];
	double level;
	double i;

	/* this is never called, it forces the linker to add mobile functions */
	if (debuglevel == -1000) main_mobile();

	debuglevel = DEBUG_DEBUG;

	init_emphasis(&estate, SAMPLERATE, cut_off);

	printf("testing pre-emphasis filter with cut-off frequency %.1f\n", cut_off);

	for (i = 31.25; i < 4001; i = i * sqrt(sqrt(2.0))) {
		gen_samples(samples, (double)i);
		pre_emphasis(&estate, samples, SAMPLERATE);
		level = get_level(samples);
		printf("%s%.0f Hz: %.1f dB", debug_db(level), i, level2db(level));
		if ((int)round(i) == 1000)
			printf(" level=%.6f\n", level);
		else
			printf("\n");
	}

	printf("testing de-emphasis filter with cut-off frequency %.1f\n", cut_off);

	for (i = 31.25; i < 4001; i = i * sqrt(sqrt(2.0))) {
		gen_samples(samples, (double)i);
		dc_filter(&estate, samples, SAMPLERATE);
		de_emphasis(&estate, samples, SAMPLERATE);
		level = get_level(samples);
		printf("%s%.0f Hz: %.1f dB", debug_db(level), i, level2db(level));
		if ((int)round(i) == 1000)
			printf(" level=%.6f\n", level);
		else
			printf("\n");
	}

	return 0;
}

