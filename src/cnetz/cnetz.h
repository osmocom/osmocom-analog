#include "../common/compander.h"
#include "../common/sender.h"
#include "fsk_fm_demod.h"
#include "scrambler.h"

#define CNETZ_OGK_KANAL		131

/* dsp modes of transmission */
enum dsp_mode {
	DSP_SCHED_NONE = 0,	/* use for sheduling: nothing to shedule */
	DSP_MODE_OFF,		/* send nothing on unused SpK */
	DSP_MODE_OGK,		/* send "Telegramm" on OgK */
	DSP_MODE_SPK_K,		/* send concentrated "Telegramm" SpK */
	DSP_MODE_SPK_V,		/* send distributed "Telegramm" SpK */
};

enum cnetz_chan_type {
	CHAN_TYPE_OGK,		/* pure standard organization channel (channel 131) */
	CHAN_TYPE_SPK,		/* pure traffic channel */
	CHAN_TYPE_OGK_SPK,	/* combined OGK + SPK; note: some phones may reject SPK on channel 131 */
};

/* current state of c-netz sender */
enum cnetz_state {
	CNETZ_NULL,		/* before power on */
	CNETZ_IDLE,		/* broadcasting LR/MLR on Ogk */
	CNETZ_BUSY,		/* currently processing a call, no other transaction allowed */
};

	/* login to the network */
#define	TRANS_EM	(1 << 0)	/* attach request received, sending reply */
	/* roaming to different base station/network */
#define	TRANS_UM	(1 << 1)	/* roaming request received, sending reply */
	/* check if phone is still on */
#define	TRANS_MA	(1 << 2)	/* periodic online check sent, waiting for reply */
	/* mobile originated call */
#define	TRANS_VWG	(1 << 3)	/* received dialing request, waiting for time slot to send dial order */
#define	TRANS_WAF	(1 << 4)	/* dial order sent, waiting for dialing */
#define	TRANS_WBP	(1 << 5)	/* dialing received, waiting for time slot to acknowledge call */
#define	TRANS_WBN	(1 << 6)	/* dialing received, waiting for time slot to reject call */
#define	TRANS_VAG	(1 << 7)	/* establishment of call sent, switching channel */
	/* mobile terminated call */
#define	TRANS_VAK	(1 << 8)	/* establishment of call sent, switching channel */
	/* traffic channel */
#define	TRANS_BQ	(1 << 9)	/* accnowledge channel */
#define	TRANS_VHQ	(1 << 10)	/* hold call */
#define	TRANS_RTA	(1 << 11)	/* hold call and make the phone ring */
#define	TRANS_DS	(1 << 12)	/* establish speech connection */
#define	TRANS_AHQ	(1 << 13)	/* establish speech connection after answer */
	/* release */
#define	TRANS_AF	(1 << 14)	/* release connection by base station */
#define	TRANS_AT	(1 << 15)	/* release connection by mobile station */

/* timers */
#define F_BQ		8		/* number of not received frames at BQ state */
#define F_VHQK		16		/* number of not received frames at VHQ state during concentrated signalling */
#define F_VHQ		16		/* number of not received frames at VHQ state during distributed signalling */
#define F_DS		16		/* number of not received frames at DS state */
#define F_RTA		16		/* number of not received frames at RTA state */
#define N_AFKT		6		/* number of release frames to send during concentrated signalling */
#define N_AFV		4		/* number of release frames to send during distributed signalling */

/* clear causes */
#define CNETZ_CAUSE_TEILNEHMERBESETZT	0	/* subscriber busy */
#define CNETZ_CAUSE_GASSENBESETZT	1	/* network congested */
#define CNETZ_CAUSE_FUNKTECHNISCH	2	/* radio transmission fault */

struct cnetz;
struct telegramm;

typedef struct transaction {
	struct transaction	*next;			/* pointer to next node in list */
	struct cnetz		*cnetz;			/* pointer to cnetz instance */
	uint8_t			futln_nat;		/* current station ID (3 values) */
	uint8_t			futln_fuvst;
	uint16_t		futln_rest;
	char			dialing[17];		/* number dialed by the phone */
	int32_t			state;			/* state of transaction */
	int8_t			release_cause;		/* reason for release, (c-netz coding) */
	int			count;			/* counts resending things */
	struct timer		timer;			/* for varous timeouts */
	int			mo_call;		/* flags a moile originating call */
	int			mt_call;		/* flags a moile terminating call */
} transaction_t;

