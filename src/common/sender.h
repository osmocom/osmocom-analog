#include "sound.h"
#include "wave.h"
#include "samplerate.h"
#include "jitter.h"
#include "loss.h"
#include "emphasis.h"

/* common structure of each transmitter */
typedef struct sender {
	struct sender		*next;

	/* call reference */
	int			callref;

	/* system info */
	int			kanal;			/* channel number */

	/* sound */
	void			*sound;
	int			samplerate;
	samplerate_t		srstate;		/* sample rate conversion state */
	int			pre_emphasis;		/* use pre_emhasis, done by sender */
	int			de_emphasis;		/* use de_emhasis, done by sender */
	emphasis_t		estate;			/* pre and de emphasis */

	/* loopback test */
	int			loopback;		/* 0 = off, 1 = internal, 2 = external */

	/* record and playback */
	wave_rec_t		wave_rec;		/* wave recording */
	wave_play_t		wave_play;		/* wave playback */

	/* audio buffer for audio to send to transmitter (also used as loopback buffer) */
	jitter_t		audio;

	/* audio buffer for audio to send to caller (20ms = 160 samples @ 8000Hz) */
	int16_t			rxbuf[160];
	int			rxbuf_pos;		/* current fill of buffer */

	/* loss of carrier detection */
	double			loss_volume;
	loss_t			loss;

	/* pilot tone */
	int			use_pilot_signal;	/* -1 if not used, 1 for positive, 0 for negative, 2 for tone */
	int			pilot_on;		/* 1 or 0 for on or off */
	double			pilotton_phaseshift;	/* phase to shift every sample */
	double			pilotton_phase; 	/* current phase */
} sender_t;

/* list of all senders */
extern sender_t *sender_head;

int sender_create(sender_t *sender, const char *sounddev, int samplerate, int pre_emphasis, int de_emphasis, const char *write_wave, const char *read_wave, int kanal, int loopback, double loss_volume, int use_pilot_signal);
void sender_destroy(sender_t *sender);
void sender_send(sender_t *sender, int16_t *samples, int count);
void sender_receive(sender_t *sender, int16_t *samples, int count);
void main_loop(int *quit, int latency);

