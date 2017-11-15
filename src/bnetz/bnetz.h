#include "../libsquelch/squelch.h"
#include "../common/fsk.h"
#include "../common/sender.h"
#include "../libtimer/timer.h"

/* fsk modes of transmission */
enum dsp_mode {
	DSP_MODE_SILENCE,	/* sending silence */
	DSP_MODE_AUDIO,		/* stream audio */
	DSP_MODE_AUDIO_METER,	/* stream audio */
	DSP_MODE_0,		/* send tone 0 */
	DSP_MODE_1,		/* send tone 1 */
	DSP_MODE_TELEGRAMM,	/* send "Telegramm" */
};

/* current state of b-netz sender */
enum bnetz_state {
	BNETZ_NULL = 0,
	BNETZ_FREI,		/* sending 'Gruppenfreisignal' */
	BNETZ_WAHLABRUF,	/* sending 'Wahlabruf', receiving call setup */
	BNETZ_SELEKTIVRUF_EIN,	/* paging phone (switch to calling channel) */
	BNETZ_SELEKTIVRUF_AUS,	/* paging phone (wait before switching back) */
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

/* current dialing type (metering support) */
enum dial_type {
	DIAL_TYPE_NOMETER,
	DIAL_TYPE_METER,
	DIAL_TYPE_METER_MUENZ,
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
	int			metering;		/* use metering pulses in seconds 0 = off, < 0 = force */

	/* switch sender to channel 19 */
	char			paging_file[256];	/* if set, write to given file to switch to channel 19 or back */
	char			paging_on[256];		/* what to write to switch to channel 19 */
	char			paging_off[256];	/* what to write to switch back */
	int			paging_is_on;		/* set, if we are on channel 19. also used to switch back on exit */

	/* all bnetz states */
	enum bnetz_state	state;			/* main state of sender */
	int			callref;		/* call reference */
	enum dial_mode		dial_mode;		/* sub state while dialing is received */
	enum dial_type		dial_type;		/* defines if mobile supports metering pulses */
	char			dial_number[14];	/* dial string received */
	int			dial_pos;		/* current position while receiving digits */
	char			station_id[6];		/* current station ID */
	int			station_id_pos;		/* position while transmitting */
	enum page_mode		page_mode;		/* sub state while paging */
	int			page_try;		/* try number (1 or 2) */
	struct timer		timer;
	int			trenn_count;		/* count number of release messages */

	/* display measurements */
	dispmeasparam_t		*dmp_tone_level;
	dispmeasparam_t		*dmp_tone_quality;
	dispmeasparam_t		*dmp_frame_level;
	dispmeasparam_t		*dmp_frame_stddev;
	dispmeasparam_t		*dmp_frame_quality;

	/* dsp states */
	enum dsp_mode		dsp_mode;		/* current mode: audio, durable tone 0 or 1, "Telegramm" */
	fsk_t			fsk;			/* fsk modem instance */
	uint16_t		rx_telegramm;		/* rx shift register for receiveing telegramm */
	double			rx_telegramm_quality[16];/* quality of each bit in telegramm */
	double			rx_telegramm_level[16];	/* level of each bit in telegramm */
	int			rx_telegramm_qualidx;	/* index of quality array above */
	int			tone_detected;		/* what tone has been detected */
	int			tone_count;		/* how long has that tone been detected */
	const char		*tx_telegramm;		/* carries bits of one frame to transmit */
	int			tx_telegramm_pos;
	double			meter_phaseshift65536;	/* how much the phase of sine wave changes per sample */
	double			meter_phase65536;	/* current phase */
	squelch_t		squelch;		/* squelch detection process */

	/* loopback test for latency */
	int			loopback_count;		/* count digits from 0 to 9 */
	double			loopback_time[10];	/* time stamp when sending digits 0..9 */
} bnetz_t;

double bnetz_kanal2freq(int kanal, int unterband);
int bnetz_init(void);
int bnetz_create(int kanal, const char *audiodev, int use_sdr, int samplerate, double rx_gain, int gfs, int pre_emphasis, int de_emphasis, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, const char *read_tx_wave, int loopback, double squelch_db, const char *paging, int metering);
void bnetz_destroy(sender_t *sender);
void bnetz_loss_indication(bnetz_t *bnetz, double loss_time);
void bnetz_receive_tone(bnetz_t *bnetz, int bit);
void bnetz_receive_telegramm(bnetz_t *bnetz, uint16_t telegramm, double level_avg, double level_dev, double quality_avg);
const char *bnetz_get_telegramm(bnetz_t *bnetz);

