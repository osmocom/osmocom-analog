#include "../libfilter/iir_filter.h"

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

/* refers to NMT specs, cnetz uses different emphasis cutoff */
#define CUT_OFF_EMPHASIS_DEFAULT 300.0
#define CUT_OFF_HIGHPASS_DEFAULT 300.0
#define CUT_OFF_LOWPASS_DEFAULT 3400.0

double timeconstant2cutoff(double time_constant_us);
int init_emphasis(emphasis_t *state, int samplerate, double cut_off, double cut_off_h, double cut_off_l);
void pre_emphasis(emphasis_t *state, sample_t *samples, int num);
void de_emphasis(emphasis_t *state, sample_t *samples, int num);
void dc_filter(emphasis_t *state, sample_t *samples, int num);

