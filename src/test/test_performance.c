#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>
#include "../common/sample.h"
#include "../libfilter/iir_filter.h"
#include "../common/fm_modulation.h"
#include "../common/debug.h"

struct timeval start_tv, tv;
double duration;
int tot_samples;

#define T_START() \
	gettimeofday(&start_tv, NULL); \
	tot_samples = 0; \
	while (1) {

#define T_STOP(what, samples) \
		gettimeofday(&tv, NULL); \
		duration = (double)tv.tv_sec + (double)tv.tv_usec / 1e6; \
		duration -= (double)start_tv.tv_sec + (double)start_tv.tv_usec / 1e6; \
		tot_samples += samples; \
		if (duration >= 0.5) \
			break; \
	} \
	printf("%s: %.3f mega samples/sec\n", what, (double)tot_samples / duration / 1e6); \


#define SAMPLES 1000
sample_t samples[SAMPLES], I[SAMPLES], Q[SAMPLES];
uint8_t power[SAMPLES];
float buff[SAMPLES * 2];
fm_mod_t mod;
fm_demod_t demod;
iir_filter_t lp;

int main(void)
{
	memset(power, 1, sizeof(power));
	fm_mod_init(&mod, 50000, 0, 0.333);
	T_START()
	fm_modulate_complex(&mod, samples, power, SAMPLES, buff);
	T_STOP("FM modulate", SAMPLES)

	fm_demod_init(&demod, 50000, 0, 10000.0);
	T_START()
	fm_demodulate_complex(&demod, samples, SAMPLES, buff, I, Q);
	T_STOP("FM demodulate", SAMPLES)

	iir_lowpass_init(&lp, 10000.0 / 2.0, 50000, 1);
	T_START()
	iir_process(&lp, samples, SAMPLES);
	T_STOP("low-pass filter (second order)", SAMPLES)

	iir_lowpass_init(&lp, 10000.0 / 2.0, 50000, 2);
	T_START()
	iir_process(&lp, samples, SAMPLES);
	T_STOP("low-pass filter (fourth order)", SAMPLES)

	iir_lowpass_init(&lp, 10000.0 / 2.0, 50000, 4);
	T_START()
	iir_process(&lp, samples, SAMPLES);
	T_STOP("low-pass filter (eigth order)", SAMPLES)

	return 0;
}

