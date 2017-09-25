/* Common transceiver functions
 *
 * (C) 2016 by Andreas Eversberg <jolly@eversberg.eu>
 * All Rights Reserved
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define CHAN sender->kanal

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "sample.h"
#include "debug.h"
#include "sender.h"
#include "timer.h"

/* debug time consumption of audio processing */
//#define DEBUG_TIME_CONSUMPTION

sender_t *sender_head = NULL;
static sender_t **sender_tailp = &sender_head;
int cant_recover = 0;

/* Init transceiver instance and link to list of transceivers. */
int sender_create(sender_t *sender, int kanal, double sendefrequenz, double empfangsfrequenz, const char *audiodev, int use_sdr, int samplerate, double rx_gain, int pre_emphasis, int de_emphasis, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, const char *read_tx_wave, int loopback, double loss_volume, enum paging_signal paging_signal)
{
	sender_t *master, *slave;
	int rc = 0;

	sender->kanal = kanal;
	sender->sendefrequenz = sendefrequenz;
	sender->empfangsfrequenz = (loopback) ? sendefrequenz : empfangsfrequenz;
	strncpy(sender->audiodev, audiodev, sizeof(sender->audiodev) - 1);
	sender->samplerate = samplerate;
	sender->rx_gain = rx_gain;
	sender->pre_emphasis = pre_emphasis;
	sender->de_emphasis = de_emphasis;
	sender->loopback = loopback;
	sender->loss_volume = loss_volume;
	sender->paging_signal = paging_signal;
	sender->write_rx_wave = write_rx_wave;
	sender->write_tx_wave = write_tx_wave;
	sender->read_rx_wave = read_rx_wave;
	sender->read_tx_wave = read_tx_wave;

	/* no gain with SDR */
	if (use_sdr)
		sender->rx_gain = 1.0;

	PDEBUG_CHAN(DSENDER, DEBUG_DEBUG, "Creating 'Sender' instance\n");

	/* if we find a channel that uses the same device as we do,
	 * we will link us as slave to this master channel. then we 
	 * receive and send audio via second channel of the device
	 * of the master channel.
	 */
	for (master = sender_head; master; master = master->next) {
		if (master->kanal == kanal) {
			PDEBUG(DSENDER, DEBUG_ERROR, "Channel %d may not be defined for multiple transceivers!\n", kanal);
			rc = -EIO;
			goto error;
		}
		if (abs(master->kanal - kanal) == 1) {
			PDEBUG(DSENDER, DEBUG_NOTICE, "------------------------------------------------------------------------\n");
			PDEBUG(DSENDER, DEBUG_NOTICE, "NOTE: Channel %d is next to channel %d. This will cause interferences.\n", kanal, master->kanal);
			PDEBUG(DSENDER, DEBUG_NOTICE, "Please use at least one channel distance to avoid that.\n");
			PDEBUG(DSENDER, DEBUG_NOTICE, "------------------------------------------------------------------------\n");
		}
		if (!strcmp(master->audiodev, audiodev))
			break;
	}
	if (master) {
		if (master->paging_signal != PAGING_SIGNAL_NONE && !use_sdr) {
			PDEBUG(DSENDER, DEBUG_ERROR, "Cannot share audio device with channel %d, because its second audio channel is used for paging signal! Use different audio device.\n", master->kanal);
			rc = -EBUSY;
			goto error;
		}
		if (paging_signal != PAGING_SIGNAL_NONE && !use_sdr) {
			PDEBUG(DSENDER, DEBUG_ERROR, "Cannot share audio device with channel %d, because we need a second audio channel for paging signal! Use different audio device.\n", master->kanal);
			rc = -EBUSY;
			goto error;
		}
		/* link us to a master */
		sender->master = master;
		/* link master (or last slave) to us */
		for (slave = master; ; slave = slave->slave) {
			if (!slave->slave)
				break;
		}
		slave->slave = sender;
	} else {
		/* link audio device */
#ifdef HAVE_SDR
		if (use_sdr) {
			sender->audio_open = sdr_open;
			sender->audio_start = sdr_start;
			sender->audio_close = sdr_close;
			sender->audio_read = sdr_read;
			sender->audio_write = sdr_write;
			sender->audio_get_tosend = sdr_get_tosend;
		} else
#endif
		{
			sender->audio_open = sound_open;
			sender->audio_start = sound_start;
			sender->audio_close = sound_close;
			sender->audio_read = sound_read;
			sender->audio_write = sound_write;
			sender->audio_get_tosend = sound_get_tosend;
		}
	}

	rc = init_samplerate(&sender->srstate, 8000.0, (double)samplerate, 3300.0);
	if (rc < 0) {
		PDEBUG(DSENDER, DEBUG_ERROR, "Failed to init sample rate conversion!\n");
		goto error;
	}

	rc = jitter_create(&sender->dejitter, samplerate / 5);
	if (rc < 0) {
		PDEBUG(DSENDER, DEBUG_ERROR, "Failed to create and init audio buffer!\n");
		goto error;
	}

	rc = init_emphasis(&sender->estate, samplerate, CUT_OFF_EMPHASIS_DEFAULT);
	if (rc < 0)
		goto error;

	*sender_tailp = sender;
	sender_tailp = &sender->next;

	display_wave_init(sender, samplerate);
	display_measurements_init(sender, samplerate);

	return 0;
error:
	sender_destroy(sender);

	return rc;
}

