#define DISPLAY_INTERVAL 0.04

#define MAX_DISPLAY_WIDTH 1024

typedef struct sender sender_t;

typedef struct display_wave {
	int	interval_pos;
	int	interval_max;
	int	offset;
	sample_t buffer[MAX_DISPLAY_WIDTH];
} dispwav_t;

#define MAX_DISPLAY_IQ 256

typedef struct display_iq {
	int	interval_pos;
	int	interval_max;
	float	buffer[MAX_DISPLAY_IQ * 2];
} dispiq_t;

#define MAX_DISPLAY_SPECTRUM 256

typedef struct display_spectrum {
	int	interval_pos;
	int	interval_max;
	double	buffer_I[MAX_DISPLAY_SPECTRUM];
	double	buffer_Q[MAX_DISPLAY_SPECTRUM];
} dispspectrum_t;

void get_win_size(int *w, int *h);

void display_wave_init(sender_t *sender, int samplerate);
void display_wave_on(int on);
void display_wave_limit_scroll(int on);
void display_wave(sender_t *sender, sample_t *samples, int length, double range);

void display_iq_init(int samplerate);
void display_iq_on(int on);
void display_iq_limit_scroll(int on);
void display_iq(float *samples, int length);

void display_spectrum_init(int samplerate);
void display_spectrum_on(int on);
void display_spectrum_limit_scroll(int on);
void display_spectrum(float *samples, int length);
