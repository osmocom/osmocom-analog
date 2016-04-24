#include "filter.h"

typedef struct samplerate {
	double factor;
	struct {
		biquad_low_pass_t bq;
		double in_index;
	} down;
	struct {
		biquad_low_pass_t bq;
		double in_index;
	} up;
} samplerate_t;

int init_samplerate(samplerate_t *state, double samplerate);
int samplerate_downsample(samplerate_t *state, int16_t *input, int input_num, int16_t *output);
int samplerate_upsample(samplerate_t *state, int16_t *input, int input_num, int16_t *output);
