#ifndef _FILTER_H
#define _FILTER_H

typedef struct biquad_low_pass {
	double a0, a1, a2, b1, b2;
	double z1[10], z2[10];
} biquad_low_pass_t;

void biquad_init(biquad_low_pass_t *bq, double frequency, int samplerate);
void biquad_process(biquad_low_pass_t *bq, double *samples, int length, int iterations);

#endif /* _FILTER_H */
