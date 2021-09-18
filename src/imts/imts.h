#include "../libsquelch/squelch.h"
#include "../libfm/fm.h"
#include "../libmobile/sender.h"

enum dsp_mode {
	DSP_MODE_OFF = 0,	/* transmitter off */
	DSP_MODE_TONE,		/* send tone or silence */
	DSP_MODE_AUDIO,		/* send audio */
};

#define TONE_GUARD		0
#define TONE_IDLE		1
#define TONE_SEIZE		2
#define TONE_CONNECT		3
#define TONE_DISCONNECT		4

#define TONE_600		5
#define TONE_1500		6

#define TONE_SILENCE		7
#define TONE_NOISE		8
#define TONE_DIALTONE		9

#define NUM_SIG_TONES		7

enum mode {
	MODE_IMTS = 0,
	MODE_MTS,
};

enum imts_state {
	IMTS_NULL = 0,
	/* channel is idle */
	IMTS_OFF,		/* base station not in use and turned off */
	IMTS_IDLE,		/* base station not in use and sending 2000 Hz Idle tone */
	/* mobile originated */
	IMTS_SEIZE,		/* base station sends seize to acknowldege call from mobile */
	IMTS_ANI,		/* base station is receiving ANI from mobile */
	IMTS_DIALING,		/* base station is receiving dial digits */
	/* mobile terminated */
	IMTS_PAGING,		/* base station is paging mobile */
	IMTS_RINGING,		/* base station is ringing mobile */
	/* active call */
	IMTS_CONVERSATION,	/* base station and mobile have conversation */
	/* releasing call */
	IMTS_RELEASE,		/* base station turned off */
	/* loopback test */
	IMTS_PAGING_TEST,	/* loopback test sequence */
	/* detector test */
	IMTS_DETECTOR_TEST,	/* detector test sequence */
};

typedef struct imts {
	sender_t		sender;

	/* channel's states */
	enum imts_state		state;			/* current sender's state */
	int			pre_emphasis;		/* use pre_emphasis by this instance */
	int			de_emphasis;		/* use de_emphasis by this instance */
	emphasis_t		estate;
	int			callref;		/* call reference */
	char			station_id[11];		/* current station ID (also used for test pattern) */
	int			station_length;		/* digit length of station ID */
	char			dial_number[33];	/* number dialing */
	struct timer		timer;
	int			last_tone;		/* last tone received */
	double			last_sigtone_amplitude;	/* amplitude of last signaling tone received */
	double			fast_seize;		/* fast seize: guard-length - roundtrip-delay */
	double			rx_guard_timestamp;	/* start of guard tone (seize by mobile) */
	double			rx_guard_duration;	/* duration of guard (only long guards are detected) */
	int			rx_ani_pulse;		/* current pulse # receiving */
	int			rx_ani_index;		/* current digit # receiving */
	int			rx_ani_totpulses;	/* total pulses count receiving */
	int			rx_dial_pulse;		/* current pulse # receiving */
	int			rx_dial_index;		/* current digit # receiving */
	int			rx_disc_pulse;		/* current pulse # receiving */
	int			tx_page_pulse;		/* current pulse # transmitting */
	int			tx_page_index;		/* current digit # transmitting */
	double			tx_page_timestamp;	/* last pulse of digit transmitting */
	int			tx_ring_pulse;		/* current pulse # transmitting */
	int			rx_page_pulse;		/* current pulse # receiving */
	double			detector_test_length_1;	/* detector test tone duration */
	double			detector_test_length_2;	/* detector test tone duration */
	double			detector_test_length_3;	/* detector test tone duration */

	/* MTS additional states */
	enum mode		mode;			/* set if MTS mode is used */
	const char		*operator;		/* operator's number to call when seizing the channel */

	/* display measurements */
	dispmeasparam_t		*dmp_tone_level;
	dispmeasparam_t		*dmp_tone_quality;

	/* dsp states */
	double			sample_duration;	/* 1 / samplerate */
	double			demod_center;		/* center frequency for tone demodulation */
	double			demod_bandwidth;	/* bandwidth for tone demodulation */
	fm_demod_t		demod;			/* demodulator for frequency / amplitude */
	iir_filter_t		demod_freq_lp;		/* filter for frequency response */
	iir_filter_t		demod_ampl_lp;		/* filter for amplitude response */
	int			demod_current_tone;	/* current tone being detected */
	int			demod_sig_tone;		/* current tone is a signaling tone */
	int			demod_last_tone;	/* last tone being detected */
	double			demod_sustain;		/* how long a tone must sustain */
	double			demod_duration;		/* duration of last tone */
	double			demod_quality_time;	/* time counter to measure quality */
	int			demod_quality_count;	/* counter to measure quality */
	double			demod_quality_value;	/* sum of quality samples (must be divided by count) */
	double			display_interval;	/* used to update tone levels */
	enum dsp_mode		dsp_mode;		/* current mode: audio, durable tone 0 or 1, paging */
	int			ptt;			/* set, if push to talk is used (transmitter of phone off) */
	int			tone;			/* current tone to send */
	int			tone_duration;		/* if set, tone is limited to this duration (in samples) */
	double			tone_idle_phaseshift65536;/* how much the phase of sine wave changes per sample */
	double			tone_seize_phaseshift65536;/* how much the phase of sine wave changes per sample */
	double			tone_600_phaseshift65536;/* how much the phase of sine wave changes per sample */
	double			tone_1500_phaseshift65536;/* how much the phase of sine wave changes per sample */
	double			tone_dialtone_phaseshift65536[2];/* how much the phase of sine wave changes per sample */
	double			tone_phase65536[2];	/* current phase */
	squelch_t		squelch;		/* squelch detection process */
	int			is_mute;		/* set if quelch has muted */
	int			rf_signal;		/* set if we have currently an RF signal */
	sample_t		*delay_spl;		/* delay buffer for delaying audio */
	int			delay_pos;		/* position in delay buffer */
	int			delay_max;		/* number of samples in delay buffer */
} imts_t;


void imts_list_channels(void);
double imts_channel2freq(const char *kanal, int uplink);
int imts_init(void);
int imts_create(const char *channel, const char *device, int use_sdr, int samplerate, double rx_gain, double tx_gain, int pre_emphasis, int de_emphasis, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, const char *read_tx_wave, int loopback, double squelch_db, int ptt, int station_length, double fast_seize, enum mode mode, const char *operator, double length_1, double length_2, double length_3);
void imts_destroy(sender_t *sender);
void imts_loss_indication(imts_t *imts, double loss_time);
void imts_signal_indication(imts_t *imts);
void imts_receive_tone(imts_t *imts, int tone, double elapsed, double amplitude);
void imts_lost_tone(imts_t *imts, int tone, double elapsed);
void imts_tone_sent(imts_t *imts, int tone);

