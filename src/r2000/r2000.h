#include "../libcompandor/compandor.h"
#include "../libtimer/timer.h"
#include "../libmobile/sender.h"
#include "../libmobile/call.h"
#include "../libfsk/fsk.h"

enum dsp_mode {
	DSP_MODE_OFF,		/* no transmission */
	DSP_MODE_AUDIO_TX,	/* stream audio (TX only) */
	DSP_MODE_AUDIO_TX_RX,	/* stream audio */
	DSP_MODE_FRAME,		/* send frames */
};

enum r2000_chan_type {
	CHAN_TYPE_CC,		/* calling channel */
	CHAN_TYPE_TC,		/* traffic channel */
	CHAN_TYPE_CC_TC,	/* combined CC + TC */
};

enum r2000_state {
	STATE_NULL = 0,		/* power off state */
	STATE_IDLE,		/* channel is not in use */
	STATE_INSCRIPTION,	/* SM registers */
	STATE_OUT_ASSIGN,	/* assign outgoing call on CC */
	STATE_IN_ASSIGN,	/* assign incoming call on CC */
	STATE_RECALL_ASSIGN,	/* assign outgoing recall on CC */
	STATE_OUT_IDENT,	/* identity outgoing call on TC */
	STATE_IN_IDENT,		/* identity incoming call on TC */
	STATE_RECALL_IDENT,	/* identity outgoing recall on TC */
	STATE_OUT_DIAL1,	/* dialing outgoing call on TC */
	STATE_OUT_DIAL2,	/* dialing outgoing call on TC */
	STATE_SUSPEND,		/* suspend after dialing outgoing call on TC */
	STATE_RECALL_WAIT,	/* wait for calling back the phone */
	STATE_IN_ALERT,		/* alerting incoming call on TC */
	STATE_OUT_ALERT,	/* alerting outgoing call on TC */
	STATE_RECALL_ALERT,	/* alerting outgoing recall on TC */
	STATE_ACTIVE,		/* channel is in use */
	STATE_RELEASE_CC,	/* release call on CC */
	STATE_RELEASE_TC,	/* release call on TC */
};

typedef struct r2000_subscriber {
	uint8_t			type;			/* mobile station type */
	uint16_t		relais;			/* home relais */
	uint16_t		mor;			/* mobile ID */
	char			dialing[21];		/* dial string */
} r2000_subscriber_t;

typedef struct r2000_sysinfo {
	enum r2000_chan_type	chan_type;		/* channel type */
	uint8_t			deport;			/* sub-station number */
	uint8_t			agi;			/* inscription parameter */
	uint8_t			sm_power;		/* station mobile power 1 = high */
	uint8_t			taxe;			/* rate parameter */
	uint16_t		relais;			/* relais ID */
	uint8_t			crins;			/* response to inscription */
	uint8_t			nconv;			/* supervisory value */
	int			recall;			/* do a recall when called party answered */
} r2000_sysinfo_t;

typedef struct r2000 {
	sender_t		sender;
	r2000_sysinfo_t		sysinfo;
	compandor_t		cstate;
	int			pre_emphasis;		/* use pre_emphasis by this instance */
	int			de_emphasis;		/* use de_emphasis by this instance */
	emphasis_t		estate;

	/* sender's states */
	enum r2000_state	state;
	int			callref;
	struct timer		timer;
	r2000_subscriber_t	subscriber;
	int			page_try;		/* the try number of calling the mobile */
	int			tx_frame_count;		/* to count repeated frames */

	/* display measurements */
	dispmeasparam_t		*dmp_frame_level;
	dispmeasparam_t		*dmp_frame_quality;
	dispmeasparam_t		*dmp_super_level;
	dispmeasparam_t		*dmp_super_quality;

	/* features */
	int			compandor;		/* if compandor shall be used */

	/* dsp states */
	enum dsp_mode		dsp_mode;		/* current mode: audio, durable tone 0 or 1, paging */
	fsk_mod_t		fsk_mod;		/* fsk processing */
	fsk_demod_t		fsk_demod;
	char			tx_frame[208];		/* carries bits of one frame to transmit */
	int			tx_frame_length;
	int			tx_frame_pos;
	uint16_t		rx_sync;		/* shift register to detect sync */
	int			rx_in_sync;		/* if we are in sync and receive bits */
	int			rx_mute;		/* mute count down after sync */
	int			rx_max;			/* maximum bits to receive (including 32 bits sync sequence) */
	char			rx_frame[177];		/* receive frame (one extra byte to terminate string) */
	int			rx_count;		/* next bit to receive */
	double			rx_level[256];		/* level infos */
	double			rx_quality[256];	/* quality infos */
	uint64_t		rx_bits_count;		/* sample counter */
	uint64_t		rx_bits_count_current;	/* sample counter of current frame */
	uint64_t		rx_bits_count_last;	/* sample counter of last frame */

	/* supervisory dsp states */
	fsk_mod_t		super_fsk_mod;		/* fsk processing */
	fsk_demod_t		super_fsk_demod;
	uint32_t		super_tx_word;		/* supervisory info to transmit */
	int			super_tx_word_length;
	int			super_tx_word_pos;
	iir_filter_t		super_tx_hp;		/* filters away the speech that overlaps with the supervisory */
	uint32_t		super_rx_word;		/* shift register for received supervisory info */
	double			super_rx_level[20];	/* level infos */
	double			super_rx_quality[20];	/* quality infos */
	int			super_rx_index;		/* index for level and quality buffer */
	iir_filter_t		super_rx_hp;		/* filters away the supervisory */
double super_bittime;
double super_bitpos;

} r2000_t;

void r2000_channel_list(void);
int r2000_channel_by_short_name(const char *short_name);
const char *chan_type_short_name(enum r2000_chan_type chan_type);
const char *chan_type_long_name(enum r2000_chan_type chan_type);
int r2000_create(int band, const char *kanal, enum r2000_chan_type chan_type, const char *device, int use_sdr, int samplerate, double rx_gain, double tx_gain, int pre_emphasis, int de_emphasis, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, const char *read_tx_wave, uint16_t relais, uint8_t deport, uint8_t agi, uint8_t sm_power, uint8_t taxe, uint8_t crins, int destruction, uint8_t nconv, int recall, int loopback);
void r2000_check_channels(void);
void r2000_destroy(sender_t *sender);
void r2000_go_idle(r2000_t *r2000);
void r2000_band_list(void);
double r2000_channel2freq(int band, int channel, int uplink);
const char *r2000_number_valid(const char *number);
const char *r2000_get_frame(r2000_t *r2000);
void r2000_receive_frame(r2000_t *r2000, const char *bits, double quality, double level);
void r2000_receive_super(r2000_t *r2000, uint8_t super, double quality, double level);

