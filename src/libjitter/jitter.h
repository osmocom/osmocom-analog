
typedef struct jitter {
	sample_t		*spl;			/* pointer to sample buffer */
	int			len;			/* buffer size: number of samples */
	int			inptr, outptr;		/* write pointer and read pointer */
} jitter_t;

int jitter_create(jitter_t *jitter, int length);
void jitter_reset(jitter_t *jitter);
void jitter_destroy(jitter_t *jitter);
void jitter_save(jitter_t *jb, sample_t *samples, int length);
void jitter_load(jitter_t *jb, sample_t *samples, int length);
void jitter_clear(jitter_t *jb);

