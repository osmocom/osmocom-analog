#include "../libfilter/iir_filter.h"
#include "../libfilter/fir_filter.h"

typedef struct psk_mod {
	int		(*send_bit)(void *inst);
	void		*inst;

	double		symbol_pos;		/* current position in symbol */
	double		symbols_per_sample;	/* change of position per sample */
	double		phase_shift;		/* carrier phase shift */
	double		carrier_phase;		/* current carrier phase */
	double		carrier_phaseshift;	/* shift of phase per sample */

	fir_filter_t	*lp[2];			/* filter for limiting spectrum */

	int		spl_count;		/* SIT: counter for 30 samples (symbol duration) */
	int		sym_list[5];		/* SIT: list of 5 symbols */
	int		sym_count;		/* SIT: current list index */
} psk_mod_t;

typedef struct psk_demod {
	void		(*receive_bit)(void *inst, int bit);
	void		*inst;

	double		carrier_phase;		/* current carrier phase */
	double		carrier_phaseshift;	/* shift of phase per sample */

	fir_filter_t	*lp[2];			/* filter for limiting spectrum */
	iir_filter_t	lp_error[2];		/* filter for phase correction */
	iir_filter_t	lp_clock;		/* filter for symbol clock */

	uint16_t	last_phase_error;	/* error phase of last sample */
	int32_t		phase_error;		/* current phase error */

	sample_t	last_amplitude;		/* clock amplitude of last sample */
	int		sample_delay;		/* delay of quarter symbol length in samples */
	int		sample_timer;		/* counter to wait for the symbol's sample point */

	uint8_t		last_sector;		/* sector of last symbol */
} psk_demod_t;

int psk_mod_init(psk_mod_t *psk, void *inst, int (*send_bit)(void *inst), int samplerate, double symbolrate);
void psk_mod_exit(psk_mod_t *psk);
void psk_mod(psk_mod_t *psk, sample_t *sample, int length);
int psk_demod_init(psk_demod_t *psk, void *inst, void (*receive_bit)(void *inst, int bit), int samplerate, double symbolrate);
void psk_demod_exit(psk_demod_t *psk);
void psk_demod(psk_demod_t *psk, sample_t *sample, int length);

