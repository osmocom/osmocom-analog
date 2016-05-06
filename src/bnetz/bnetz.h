#include "../common/sender.h"

/* fsk modes of transmission */
enum dsp_mode {
	DSP_MODE_AUDIO,		/* stream audio */
	DSP_MODE_SILENCE,	/* sending silence */
	DSP_MODE_0,		/* send tone 0 */
	DSP_MODE_1,		/* send tone 1 */
	DSP_MODE_TELEGRAMM,	/* send "Telegramm" */
};

/* current state of b-netz sender */
enum bnetz_state {
	BNETZ_FREI,		/* sending 'Gruppenfreisignal' */
	BNETZ_WAHLABRUF,	/* sending 'Wahlabruf', receiving call setup */
	BNETZ_SELEKTIVRUF,	/* paging phone */
	BNETZ_RUFBESTAETIGUNG,	/* waitig for acknowledge from phone */
	BNETZ_RUFHALTUNG,	/* phone is ringing */
	BNETZ_GESPRAECH,	/* during conversation */
	BNETZ_TRENNEN,		/* release of call */
};

/* current state of received dialing */
enum dial_mode {
	DIAL_MODE_START,
	DIAL_MODE_STATIONID,
	DIAL_MODE_NUMBER,
	DIAL_MODE_START2,
	DIAL_MODE_STATIONID2,
	DIAL_MODE_NUMBER2,
};

/* current state of paging mobile station */
enum page_mode {
	PAGE_MODE_NUMBER,
	PAGE_MODE_KANALBEFEHL,
};

/* instance of bnetz sender */
typedef struct bnetz {
	sender_t		sender;

	/* system info */
	int			gfs;			/* 'Gruppenfreisignal' */

	/* switch sender to channel 19 */
	char			pilot_file[256];	/* if set, write to given file to switch to channel 19 or back */
	char			pilot_on[256];		/* what to write to switch to channel 19 */
	char			pilot_off[256];		/* what to write to switch back */
	int			pilot_is_on;		/* set, if we are on channel 19. also used to switch back on exit */

	/* all bnetz states */
	enum bnetz_state	state;			/* main state of sender */
	enum dial_mode		dial_mode;		/* sub state while dialing is received */
	int			dial_metering;		/* set, if phone supports metering pulses */
	char			dial_number[14];	/* dial string received */
	int			dial_pos;		/* current position while receiving digits */
	char			station_id[6];		/* current station ID */
	int			station_id_pos;		/* position while transmitting */
	enum page_mode		page_mode;		/* sub state while paging */
	int			page_try;		/* try number (1 or 2) */
	struct timer		timer;
	int			trenn_count;		/* count number of release messages */

	/* dsp states */
	enum dsp_mode		dsp_mode;		/* current mode: audio, durable tone 0 or 1, "Telegramm" */
	int			fsk_coeff[2];		/* coefficient k = 2*cos(2*PI*f/samplerate), k << 15 */
	int			samples_per_bit;	/* how many samples lasts one bit */
	int16_t			*fsk_filter_spl;	/* array with samples_per_bit */
	int			fsk_filter_pos;		/* current sample position in filter_spl */
	int			fsk_filter_step;	/* number of samples for each analyzation */
	int			fsk_filter_bit;		/* last bit, so we detect a bit change */
	int			fsk_filter_sample;	/* count until it is time to sample bit */
	uint16_t		fsk_filter_telegramm;	/* rx shift register for receiveing telegramm */
	double			fsk_filter_quality[16];	/* quality of each bit in telegramm */
	double			fsk_filter_level[16];	/* level of each bit in telegramm */
	int			fsk_filter_qualidx;	/* index of quality array above */
	int			tone_detected;		/* what tone has been detected */
	int			tone_count;		/* how long has that tone been detected */
	double			phaseshift256[2];	/* how much the phase of sine wave changes per sample */
	double			phase256;		/* current phase */
	int			telegramm;		/* set, if there is a valid telegram */
	int16_t			*telegramm_spl;		/* 16 * samples_per_bit */
	int			telegramm_pos;		/* current sample position in telegramm_spl */

	/* loopback test for latency */
	int			loopback_count;		/* count digits from 0 to 9 */
	double			loopback_time[10];	/* time stamp when sending digits 0..9 */
} bnetz_t;

double bnetz_kanal2freq(int kanal, int unterband);
int bnetz_init(void);
int bnetz_create(int kanal, const char *sounddev, int samplerate, int cross_channels, double rx_gain, int gfs, int pre_emphasis, int de_emphasis, const char *write_wave, const char *read_wave, int loopback, double loss_factor, const char *pilot);
void bnetz_destroy(sender_t *sender);
void bnetz_loss_indication(bnetz_t *bnetz);
void bnetz_receive_tone(bnetz_t *bnetz, int bit);
void bnetz_receive_telegramm(bnetz_t *bnetz, uint16_t telegramm, double level, double quality);
const char *bnetz_get_telegramm(bnetz_t *bnetz);

