#ifndef _FILTER_H
#define _FILTER_H

typedef struct filter_lowpass {
	double a0, a1, a2, b1, b2;
	double z1[10], z2[10];
} filter_lowpass_t;

void filter_lowpass_init(filter_lowpass_t *bq, double frequency, int samplerate);
void filter_lowpass_process(filter_lowpass_t *bq, double *samples, int length, int iterations);

#endif /* _FILTER_H */
