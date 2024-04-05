
#include <stdint.h>
#include "tones.h"

static int16_t pattern[] = {
	0, 5320, 10063, 13716, 15883, 16328, 15004, 12054, 7798, 2697, -2697, -7798, -12054, -15004, -16328, -15883, -13716, -10063, -5320,
	//0, 2660, 5032, 6858, 7941, 8164, 7502, 6027, 3899, 1348, -1348, -3899, -6027, -7502, -8164, -7941, -6858, -5032, -2660,
};

static int16_t tone[7999];

extern int16_t *ringback_spl;
extern int ringback_size;
extern int ringback_max;
extern int16_t *busy_spl;
extern int busy_size;
extern int busy_max;
extern int16_t *congestion_spl;
extern int congestion_size;
extern int congestion_max;

void init_nmt_tones(void)
{
	int i, j;

	for (i = 0, j = 0; i < 7999; i++) {
		tone[i] = pattern[j++];
		if (j == 19)
			j = 0;
	}

	ringback_spl = tone;
	ringback_size = 7999;
	ringback_max = 8 * 5000; /* 1000 / 4000 */

	busy_spl = tone;
	busy_size = 1995;
	busy_max = 8 * 500; /* 250 / 250 */

	congestion_spl = tone;
	congestion_size = 1995;
	congestion_max = 8 * 500; /* 250 / 250 */
}

