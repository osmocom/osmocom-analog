
#define BITS_PER_SUPERFRAME	12672.0	/* super frame (Oberrahmen) has duration of excactly 2.4 seconds */
#define BITS_PER_BLOCK		198.0	/* block has duration of excactly 37.5 milli seconds */
#define BITS_PER_SPK_BLOCK	66.0	/* spk block has a duration of exactly 12.5 milli seconds */

/* fsk rx sync state */
enum fsk_sync {
	FSK_SYNC_NONE = 0,
	FSK_SYNC_POSITIVE,
	FSK_SYNC_NEGATIVE,
};

typedef struct cnetz cnetz_t;

typedef struct fsk_fm_demod {
	cnetz_t		*cnetz;			/* pointer back to cnetz instance */

	/* clock */
	double		bit_time;		/* current time in bits inside superframe */
	double		bit_time_uncorrected;	/* same as above, but not corrected by sync */

	/* bit detection */
	sample_t	*bit_buffer_spl;	/* samples ring buffer */
	int		bit_buffer_len;		/* number of samples in ring buffer */
	int		bit_buffer_half;	/* half of ring buffer */
	int		bit_buffer_pos;		/* current position to write next sample */
	int		level_threshold;	/* threshold for detection of next level change */
	double		bits_per_sample;	/* duration of one sample in bits */
	double		next_bit;		/* count time to detect bits */
	int		bit_count;		/* counts bits, to match 4 bits at distributed signaling */
	int		last_change_positive;	/* flags last level change direction */
	enum fsk_sync	sync;			/* set, if we are in sync and what polarity we receive */
	double		sync_level;		/* what was the level, when we received the sync */
	double		sync_time;		/* when did we receive sync, relative to super frame */
	double		sync_jitter;		/* what was the jitter of the sync */

	/* speech */
	sample_t	*speech_buffer;		/* holds one chunk of 12.5ms */
	int		speech_size;
	int		speech_count;

	/* bit decoder */
	uint64_t	rx_sync;		/* sync shift register */
	char		rx_buffer[151];		/* 150 bits + termination */
	int		rx_buffer_count;	/* counter when receiving bits */

	/* statistics */
	int		change_levels[256];	/* ring buffer to store levels */
	double		change_when[256];	/* ring buffer to store time when level has changed */
	uint8_t		change_pos;		/* index for next write */
} fsk_fm_demod_t;

int fsk_fm_init(fsk_fm_demod_t *fsk, cnetz_t *cnetz, int samplerate, double bitrate);
void fsk_fm_exit(fsk_fm_demod_t *fsk);
void fsk_fm_demod(fsk_fm_demod_t *fsk, sample_t *samples, int length);
void fsk_correct_sync(fsk_fm_demod_t *fsk, double offset);
void fsk_copy_sync(fsk_fm_demod_t *fsk_to, fsk_fm_demod_t *fsk_from);
void fsk_demod_reset(fsk_fm_demod_t *fsk);

