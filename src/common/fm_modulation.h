
typedef struct fm_mod {
	double samplerate;	/* sample rate of in and out */
	double offset;		/* offset to calculated center frequency */
	double amplitude;	/* how much amplitude to add to the buff */
	double phase;		/* current phase of FM (used to shift and modulate ) */
	double *sin_tab;	/* sine/cosine table for modulation */
} fm_mod_t;

void fm_mod_init(fm_mod_t *mod, double samplerate, double offset, double amplitude);
void fm_modulate(fm_mod_t *mod, sample_t *samples, int num, float *buff);

typedef struct fm_demod {
	double samplerate;	/* sample rate of in and out */
	double phase;		/* current rotation phase (used to shift) */
	double rot;		/* rotation step per sample to shift rx frequency (used to shift) */
	double last_phase;	/* last phase of FM (used to demodulate) */
	iir_filter_t lp[2];	/* filters received IQ signal */
	double *sin_tab;	/* sine/cosine table rotation */
} fm_demod_t;

void fm_demod_init(fm_demod_t *demod, double samplerate, double offset, double bandwidth);
void fm_demodulate(fm_demod_t *demod, sample_t *samples, int num, float *buff);

