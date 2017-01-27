#include "filter.h"

typedef struct samplerate {
	double factor;
	struct {
		filter_t lp;
		double in_index;
	} down;
	struct {
		filter_t lp;
		double in_index;
	} up;
} samplerate_t;

int init_samplerate(samplerate_t *state, double samplerate);
int samplerate_downsample(samplerate_t *state, sample_t *samples, int input_num);
int samplerate_upsample(samplerate_t *state, sample_t *input, int input_num, sample_t *output);
