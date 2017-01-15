#ifndef _FILTER_H
#define _FILTER_H

typedef struct filter {
	int iter;
	double a0, a1, a2, b1, b2;
	double z1[64], z2[64];
} filter_t;

void filter_lowpass_init(filter_t *bq, double frequency, int samplerate, int iterations);
void filter_highpass_init(filter_t *bq, double frequency, int samplerate, int iterations);
void filter_process(filter_t *bq, double *samples, int length);

#endif /* _FILTER_H */
