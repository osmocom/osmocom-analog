#include "../common/sender.h"
#include "../common/compandor.h"
#include "../common/dtmf.h"
#include "../common/call.h"
#include "dms.h"
#include "sms.h"


enum dsp_mode {
	DSP_MODE_SILENCE,	/* stream nothing */
	DSP_MODE_DIALTONE,	/* stream dial tone to mobile phone */
	DSP_MODE_AUDIO,		/* stream audio */
	DSP_MODE_FRAME,		/* send frames */
	DSP_MODE_DTMF,		/* send DTMF tones */
};

enum nmt_chan_type {
	CHAN_TYPE_CC,		/* calling channel */
	CHAN_TYPE_TC,		/* traffic channel */
	CHAN_TYPE_CC_TC,	/* combined CC + TC */
	CHAN_TYPE_TEST,		/* test channel */
};

enum nmt_state {
	STATE_NULL = 0,		/* power off state */
	STATE_IDLE,		/* channel is not in use */
	STATE_ROAMING_IDENT,	/* seizure received, waiting for identity */
	STATE_ROAMING_CONFIRM,	/* identity received, sending confirm */
	STATE_MO_IDENT,		/* seizure of mobile originated call, waiting for identity */
	STATE_MO_CONFIRM,	/* identity received, sending confirm */
	STATE_MO_DIALING,	/* receiving digits from phone */
	STATE_MO_COMPLETE,	/* all digits received, completing call */
	STATE_MT_PAGING,	/* paging mobile phone */
	STATE_MT_CHANNEL,	/* assigning traffic channel */
	STATE_MT_IDENT,		/* waiting for identity */
	STATE_MT_AUTOANSWER,	/* sending autoanswer, waiting for reply */
	STATE_MT_RINGING,	/* mobile phone is ringing, waiting for answer */
	STATE_MT_COMPLETE,	/* mobile phone has answered, completing call */
	STATE_ACTIVE,		/* during active call */
	STATE_MO_RELEASE,	/* acknowlegde release from mobile phone */
	STATE_MT_RELEASE,	/* sending release toward mobile phone */
};

enum nmt_active_state {
	ACTIVE_STATE_VOICE,	/* normal conversation */
	ACTIVE_STATE_MFT_IN,	/* ack MFT converter in */
	ACTIVE_STATE_MFT,	/* receive digits in MFT mode */
	ACTIVE_STATE_MFT_OUT,	/* ack MFT converter out */
};

enum nmt_direction {
	MTX_TO_MS,
	MTX_TO_BS,
	MTX_TO_XX,
	BS_TO_MTX,
	MS_TO_MTX,
	XX_TO_MTX,
};

typedef struct nmt_sysinfo {
	enum nmt_chan_type	chan_type;		/* channel type */
	int			ms_power;		/* ms power level 3 = full */
	uint8_t			traffic_area;		/* two digits traffic area, encoded as YY */
	uint8_t			area_no;		/* Area no. 1..4, 0 = no Area no. */
} nmt_sysinfo_t;

const char *nmt_dir_name(enum nmt_direction dir);

