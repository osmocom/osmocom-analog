typedef struct compandor {
	struct {
		double	unaffected;
		double	step_up;
		double	step_down;
		double	peak;
		double	envelope;
	} c;
	struct {
		double	unaffected;
		double	step_up;
		double	step_down;
		double	peak;
		double	envelope;
	} e;
} compandor_t;

void init_compandor(compandor_t *state, int samplerate, double attack_ms, double recovery_ms, int unaffected_level);
void compress_audio(compandor_t *state, int16_t *samples, int num);
void expand_audio(compandor_t *state, int16_t *samples, int num);