int sender_open_audio(int latspl)
{
	sender_t *master, *inst;
	int channels;
	int i;
	int rc;

	for (master = sender_head; master; master = master->next) {
		/* skip audio slaves */
		if (master->master)
			continue;

		/* get list of frequencies */
		channels = 0;
		for (inst = master; inst; inst = inst->slave) {
			channels++;
		}
		double tx_f[channels], rx_f[channels], paging_frequency = 0.0;
		for (i = 0, inst = master; inst; i++, inst = inst->slave) {
			tx_f[i] = inst->sendefrequenz;
			rx_f[i] = inst->empfangsfrequenz;
			if (inst->ruffrequenz)
				paging_frequency = inst->ruffrequenz;
		}

		if (master->write_rx_wave) {
			rc = wave_create_record(&master->wave_rx_rec, master->write_rx_wave, master->samplerate, channels, master->max_deviation);
			if (rc < 0) {
				PDEBUG(DSENDER, DEBUG_ERROR, "Failed to create WAVE recoding instance!\n");
				return rc;
			}
		}
		if (master->write_tx_wave) {
			rc = wave_create_record(&master->wave_tx_rec, master->write_tx_wave, master->samplerate, channels, master->max_deviation);
			if (rc < 0) {
				PDEBUG(DSENDER, DEBUG_ERROR, "Failed to create WAVE recoding instance!\n");
				return rc;
			}
		}
		if (master->read_rx_wave) {
			rc = wave_create_playback(&master->wave_rx_play, master->read_rx_wave, master->samplerate, channels, master->max_deviation);
			if (rc < 0) {
				PDEBUG(DSENDER, DEBUG_ERROR, "Failed to create WAVE playback instance!\n");
				return rc;
			}
		}
		if (master->read_tx_wave) {
			rc = wave_create_playback(&master->wave_tx_play, master->read_tx_wave, master->samplerate, channels, master->max_deviation);
			if (rc < 0) {
				PDEBUG(DSENDER, DEBUG_ERROR, "Failed to create WAVE playback instance!\n");
				return rc;
			}
		}

		/* open device */
		master->audio = master->audio_open(master->audiodev, tx_f, rx_f, channels, paging_frequency, master->samplerate, latspl, master->max_deviation, master->max_modulation);
		if (!master->audio) {
			PDEBUG(DSENDER, DEBUG_ERROR, "No audio device!\n");
			return -EIO;
		}
	}

	return 0;
}

