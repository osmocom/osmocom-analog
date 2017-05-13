#ifndef _FILTER_H
#define _FILTER_H

typedef struct iir_filter {
	int iter;
	double a0, a1, a2, b1, b2;
	double z1[64], z2[64];
} iir_filter_t;

void iir_lowpass_init(iir_filter_t *filter, double frequency, int samplerate, int iterations);
void iir_highpass_init(iir_filter_t *filter, double frequency, int samplerate, int iterations);
void iir_bandpass_init(iir_filter_t *filter, double frequency, int samplerate, int iterations);
void iir_notch_init(iir_filter_t *filter, double frequency, int samplerate, int iterations);
void iir_process(iir_filter_t *filter, sample_t *samples, int length);

#endif /* _FILTER_H */
