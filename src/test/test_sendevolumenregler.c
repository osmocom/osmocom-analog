#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "../libsample/sample.h"
#include "../libsendevolumenregler/sendevolumenregler.h"

#define level2db(level)		(20 * log10(level))
#define db2level(db)		pow(10, (double)db / 20.0)

#define SAMPLERATE		48000
#define ABWAERTS_DBS		(2.6 / 0.020) /* 2.6 dB in 20 ms */
#define AUFWAERTS_DBS		4.3
#define MINIMUM_DB		-16.0
#define MAXIMUM_DB		2.6

static double test_frequency[3] = { 1000.0, 4000.0, 300.0 };

static sample_t samples_plus4db[SAMPLERATE];
static sample_t samples_minus20db[SAMPLERATE];
static sample_t samples_0db[SAMPLERATE];

/* generate 2 samples: one with -4 dB, the other with -16 dB */
static void generate_test_sample(double test_frequency)
{
	int i;
	double value;

	for (i = 0; i < SAMPLERATE; i++) {
		value = cos(2.0 * M_PI * test_frequency / (double)SAMPLERATE * i);
		samples_plus4db[i] = value * db2level(4);
		samples_minus20db[i] = value * db2level(-20);
		samples_0db[i] = value;
	}
}

static void check_level(sample_t *samples, const char *desc, double target_db, int when)
{
	int i;
	double last = 0.0, envelope = 0.0;
	int up = 0;
	double factor;

	for (i = 0; i < when; i++) {
//puts(debug_amplitude(samples[i]));
		if (last < samples[i]) {
			up = 1;
		} else if (last > samples[i]) {
			if (up) {
				envelope = last;
			}
			up = 0;
		}
#if 0
		if ((i % (SAMPLERATE/(SAMPLERATE/100))) == 0)
			printf("%s: envelope = %.4f (val=%.4f) (when=%d ms)\n", desc, level2db(envelope), envelope, i * 1000 / SAMPLERATE);
#endif
		last = samples[i];
	}
	factor = envelope / db2level(target_db);
	printf("%s: envelope after %d ms is %.4f db (factor %.4f)\n", desc, i * 1000 / SAMPLERATE, level2db(envelope), factor);
	if (factor > 1.1 || factor < 0.9) {
		printf("**** ERROR: we expected a factor close to 1.0\n");
	}
}

int main(void)
{
	sendevolumenregler_t state;
	sample_t samples[SAMPLERATE * 2];
	int f;

	for (f = 0; f < 3; f++) {
		printf("Testing frequency %.0f Hz:\n", test_frequency[f]);
		generate_test_sample(test_frequency[f]);

		/* increment low level */
		memcpy(samples, samples_minus20db, SAMPLERATE * sizeof(sample_t));

		init_sendevolumenregler(&state, SAMPLERATE, ABWAERTS_DBS, AUFWAERTS_DBS, MAXIMUM_DB, MINIMUM_DB, 1.0);
		sendevolumenregler(&state, samples, SAMPLERATE);

		check_level(samples, "aufwaerts (-20 dB)", AUFWAERTS_DBS - 20.0, 1000 * SAMPLERATE / 1000);

		/* decrement high level */
		memcpy(samples, samples_plus4db, SAMPLERATE * sizeof(sample_t));

		init_sendevolumenregler(&state, SAMPLERATE, ABWAERTS_DBS, AUFWAERTS_DBS, MAXIMUM_DB, MINIMUM_DB, 1.0);
		sendevolumenregler(&state, samples, SAMPLERATE);

		check_level(samples, "abwaerts (+4 dB)", MAXIMUM_DB / 2.0, 10 * SAMPLERATE / 1000);
		check_level(samples, "abwaerts (+4 dB)", 0.0, 20 * SAMPLERATE / 1000);

		/* 0 DB */
		memcpy(samples, samples_0db, SAMPLERATE * sizeof(sample_t));

		init_sendevolumenregler(&state, SAMPLERATE, ABWAERTS_DBS, AUFWAERTS_DBS, MAXIMUM_DB, MINIMUM_DB, 1.0);
		check_level(samples, "unaffected level", 0.0, 1000 * SAMPLERATE / 1000);

		puts("");
	}

	return 0;
}