int sender_start_audio(void)
{
	sender_t *master;
	int rc = 0;

	for (master = sender_head; master; master = master->next) {
		/* skip audio slaves */
		if (master->master)
			continue;

		rc = master->audio_start(master->audio);
		if (rc)
			break;
	}

	return rc;
}

/* Destroy transceiver instance and unlink from list. */
void sender_destroy(sender_t *sender)
{
	PDEBUG_CHAN(DSENDER, DEBUG_DEBUG, "Destroying 'Sender' instance\n");

	sender_tailp = &sender_head;
	while (*sender_tailp) {
		if (sender == *sender_tailp)
			*sender_tailp = (*sender_tailp)->next;
		else
			sender_tailp = &((*sender_tailp)->next);
	}

	if (sender->audio) {
		sender->audio_close(sender->audio);
		sender->audio = NULL;
	}

	wave_destroy_record(&sender->wave_rx_rec);
	wave_destroy_record(&sender->wave_tx_rec);
	wave_destroy_playback(&sender->wave_rx_play);
	wave_destroy_playback(&sender->wave_tx_play);

	jitter_destroy(&sender->dejitter);
}

void sender_set_fm(sender_t *sender, double max_deviation, double max_modulation, double dBm0_deviation, double max_display)
{
	sender->max_deviation = max_deviation;
	sender->max_modulation = max_modulation;
	sender->dBm0_deviation = dBm0_deviation;
	sender->max_display = max_display;

	PDEBUG_CHAN(DSENDER, DEBUG_DEBUG, "Maxium deviation: %.1f kHz, Maximum modulation: %.1f kHz\n", max_deviation / 1000.0, max_modulation / 1000.0);
	PDEBUG_CHAN(DSENDER, DEBUG_DEBUG, "Deviation at dBm0 (audio level): %.1f kHz\n", dBm0_deviation / 1000.0);
}

static void gain_samples(sample_t *samples, int length, double gain)
{
	int i;

	for (i = 0; i < length; i++)
		*samples++ *= gain;
}