struct clock_speed {
	double			meas_ti;		/* time stamp for measurement interval */
	double			start_ti[4];		/* time stamp for start of counting */
	double			last_ti[4];		/* time stamp of last received time */
	uint64_t		spl_count[4];		/* sample counter for sound card */
};

/* instance of cnetz sender */
typedef struct cnetz {
	sender_t		sender;
	enum cnetz_chan_type	chan_type;		/* channel type */
	scrambler_t		scrambler_tx;		/* mirror what we transmit to MS */
	scrambler_t		scrambler_rx;		/* mirror what we receive from MS */
	compander_t		cstate;
	int			pre_emphasis;		/* use pre_emphasis by this instance */
	int			de_emphasis;		/* use de_emphasis by this instance */
	emphasis_t		estate;

	/* cell config */
	int			ms_power;		/* power level of MS, use 0..3 */
	int			auth;			/* authentication support of the cell */

	/* all cnetz states */
	enum cnetz_state	state;			/* main state of sender */

	/* scheduler */
	int			sched_ts;		/* current time slot */
	int			last_tx_timeslot;	/* last timeslot we transmitted, so we can match MS timeslot */
	int			sched_r_m;		/* Rufblock (0) / Meldeblock (1) */
	int			sched_switch_mode;	/* counts slots until mode is switched */
	enum dsp_mode		sched_dsp_mode;		/* what mode shall be switched to  */

	/* dsp states */
	enum dsp_mode		dsp_mode;		/* current mode: audio, "Telegramm", .... */
	fsk_fm_demod_t		fsk_demod;		/* demod process */
	int16_t			fsk_deviation;		/* deviation used for digital signal */
	int16_t			fsk_ramp_up[256];	/* samples of upward ramp shape */
	int16_t			fsk_ramp_down[256];	/* samples of downward ramp shape */
	double			fsk_noise;		/* send static between OgK frames */
	double			fsk_bitduration;	/* duration of a bit in samples */
	int16_t			*fsk_tx_buffer;		/* tx buffer for one data block */
	int			fsk_tx_buffer_size;	/* size of tx buffer (in samples) */
	int			fsk_tx_buffer_length;	/* usage of buffer (in samples) */
	int			fsk_tx_buffer_pos;	/* current position sending buffer */
	double			fsk_tx_bitstep;		/* fraction of a bit each sample */
	double			fsk_tx_phase;		/* current bit position */
	int			scrambler;		/* 0 = normal speech, 1 = scrambled speech */
	int16_t			*dsp_speech_buffer;	/* samples in one chunk */
	int			dsp_speech_length;	/* number of samples */
	int			dsp_speech_pos;		/* current position in buffer */

	int			frame_last_count;	/* master's count position of last frame sync */
	double			frame_last_phase;	/* master's bit phase of last frame sync */

	/* audio offset removal */
	double			offset_removal_factor;	/* how much to remove every sample */
	int16_t			offset_last_sample;	/* last sample of last audio chunk */

	/* measurements */
	int			measure_speed;		/* measure clock speed */
	struct clock_speed	clock_speed;

	transaction_t		*trans_list;		/* list of transactions */
} cnetz_t;

double cnetz_kanal2freq(int kanal, int unterband);
void cnetz_channel_list(void);
int cnetz_channel_by_short_name(const char *short_name);
const char *chan_type_short_name(enum cnetz_chan_type chan_type);
const char *chan_type_long_name(enum cnetz_chan_type chan_type);
int cnetz_init(void);
int cnetz_create(int kanal, enum cnetz_chan_type chan_type, const char *sounddev, int samplerate, int cross_channels, int auth, int ms_power, int measure_speed, double clock_speed[2], double deviation, double noise, int pre_emphasis, int de_emphasis, const char *write_wave, const char *read_wave, int loopback);
void cnetz_destroy(sender_t *sender);
void cnetz_sync_frame(cnetz_t *cnetz, double sync, int ts);
const struct telegramm *cnetz_transmit_telegramm_rufblock(cnetz_t *cnetz);
const struct telegramm *cnetz_transmit_telegramm_meldeblock(cnetz_t *cnetz);
void cnetz_receive_telegramm_ogk(cnetz_t *cnetz, struct telegramm *telegramm, int block);
const struct telegramm *cnetz_transmit_telegramm_spk_k(cnetz_t *cnetz);
void cnetz_receive_telegramm_spk_k(cnetz_t *cnetz, struct telegramm *telegramm);
const struct telegramm *cnetz_transmit_telegramm_spk_v(cnetz_t *cnetz);
void cnetz_receive_telegramm_spk_v(cnetz_t *cnetz, struct telegramm *telegramm);

