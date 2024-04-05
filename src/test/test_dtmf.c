#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "../liblogging/logging.h"
#include "../libsample/sample.h"
#include "../libdtmf/dtmf_decode.h"
#include "../libdtmf/dtmf_encode.h"

#define level2db(level)		(20 * log10(level))
#define db2level(db)		pow(10, (double)db / 20.0)

#define SAMPLERATE		8000

static double test_frequency[8] = { 697.0, 770.0, 852.0, 941.0, 1209.0, 1336.0, 1477.0, 1633.0 };
static const char *test_digits = "*#0123456789ABCD";

static sample_t samples[SAMPLERATE];

/* generate samples with two tones */
static void generate_test_sample(double frequency1, double frequency2, double amplitude1, double amplitude2)
{
	int i;
	double value;

	for (i = 0; i < SAMPLERATE; i++) {
		value = cos(2.0 * M_PI * frequency1 / (double)SAMPLERATE * i) * amplitude1;
		value += cos(2.0 * M_PI * frequency2 / (double)SAMPLERATE * i) * amplitude2;
		samples[i] = value;
	}
}

static void check_level(sample_t *samples, const char *desc, double target, int when1, int when2)
{
	int i;
	double amplitude = 0.0, diff;

	for (i = when1; i < when2; i++) {
		amplitude += samples[i];
	}
	amplitude = amplitude / (when2 - when1);
	diff = fabs(amplitude - target);
	printf("%s: amplitude between %d and %d ms is %.4f / %.4f db (expected %.4f)\n", desc, when1 * 1000 / SAMPLERATE, when2 * 1000 / SAMPLERATE, amplitude, level2db(amplitude), level2db(target));
	if (diff < -0.1 || diff > 0.1)
		printf("**** ERROR: we expected a diff close to 0.0\n");
	else
		printf("OK!\n");
}

static char got_digit;

static void recv_digit(void __attribute__((unused)) *inst, char digit, dtmf_meas_t *meas)
{
	printf("decoded digit '%c'  frequency %.1f %.1f  amplitude %.1f %.1f dB\n", digit, meas->frequency_low, meas->frequency_high, level2db(meas->amplitude_low), level2db(meas->amplitude_high));
	got_digit = digit;
}

int main(void)
{
	dtmf_dec_t dtmf_dec;
	dtmf_enc_t dtmf_enc;
	sample_t frequency1[SAMPLERATE], frequency2[SAMPLERATE], amplitude1[SAMPLERATE], amplitude2[SAMPLERATE];
	int f, i;
	double target;

	fm_init(0);

	dtmf_decode_init(&dtmf_dec, NULL, recv_digit, SAMPLERATE, db2level(0), db2level(-30.0));

	for (f = 0; f < 8; f++) {
		printf("Testing filter with frequency %.0f Hz:\n", test_frequency[f]);
		generate_test_sample(test_frequency[f], 0.0, 1.0, 0.0);

		dtmf_decode_filter(&dtmf_dec, samples, SAMPLERATE, frequency1, frequency2, amplitude1, amplitude2);

		if (f == 0 || f == 3)
			target = sqrt(0.5);
		else if (f == 1 || f == 2)
			target = 1.0;
		else
			target = 0.0;
		check_level(amplitude1, "frequency level", target, 900 * SAMPLERATE / 1000, 1000 * SAMPLERATE / 1000);
		if (f == 4 || f == 7)
			target = sqrt(0.5);
		else if (f == 5 || f == 6)
			target = 1.0;
		else
			target = 0.0;
		check_level(amplitude2, "frequency level", target, 900 * SAMPLERATE / 1000, 1000 * SAMPLERATE / 1000);

		puts("");
	}

	dtmf_encode_init(&dtmf_enc, SAMPLERATE, 1.0);

	for (i = 0; i < 16; i++) {
		printf("Testing digit '%c' encoding and decoding:\n", test_digits[i]);
		memset(samples, 0, sizeof(samples[0]) * SAMPLERATE);
		dtmf_encode_set_tone(&dtmf_enc, test_digits[i], 1.0, 0.0);
		dtmf_encode(&dtmf_enc, samples + SAMPLERATE / 10, SAMPLERATE / 20);
		got_digit = 0;
		dtmf_decode(&dtmf_dec, samples, SAMPLERATE);
		if (got_digit == 0)
			printf("**** ERROR: we expected to decode digit '%c', but nothing was decoded\n", test_digits[i]);
		else if (got_digit != test_digits[i])
			printf("**** ERROR: we expected to decode digit '%c', but we decoded digit '%c'\n", test_digits[i], got_digit);
		else
			printf("OK!\n");
		puts("");
	}

	dtmf_decode_exit(&dtmf_dec);

	fm_exit();

	return 0;
}

