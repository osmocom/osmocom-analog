typedef struct compander {
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
} compander_t;

void init_compander(compander_t *state, int samplerate, double attack_ms, double recovery_ms, int unaffected_level);
void compress_audio(compander_t *state, int16_t *samples, int num);
void expand_audio(compander_t *state, int16_t *samples, int num);

