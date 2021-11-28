
double audio_mean_level(sample_t *samples, int length);

typedef struct goertzel {
	double coeff;
} goertzel_t;

void audio_goertzel_init(goertzel_t *goertzel, double freq, int samplerate);
void audio_goertzel(goertzel_t *goertzel, sample_t *samples, int length, int offset, double *result, int k);

