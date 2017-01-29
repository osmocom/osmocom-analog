
typedef double sample_t;

#define	SPEECH_LEVEL	0.1585

void samples_to_int16(int16_t *spl, sample_t *samples, int length);
void int16_to_samples(sample_t *samples, int16_t *spl, int length);

