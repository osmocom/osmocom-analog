#include "../common/goertzel.h"
#include "../common/sender.h"

enum dsp_mode {
	DSP_MODE_SILENCE,	/* send silence to transmitter, block audio from receiver */
	DSP_MODE_AUDIO,		/* stream audio */
	DSP_MODE_TONE,		/* send 2280 Hz tone 0 */
	DSP_MODE_PAGING,	/* send four paging tones */
};

enum anetz_state {
	ANETZ_NULL = 0,
	ANETZ_FREI,		/* sending 2280 Hz tone */
	ANETZ_GESPRAECH,	/* during conversation */
	ANETZ_ANRUF,		/* phone is paged */
	ANETZ_AUSLOESEN,	/* releasing towards phone */
};

typedef struct anetz {
	sender_t		sender;

	/* sender's states */
	enum anetz_state	state;			/* current sender's state */
	int			callref;		/* call reference */
	char			station_id[8];		/* current station ID */
	struct timer		timer;

	/* display measurements */
	dispmeasparam_t		*dmp_tone_level;
	dispmeasparam_t		*dmp_tone_quality;

	/* dsp states */
	enum dsp_mode		dsp_mode;		/* current mode: audio, durable tone 0 or 1, paging */
	goertzel_t		fsk_tone_goertzel[2];	/* filter for tone decoding */
	int			samples_per_chunk;	/* how many samples lasts one chunk */
	sample_t		*fsk_filter_spl;	/* array with samples_per_chunk */
	int			fsk_filter_pos;		/* current sample position in filter_spl */
	int			tone_detected;		/* what tone has been detected */
	int			tone_count;		/* how long has that tone been detected */
	double			tone_phaseshift65536;	/* how much the phase of sine wave changes per sample */
	double			tone_phase65536;	/* current phase */
	double			page_gain;		/* factor to raise the paging tones */
	int			page_sequence;		/* if set, use paging tones in sequence rather than parallel */
	double			paging_phaseshift65536[4];/* how much the phase of sine wave changes per sample */
	double			paging_phase65536[4];	/* current phase */
	int			paging_tone;		/* current tone (0..3) in sequenced mode */
	int			paging_count;		/* current sample count of tone in seq. mode */
	int			paging_transition;	/* set to number of samples during transition */
} anetz_t;


double anetz_kanal2freq(int kanal, int unterband);
int anetz_init(void);
int anetz_create(int kanal, const char *audiodev, int use_sdr, int samplerate, double rx_gain, double page_gain, int page_sequence, int pre_emphasis, int de_emphasis, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, const char *read_tx_wave, int loopback, double loss_volume);
void anetz_destroy(sender_t *sender);
void anetz_loss_indication(anetz_t *anetz);
void anetz_receive_tone(anetz_t *anetz, int bit);

