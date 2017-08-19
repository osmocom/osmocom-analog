#include "../common/fm_modulation.h"

typedef struct ffsk {
	void		*inst;
	int (*send_bit)(void *inst);
	void (*receive_bit)(void *inst, int bit, double quality, double level);
	fm_demod_t	demod;
	double		bits_per_sample;	/* fraction of a bit per sample */
	double		*sin_tab;		/* sine table with correct peak level */
	double		phaseshift65536[2];	/* how much the phase of fsk synbol changes per sample */
	double		cycles_per_bit65536[2];	/* cacles of one bit */
	double		tx_phase65536;		/* current transmit phase */
	double		level;			/* level (amplitude) of signal */
	int		coherent;		/* set, if coherent TX mode */
	double		f0_deviation;		/* deviation of frequencies, relative to center */
	double		f1_deviation;
	int		low_bit, high_bit;	/* a low or high deviation means which bit? */
	int		tx_bit;			/* current transmitting bit (-1 if not set) */
	int		rx_bit;			/* current receiving bit (-1 if not yet measured) */
	double		tx_bitpos;		/* current transmit position in bit */
	double		rx_bitpos;		/* current receive position in bit (sampleclock) */
	double		rx_bitadjust;		/* how much does a bit change cause the sample clock to be adjusted in phase */
} fsk_t;

int fsk_init(fsk_t *fsk, void *inst, int (*send_bit)(void *inst), void (*receive_bit)(void *inst, int bit, double quality, double level), int samplerate, double bitrate, double f0, double f1, double level, int coherent, double bitadjust);
void fsk_cleanup(fsk_t *fsk);
void fsk_receive(fsk_t *fsk, sample_t *sample, int length);
int fsk_send(fsk_t *fsk, sample_t *sample, int length, int add);
void fsk_tx_reset(fsk_t *fsk);

