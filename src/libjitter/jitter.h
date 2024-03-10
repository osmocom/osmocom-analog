
#define JITTER_FLAG_NONE	0		// no flags at all
#define	JITTER_FLAG_LATENCY	(1 << 0)	// keep latency close to target_window_duration
#define	JITTER_FLAG_REPEAT	(1 << 1)	// repeat audio to extrapolate gaps

/* window settings for low latency audio and extrapolation of gaps */
#define JITTER_AUDIO		0.060, 1.000, JITTER_FLAG_LATENCY | JITTER_FLAG_REPEAT
/* window settings for analog data (fax/modem) or digial data (HDLC) */
#define JITTER_DATA		0.100, 0.200, JITTER_FLAG_NONE

typedef struct jitter_frame {
	struct jitter_frame *next;
	void (*decoder)(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void *priv);
	void *decoder_priv;
	uint8_t marker;
	uint16_t sequence;
	uint32_t timestamp;
	uint32_t ssrc;
	int size;
	uint8_t data[0];
} jitter_frame_t;

typedef struct jitter {
	char name[64];

	/* frame properties */
	double sample_duration;		/* duration of a frame (ms) */
	int samples_20ms;		/* samples to compensate a gap of unknown size */

	/* window properties */
	bool unlocked;			/* jitter buffer will be locked until some reads from it */
	uint32_t window_flags;		/* flags to alter behaviour of jitter buffer */
	int target_window_size;	/* target size of window (frames) */
	int max_window_size;	/* maximum size of window (frames) */
	bool window_valid;		/* set, if first frame has been received */
	uint32_t window_ssrc;		/* current sync source of window */
	uint32_t window_timestamp;	/* lowest timestamp number in window */

	/* reduction of delay */
	double delay_interval;		/* interval for delay measurement (seconds) */
	double delay_counter;		/* current counter to count interval (seconds) */
	int min_delay;			/* minimum delay measured during interval (frames) */

	/* list of frames */
	jitter_frame_t *frame_list;

	/* sample buffer (optional) */
	uint8_t *spl_buf;		/* current samples buffer */
	int spl_pos;			/* position of in buffer */
	int spl_len;			/* total buffer size */
	bool spl_valid;			/* if buffer has valid frame (not repeated) */

} jitter_t;

int jitter_create(jitter_t *jb, const char *name, double samplerate, double target_window_duration, double max_window_duration, uint32_t window_flags);
void jitter_reset(jitter_t *jb);
void jitter_destroy(jitter_t *jb);
jitter_frame_t *jitter_frame_alloc(void (*decoder)(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void *priv), void *decoder_priv, uint8_t *data, int size, uint8_t marker, uint16_t sequence, uint32_t timestamp, uint32_t ssrc);
void jitter_frame_free(jitter_frame_t *jf);
void jitter_frame_get(jitter_frame_t *jf, void (**decoder)(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void *priv), void **decoder_priv, uint8_t **data, int *size, uint8_t *marker, uint16_t *sequence, uint32_t *timestamp, uint32_t *ssrc);
void jitter_save(jitter_t *jb, jitter_frame_t *jf);
int32_t jitter_offset(jitter_t *jb);
jitter_frame_t *jitter_load(jitter_t *jb);
void jitter_advance(jitter_t *jb, uint32_t offset);
void jitter_load_samples(jitter_t *jb, uint8_t *spl, int len, size_t sample_size, void (*conceal)(uint8_t *spl, int len, void *priv), void *conceal_priv);
void jitter_conceal_s16(uint8_t *_spl, int len, void __attribute__((unused)) *priv);

