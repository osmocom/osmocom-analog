#include "../libmobile/sender.h"
#include "../libtimer/timer.h"
#include "../libcompandor/compandor.h"
#include "../libdtmf/dtmf_encode.h"
#include "../libmobile/call.h"
#include "../libfsk/fsk.h"
#include "../libgoertzel/goertzel.h"
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
	CHAN_TYPE_CCA,		/* calling channel type A mobiles */
	CHAN_TYPE_CCB,		/* calling channel type B mobiles */
	CHAN_TYPE_TC,		/* traffic channel */
	CHAN_TYPE_AC_TC,	/* combined AC + TC */
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
	int			system;			/* 450 or 900 */
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
	dtmf_enc_t		dtmf;
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

	/* display measurements */
	dispmeasparam_t		*dmp_frame_level;
	dispmeasparam_t		*dmp_frame_quality;
	dispmeasparam_t		*dmp_super_level;
	dispmeasparam_t		*dmp_super_quality;

	/* features */
	int			compandor;		/* if compandor shall be used */
	int			supervisory;		/* if set, use supervisory signal 1..4 */
	int			send_callerid;		/* if set, send caller ID while ringing the phone */

	/* dsp states */
	enum dsp_mode		dsp_mode;		/* current mode: audio, durable tone 0 or 1, paging */
	fsk_mod_t		fsk_mod;		/* fsk processing */
	fsk_demod_t		fsk_demod;
	int			super_samples;		/* number of samples in buffer for supervisory detection */
	goertzel_t		super_goertzel[5];	/* filter for supervisory decoding */
	sample_t		*super_filter_spl;	/* array with sample buffer for supervisory detection */
	int			super_filter_pos;	/* current sample position in filter_spl */
	double			super_phaseshift65536[4];/* how much the phase of sine wave changes per sample */
	double			super_phase65536;	/* current phase */
	int			super_print;		/* counts when to print result */
	double			dial_phaseshift65536;	/* how much the phase of sine wave changes per sample */
	double			dial_phase65536;	/* current phase */
	uint16_t		rx_sync;		/* shift register to detect sync */
	int			rx_in_sync;		/* if we are in sync and receive bits */
	int			rx_mute;		/* mute count down after sync */
	char			rx_frame[141];		/* receive frame (one extra byte to terminate string) */
	int			rx_count;		/* next bit to receive */
	double			rx_level[256];		/* level infos */
	double			rx_quality[256];	/* quality infos */
	uint64_t		rx_bits_count;		/* sample counter */
	uint64_t		rx_bits_count_current;	/* sample counter of current frame */
	uint64_t		rx_bits_count_last;	/* sample counter of last frame */
	int			super_detected;		/* current detection state flag */
	int			super_detect_count;	/* current number of consecutive detections/losses */
	char			tx_frame[166];		/* carries bits of one frame to transmit */
	int			tx_frame_length;
	int			tx_frame_pos;

	/* DMS/SMS states */
	dms_t			dms;			/* DMS states */
	sms_t			sms;			/* SMS states */
	char			smsc_number[33];	/* digits to match SMSC */
	struct timer		sms_timer;
} nmt_t;

void nmt_channel_list(int nmt_system);
int nmt_channel_by_short_name(int nmt_system, const char *short_name);
const char *chan_type_short_name(int nmt_system, enum nmt_chan_type chan_type);
const char *chan_type_long_name(int nmt_system, enum nmt_chan_type chan_type);
int nmt_create(int nmt_system, const char *country, const char *kanal, enum nmt_chan_type chan_type, const char *audiodev, int use_sdr, int samplerate, double rx_gain, int pre_emphasis, int de_emphasis, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, const char *read_tx_wave, uint8_t ms_power, uint8_t traffic_area, uint8_t area_no, int compandor, int supervisory, const char *smsc_number, int send_callerid, int loopback);
void nmt_check_channels(int nmt_system);
void nmt_destroy(sender_t *sender);
void nmt_go_idle(nmt_t *nmt);
void nmt_receive_frame(nmt_t *nmt, const char *bits, double quality, double level, int frames_elapsed);
const char *nmt_get_frame(nmt_t *nmt);
void nmt_rx_super(nmt_t *nmt, int tone, double quality);
void timeout_mt_paging(struct transaction *trans);
void deliver_sms(const char *sms);
int submit_sms(const char *sms);

