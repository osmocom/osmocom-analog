
#define JITTER_FLAG_NONE	0		// no flags at all
#define	JITTER_FLAG_LATENCY	(1 << 0)	// keep latency close to target_window_duration
#define	JITTER_FLAG_REPEAT	(1 << 1)	// repeat audio to extrapolate gaps

/* window settings for low latency audio and extrapolation of gaps */
#define JITTER_AUDIO		0.050, 1.000, JITTER_FLAG_LATENCY | JITTER_FLAG_REPEAT
/* window settings for analog data (fax/modem) or digial data (HDLC) */
#define JITTER_DATA		0.100, 0.200, JITTER_FLAG_NONE

typedef struct jitter_frame {
	struct jitter_frame *next;
	uint16_t sequence;
	uint32_t timestamp;
	int length;
	uint8_t samples[0];
} jitter_frame_t;

typedef struct jitter {
	char name[64];

	/* sample properties */
	int sample_size;
	double sample_duration;

	/* automatic sequence generation */
	uint16_t next_sequence;
	uint32_t next_timestamp;

	/* window properties */
	int unlocked;
	uint32_t window_flags;
	int target_window_size;
	int max_window_size;
	int window_valid;
	uint32_t window_ssrc;
	uint32_t window_timestamp;

	/* reduction of delay */
	double delay_interval;
	double delay_counter;
	int32_t min_delay_value;

	/* extrapolation */
	int extra_size;
	int extra_index;
	void *extra_samples;
	int extra_timeout_max;
	int extra_timeout_count;

	/* list of frames */
	jitter_frame_t *frame_list;
} jitter_t;

int jitter_create(jitter_t *jb, const char *name, double samplerate, int sample_size, double target_window_duration, double max_window_duration, uint32_t window_flags);
void jitter_reset(jitter_t *jb);
void jitter_destroy(jitter_t *jb);
void jitter_save(jitter_t *jb, void *samples, int length, int has_sequence, uint16_t sequence, uint32_t timestamp, uint32_t ssrc);
void jitter_load(jitter_t *jb, void *samples, int length);

