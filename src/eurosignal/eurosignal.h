#include "../libfm/fm.h"
#include "../libmobile/sender.h"
#include "../libtimer/timer.h"

/* current state of transmitter */
enum euro_health_state {
	EURO_HEALTH_WORKING = 0,/*  */
	EURO_HEALTH_DEGRADED,	/*  */
	EURO_HEALTH_OUTOFORDER,	/*  */
};

/* current state of incoming call */
enum euro_call_state {
	EURO_CALL_NULL = 0,
	EURO_CALL_ANSWER,	/* answer the call */
	EURO_CALL_DEGRADED,	/* play announcement 'teilgestoert' */
	EURO_CALL_ACKNOWLEDGE,	/* play announcement 'eurosignal xxx', transmit ID */
	EURO_CALL_RELEASED,	/* caller hangs up, ID still not transmitted */
	EURO_CALL_UNASSIGNED,	/* play announcement 'kein anschluss' */
	EURO_CALL_OUTOFORDER,	/* play announcement 'gestoert' */
	EURO_CALL_BEEPING,	/* call towards MNCC that beeps */
};

struct eurosignal;

/* instance of incoming call */
typedef struct euro_call {
	struct euro_call	*next;
	struct eurosignal	*euro;
	int			callref;		/* call reference */
	char			station_id[7];		/* current station ID */
	int			page_count;		/* number of transmissions left */
	struct timer		timer;
	enum euro_call_state	state;			/* current state */
	int			announcement_count;	/* used to replay annoucements */
	int16_t			*announcement_spl;	/* current sample */
	int			announcement_size;	/* current size */
	int			announcement_index;	/* current sample index */
} euro_call_t;

/* instance of eurosignal transmitter/receiver */
typedef struct eurosignal {
	sender_t		sender;

	/* system info */
	int			tx; 			/* can transmit */
	int			rx;			/* can receive */
	int			repeat;			/* repetitions of ID transmission */
	int			degraded;		/* station is degraded */
	int			random;			/* random ID transmission */
	char 			random_id[7];		/* current ID */
	char			random_count;		/* number of transmissions left */
	uint32_t		scan_from;		/* scan ID from */
	uint32_t		scan_to;		/* scan ID to (scan if not 0) */

	/* calls */
	euro_call_t		*call_list;		/* linked list of all calls */

	/* display measurements */
	dispmeasparam_t		*dmp_tone_level;
	dispmeasparam_t		*dmp_tone_quality;

	/* dsp states */
	double			sample_duration;	/* how many seconds lasts a sample */
	double			tx_phaseshift65536;	/* current tone's phase shift per sample */
	double			tx_phase;		/* current phase of tone */
	double			tx_time;		/* current elapsed time of tone */
	char			tx_digits[7];		/* current ID beeing transmitted */
	int			tx_digit_index;		/* current digit beein transmitted */
	int			chunk_count;		/* current elapsed sample of 20ms audio chunk */
	fm_demod_t		rx_demod;		/* demodulator for frequency */
	iir_filter_t		rx_lp;			/* low pass to filter the frequency result */
	int			rx_digit_count;		/* count the tone until detected */
	char			rx_digit_last;		/* last tone, so we detect any change */
	int			rx_digit_receiving;	/* we recive digis */
	char			rx_digits[7];		/* current ID being received */
	int			rx_digit_index;		/* current digit receiving */
	int			rx_timeout_count;	/* count the timeout */
} euro_t;

void euro_add_id(const char *id);
double euro_kanal2freq(const char *kanal, int fm);
void euro_list_channels(void);
int euro_init(void);
void euro_exit(void);
int euro_create(const char *kanal, const char *audiodev, int use_sdr, int samplerate, double rx_gain, int fm, int tx, int rx, int repeat, int degraded, int random, uint32_t scan_from, uint32_t scan_to, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, const char *read_tx_wave, int loopback);
void euro_destroy(sender_t *sender);
void euro_get_id(euro_t *euro, char *id);
void euro_receive_id(euro_t *euro, char *id);
void euro_clock_chunk(sender_t *sender);