/* Handle audio streaming of one transceiver. */
void process_sender_audio(sender_t *sender, int *quit, int latspl)
{
	sender_t *inst;
	int rc, count;
	int num_chan, i;
#ifdef DEBUG_TIME_CONSUMPTION
	static double t1, t2, t3, t4, t5, d1 = 0, d2 = 0, d3 = 0, d4 = 0, s = 0;
#endif

	/* count instances for audio channel */
	for (num_chan = 0, inst = sender; inst; num_chan++, inst = inst->slave);
	sample_t buff[num_chan][latspl], *samples[num_chan];
	uint8_t pbuff[num_chan][latspl], *power[num_chan];
	enum paging_signal paging_signal[num_chan];
	int on[num_chan];
	for (i = 0; i < num_chan; i++) {
		samples[i] = buff[i];
		power[i] = pbuff[i];
	}

#ifdef DEBUG_TIME_CONSUMPTION
	t1 = get_time();
#endif
	count = sender->audio_get_tosend(sender->audio, latspl);
	if (count < 0) {
		PDEBUG(DSENDER, DEBUG_ERROR, "Failed to get samples in buffer (rc = %d)!\n", count);
		if (count == -EPIPE) {
			if (cant_recover) {
cant_recover:
				PDEBUG(DSENDER, DEBUG_ERROR, "Cannot recover due to measurements, quitting!\n");
				*quit = 1;
				return;
			}
			PDEBUG(DSENDER, DEBUG_ERROR, "Trying to recover!\n");
		}
		return;
	}
#ifdef DEBUG_TIME_CONSUMPTION
	t2 = get_time();
#endif
	if (count > 0) {
		/* limit to our buffer */
		if (count > latspl)
			count = latspl;
		/* loop through all channels */
		for (i = 0, inst = sender; inst; i++, inst = inst->slave) {
			/* load TX data from audio loop or from sender instance */
			if (inst->loopback == 3)
				jitter_load(&inst->dejitter, samples[i], count);
			else
				sender_send(inst, samples[i], power[i], count);
			/* internal loopback: loop back TX audio to RX */
			if (inst->loopback == 1) {
				display_wave(inst, samples[i], count, inst->max_display);
				sender_receive(inst, samples[i], count);
			}
			/* do pre emphasis towards radio */
			if (inst->pre_emphasis)
				pre_emphasis(&inst->estate, samples[i], count);
			/* normal level to frequency deviation of dBm0 */
			gain_samples(samples[i], count, inst->dBm0_deviation);
			/* set paging signal */
			paging_signal[i] = inst->paging_signal;
			on[i] = inst->paging_on;
		}

#ifdef DEBUG_TIME_CONSUMPTION
		t2 = get_time();
#endif
		if (sender->wave_tx_rec.fp)
			wave_write(&sender->wave_tx_rec, samples, count);
		if (sender->wave_tx_play.fp)
			wave_read(&sender->wave_tx_play, samples, count);

		rc = sender->audio_write(sender->audio, samples, power, count, paging_signal, on, num_chan);
		if (rc < 0) {
			PDEBUG(DSENDER, DEBUG_ERROR, "Failed to write TX data to audio device (rc = %d)\n", rc);
			if (rc == -EPIPE) {
				if (cant_recover)
					goto cant_recover;
				PDEBUG(DSENDER, DEBUG_ERROR, "Trying to recover!\n");
			}
			return;
		}
	}
#ifdef DEBUG_TIME_CONSUMPTION
	t3 = get_time();
#endif

	count = sender->audio_read(sender->audio, samples, latspl, num_chan);
	if (count < 0) {
		/* special case when audio_read wants us to quit */
		if (count == -EPERM) {
			*quit = 1;
			return;
		}
		PDEBUG(DSENDER, DEBUG_ERROR, "Failed to read from audio device (rc = %d)!\n", count);
		if (count == -EPIPE) {
			if (cant_recover)
				goto cant_recover;
			PDEBUG(DSENDER, DEBUG_ERROR, "Trying to recover!\n");
		}
		return;
	}
#ifdef DEBUG_TIME_CONSUMPTION
	t4 = get_time();
#endif
	if (count) {
		if (sender->wave_rx_rec.fp)
			wave_write(&sender->wave_rx_rec, samples, count);
		if (sender->wave_rx_play.fp)
			wave_read(&sender->wave_rx_play, samples, count);

		/* loop through all channels */
		for (i = 0, inst = sender; inst; i++, inst = inst->slave) {
			/* frequency deviation of dBm0 to normal level */
			gain_samples(samples[i], count, 1.0 / inst->dBm0_deviation);
			/* rx gain */
			if (inst->rx_gain != 1.0)
				gain_samples(samples[i], count, inst->rx_gain);
			/* do filter and de-emphasis from radio receive audio, process echo test */
			if (inst->de_emphasis) {
				dc_filter(&inst->estate, samples[i], count);
				de_emphasis(&inst->estate, samples[i], count);
			}
			if (inst->loopback != 1) {
				display_wave(inst, samples[i], count, inst->max_display);
				sender_receive(inst, samples[i], count);
			}
			if (inst->loopback == 3)
				jitter_save(&inst->dejitter, samples[i], count);
		}
	}
#ifdef DEBUG_TIME_CONSUMPTION
	t5 = get_time();
	d1 += (t2 - t1);
	d2 += (t3 - t2);
	d3 += (t4 - t3);
	d4 += (t5 - t4);
	if (get_time() - s >= 1.0) {
		printf("duration: %.3f (process TX), %.3f (send TX), %.3f (receive RX), %.3f (process RX)\n", d1, d2, d3, d4);
		s = get_time();
		d1 = d2 = d3 = d4 = 0;
	}
#endif
}

void sender_paging(sender_t *sender, int on)
{
	sender->paging_on = on;
}

sender_t *get_sender_by_empfangsfrequenz(double freq)
{
	sender_t *sender;

	for (sender = sender_head; sender; sender = sender->next) {
		if (sender->empfangsfrequenz == freq)
			return sender;
	}

	return NULL;
}

