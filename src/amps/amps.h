#include "../common/goertzel.h"
#include "../common/sender.h"
#include "../common/compandor.h"
#include "sysinfo.h"
#include "transaction.h"

enum dsp_mode {
	DSP_MODE_OFF,			/* channel not active (VC) */
	DSP_MODE_AUDIO_RX_AUDIO_TX,	/* stream audio */
	DSP_MODE_AUDIO_RX_FRAME_TX,	/* stream audio, send frames */
	DSP_MODE_FRAME_RX_FRAME_TX,	/* send and decode frames */
};

enum amps_chan_type {
	CHAN_TYPE_CC,		/* control channel */
	CHAN_TYPE_PC,		/* paging channel */
	CHAN_TYPE_CC_PC,	/* combined CC + PC */
	CHAN_TYPE_VC,		/* voice channel */
	CHAN_TYPE_CC_PC_VC,	/* combined CC + PC + TC */
};

enum amps_state {
	STATE_NULL,		/* power off state */
	STATE_IDLE,		/* channel is not in use */
	STATE_BUSY,		/* channel busy (call) */
};

enum fsk_rx_sync {
	FSK_SYNC_NONE,		/* we are not in sync and wait for valid dotting sequence */
	FSK_SYNC_DOTTING,	/* we received a valid dotting sequence and check for sync sequence */
	FSK_SYNC_POSITIVE,	/* we have valid sync and read all the bits of the frame */
	FSK_SYNC_NEGATIVE,	/* as above, but negative sync (high frequency deviation detected as low signal)  */
};

#define FSK_MAX_BITS		1032	/* maximum number of bits to process (FVC with dotting+sync) */