typedef struct nmt {
	sender_t		sender;
	nmt_sysinfo_t		sysinfo;
	compandor_t		cstate;
	dtmf_t			dtmf;
	struct transaction	*trans;			/* pointer to transaction, if bound to channel */

	/* sender's states */
	enum nmt_state		state;
	int			wait_autoanswer;	/* wait for frame 15 before we can send autoanswer */
	enum nmt_active_state	active_state;
	struct timer		timer;
	int			rx_frame_count;		/* receive frame counter */
	int			tx_frame_count;		/* transmit frame counter */
	int			tx_callerid_count;	/* counter for caller ID repetition */
	char			dialing[33];		/* dialed digits */
	int			mft_num;		/* counter for digit for MFT */

	/* features */
	int			compandor;		/* if compandor shall be used */
	int			supervisory;		/* if set, use supervisory signal 1..4 */
	int			send_callerid;		/* if set, send caller ID while ringing the phone */

	/* dsp states */
	enum dsp_mode		dsp_mode;		/* current mode: audio, durable tone 0 or 1, paging */
	double			fsk_samples_per_bit;	/* number of samples for one bit (1200 Baud) */
	double			fsk_bits_per_sample;	/* fraction of a bit per sample */
	int			super_samples;		/* number of samples in buffer for supervisory detection */
	int			fsk_coeff[2];		/* coefficient k = 2*cos(2*PI*f/samplerate), k << 15 */
	int			super_coeff[5];		/* coefficient for supervisory signal */
	int			fsk_polarity;		/* current polarity state of bit */
	int16_t			*fsk_filter_spl;	/* array to hold ring buffer for bit decoding */
	int			fsk_filter_size;	/* size of ring buffer */
	int			fsk_filter_pos;		/* position to write next sample */
	double			fsk_filter_step;	/* counts bit duration, to trigger decoding every 10th bit */
	int			fsk_filter_bit;		/* last bit state, so we detect a bit change */
	int			fsk_filter_sample;	/* count until it is time to sample bit */
	uint16_t		fsk_filter_sync;	/* shift register to detect sync */
	int			fsk_filter_in_sync;	/* if we are in sync and receive bits */
	int			fsk_filter_mute;	/* mute count down after sync */
	char			fsk_filter_frame[141];	/* receive frame (one extra byte to terminate string) */
	int			fsk_filter_count;	/* next bit to receive */
	double			fsk_filter_level[256];	/* level infos */
	double			fsk_filter_quality[256];/* quality infos */
	int16_t			*super_filter_spl;	/* array with sample buffer for supervisory detection */
	int			super_filter_pos;	/* current sample position in filter_spl */
	double			super_phaseshift256[4];	/* how much the phase of sine wave changes per sample */
	double			super_phase256;		/* current phase */
	double			dial_phaseshift256;	/* how much the phase of sine wave changes per sample */
	double			dial_phase256;		/* current phase */
	double			fsk_phaseshift256;	/* how much the phase of fsk synbol changes per sample */
	double			fsk_phase256;		/* current phase */
	int16_t			*frame_spl;		/* samples to store a complete rendered frame */
	int			frame_size;		/* total size of sample buffer */
	int			frame_length;		/* current length of data in sample buffer */
	int			frame_pos;		/* current sample position in frame_spl */
	double			rx_bits_count;		/* sample counter */
	double			rx_bits_count_current;	/* sample counter of current frame */
	double			rx_bits_count_last;	/* sample counter of last frame */
	int			super_detected;		/* current detection state flag */
	int			super_detect_count;	/* current number of consecutive detections/losses */

	/* DMS/SMS states */
	dms_t			dms;			/* DMS states */
	int			dms_call;		/* indicates that this call is a DMS call */
	sms_t			sms;			/* SMS states */
	char			smsc_number[33];	/* digits to match SMSC */
	struct timer		sms_timer;
} nmt_t;

void nmt_channel_list(void);
int nmt_channel_by_short_name(const char *short_name);
const char *chan_type_short_name(enum nmt_chan_type chan_type);
const char *chan_type_long_name(enum nmt_chan_type chan_type);
double nmt_channel2freq(int channel, int uplink);
void nmt_country_list(void);
uint8_t nmt_country_by_short_name(const char *short_name);
int nmt_create(int channel, enum nmt_chan_type chan_type, const char *sounddev, int samplerate, double rx_gain, int pre_emphasis, int de_emphasis, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, uint8_t ms_power, uint8_t traffic_area, uint8_t area_no, int compandor, int supervisory, const char *smsc_number, int send_callerid, int loopback);
void nmt_check_channels(void);
void nmt_destroy(sender_t *sender);
void nmt_go_idle(nmt_t *nmt);
void nmt_receive_frame(nmt_t *nmt, const char *bits, double quality, double level, double frames_elapsed);
const char *nmt_get_frame(nmt_t *nmt);
void nmt_rx_super(nmt_t *nmt, int tone, double quality);
void timeout_mt_paging(struct transaction *trans);
void deliver_sms(const char *sms);
int submit_sms(const char *sms);

