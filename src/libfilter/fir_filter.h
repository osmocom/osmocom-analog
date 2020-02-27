#ifndef _FIR_FILTER_H
#define _FIR_FILTER_H

typedef struct fir_filter {
	int	ntaps;
	int	delay;
	double	*taps;
	double	*buffer;
	int	buffer_pos;
} fir_filter_t;

fir_filter_t *fir_lowpass_init(double samplerate, double cutoff, double transition_bandwidth);
fir_filter_t *fir_highpass_init(double samplerate, double cutoff, double transition_bandwidth);
fir_filter_t *fir_allpass_init(double samplerate, double transition_bandwidth);
fir_filter_t *fir_twopass_init(double samplerate, double cutoff_low, double cutoff_high, double transition_bandwidth);
void fir_exit(fir_filter_t *fir);
void fir_process(fir_filter_t *fir, sample_t *samples, int num);
int fir_get_delay(fir_filter_t *fir);

#endif /* _FIR_FILTER_H */