typedef struct amps {
	sender_t		sender;
	compandor_t		cstate;
	int			pre_emphasis;		/* use pre_emphasis by this instance */
	int			de_emphasis;		/* use de_emphasis by this instance */
	emphasis_t		estate;

	/* sender's states */
	enum amps_chan_type	chan_type;
	enum amps_state		state;
	int			channel_busy;		/* indicate channel is busy while receiving */

	/* system info */
	amps_si			si;

	/* cell nr selection */
	int			cell_auto;		/* if set, cell_nr is selected automatically */

	/* dsp states */
	enum dsp_mode		dsp_mode;		/* current mode: audio, durable tone 0 or 1, paging */
	int			flip_polarity;		/* 1 = flip */
	double			fsk_deviation;		/* deviation of FSK signal on sound card */
	sample_t		fsk_ramp_up[256];	/* samples of upward ramp shape */
	sample_t		fsk_ramp_down[256];	/* samples of downward ramp shape */
	double			fsk_bitduration;	/* duration of one bit in samples */
	double			fsk_bitstep;		/* fraction of one bit each sample */
	/* tx bits generation */
	char			fsk_tx_frame[FSK_MAX_BITS + 1];	/* +1 because 0-termination */
	int			fsk_tx_frame_pos;	/* current position sending bits */
	sample_t		*fsk_tx_buffer;		/* tx buffer for one data block */
	int			fsk_tx_buffer_size;	/* size of tx buffer (in samples) */
	int			fsk_tx_buffer_length;	/* usage of buffer (in samples) */
	int			fsk_tx_buffer_pos;	/* current position sending buffer */
	double			fsk_tx_phase;		/* current bit position */
	char			fsk_tx_last_bit;	/* save last bit of frame (for next frame's ramp) */
	/* high-pass filter to remove DC offset from RX signal */
	double			highpass_factor;	/* high pass filter factor */
	double			highpass_x_last;	/* last input value */
	double			highpass_y_last;	/* last output value */
	/* rx detection of bits and sync */
	sample_t		fsk_rx_last_sample;	/* last sample (for level change detection) */
	double			fsk_rx_elapsed;		/* bit duration since last level change */
	enum fsk_rx_sync	fsk_rx_sync;		/* sync state */
	uint16_t		fsk_rx_sync_register;	/* shift register to detect sync word */
	int			fsk_rx_sync_tolerant;	/* be more tolerant to sync */
	/* the dotting buffer stores the elapsed samples, so we can calculate
	 * an average time of zero-crossings during dotting sequence.
	 * this buffer wrapps every 256 values */
	double			fsk_rx_dotting_elapsed[256]; /* dotting buffer with elapsed samples since last zero-crossing */
	uint8_t			fsk_rx_dotting_pos;	/* position of next value in dotting buffer */
	int			fsk_rx_dotting_life;	/* counter to expire when no sync was found after dotting */
	double			fsk_rx_dotting_average;	/* last average slope position of dotting sequnece. */
	/* the ex buffer holds the duration of one bit, and wrapps every
	 * bit. */
	double			fsk_rx_bitcount;	/* counts the bit. if it reaches or exceeds 1, the bit is complete and the next bit starts */
	sample_t		*fsk_rx_window;		/* rx buffer for one bit */
	int			fsk_rx_window_length;	/* length of rx buffer */
	int			fsk_rx_window_half;	/* half of length of rx buffer */
	int			fsk_rx_window_begin;	/* where to begin detecting level */
	int			fsk_rx_window_end;	/* where to end detecting level */
	int			fsk_rx_window_pos;	/* current position in buffer */
	/* the rx bufffer received one frame until rx length */
	char			fsk_rx_frame[FSK_MAX_BITS + 1];	/* +1 because 0-termination */
	int			fsk_rx_frame_length;	/* length of expected frame */
	int			fsk_rx_frame_count;	/* count number of received bit */
	double			fsk_rx_frame_level;	/* sum of level of all bits */
	double			fsk_rx_frame_quality;	/* sum of quality of all bits */
	/* RECC frame states */
	int			rx_recc_nawc;		/* counts down received words */
	int			rx_recc_word_count;	/* counts up received words */
	uint32_t		rx_recc_min1;		/* mobile id */
	uint16_t		rx_recc_min2;
	uint8_t			rx_recc_msg_type;	/* message (3 values) */
	uint8_t			rx_recc_ordq;
	uint8_t			rx_recc_order;
	uint32_t		rx_recc_esn;
	uint32_t		rx_recc_scm;
	uint8_t			rx_recc_mpci;
	char			rx_recc_dialing[33];	/* received dial string */
	/* FOCC frame states */
	int			rx_focc_word_count;	/* counts received words */
	int			tx_focc_frame_count;	/* used to schedule system informations */
	int			tx_focc_send;		/* if set, send message words */
	uint32_t		tx_focc_min1;		/* mobile id */
	uint16_t		tx_focc_min2;
	int			tx_focc_chan;		/* channel to assign for voice call */
	uint8_t			tx_focc_msg_type;	/* message (3 values) */
	uint8_t			tx_focc_ordq;
	uint8_t			tx_focc_order;
	int			tx_focc_word_count;	/* counts transmitted words in a muli word message */
	int			tx_focc_word_repeat;	/* countrs repeats of mulit word message */
	/* FVC frame states */
	int			tx_fvc_send;		/* if set, send message words */
	int			tx_fvc_chan;		/* channel to assign for voice call */
	uint8_t			tx_fvc_msg_type;	/* message (3 values) */
	uint8_t			tx_fvc_ordq;
	uint8_t			tx_fvc_order;
	/* SAT tone */
	int			sat;			/* use SAT tone 0..2 */
	int			sat_samples;		/* number of samples in buffer for supervisory detection */
	goertzel_t		sat_goertzel[5];	/* filter for SAT signal decoding */
	sample_t		*sat_filter_spl;	/* array with sample buffer for supervisory detection */
	int			sat_filter_pos;		/* current sample position in filter_spl */
	double			sat_phaseshift65536[3];	/* how much the phase of sine wave changes per sample */
	double			sat_phase65536;		/* current phase */
	int			sat_detected;		/* current detection state flag */
	int			sat_detect_count;	/* current number of consecutive detections/losses */
	int			sig_detected;		/* current detection state flag */
	int			sig_detect_count;	/* current number of consecutive detections/losses */
	double			test_phaseshift65536;	/* how much the phase of sine wave changes per sample */
	double			test_phase65536;	/* current phase */

	transaction_t		*trans_list;		/* list of transactions */

	/* delay measurement in loopback mode */
	double			when_received;		/* time stamp of received frame start (start of dotting) */
	double			when_transmitted[16];	/* time stamps of filler frames with different count */
	int			when_count;		/* counter of the filler frame */
} amps_t;

void amps_channel_list(void);
int amps_channel_by_short_name(const char *short_name);
const char *chan_type_short_name(enum amps_chan_type chan_type);
const char *chan_type_long_name(enum amps_chan_type chan_type);
double amps_channel2freq(int channel, int uplink);
enum amps_chan_type amps_channel2type(int channel);
const char *amps_channel2band(int channel);
const char *amps_min22number(uint16_t min2);
const char *amps_min12number(uint32_t min1);
void amps_number2min(const char *number, uint32_t *min1, uint16_t *min2);
const char *amps_min2number(uint32_t min1, uint16_t min2);
const char *amps_scm(uint8_t scm);
int amps_create(int channel, enum amps_chan_type chan_type, const char *audiodev, int use_sdr, int samplerate, double rx_gain, int pre_emphasis, int de_emphasis, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, amps_si *si, uint16_t sid, uint8_t sat, int polarity, int tolerant, int loopback);
void amps_destroy(sender_t *sender);
void amps_rx_signaling_tone(amps_t *amps, int tone, double quality);
void amps_rx_sat(amps_t *amps, int tone, double quality);
void amps_rx_recc(amps_t *amps, uint8_t scm, uint8_t mpci, uint32_t esn, uint32_t min1, uint16_t min2, uint8_t msg_type, uint8_t ordq, uint8_t order, const char *dialing);
transaction_t *amps_tx_frame_focc(amps_t *amps);
transaction_t *amps_tx_frame_fvc(amps_t *amps);

