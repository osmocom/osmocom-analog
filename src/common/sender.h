#include "sound.h"
#include "wave.h"
#include "samplerate.h"
#include "jitter.h"
#include "loss.h"
#include "emphasis.h"
#include "display_wave.h"

#define MAX_SENDER	16

/* how to send a 'pilot' signal (trigger transmitter) */
enum pilot_signal {
	PILOT_SIGNAL_NONE = 0,
	PILOT_SIGNAL_TONE,
	PILOT_SIGNAL_NOTONE,
	PILOT_SIGNAL_POSITIVE,
	PILOT_SIGNAL_NEGATIVE,
};

/* common structure of each transmitter */
typedef struct sender {
	struct sender		*next;
	struct sender		*slave;			/* points to 'slave' that uses next channel of audio device */
	struct sender		*master;		/* if set, the audio device is owned by 'master' */

	/* system info */
	int			kanal;			/* channel number */

	/* sound */
	void			*sound;
	char			sounddev[64];		/* sound device name */
	int			samplerate;
	samplerate_t		srstate;		/* sample rate conversion state */
	double			rx_gain;		/* factor of level to apply on rx samples */
	int			pre_emphasis;		/* use pre_emhasis, done by sender */
	int			de_emphasis;		/* use de_emhasis, done by sender */
	emphasis_t		estate;			/* pre and de emphasis */

	/* loopback test */
	int			loopback;		/* 0 = off, 1 = internal, 2 = external */

	/* record and playback */
	wave_rec_t		wave_rx_rec;		/* wave recording (from rx) */
	wave_rec_t		wave_tx_rec;		/* wave recording (from tx) */
	wave_play_t		wave_rx_play;		/* wave playback (as rx) */

	/* audio buffer for audio to send to transmitter (also used as loopback buffer) */
	jitter_t		audio;

	/* audio buffer for audio to send to caller (20ms = 160 samples @ 8000Hz) */
	int16_t			rxbuf[160];
	int			rxbuf_pos;		/* current fill of buffer */

	/* loss of carrier detection */
	double			loss_volume;
	loss_t			loss;

	/* pilot tone */
	enum pilot_signal	pilot_signal;		/* if pilot signal is used and how it is performed */
	int			pilot_on;		/* 1 or 0 for on or off */
	double			pilotton_phaseshift;	/* phase to shift every sample */
	double			pilotton_phase; 	/* current phase */

	/* display wave */
	dispwav_t		dispwav;		/* display wave form */
} sender_t;

/* list of all senders */
extern sender_t *sender_head;
extern int cant_recover;

int sender_create(sender_t *sender, int kanal, const char *sounddev, int samplerate, double rx_gain, int pre_emphasis, int de_emphasis, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, int loopback, double loss_volume, enum pilot_signal pilot_signal);
void sender_destroy(sender_t *sender);
void process_sender_audio(sender_t *sender, int *quit, int latspl);
void sender_send(sender_t *sender, int16_t *samples, int count);
void sender_receive(sender_t *sender, int16_t *samples, int count);
void sender_pilot(sender_t *sender, int on);

