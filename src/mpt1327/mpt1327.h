#include "../libsquelch/squelch.h"
#include "../libfsk/fsk.h"
#include "../libmobile/sender.h"

enum mpt1327_band {
	BAND_MPT1343_SUB1 = 0,
	BAND_MPT1343_SUB2,
	BAND_REGIONET43_SUB1,
	BAND_REGIONET43_SUB2,
};

enum mpt1327_chan_type {
	CHAN_TYPE_CC,		/* control channel */
	CHAN_TYPE_TC,		/* traffic channel */
	CHAN_TYPE_CC_TC,	/* combined CC + TC */
};

enum mpt1327_state {
	STATE_NULL = 0,
	STATE_IDLE,
	STATE_BUSY,
};

enum dsp_mode {
	DSP_MODE_OFF = 0,	/* no transmission, no reception */
	DSP_MODE_CONTROL,	/* send/receive codewords */
	DSP_MODE_TRAFFIC,	/* send/receive codewords and audio */
};

enum mpt1327_sched_state {
	/* states on control channel */
	SCHED_STATE_CC_IDLE,
	SCHED_STATE_CC_STARTUP,
	SCHED_STATE_CC_CCSC,
	SCHED_STATE_CC_ADDR,
	SCHED_STATE_CC_DATA,
	/* states on traffic channel */
	SCHED_STATE_TC_IDLE,
	SCHED_STATE_TC_SYNT,
	SCHED_STATE_TC_ADDR,
	SCHED_STATE_TC_DATA,
};

enum mpt1327_called_type {
	CALLED_TYPE_UNIT,	/* call to radio unit / line unit / group */
	CALLED_TYPE_INTERPFX,	/* same as above with different prefix */
	CALLED_TYPE_SYSTEM,	/* system wide call */
	CALLED_TYPE_PBX_SHORT,	/* call to short PBX extenstion */
	CALLED_TYPE_PBX_LONG,	/* call to long PBX extenstion */
	CALLED_TYPE_PSTN_PRE,	/* call to PSTN with prearranged number */
	CALLED_TYPE_PSTN_LONG1,	/* call to PSTN with 1..9 digits */
	CALLED_TYPE_PSTN_LONG2,	/* call to PSTN with 10..31 digits */
};

typedef struct mpt1327_sysdef {
	uint16_t		sys;			/* system idenity */
        int			wt;
	int			per;
	int			pon;
	double			timeout;
	int			framelength;
	int			bcast_slots;
} mpt1327_sysdef_t;

typedef struct mpt1327_tx_sched {
	enum mpt1327_sched_state state;			/* what was currently scheduled */
	int			frame_length;		/* number of slots in frame */
	int			frame_count;		/* current slot number */
	int			dummy_slot;		/* set, if next slot uses a dummy AHY */
	int			bcast_count;		/* counts slots until sending broadcast */
} mpt1327_tx_sched_t;

typedef struct mpt1327_rx_sched {
	int			data_num;		/* set if N data words are awaited */
	int			data_count;		/* count data words */
	int			data_word;		/* what data word to parse */
	uint8_t			data_prefix;		/* unit that requires that data word */
	uint16_t		data_ident;
} mpt1327_rx_sched_t;

struct mpt1327;

typedef struct mpt1327_unit {
	struct mpt1327_unit	*next;
	uint64_t		state;
	int			repeat;			/* number of repeating messages / retries after timeout */
	struct timer		timer;			/* timeout waiting for unit response */
	struct mpt1327		*tc;			/* link to transceiver */
	uint8_t			prefix;			/* unit's prefix */
	uint16_t		ident;			/* unit's ident */
	uint8_t			called_prefix;
	uint16_t		called_ident;
	enum mpt1327_called_type called_type;
	char			called_number[33];	/* 0+number+'\0' */
	uint32_t		callref;		/* PBX/PSTN link to call control */
} mpt1327_unit_t;

typedef struct mpt1327 {
	sender_t		sender;
	enum mpt1327_band	band;
	enum mpt1327_chan_type	chan_type;

	/* sender's states */
	enum mpt1327_state	state;			/* current sender's state */
	struct timer		timer;			/* inactivity timer to clear channel */
	mpt1327_unit_t		*unit;			/* link to unit */

	/* display measurements */
	dispmeasparam_t		*dmp_frame_level;
	dispmeasparam_t		*dmp_frame_quality;
	dispmeasparam_t		*dmp_super_level;
	dispmeasparam_t		*dmp_super_quality;

	/* scheduler states */
	mpt1327_tx_sched_t	tx_sched;		/* downlink scheduler states, see above */
	mpt1327_rx_sched_t	rx_sched;		/* uplink scheduler states, see above */

	/* dsp states */
	int			repeater;		/* in repeater mode the received audio is repeated */
	jitter_t		repeater_dejitter;	/* forwarding audio */
	int			pressel_on;		/* set if somebody transmitting on TC */
	enum dsp_mode		dsp_mode;		/* current mode: audio, durable tone 0 or 1, paging */
	fsk_mod_t		fsk_mod;		/* fsk processing */
	fsk_demod_t		fsk_demod;
	uint16_t		sync_word;		/* current sync word for channel */
	uint16_t		rx_sync;		/* shift register to detect sync */
	int			rx_in_sync;		/* if we are in sync and receive bits */
	int			rx_mute;		/* set, if currently receiving a message */
	uint64_t		rx_bits;		/* receive frame (one extra byte to terminate string) */
	int			rx_count;		/* next bit to receive */
	double			rx_level[256];		/* level infos */
	double			rx_quality[256];	/* quality infos */
	uint64_t		tx_bits;		/* carries bits of one frame to transmit */
	int			tx_bit_num;		/* number of bits to tansmit, or 0, if no transmission */
	int			tx_count;		/* next bit to transmit */
	squelch_t		squelch;		/* squelch detection process */
} mpt1327_t;

void init_sysdef (uint16_t sys, int wt, int per, int pon, int timeout);
void flush_units(void);
double mpt1327_channel2freq(enum mpt1327_band band, int channel, int uplink);
const char *mpt1327_band_name(enum mpt1327_band band);
void mpt1327_band_list(void);
int mpt1327_band_by_short_name(const char *short_name);
void mpt1327_channel_list(void);
int mpt1327_channel_by_short_name(const char *short_name);
const char *chan_type_short_name(enum mpt1327_chan_type chan_type);
const char *chan_type_long_name(enum mpt1327_chan_type chan_type);
int mpt1327_create(enum mpt1327_band band, const char *kanal, enum mpt1327_chan_type chan_type, const char *device, int use_sdr, int samplerate, double rx_gain, double tx_gain, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, const char *read_tx_wave, int loopback, double squelch_db);
void mpt1327_check_channels(void);
void mpt1327_destroy(sender_t *sender);
void mpt1327_receive_codeword(mpt1327_t *mpt1327, uint64_t bits, double quality, double level);
int mpt1327_send_codeword(mpt1327_t *mpt1327, uint64_t *bits);
void mpt1327_signal_indication(mpt1327_t *mpt1327);

