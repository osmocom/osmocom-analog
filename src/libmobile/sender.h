#ifdef HAVE_ALSA
#include "../libsound/sound.h"
#endif
#ifdef HAVE_SDR
#include "../libsdr/sdr.h"
#endif
#include "../libwave/wave.h"
#include "../libsamplerate/samplerate.h"
#include "../libjitter/jitter.h"
#include "../libemphasis/emphasis.h"
#include "../libdisplay/display.h"

#define MAX_SENDER	16

/* how to send a 'paging' signal (trigger transmitter) */
enum paging_signal {
	PAGING_SIGNAL_NONE = 0,
	PAGING_SIGNAL_TONE,
	PAGING_SIGNAL_NOTONE,
	PAGING_SIGNAL_POSITIVE,
	PAGING_SIGNAL_NEGATIVE,
};

/* common structure of each transmitter */
typedef struct sender {
	struct sender		*next;
	struct sender		*slave;			/* points to 'slave' that uses next channel of audio device */
	struct sender		*master;		/* if set, the audio device is owned by 'master' */

	/* system info */
	const char		*kanal;			/* channel number */
	double			sendefrequenz;		/* transmitter frequency */
	double			empfangsfrequenz;	/* receiver frequency */
	double			ruffrequenz;		/* special paging frequency used for B-Netz */

	/* FM/AM levels */
	int			am;			/* use AM instead of FM */
	double			max_deviation;		/* max frequency deviation / level */
	double			max_modulation;		/* max frequency modulated */
	double			speech_deviation;	/* deviation / level of 1000 Hz reference tone at speech level */
	double			modulation_index;	/* AM modulation index */
	double			max_display;		/* level of displaying wave form */

	/* audio */
	void			*audio;
	char			device[64];		/* audio device name (alsa or sdr) */
	void			*(*audio_open)(const char *, double *, double *, int *, int, double, int, int, double, double, double, double);
	int 			(*audio_start)(void *);
	void 			(*audio_close)(void *);
	int			(*audio_write)(void *, sample_t **, uint8_t **, int, enum paging_signal *, int *, int);
	int			(*audio_read)(void *, sample_t **, int, int, double *);
	int			(*audio_get_tosend)(void *, int);
	int			samplerate;
	samplerate_t		srstate;		/* sample rate conversion state */
	double			rx_gain;		/* factor of level to apply on RX samples */
	double			tx_gain;		/* factor of level to apply on TX samples */
	int			pre_emphasis;		/* use pre_emhasis, done by sender */
	int			de_emphasis;		/* use de_emhasis, done by sender */
	emphasis_t		estate;			/* pre and de emphasis */

	/* loopback test */
	int			loopback;		/* 0 = off, 1 = internal, 2 = external */

	/* record and playback */
	const char		*write_rx_wave;		/* file name pointers */
	const char		*write_tx_wave;
	const char		*read_rx_wave;
	const char		*read_tx_wave;
	wave_rec_t		wave_rx_rec;		/* wave recording (from rx) */
	wave_rec_t		wave_tx_rec;		/* wave recording (from tx) */
	wave_play_t		wave_rx_play;		/* wave playback (as rx) */
	wave_play_t		wave_tx_play;		/* wave playback (as tx) */

	/* audio buffer for audio to send to transmitter (also used as loopback buffer) */
	jitter_t		dejitter;

	/* audio buffer for audio to send to caller (20ms = 160 samples @ 8000Hz) */
	sample_t		rxbuf[160];
	int			rxbuf_pos;		/* current fill of buffer */

	/* paging tone */
	enum paging_signal	paging_signal;		/* if paging signal is used and how it is performed */
	int			paging_on;		/* 1 or 0 for on or off */

	/* display wave */
	dispwav_t		dispwav;		/* display wave form */

	/* display measurements */
	dispmeas_t		dispmeas;		/* display measurements */
} sender_t;

extern sender_t *sender_head;
extern int cant_recover;
extern int check_channel;

int sender_create(sender_t *sender, const char *kanal, double sendefrequenz, double empfangsfrequenz, const char *device, int use_sdr, int samplerate, double rx_gain, double tx_gain, int pre_emphasis, int de_emphasis, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, const char *read_tx_wave, int loopback, enum paging_signal paging_signal);
void sender_destroy(sender_t *sender);
void sender_set_fm(sender_t *sender, double max_deviation, double max_modulation, double speech_deviation, double max_display);
void sender_set_am(sender_t *sender, double max_modulation, double speech_deviation, double max_display, double modulation_index);
int sender_open_audio(int buffer_size, double interval);
int sender_start_audio(void);
void process_sender_audio(sender_t *sender, int *quit, int buffer_size);
void sender_send(sender_t *sender, sample_t *samples, uint8_t *power, int count);
void sender_receive(sender_t *sender, sample_t *samples, int count, double rf_level_db);
void sender_paging(sender_t *sender, int on);
sender_t *get_sender_by_empfangsfrequenz(double freq);

