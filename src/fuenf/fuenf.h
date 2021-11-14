#include "../libmobile/sender.h"
#include "../libgoertzel/goertzel.h"
#include "../libfm/fm.h"

enum fuenf_state {
	FUENF_STATE_IDLE = 0,
	FUENF_STATE_RUF,
	FUENF_STATE_DURCHSAGE,
};

extern const char *fuenf_funktion_name[8];

enum fuenf_funktion {
	FUENF_FUNKTION_RUF = 0,
	FUENF_FUNKTION_FEUER,
	FUENF_FUNKTION_PROBE,
	FUENF_FUNKTION_WARNUNG,
	FUENF_FUNKTION_ABC,
	FUENF_FUNKTION_ENTWARNUNG,
	FUENF_FUNKTION_KATASTROPHE,
	FUENF_FUNKTION_TURBO, /* used for turbo scanning, where only pause and 5 tones are sent */
};

enum rx_state {
	RX_STATE_RESET = 0,	/* wait for silence (after init, double tone or error) */
	RX_STATE_IDLE,		/* receive silence, wait for digit */
	RX_STATE_DIGIT,		/* wait for end of digit (next digit or silence) */
	RX_STATE_WAIT_SIGNAL,	/* wait for double tone (up to 6 sec) */
	RX_STATE_SIGNAL,	/* receive double tone and wait for minimum length (2 sec) */
};

/* definition for tone sequence */
typedef struct tone_seq {
	double	phasestep1, phasestep2;
	double	duration;
} tone_seq_t;

#define DSP_NUM_DIGITS 11
#define DSP_NUM_TONES 4

/* instance of pocsag transmitter/receiver */
typedef struct fuenf {
	sender_t		sender;

	/* system info */
	int			tx; 			/* can transmit */
	int			rx;			/* can receive */
	enum fuenf_funktion	default_funktion;	/* default function, if not given by caller */

	/* tx states */
	enum fuenf_state	state;			/* state (idle, preamble, message) */
	uint32_t		scan_from, scan_to;	/* if not equal: scnning mode */

	/* rx states */

	/* calls */
	int			callref;

	/* TX dsp states */
	double			sample_duration;	/* length between samples in seconds */
	enum fuenf_funktion	tx_funktion;
	tone_seq_t		tx_seq[64];		/* transmit tone sequence */
	int			tx_seq_length;		/* size of current tone sequence */
	int			tx_seq_index;		/* current tone that is played */
	double			tx_count;		/* counts duration of current tone */
	double			tx_phase1, tx_phase2;	/* current phase of tone */

	/* display measurements */
	dispmeasparam_t		*dmp_digit_level;
	dispmeasparam_t		*dmp_tone_levels[DSP_NUM_TONES];

	/* RX dsp states */
	enum rx_state		rx_state;		/* current state of decoder */
	fm_demod_t		rx_digit_demod;		/* demodulator for frequency */
	iir_filter_t		rx_digit_lp;		/* low pass to filter the frequency result */
	int			rx_digit_last;		/* track if digit changes */
	int			rx_digit_count;		/* count samples after digit changes */
	goertzel_t		rx_tone_goertzel[DSP_NUM_TONES]; /* rx filter */
	sample_t		*rx_tone_filter_spl;	/* buffer for rx filter */
	int			rx_tone_filter_size;	/* length of buffer, will affect bandwidth of filter */
	int			rx_tone_filter_pos;	/* samples in buffer */
	double			rx_tone_levels[DSP_NUM_TONES]; /* last detected levels */
	char			rx_callsign[6];		/* 5 digits + '\0' */
	int			rx_callsign_count;	/* number of (complete) digits received */
	enum fuenf_funktion	rx_function;		/* received signal */
	int			rx_function_count;	/* counts duration in samples */
} fuenf_t;

void bos_list_channels(void);
double bos_kanal2freq(const char *kanal);
const char *bos_freq2kanal(const char *freq);
const char *bos_number_valid(const char *number);
int fuenf_init(void);
void fuenf_exit(void);
int fuenf_create(const char *kanal, double frequency, const char *device, int use_sdr, int samplerate, double rx_gain, double tx_gain, int tx, int rx, double max_deviation, double signal_deviation, enum fuenf_funktion funktion, uint32_t scan_from, uint32_t scan_to, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, const char *read_tx_wave, int loopback);
void fuenf_destroy(sender_t *sender);
void fuenf_tx_done(fuenf_t *fuenf);
void fuenf_rx_callsign(fuenf_t *fuenf, const char *callsign);
void fuenf_rx_function(fuenf_t *fuenf, enum fuenf_funktion funktion);

