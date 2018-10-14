
typedef struct compandor {
	struct {
		double	step_up;
		double	step_down;
		double	peak;
		double	envelope;
	} c;
	struct {
		double	step_up;
		double	step_down;
		double	peak;
		double	envelope;
	} e;
} compandor_t;

void init_compandor(compandor_t *state, double samplerate, double attack_ms, double recovery_ms);
void compress_audio(compandor_t *state, sample_t *samples, int num);
void expand_audio(compandor_t *state, sample_t *samples, int num);

