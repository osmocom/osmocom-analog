#include "../libfm/fm.h"

typedef struct ftmf_meas {
	double		frequency_low;
	double		frequency_high;
	double		amplitude_low;
	double		amplitude_high;
	int		count;
} dtmf_meas_t;

typedef struct dtmf_dec {
	void		*priv;
	void		(*recv_digit)(void *priv, char digit, dtmf_meas_t *meas);
	int		samplerate;		/* samplerate */
	double		freq_margin;		/* +- limit of frequency deviation (percent) valid tone*/
	double		min_amplitude;		/* minimum amplitude relative to 0 dBm */
	double		max_amplitude;		/* maximum amplitude relative to 0 dBm */
	double		forward_twist;		/* how much do higher frequencies are louder than lower frequencies */
	double		reverse_twist;		/* how much do lower frequencies are louder than higher frequencies */
	int		time_detect;
	int		time_meas;
	int		time_pause;
	fm_demod_t	demod_low;		/* demodulator for low frequencies */
	fm_demod_t	demod_high;		/* demodulator for high frequencies */
	iir_filter_t	freq_lp[2];		/* low pass to filter the frequency result */
	char		detected;		/* currently detected DTMF digit or 0 for no detection */
	int		count;			/* counter to count detection or loss (pause) of signal */
	dtmf_meas_t	meas;			/* measurements */
} dtmf_dec_t;

int dtmf_decode_init(dtmf_dec_t *dtmf, void *priv, void (*recv_digit)(void *priv, char digit, dtmf_meas_t *meas), int samplerate, double max_amplitude, double min_amplitude);
void dtmf_decode_exit(dtmf_dec_t *dtmf);
void dtmf_decode_reset(dtmf_dec_t *dtmf);
void dtmf_decode(dtmf_dec_t *dtmf, sample_t *samples, int length);
void dtmf_decode_filter(dtmf_dec_t *dtmf, sample_t *samples, int length, sample_t *frequency_low, sample_t *frequency_high, sample_t *amplitude_low, sample_t *amplitude_high);

