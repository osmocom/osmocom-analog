typedef struct emphasis {
	struct {
		iir_filter_t lp;
		double x_last;
		double factor;
		double amp;
	} p;
	struct {
		iir_filter_t hp;
		double y_last;
		double factor;
		double amp;
	} d;
} emphasis_t;

#define CUT_OFF_EMPHASIS_DEFAULT 300.0

int init_emphasis(emphasis_t *state, int samplerate, double cut_off);
void pre_emphasis(emphasis_t *state, sample_t *samples, int num);
void de_emphasis(emphasis_t *state, sample_t *samples, int num);
void dc_filter(emphasis_t *state, sample_t *samples, int num);

