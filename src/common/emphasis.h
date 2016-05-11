typedef struct emphasis {
	struct {
		double x_last;
		double factor;
		double amp;
	} p;
	struct {
		double y_last;
		double z_last;
		double d_factor;
		double h_factor;
		double amp;
	} d;
} emphasis_t;

int init_emphasis(emphasis_t *state, int samplerate);
void pre_emphasis(emphasis_t *state, int16_t *samples, int num);
void de_emphasis(emphasis_t *state, int16_t *samples, int num);

