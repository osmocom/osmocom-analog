#include "../libmobile/sender.h"

#define TYPE_TONE	0	/* TONE only */
#define TYPE_VOICE	1	/* TONE + VOICE */
#define TYPE_ALPHA	2	/* TONE + DATA */
#define TYPE_NUMERIC	3	/* TONE + DATA */
#define MAX_ADB		10	/* 80 characters */
#define MAX_NDB		2	/* 24 digits */

/* instance of outgoing message */
typedef struct gsc_msg {
	struct gsc_msg		*next;
	char			address[8];		/* 7 digits + EOL */
	int			force_type;		/* override type from address digit 7 */
	char			data[256];		/* message to be transmitted */
} gsc_msg_t;

typedef struct gsc {
	sender_t		sender;
	int			tx;

	gsc_msg_t		*msg_list;		/* queue of messages */
	const char		*default_message;

	/* current trasmitting message */
	uint8_t			bit[4096];
	int			bit_num;
	int			bit_index;		/* when playing out */
	int			bit_overflow;

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
} gsc_t;

int golay_create(const char *kanal, double frequency, const char *device, int use_sdr, int samplerate, double rx_gain, double tx_gain, double deviation, double polarity, const char *message, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, const char *read_tx_wave, int loopback);
void golay_destroy(sender_t *sender);

void init_golay(void);
void init_bch(void);

int8_t get_bit(gsc_t *gsc);
void golay_msg_send(const char *buffer);

