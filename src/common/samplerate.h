#include "iir_filter.h"

typedef struct samplerate {
	double factor;
	struct {
		iir_filter_t lp;
		sample_t last_sample;
		double in_index;
	} down;
	struct {
		iir_filter_t lp;
		sample_t last_sample;
		double in_index;
	} up;
} samplerate_t;

int init_samplerate(samplerate_t *state, double low_samplerate, double high_samplerate, double filter_cutoff);
int samplerate_downsample(samplerate_t *state, sample_t *samples, int input_num);
int samplerate_upsample(samplerate_t *state, sample_t *input, int input_num, sample_t *output);
