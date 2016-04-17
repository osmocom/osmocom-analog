#include "filter.h"

typedef struct samplerate {
	double factor;
	struct {
		double sum;
		double sum_count;
		biquad_low_pass_t bq;
	} down;
	struct {
		double last_sample;
		biquad_low_pass_t bq;
	} up;
} samplerate_t;

int init_samplerate(samplerate_t *state, double samplerate);
int samplerate_downsample(samplerate_t *state, int16_t *input, int input_num, int16_t *output);
int samplerate_upsample(samplerate_t *state, int16_t *input, int input_num, int16_t *output);
