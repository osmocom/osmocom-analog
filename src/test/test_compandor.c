#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "../libsample/sample.h"
#include "../libcompandor/compandor.h"

#define level2db(level)		(20 * log10(level))
#define db2level(db)		pow(10, (double)db / 20.0)

#define SAMPLERATE		48000
#define ATTACK_MS		15.0
#define RECOVERY_MS		15.0

static double test_frequency[3] = { 2000.0, 4000.0, 1000.0 };

static sample_t samples_4db[SAMPLERATE];
static sample_t samples_16db[SAMPLERATE];
static sample_t samples_2db[SAMPLERATE];
static sample_t samples_8db[SAMPLERATE];
static sample_t samples_0db[SAMPLERATE];

/* generate 2 samples: one with -4 dB, the other with -16 dB */
static void generate_test_sample(double test_frequency)
{
	int i;
	double value;

	for (i = 0; i < SAMPLERATE; i++) {
		value = cos(2.0 * M_PI * test_frequency / (double)SAMPLERATE * i);
		samples_4db[i] = value * db2level(-4);
		samples_16db[i] = value * db2level(-16);
		samples_2db[i] = value * db2level(-2);
		samples_8db[i] = value * db2level(-8);
		samples_0db[i] = value;
	}
}

static void check_level(sample_t *samples, double duration, const char *desc, double target_db)
{
	int i;
	double last = 0.0, envelop = 0.0;
	int up = 0;
	double factor;

	int when = (int)((double)SAMPLERATE + (double)SAMPLERATE * duration / 1000.0 + 0.5);
//printf("%d %d\n", SAMPLERATE, when);

	for (i = 0; i < when; i++) {
//puts(debug_amplitude(samples[i]));
		if (last < samples[i]) {
			up = 1;
		} else if (last > samples[i]) {
			if (up) {
				envelop = last;
			}
			up = 0;
		}
#if 0
		if (i >= (SAMPLERATE-(SAMPLERATE/100)) && (i % (SAMPLERATE/(SAMPLERATE/10))) == 0)
			printf("%s: envelop = %.4f (val=%.4f) (when=%d)\n", desc, level2db(envelop), envelop, i);
#endif
		last = samples[i];
	}
	factor = envelop / db2level(target_db);
	printf("%s: envelop after the instance of %.1f ms is %.4f db factor =%.4f\n", desc, duration, level2db(envelop), factor);
}

int main(void)
{
	compandor_t cstate;
	sample_t samples[SAMPLERATE * 2];
	int f;

	compandor_init();
	setup_compandor(&cstate, SAMPLERATE, ATTACK_MS, RECOVERY_MS);

	for (f = 0; f < 3; f++) {
		/* -16 and -4 dB */
		printf("Testing frequency %.0f Hz:\n", test_frequency[f]);
		generate_test_sample(test_frequency[f]);

#if 0
		check_level(samples_0db, -100.0, "generate sample with 0db", 0.0);
		check_level(samples_2db, -100.0, "generate sample with -2db", -2.0);
		check_level(samples_4db, -100.0, "generate sample with -4db", -4.0);
		check_level(samples_8db, -100.0, "generate sample with -8db", -8.0);
		check_level(samples_16db, -100.0, "generate sample with -16db", -16.0);
#endif

		/* low to high transition */
		memcpy(samples, samples_16db, SAMPLERATE * sizeof(sample_t));
		memcpy(samples + SAMPLERATE, samples_4db, SAMPLERATE * sizeof(sample_t));

		compress_audio(&cstate, samples, SAMPLERATE * 2);

		check_level(samples, ATTACK_MS, "compressor attack", -2.0);

		/* high to low transition */
		memcpy(samples, samples_4db, SAMPLERATE * sizeof(sample_t));
		memcpy(samples + SAMPLERATE, samples_16db, SAMPLERATE * sizeof(sample_t));

		compress_audio(&cstate, samples, SAMPLERATE * 2);

		check_level(samples, RECOVERY_MS, "compressor recovery", -8.0);

		/* low to high transition */
		memcpy(samples, samples_8db, SAMPLERATE * sizeof(sample_t));
		memcpy(samples + SAMPLERATE, samples_2db, SAMPLERATE * sizeof(sample_t));

		expand_audio(&cstate, samples, SAMPLERATE * 2);

		check_level(samples, ATTACK_MS, "expander attack", -4.0);

		/* high to low transition */
		memcpy(samples, samples_2db, SAMPLERATE * sizeof(sample_t));
		memcpy(samples + SAMPLERATE, samples_8db, SAMPLERATE * sizeof(sample_t));

		expand_audio(&cstate, samples, SAMPLERATE * 2);

		check_level(samples, RECOVERY_MS, "expander recovery", -16.0);

		/* 0 DB */
		memcpy(samples, samples_0db, SAMPLERATE * sizeof(sample_t));
		memcpy(samples + SAMPLERATE, samples_0db, SAMPLERATE * sizeof(sample_t));

		check_level(samples, RECOVERY_MS, "unaffected level", 0.0);

		puts("");
	}

	return 0;
}

