#include "../libmobile/sender.h"

enum pocsag_function {
	POCSAG_FUNCTION_NUMERIC = 0,
	POCSAG_FUNCTION_BEEP1,
	POCSAG_FUNCTION_BEEP2,
	POCSAG_FUNCTION_ALPHA,
};

extern const char *pocsag_function_name[4];

enum pocsag_state {
	POCSAG_IDLE = 0,
	POCSAG_PREAMBLE,
	POCSAG_MESSAGE,
};

enum pocsag_language {
	LANGUAGE_DEFAULT = 0,
	LANGUAGE_GERMAN,
};

struct pocsag;

/* instance of outgoing message */
typedef struct pocsag_msg {
	struct pocsag_msg	*next;
	struct pocsag		*pocsag;
	int			callref;		/* call reference */
	uint32_t		ric;			/* current pager ID */
	enum pocsag_function	function;		/* current function */
	char			data[256];		/* message to be transmitted */
	int			data_length;		/* length of message that is not 0-terminated */
	int			data_index;		/* current character transmitting */
	int			bit_index;		/* current bit transmitting */
	int			repeat;			/* how often the message is sent */
} pocsag_msg_t;

/* instance of pocsag transmitter/receiver */
typedef struct pocsag {
	sender_t		sender;

	/* system info */
	int			tx; 			/* can transmit */
	int			rx;			/* can receive */
	enum pocsag_language	language;		/* special characters */
	enum pocsag_function	default_function;	/* default function, if not given by caller */
	const char 		*default_message;	/* default message, if caller has no caller ID */

	/* tx states */
	enum pocsag_state	state;			/* state (idle, preamble, message) */
	pocsag_msg_t		*current_msg;		/* msg, if message codewords are transmitted */
	int			word_count;		/* counter for codewords */
	int			idle_count;		/* counts when to go idle */
	uint32_t		scan_from, scan_to;	/* if not equal: scnning mode */

	/* rx states */
	int			rx_msg_valid;		/* currently in receiving message state */
	uint32_t		rx_msg_ric;		/* ric of message */
	enum pocsag_function	rx_msg_function;	/* function of message */
	char			rx_msg_data[256];	/* data buffer */
	int			rx_msg_data_length;	/* complete characters received */
	int			rx_msg_bit_index;	/* current bit received for alphanumeric */

	/* calls */
	pocsag_msg_t		*msg_list;		/* linked list of all calls */

	/* dsp states */
	double			fsk_deviation;		/* deviation of FSK signal on sound card */
	double			fsk_polarity;		/* polarity of FSK signal (-1.0 = bit '1' is down) */
	sample_t		fsk_ramp_up[256];	/* samples of upward ramp shape */
	sample_t		fsk_ramp_down[256];	/* samples of downward ramp shape */
	double			fsk_bitduration;	/* duration of a bit in samples */
	double			fsk_bitstep;		/* fraction of a bit each sample */
	sample_t		*fsk_tx_buffer;		/* tx buffer for one data block */
	int			fsk_tx_buffer_size;	/* size of tx buffer (in samples) */
	int			fsk_tx_buffer_length;	/* usage of buffer (in samples) */
	int			fsk_tx_buffer_pos;	/* current position sending buffer */
	double			fsk_tx_phase;		/* current bit position */
	uint8_t			fsk_tx_lastbit;		/* last bit of last message, to correctly ramp */
	double			fsk_rx_phase;		/* current sample position */
	uint8_t			fsk_rx_lastbit;		/* last bit of last message, to detect level */
	uint32_t		fsk_rx_word;		/* shift register to receive codeword */
	int			fsk_rx_sync;		/* counts down to next sync */
	int			fsk_rx_index;		/* counts bits of received codeword */
} pocsag_t;

int msg_receive(const char *text);

int pocsag_function_name2value(const char *text);
void pocsag_list_channels(void);
double pocsag_channel2freq(const char *kanal, double *deviation, double *polarity, int *baudrate);
const char *pocsag_number_valid(const char *number);
void pocsag_add_id(const char *id);
int pocsag_init(void);
void pocsag_exit(void);
int pocsag_init(void);
void pocsag_exit(void);
void pocsag_new_state(pocsag_t *pocsag, enum pocsag_state new_state);
void pocsag_msg_receive(enum pocsag_language language, const char *channel, uint32_t ric, enum pocsag_function function, const char *message);
int pocsag_create(const char *kanal, double frequency, const char *device, int use_sdr, int samplerate, double rx_gain, double tx_gain, int tx, int rx, enum pocsag_language language, int baudrate, double deviation, double polarity, enum pocsag_function function, const char *message, uint32_t scan_from, uint32_t scan_to, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, const char *read_tx_wave, int loopback);
void pocsag_destroy(sender_t *sender);
void pocsag_msg_send(enum pocsag_language language, const char *text);
void pocsag_msg_destroy(pocsag_msg_t *msg);
void pocsag_get_id(pocsag_t *euro, char *id);
void pocsag_receive_id(pocsag_t *euro, char *id);
void pocsag_msg_done(pocsag_t *pocsag);
