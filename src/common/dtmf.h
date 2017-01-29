
typedef struct dtmf {
	int	samplerate;		/* samplerate */
	char	tone;			/* current tone to be played */
	int	pos;			/* sample counter for tone */
	int	max;			/* max number of samples for tone duration */
	double	phaseshift65536[2];	/* how much the phase of sine wave changes per sample */
	double	phase65536[2];		/* current phase */
} dtmf_t;

void dtmf_init(dtmf_t *dtmf, int samplerate);
void dtmf_set_tone(dtmf_t *dtmf, char tone);
void dtmf_tone(dtmf_t *dtmf, sample_t *samples, int length);

