#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "../common/emphasis.h"
#include "../common/debug.h"

#define level2db(level)		(20 * log10(level))
#define db2level(db)		pow(10, (double)db / 20.0)

#define SAMPLERATE	48000
#define DEVIATION	8000.0

static double test_freq[] = { 25, 50, 100, 200, 250, 300, 400, 500, 1000, 2000, 4000, 0 };

static void check_level(int16_t *samples, double freq, const char *desc)
{
	int i;
	int last = 0, envelope = 0;
	int up = 0;

	for (i = 0; i < SAMPLERATE; i++) {
		if (last < samples[i]) {
			up = 1;
		} else if (last > samples[i]) {
			if (up) {
				if (last > envelope)
					envelope = last;
			}
			up = 0;
		}
		last = samples[i];
	}
	printf("%s: f = %.0f envelop = %.4f\n", desc, freq, level2db((double)envelope / DEVIATION));
}

static void gen_samples(int16_t *samples, double freq)
{
	int i;
	double value;

	for (i = 0; i < SAMPLERATE; i++) {
		value = sin(2.0 * M_PI * freq / (double)SAMPLERATE * (double)i);
		samples[i] = value * DEVIATION;
	}
}

int main(void)
{
	emphasis_t estate;
	int16_t samples[SAMPLERATE];
	int i;

	debuglevel = DEBUG_DEBUG;

	printf("1000 Hz shall be close to 0 dB, that is no significant change in volume.\n\n");

	init_emphasis(&estate, SAMPLERATE, CUT_OFF_EMPHASIS_DEFAULT);

	for (i = 0; test_freq[i]; i++) {
		gen_samples(samples, test_freq[i]);
		pre_emphasis(&estate, samples, SAMPLERATE);
		check_level(samples, test_freq[i], "pre-emphasis");
	}

	/* generate sweep 0..4khz */
	for (i = 0; test_freq[i]; i++) {
		gen_samples(samples, test_freq[i]);
		de_emphasis(&estate, samples, SAMPLERATE);
		check_level(samples, test_freq[i], "de-emphasis");
	}

	return 0;
}

