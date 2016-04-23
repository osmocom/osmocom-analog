#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "../common/emphasis.h"

#define level2db(level)		(20 * log10(level))
#define db2level(db)		pow(10, (double)db / 20.0)

#define SAMPLERATE	48000
#define DEVIATION	8000.0

static void check_level(int16_t *samples, const char *desc)
{
	int i;
	int last = 0, envelope = 0;
	int up = 0;
	int freq;

	for (i = 0; i < SAMPLERATE; i++) {
		if (last < samples[i]) {
			up = 1;
		} else if (last > samples[i]) {
			if (up) {
				envelope = last;
			}
			up = 0;
		}
		if ((i % (SAMPLERATE/40)) == 0) {
			freq = 500 + 500 * (i / (SAMPLERATE / 8));
			printf("%s: f = %d envelop = %.4f\n", desc, freq, level2db((double)envelope / DEVIATION));
		}
		last = samples[i];
	}
}

static void gen_samples(int16_t *samples)
{
	int i;
	double value;
	int freq;

	for (i = 0; i < SAMPLERATE; i++) {
		freq = 500 + 500 * (i / (SAMPLERATE / 8));
		value = sin(2.0 * M_PI * (double)freq / (double)SAMPLERATE * (double)i);
		samples[i] = value * DEVIATION;
	}
}

int main(void)
{
	emphasis_t estate;
	int16_t samples[SAMPLERATE];

	/* generate sweep 0..4khz */
	gen_samples(samples);

	init_emphasis(&estate, SAMPLERATE);

//	check_level(samples, "unchanged");

	pre_emphasis(&estate, samples, SAMPLERATE);

	check_level(samples, "pre-emphasis");

	/* generate sweep 0..4khz */
	gen_samples(samples);

	de_emphasis(&estate, samples, SAMPLERATE);

	check_level(samples, "de-emphasis");

	return 0;
}

