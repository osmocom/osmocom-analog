#include "../common/sender.h"

enum dsp_mode {
	DSP_MODE_SILENCE,	/* send silence to transmitter, block audio from receiver */
	DSP_MODE_AUDIO,		/* stream audio */
	DSP_MODE_TONE,		/* send 2280 Hz tone 0 */
	DSP_MODE_PAGING,	/* send four paging tones */
};

enum anetz_state {
	ANETZ_FREI,		/* sending 2280 Hz tone */
	ANETZ_GESPRAECH,	/* during conversation */
	ANETZ_ANRUF,		/* phone is paged */
	ANETZ_AUSLOESEN,	/* releasing towards phone */
};

typedef struct anetz {
	sender_t		sender;

	/* sender's states */
	enum anetz_state	state;			/* current sender's state */
	char			station_id[8];		/* current station ID */
	struct timer		timer;

	/* dsp states */
	enum dsp_mode		dsp_mode;		/* current mode: audio, durable tone 0 or 1, paging */
	int			fsk_tone_coeff[2];	/* coefficient k = 2*cos(2*PI*f/samplerate), k << 15 */
	int			samples_per_chunk;	/* how many samples lasts one chunk */
	int16_t			*fsk_filter_spl;	/* array with samples_per_chunk */
	int			fsk_filter_pos;		/* current sample position in filter_spl */
	int			tone_detected;		/* what tone has been detected */
	int			tone_count;		/* how long has that tone been detected */
	double			tone_phaseshift256;	/* how much the phase of sine wave changes per sample */
	double			tone_phase256;		/* current phase */
	double			paging_phaseshift256[4];/* how much the phase of sine wave changes per sample */
	double			paging_phase256[4];	/* current phase */
	int			paging_tone;		/* current tone (0..3) in sequenced mode */
	int			paging_count;		/* current sample count of tone in seq. mode */
} anetz_t;


double anetz_kanal2freq(int kanal, int unterband);
int anetz_init(void);
int anetz_create(const char *sounddev, int samplerate, int pre_emphasis, int de_emphasis, const char *write_wave, const char *read_wave, int kanal, int loopback, double loss_volume);
void anetz_destroy(sender_t *sender);
void anetz_loss_indication(anetz_t *anetz);
void anetz_receive_tone(anetz_t *anetz, int bit);

