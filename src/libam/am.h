#include "../libfilter/iir_filter.h"

int am_init(int fast_math);
void am_exit(void);

typedef struct am_mod {
	double	rot;		/* angle to rotate vector per sample */
	double	phase;		/* current phase */
	double	gain;		/* gain to be multiplied to amplitude */
	double	bias;		/* DC offset to add (carrier amplitude) */
} am_mod_t;

int am_mod_init(am_mod_t *mod, double samplerate, double offset, double gain, double bias);
void am_mod_exit(am_mod_t *mod);
void am_modulate_complex(am_mod_t *mod, sample_t *amplitude, int num, float *baseband);

typedef struct am_demod {
	double	rot;		/* angle to rotate vector per sample */
	double	phase;		/* current rotation phase (used to shift) */
	iir_filter_t lp[3];	/* filters received IQ signal/carrier */
	double	gain;		/* gain to be expected from amplitude */
	double	bias;		/* DC offset to be expected (carrier amplitude) */
} am_demod_t;

int am_demod_init(am_demod_t *demod, double samplerate, double offset, double gain, double bias);
void am_demod_exit(am_demod_t *demod);
void am_demodulate_complex(am_demod_t *demod, sample_t *amplitude, int length, float *baseband, sample_t *I, sample_t *Q, sample_t *carrier);

