
typedef struct dtmf_enc {
	int	samplerate;		/* samplerate */
	char	tone;			/* current tone to be played */
	int	pos;			/* sample counter for tone */
	int	max;			/* max number of samples for tone duration */
	double	phaseshift65536[2];	/* how much the phase of sine wave changes per sample */
	double	phase65536[2];		/* current phase */
} dtmf_enc_t;

void dtmf_encode_init(dtmf_enc_t *dtmf, int samplerate, double dBm_level);
void dtmf_encode_set_tone(dtmf_enc_t *dtmf, char tone);
void dtmf_encode(dtmf_enc_t *dtmf, sample_t *samples, int length);

