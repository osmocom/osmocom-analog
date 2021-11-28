#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "../libsample/sample.h"
#include "../libgoertzel/goertzel.h"
#include "../libdebug/debug.h"

#define level2db(level)		(20 * log10(level))
#define db2level(db)		pow(10, (double)db / 20.0)

#define SAMPLERATE	48000

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
	goertzel_t goertzel;
	sample_t samples[SAMPLERATE];
	double frequency = 1000;
	double duration = 1.0/100.0;
	double level;
	double i;

	printf("testing goertzel with frequency %.1f and duration 1 / %.0f\n", frequency, 1.0 / duration);

	for (i = 700; i < 1301; i = i + 10) {
		gen_samples(samples, (double)i);
		audio_goertzel_init(&goertzel, frequency, SAMPLERATE);
		audio_goertzel(&goertzel, samples, SAMPLERATE * duration, 0, &level, 1);
		printf("%s%.0f Hz: %.1f dB", debug_db(level), i, level2db(level));
		if ((int)round(i) == (int)round(frequency))
			printf(" level=%.6f\n", level);
		else
			printf("\n");
	}

	return 0;
}

