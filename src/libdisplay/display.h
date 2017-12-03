#define DISPLAY_INTERVAL	0.04	/* time (in seconds) for each interval */
#define DISPLAY_PARAM_HISTORIES	25	/* number of intervals (should result in one seconds) */

#define MAX_DISPLAY_WIDTH 1024

typedef struct sender sender_t;

typedef struct display_wave {
	int	interval_pos;
	int	interval_max;
	int	offset;
	sample_t buffer[MAX_DISPLAY_WIDTH];
} dispwav_t;

enum display_measurements_type {
	DISPLAY_MEAS_LAST,	/* display last value */
	DISPLAY_MEAS_PEAK,	/* display peak value */
	DISPLAY_MEAS_PEAK2PEAK,	/* display peak value of min..max range */
	DISPLAY_MEAS_AVG,	/* display average value */
};

enum display_measurements_bar {
	DISPLAY_MEAS_LEFT,	/* bar graph from left */
	DISPLAY_MEAS_CENTER,	/* bar graph from center */
};

typedef struct display_measurements_param {
	struct display_measurements_param *next;
	char	name[32];	/* parameter name (e.g. 'Deviation') */
	char	format[32];	/* unit name (e.g. "%.2f KHz") */
	enum display_measurements_type type;
	enum display_measurements_bar bar;
	double	min;		/* minimum value */
	double	max;		/* maximum value */
	double	mark;		/* mark (target) value */
	double	value;		/* current value (peak, sum...) */
	double	value2;		/* max value for min..max range */
	double	last;		/* last valid value (used for DISPLAY_MEAS_LAST) */
	int	value_count;	/* count number of values of one interval */
	double	value_history[DISPLAY_PARAM_HISTORIES]; /* history of values of last second */
	double	value2_history[DISPLAY_PARAM_HISTORIES]; /* stores max for min..max range */
	int	value_history_pos; /* next history value to write */
} dispmeasparam_t;

typedef struct display_measurements {
	dispmeasparam_t *head;
} dispmeas_t;

#define MAX_DISPLAY_IQ 1024

typedef struct display_iq {
	int	interval_pos;
	int	interval_max;
	float	buffer[MAX_DISPLAY_IQ * 2];
} dispiq_t;

#define MAX_DISPLAY_SPECTRUM 1024

typedef struct display_spectrum {
	int	interval_pos;
	int	interval_max;
	double	buffer_I[MAX_DISPLAY_SPECTRUM];
	double	buffer_Q[MAX_DISPLAY_SPECTRUM];
} dispspectrum_t;

#define MAX_HEIGHT_STATUS 32

void get_win_size(int *w, int *h);

void display_wave_init(sender_t *sender, int samplerate);
void display_wave_on(int on);
void display_wave(sender_t *sender, sample_t *samples, int length, double range);

void display_status_on(int on);
void display_status_start(void);
void display_status_channel(int channel, const char *type, const char *state);
void display_status_subscriber(const char *number, const char *state);
void display_status_end(void);

void display_measurements_init(sender_t *sender, int samplerate);
void display_measurements_exit(sender_t *sender);
void display_measurements_on(int on);
dispmeasparam_t *display_measurements_add(sender_t *sender, char *name, char *format, enum display_measurements_type type, enum display_measurements_bar bar, double min, double max, double mark);
void display_measurements_update(dispmeasparam_t *param, double value, double value2);
void display_measurements(double elapsed);

void display_iq_init(int samplerate);
void display_iq_on(int on);
void display_iq(float *samples, int length);

void display_spectrum_init(int samplerate, double center_frequency);
void display_spectrum_on(int on);
void display_spectrum(float *samples, int length);

