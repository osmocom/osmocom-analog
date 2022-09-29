
typedef struct dtmf_enc {
	int	samplerate;		/* samplerate */
	char	tone;			/* current tone to be played */
	int	on, off;		/* samples to turn on and afterwards off */
	int	pos;			/* sample counter for tone */
	int	max;			/* max number of samples for tone duration */
	double	phaseshift65536[2];	/* how much the phase of sine wave changes per sample */
	double	phase65536[2];		/* current phase */
	sample_t sine_low[65536];	/* sine tables at individual levels */
	sample_t sine_high[65536];
} dtmf_enc_t;

void dtmf_encode_init(dtmf_enc_t *dtmf, int samplerate, double dBm_level);
int dtmf_encode_set_tone(dtmf_enc_t *dtmf, char tone, double on_duration, double off_duration);
int dtmf_encode(dtmf_enc_t *dtmf, sample_t *samples, int length);

