
typedef struct wave_rec {
	FILE		*fp;
	int		channels;
	double		max_deviation;
	int		samplerate;
	uint32_t	written;	/* how much samples written */
	/* thread stuff */
	pthread_t	tid;		/* file io thread id */
	int		finish;		/* indicates end of thread */
	uint8_t		*buffer;	/* buffer to store sample data */
	int		buffer_size;	/* size of buffer in bytes */
	int		buffer_readp;	/* read pointer to next byte in buffer */
	int		buffer_writep;	/* write pointer to next byte in buffer */
} wave_rec_t;

typedef struct wave_play {
	FILE		*fp;
	int		channels;
	double		max_deviation;
	uint32_t	left;		/* how much samples left */
	/* thread stuff */
	pthread_t	tid;		/* file io thread id */
	int		finish;		/* indicates end of thread */
	uint8_t		*buffer;	/* buffer to store sample data */
	int		buffer_size;	/* size of buffer in bytes */
	int		buffer_readp;	/* read pointer to next byte in buffer */
	int		buffer_writep;	/* write pointer to next byte in buffer */
} wave_play_t;

int wave_create_record(wave_rec_t *rec, const char *filename, int samplerate, int channels, double max_deviation);
int wave_create_playback(wave_play_t *play, const char *filename, int *samplerate_p, int *channels_p, double max_deviation);
int wave_read(wave_play_t *play, sample_t **samples, int length);
int wave_write(wave_rec_t *rec, sample_t **samples, int length);
void wave_destroy_record(wave_rec_t *rec);
void wave_destroy_playback(wave_play_t *play);

