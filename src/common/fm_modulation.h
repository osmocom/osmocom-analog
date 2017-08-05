#include "../common/iir_filter.h"

typedef struct fm_mod {
	double samplerate;	/* sample rate of in and out */
	double offset;		/* offset to calculated center frequency */
	double amplitude;	/* how much amplitude to add to the buff */
	double phase;		/* current phase of FM (used to shift and modulate ) */
	double *sin_tab;	/* sine/cosine table for modulation */
} fm_mod_t;

int fm_mod_init(fm_mod_t *mod, double samplerate, double offset, double amplitude);
void fm_mod_exit(fm_mod_t *mod);
void fm_modulate_complex(fm_mod_t *mod, sample_t *frequency, int num, float *baseband);

typedef struct fm_demod {
	double samplerate;	/* sample rate of in and out */
	double phase;		/* current rotation phase (used to shift) */
	double rot;		/* rotation step per sample to shift rx frequency (used to shift) */
	double last_phase;	/* last phase of FM (used to demodulate) */
	iir_filter_t lp[2];	/* filters received IQ signal */
	double *sin_tab;	/* sine/cosine table rotation */
} fm_demod_t;

int fm_demod_init(fm_demod_t *demod, double samplerate, double offset, double bandwidth);
void fm_demod_exit(fm_demod_t *demod);
void fm_demodulate_complex(fm_demod_t *demod, sample_t *frequency, int length, float *baseband, sample_t *I, sample_t *Q);
void fm_demodulate_real(fm_demod_t *demod, sample_t *frequency, int length, sample_t *baseband, sample_t *I, sample_t *Q);

