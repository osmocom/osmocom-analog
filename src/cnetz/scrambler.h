#include "../common/filter.h"

typedef struct scrambler {
	double			carrier_phaseshift256;	/* carrier phase shift per sample */
	double			carrier_phase256;	/* current phase of carrier frequency */
	biquad_low_pass_t	bq;			/* filter to remove carrier frequency */
} scrambler_t;

void scrambler_init(void);
void scrambler_setup(scrambler_t *scrambler, int samplerate);
void scrambler(scrambler_t *scrambler, int16_t *samples, int length);

