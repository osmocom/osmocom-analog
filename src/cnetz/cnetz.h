#include "../libcompandor/compandor.h"
#include "../libtimer/timer.h"
#include "../libmobile/sender.h"
#include "fsk_demod.h"
#include "../libscrambler/scrambler.h"
#include "transaction.h"

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

/* timers */
#define F_BQ		8		/* number of not received frames at BQ state */
#define F_VHQK		16		/* number of not received frames at VHQ state during concentrated signaling */
#define F_VHQ		16		/* number of not received frames at VHQ state during distributed signaling */
#define F_ZFZ		16		/* number of not received frames at ZFZ state (guessed, no documentation avail) */
#define F_DS		16		/* number of not received frames at DS state */
#define F_RTA		16		/* number of not received frames at RTA state */
#define N_AFKT		6		/* number of release frames to send during concentrated signaling */
#define N_AFV		4		/* number of release frames to send during distributed signaling */
#define N		3		/* now many times we repeat a message on OgK */
#define T_VAG2		180		/* time on outgoing queue */
#define T_VAK		60		/* time on incoming queue */
#define T_AP		750		/* Time to wait for SIM card's authentication reply */

/* clear causes */
#define CNETZ_CAUSE_GASSENBESETZT	0	/* network congested */
#define CNETZ_CAUSE_TEILNEHMERBESETZT	1	/* subscriber busy */
#define CNETZ_CAUSE_FUNKTECHNISCH	2	/* radio transmission fault */

struct cnetz;
struct telegramm;

struct clock_speed {
	double			meas_ti;		/* time stamp for measurement interval */
	double			start_ti[4];		/* time stamp for start of counting */
	double			last_ti[4];		/* time stamp of last received time */
	double			spl_count[4];		/* sample counter for sound card */
	/* making average of measurement values */
	double			speed_ppm[4][256];	/* history of clock speed measurements */
	int			idx[4];			/* index of current value */
	int			num[4];			/* total num of values so far */
};

/* instance of cnetz sender */
typedef struct cnetz {
	sender_t		sender;
	enum cnetz_chan_type	chan_type;		/* channel type */
	scrambler_t		scrambler_tx;		/* mirror what we transmit to MS */
	scrambler_t		scrambler_rx;		/* mirror what we receive from MS */
	compandor_t		cstate;
	int			pre_emphasis;		/* use pre_emphasis by this instance */
	int			de_emphasis;		/* use de_emphasis by this instance */
	emphasis_t		estate;

	/* call config */
	int			ms_power;		/* power level of MS, use 0..3 */
	int			challenge_valid;	/* send authorizaton value */
	uint64_t		challenge;		/* authorization value */
	int			response_valid;		/* expect authorizaton response */
	uint64_t		response;		/* authorization response */
	int			warteschlange;		/* use queue */
	int			metering;		/* use metering pulses in seconds 0 = off */

	/* all cnetz states */
	enum cnetz_state	state;			/* main state of sender */

	/* cell nr selection */
	int			cell_auto;		/* if set, cell_nr is selected automatically */
	int			cell_nr;		/* current cell number to use (sysinfo) */

	/* scheduler */
	int			sched_ts;		/* current time slot */
	int			sched_last_ts[2];	/* last timeslot we transmitted, so we can match MS timeslot */
	int			sched_r_m;		/* Rufblock (0) / Meldeblock (1) */
	int			sched_switch_mode;	/* counts slots until mode is switched */
	enum dsp_mode		sched_dsp_mode;		/* what mode shall be switched to  */

	/* dsp states */
	enum dsp_mode		dsp_mode;		/* current mode: audio, "Telegramm", .... */
	iir_filter_t		lp;			/* low pass filter to eliminate noise above 5280 Hz */
	fsk_fm_demod_t		fsk_demod;		/* demod process */
	double			fsk_deviation;		/* deviation of FSK signal on sound card */
	sample_t		fsk_ramp_up[256];	/* samples of upward ramp shape */
	sample_t		fsk_ramp_down[256];	/* samples of downward ramp shape */
	double			fsk_bitduration;	/* duration of a bit in samples */
	sample_t		*fsk_tx_buffer;		/* tx buffer for one data block */
	int			fsk_tx_buffer_size;	/* size of tx buffer (in samples) */
	int			fsk_tx_buffer_length;	/* usage of buffer (in samples) */
	int			fsk_tx_buffer_pos;	/* current position sending buffer */
	double			fsk_tx_bitstep;		/* fraction of a bit each sample */
	double			fsk_tx_phase;		/* current bit position */
	uint64_t		fsk_tx_scount;		/* sample counter (used to sync multiple channels) */
	int			scrambler;		/* 0 = normal speech, 1 = scrambled speech */
	int			scrambler_switch;	/* counter to switch after 3 frames with new scrabler state */
	sample_t		*dsp_speech_buffer;	/* samples in one chunk */
	int			dsp_speech_length;	/* number of samples */
	int			dsp_speech_pos;		/* current position in buffer */

	/* sync multiple channels on one sound card */
	uint64_t		frame_last_scount;	/* master's sample count of last frame sync */
	double			frame_last_phase;	/* master's bit phase of last frame sync */

	/* audio offset removal */
	double			offset_factor;		/* filer alpha of high-pass filter */
	double			offset_y_last;		/* last stored sample */

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
int cnetz_create(const char *kanal, enum cnetz_chan_type chan_type, const char *audiodev, int use_sdr, enum demod_type demod, int samplerate, double rx_gain, int challenge_valid, uint64_t challenge, int response_valid, uint64_t response, int warteschlange, int metering, double dbm0_deviation, int ms_power, int measure_speed, double clock_speed[2], int polarity, int pre_emphasis, int de_emphasis, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, const char *read_tx_wave, int loopback);
void cnetz_destroy(sender_t *sender);
void cnetz_go_idle(cnetz_t *cnetz);
void cnetz_sync_frame(cnetz_t *cnetz, double sync, int ts);
int cnetz_meldeaufruf(uint8_t futln_nat, uint8_t futln_fuvst, uint16_t futln_rest);
const struct telegramm *cnetz_transmit_telegramm_rufblock(cnetz_t *cnetz);
const struct telegramm *cnetz_transmit_telegramm_meldeblock(cnetz_t *cnetz);
void cnetz_receive_telegramm_ogk(cnetz_t *cnetz, struct telegramm *telegramm, int block);
const struct telegramm *cnetz_transmit_telegramm_spk_k(cnetz_t *cnetz);
void cnetz_receive_telegramm_spk_k(cnetz_t *cnetz, struct telegramm *telegramm);
const struct telegramm *cnetz_transmit_telegramm_spk_v(cnetz_t *cnetz);
void cnetz_receive_telegramm_spk_v(cnetz_t *cnetz, struct telegramm *telegramm);
void cnetz_display_status(void);

