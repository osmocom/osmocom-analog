typedef struct emphasis {
	struct {
		double last_value;
		double factor;
		double amp;
	} p;
	struct {
		double last_value;
		double factor;
		double amp;
	} d;
} emphasis_t;

int init_emphasis(emphasis_t *state, int samplerate);
void pre_emphasis(emphasis_t *state, int16_t *samples, int num);
void de_emphasis(emphasis_t *state, int16_t *samples, int num);

