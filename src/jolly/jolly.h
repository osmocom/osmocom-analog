#include "../libsquelch/squelch.h"
#include "../libmobile/sender.h"
#include "../libdtmf/dtmf_decode.h"

enum jolly_state {
	STATE_NULL = 0,
	STATE_IDLE,
	STATE_OUT_DIALING,
	STATE_OUT_VERIFY,
	STATE_CALL,
	STATE_CALL_DIALING,
	STATE_IN_PAGING,
	STATE_RELEASED,
};

typedef struct jolly {
	sender_t		sender;

	/* sender's states */
	enum jolly_state	state;			/* current sender's state */
	int			callref;		/* call reference */
	char			station_id[32];		/* current station ID */
	char			dialing[32];		/* dial string */
	struct osmo_timer_list		timer;

	/* display measurements */
	dispmeasparam_t		*dmp_dtmf_low;
	dispmeasparam_t		*dmp_dtmf_high;

	/* dsp states */
	int			repeater;		/* mix audio of RX signal to TX signal */
	jitter_t		repeater_dejitter;	/* forwarding audio */
	uint16_t		repeater_sequence;	/* sequence & ts for jitter buffer */
	uint32_t		repeater_timestamp;
	int			repeater_count;		/* counter to count down repeater's "transmitter on" */
	int			repeater_max;		/* duration in samples */
	squelch_t		squelch;		/* squelch detection process */
	int			is_mute;		/* set if quelch has muted */
	dtmf_dec_t		dtmf;			/* dtmf decoder */
	double			dt_phaseshift65536[2];	/* how much the phase of sine wave changes per sample */
	double			dt_phase65536[2];	/* current phase */
	double			ack_phaseshift65536;	/* how much the phase of sine wave changes per sample */
	double			ack_phase65536;		/* current phase */
	int			ack_count;		/* counter to count down while playing ack tone */
	int			ack_max;		/* duration in samples */
	struct osmo_timer_list		speech_timer;
	char			speech_string[40];	/* speech string */
	int			speech_digit;		/* counts digits */
	int			speech_pos;		/* counts samples */
	sample_t		*delay_spl;		/* delay buffer for delaying audio */
	int			delay_pos;		/* position in delay buffer */
	int			delay_max;		/* number of samples in delay buffer */
} jolly_t;

int jolly_create(const char *kanal, double dl_freq, double ul_freq, double step, const char *device, int use_sdr, int samplerate, double rx_gain, double tx_gain, int pre_emphasis, int de_emphasis, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, const char *read_tx_wave, int loopback, double squelch_db, int nbfm, int repeater);
void jolly_destroy(sender_t *sender);
void speech_finished(jolly_t *jolly);
void jolly_receive_dtmf(void *priv, char digit, dtmf_meas_t *meas);
