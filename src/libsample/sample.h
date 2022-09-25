
typedef double sample_t;

#define	SPEECH_LEVEL	0.1585

void samples_to_int16_speech(int16_t *spl, sample_t *samples, int length);
void int16_to_samples_speech(sample_t *samples, int16_t *spl, int length);
void samples_to_int16_1mw(int16_t *spl, sample_t *samples, int length);
void int16_to_samples_1mw(sample_t *samples, int16_t *spl, int length);

