
/* definition of a tone */
enum tones_tone {
	TONES_TONE_OFF =		0, /* disable tone, output stream is not changed */
	TONES_TONE_HANGUP =		16,
	TONES_TONE_BUSY =		17,
	TONES_TONE_NOANSWER =		19,
	TONES_TONE_OUTOFORDER =		27,
	TONES_TONE_INVALIDNUMBER =	28,
	TONES_TONE_CONGESTION =		34,
	TONES_TONE_SIT =		128,
	TONES_TONE_DIALTONE =		129,
	TONES_TONE_DIALTONE_SPECIAL =	130,
	TONES_TONE_RINGBACK =		131,
	TONES_TONE_RECALL =		132, /* Off-the-air call received, waiting for mobile subscriber to answer. */
	TONES_TONE_CW =			133, /* call waiting tone */
	TONES_TONE_AUFSCHALTTON =	134, /* old German tone to indicate that operator joined the call */
	TONES_TONE_SPECIAL =		192, /* multiple special tones 192..254 */
	TONES_TONE_SILENCE =		255, /* just silent audio data */
};

/* type of tone data */
enum tones_tdata {
	TONES_TDATA_EOL = 0,		/* end of list */
	TONES_TDATA_SLIN16HOST,		/* tone is linear audio, host format */
	TONES_TDATA_ALAW,		/* tone is a-law coded */
	TONES_TDATA_ULAW,		/* tone is mu-law coded */
	TONES_TDATA_ALAWFLIPPED,	/* tone is a-law coded */
	TONES_TDATA_ULAWFLIPPED,	/* tone is mu-law coded */
};

#define TONES_DURATION_AUTO 0

/* Array: tone sequence */
typedef struct tones_seq {
	enum tones_tdata tdata;
	int duration;
	void *spl_data;
	size_t spl_data_size;
	double db;
} tones_seq_t;

/* Array: tone set for one country/epoche */
typedef struct tones_set {
	enum tones_tone tone;
	tones_seq_t *seq;
} tones_set_t;

/* Rendered tones from tone set after init */
typedef struct tones_data {
	void *spl_data[256];		/* sample data of all tones */
	int spl_duration[256];		/* duration in sample */
	int spl_repeat[256];		/* length of sample data in memory */
	size_t spl_size;		/* size of one encoded sample */
} tones_data_t;

/* Rendered tones from tone set after init */
typedef struct tones {
	tones_data_t *data;		/* current data set */
	enum tones_tone tone;		/* current tone */
	int spl_pos;			/* current read position in sample */
} tones_t;

void tones_list_tonesets(void);
int tones_init(tones_data_t *data, const char *toneset, enum tones_tdata coding);
void tones_exit(tones_data_t *data);
void tones_set_tone(tones_data_t *data, tones_t *t, enum tones_tone tone);
void tones_read_tone(tones_t *t, void *spl, int spl_count);

