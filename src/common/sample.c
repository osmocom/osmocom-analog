
#include <stdint.h>
#include "sample.h" 

void samples_to_int16(int16_t *spl, sample_t *samples, int length)
{
	while (length--) {
		if (*samples > 32767.0)
			*spl = 32767;
		else if (*samples < -32767.0)
			*spl = -32767;
		else
			*spl = (uint16_t)(*samples);
		samples++;
		spl++;
	}
}

void int16_to_samples(sample_t *samples, int16_t *spl, int length)
{
	while (length--) {
		*samples = (double)(*spl);
		samples++;
		spl++;
	}
}

