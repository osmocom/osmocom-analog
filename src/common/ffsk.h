#include "../common/goertzel.h"

typedef struct ffsk {
	void			*inst;
	void (*receive_bit)(void *inst, int bit, double quality, double level);
	int			channel;		/* channel number */
	int			samplerate;		/* current sample rate */
	double			samples_per_bit;	/* number of samples for one bit (1200 Baud) */
	double			bits_per_sample;	/* fraction of a bit per sample */
	goertzel_t		goertzel[2];		/* filter for fsk decoding */
	int			polarity;		/* current polarity state of bit */
	sample_t		*filter_spl;		/* array to hold ring buffer for bit decoding */
	int			filter_size;		/* size of ring buffer */
	int			filter_pos;		/* position to write next sample */
	double			filter_step;		/* counts bit duration, to trigger decoding every 10th bit */
	int			filter_bit;		/* last bit state, so we detect a bit change */
	int			filter_sample;		/* count until it is time to sample bit */
	double			phaseshift65536;	/* how much the phase of fsk synbol changes per sample */
	double			phase65536;		/* current phase */
} ffsk_t;

void ffsk_global_init(double peak_fsk);
int ffsk_init(ffsk_t *ffsk, void *inst, void (*receive_bit)(void *inst, int bit, double quality, double level), int channel, int samplerate);
void ffsk_cleanup(ffsk_t *ffsk);
void ffsk_receive(ffsk_t *ffsk, sample_t *sample, int lenght);
int ffsk_render_frame(ffsk_t *ffsk, const char *frame, int length, sample_t *sample);

