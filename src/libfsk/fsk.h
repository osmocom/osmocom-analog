#include "../libfm/fm.h"

typedef struct fsk_mod {
	void		*inst;
	int (*send_bit)(void *inst);
	double		bits_per_sample;	/* fraction of a bit per sample */
	double		*sin_tab;		/* sine table with correct peak level */
	double		phaseshift65536[2];	/* how much the phase of fsk synbol changes per sample */
	double		cycles_per_bit65536[2];	/* cycles of one bit */
	double		tx_phase65536;		/* current transmit phase */
	double		level;			/* level (amplitude) of signal */
	int		ffsk;			/* set, if FFSK TX mode */
	double		f0_deviation;		/* deviation of frequencies, relative to center */
	double		f1_deviation;
	int		low_bit, high_bit;	/* a low or high deviation means which bit? */
	int		tx_bit;			/* current transmitting bit (-1 if not set) */
	double		tx_bitpos;		/* current transmit position in bit */
	int		filter;			/* set, if filters are used */
	iir_filter_t	lp[2];			/* filter to smooth transmission spectrum */
} fsk_mod_t;

typedef struct fsk_demod {
	void		*inst;
	void (*receive_bit)(void *inst, int bit, double quality, double level);
	fm_demod_t	demod;
	double		bits_per_sample;	/* fraction of a bit per sample */
	double		f0_deviation;		/* deviation of frequencies, relative to center */
	double		f1_deviation;
	int		low_bit, high_bit;	/* a low or high deviation means which bit? */
	int		rx_bit;			/* current receiving bit (-1 if not yet measured) */
	double		rx_bitpos;		/* current receive position in bit (sampleclock) */
	double		rx_bitadjust;		/* how much does a bit change cause the sample clock to be adjusted in phase */
	int		rx_change;		/* set, if we have a level change before sampling the bit */
} fsk_demod_t;

int fsk_mod_init(fsk_mod_t *fsk, void *inst, int (*send_bit)(void *inst), int samplerate, double bitrate, double f0, double f1, double level, int coherent, int filter);
void fsk_mod_cleanup(fsk_mod_t *fsk);
int fsk_mod_send(fsk_mod_t *fsk, sample_t *sample, int length, int add);
void fsk_mod_tx_reset(fsk_mod_t *fsk);
int fsk_demod_init(fsk_demod_t *fsk, void *inst, void (*receive_bit)(void *inst, int bit, double quality, double level), int samplerate, double bitrate, double f0, double f1, double bitadjust);
void fsk_demod_cleanup(fsk_demod_t *fsk);
void fsk_demod_receive(fsk_demod_t *fsk, sample_t *sample, int length);

