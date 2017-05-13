#include "../common/iir_filter.h"

typedef struct scrambler {
	double		carrier_phaseshift65536;/* carrier phase shift per sample */
	double		carrier_phase65536;	/* current phase of carrier frequency */
	iir_filter_t	lp;			/* filter to remove carrier frequency */
} scrambler_t;

void scrambler_init(void);
void scrambler_setup(scrambler_t *scrambler, int samplerate);
void scrambler(scrambler_t *scrambler, sample_t *samples, int length);

