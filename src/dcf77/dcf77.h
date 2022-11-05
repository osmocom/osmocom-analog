
#include "../libsample/sample.h"
#include "../libfilter/iir_filter.h"
#include "../libdisplay/display.h"
#include <time.h>

typedef struct dcf77_tx {
	int enable;
	double phase_360;
	double carrier_phase, carrier_phase_step; /* uncorrected phase */
	double test_phase, test_phase_step;
	double level;
	int wave, waves_0, waves_1, waves_sec;
	time_t timestamp;
	int second;
	char symbol;
	uint64_t data_frame;
	char data_string[100]; /* 60 digits + spaces + '\0' */
	int test_tone;
	int weather;
	int weather_day;
	int weather_night;
	int extreme;
	int rain;
	int wind_dir;
	int wind_bft;
	int temperature_day;
	int temperature_night;
	uint64_t weather_cipher;
} dcf77_tx_t;

typedef struct dcf77_rx {
	int enable;
	double phase_360;
	double carrier_phase, carrier_phase_step; /* uncorrected phase */
	iir_filter_t carrier_lp[2]; /* filters received carrier signal */
	double sample_counter, sample_step; /* when to sample */
	double *delay_buffer;
	int delay_size, delay_index;
	int clock_count;
	double value_level, value_short, value_long; /* measured values */
	int data_receive, data_index;
	char data_string[100]; /* 60 digits + spaces + '\0' */
	int string_index;
	uint64_t data_frame;
	int weather_index;
	uint64_t weather_cipher;
	uint64_t weather_key;
} dcf77_rx_t;

typedef struct dcf77 {
	dcf77_tx_t tx;
	dcf77_rx_t rx;

	/* measurements */
	dispmeas_t dispmeas; /* display measurements */
	dispmeasparam_t *dmp_input_level;
	dispmeasparam_t *dmp_signal_level;
	dispmeasparam_t *dmp_signal_quality;
	dispmeasparam_t *dmp_current_second;

	/* wave */
	dispwav_t dispwav; /* display wave form */
} dcf77_t;

int dcf77_init(int _fast_math);
void dcf77_exit(void);
dcf77_t *dcf77_create(int samplerate, int use_tx, int use_rx, int test_tone);
void dcf77_destroy(dcf77_t *dcf77);
void dcf77_tx_start(dcf77_t *dcf77, time_t timestamp, double sub_sec);
void dcf77_encode(dcf77_t *dcf77, sample_t *samples, int length);
void dcf77_decode(dcf77_t *dcf77, sample_t *samples, int length);

void list_weather(void);
time_t dcf77_start_weather(time_t timestamp, int region, int offset);
void dcf77_set_weather(dcf77_t *dcf77, int weather_day, int weather_night, int extreme, int rain, int wind_dir, int wind_bft, int temperature_day, int temperature_night);
