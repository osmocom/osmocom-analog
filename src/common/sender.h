#include "../libsound/sound.h"
#ifdef HAVE_SDR
#include "../libsdr/sdr.h"
#endif
#include "../libwave/wave.h"
#include "../libsamplerate/samplerate.h"
#include "../libjitter/jitter.h"
#include "../libemphasis/emphasis.h"
#include "display.h"

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
	int			kanal;			/* channel number */
	double			sendefrequenz;		/* transmitter frequency */
	double			empfangsfrequenz;	/* receiver frequency */
	double			ruffrequenz;		/* special paging frequency used for B-Netz */

	/* fm levels */
	double			max_deviation;		/* max frequency deviation */
	double			max_modulation;		/* max frequency modulated */
	double			dBm0_deviation;		/* deviation of 1000 Hz reference tone at dBm0 */
	double			max_display;		/* level of displaying wave form */

	/* audio */
	void			*audio;
	char			audiodev[64];		/* audio device name (alsa or sdr) */
	void			*(*audio_open)(const char *, double *, double *, int, double, int, int, double, double);
	int 			(*audio_start)(void *);
	void 			(*audio_close)(void *);
	int			(*audio_write)(void *, sample_t **, uint8_t **, int, enum paging_signal *, int *, int);
	int			(*audio_read)(void *, sample_t **, int, int, double *);
	int			(*audio_get_tosend)(void *, int);
	int			samplerate;
	samplerate_t		srstate;		/* sample rate conversion state */
	double			rx_gain;		/* factor of level to apply on rx samples */
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

/* list of all senders */
extern sender_t *sender_head;
extern int cant_recover;

int sender_create(sender_t *sender, int kanal, double sendefrequenz, double empfangsfrequenz, const char *audiodev, int use_sdr, int samplerate, double rx_gain, int pre_emphasis, int de_emphasis, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, const char *read_tx_wave, int loopback, enum paging_signal paging_signal);
void sender_destroy(sender_t *sender);
void sender_set_fm(sender_t *sender, double max_deviation, double max_modulation, double dBm0_deviation, double max_display);
int sender_open_audio(int latspl);
int sender_start_audio(void);
void process_sender_audio(sender_t *sender, int *quit, int latspl);
void sender_send(sender_t *sender, sample_t *samples, uint8_t *power, int count);
void sender_receive(sender_t *sender, sample_t *samples, int count, double rf_level_db);
void sender_paging(sender_t *sender, int on);
sender_t *get_sender_by_empfangsfrequenz(double freq);

