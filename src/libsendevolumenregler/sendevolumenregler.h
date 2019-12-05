
typedef struct sendevolumenregler {
	double	peak;
	double	envelope;
	double	step_up;
	double	step_down;
	double	minimum_level;
	double	maximum_level;
	double	db0_level;
} sendevolumenregler_t;

void init_sendevolumenregler(sendevolumenregler_t *state, double samplerate, double abwaerts_dbs, double aufwaerts_dbs, double maximum_db, double minimum_db, double db0_level);
void sendevolumenregler(sendevolumenregler_t *state, sample_t *samples, int num);

