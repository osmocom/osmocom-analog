typedef struct compander {
	double	step_up;
	double	step_down;
	double	envelop_c;
	double	envelop_e;
} compander_t;

void init_compander(compander_t *state, int samplerate, double attack_ms, double recovery_ms);
void compress_audio(compander_t *state, int16_t *samples, int num);
void expand_audio(compander_t *state, int16_t *samples, int num);

