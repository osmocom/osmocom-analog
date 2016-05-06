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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "debug.h"
#include "sender.h"
#include "timer.h"
#include "call.h"

sender_t *sender_head = NULL;
static sender_t **sender_tailp = &sender_head;
int cant_recover = 0;

/* Init transceiver instance and link to list of transceivers. */
int sender_create(sender_t *sender, int kanal, const char *sounddev, int samplerate, int cross_channels, double rx_gain, int pre_emphasis, int de_emphasis, const char *write_wave, const char *read_wave, int loopback, double loss_volume, int use_pilot_signal)
{
	sender_t *master;
	int rc = 0;

	PDEBUG(DSENDER, DEBUG_DEBUG, "Creating 'Sender' instance\n");

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
		if (!strcmp(master->sounddev, sounddev))
			break;
	}
	if (master) {
		if (master->slave) {
			PDEBUG(DSENDER, DEBUG_ERROR, "Sound device '%s' cannot be used for channel %d. It is already shared by channel %d and %d!\n", sounddev, kanal, master->kanal, master->slave->kanal);
			rc = -EBUSY;
			goto error;
		}
		if (!sound_is_stereo_capture(master->sound) || !sound_is_stereo_playback(master->sound)) {
			PDEBUG(DSENDER, DEBUG_ERROR, "Sound device '%s' cannot be used for more than one channel, because one direction is mono!\n", sounddev);
			rc = -EBUSY;
			goto error;
		}
		if (master->use_pilot_signal >= 0) {
			PDEBUG(DSENDER, DEBUG_ERROR, "Cannot share sound device with channel %d, because second channel is used for pilot signal!\n", master->kanal);
			rc = -EBUSY;
			goto error;
		}
		if (use_pilot_signal >= 0) {
			PDEBUG(DSENDER, DEBUG_ERROR, "Cannot share sound device with channel %d, because we need a stereo channel for pilot signal!\n", master->kanal);
			rc = -EBUSY;
			goto error;
		}
		/* link us to a master */
		master->slave = sender;
		sender->master = master;
	} else {
		/* open own device */
		sender->sound = sound_open(sounddev, samplerate);
		if (!sender->sound) {
			PDEBUG(DSENDER, DEBUG_ERROR, "No sound device!\n");

			rc = -EIO;
			goto error;
		}
	}

	sender->samplerate = samplerate;
	sender->cross_channels = cross_channels;
	sender->rx_gain = rx_gain;
	sender->pre_emphasis = pre_emphasis;
	sender->de_emphasis = de_emphasis;
	sender->kanal = kanal;
	sender->loopback = loopback;
	sender->loss_volume = loss_volume;
	sender->use_pilot_signal = use_pilot_signal;
	sender->pilotton_phaseshift = 1.0 / ((double)samplerate / 1000.0);
	strncpy(sender->sounddev, sounddev, sizeof(sender->sounddev) - 1);

	rc = init_samplerate(&sender->srstate, samplerate);
	if (rc < 0) {
		PDEBUG(DSENDER, DEBUG_ERROR, "Failed to init sample rate conversion!\n");
		goto error;
	}

	rc = jitter_create(&sender->audio, samplerate / 5);
	if (rc < 0) {
		PDEBUG(DSENDER, DEBUG_ERROR, "Failed to create and init audio buffer!\n");
		goto error;
	}

	if (write_wave) {
		rc = wave_create_record(&sender->wave_rec, write_wave, samplerate);
		if (rc < 0) {
			PDEBUG(DSENDER, DEBUG_ERROR, "Failed to create WAVE recoding instance!\n");
			goto error;
		}
	}
	if (read_wave) {
		rc = wave_create_playback(&sender->wave_play, read_wave, samplerate);
		if (rc < 0) {
			PDEBUG(DSENDER, DEBUG_ERROR, "Failed to create WAVE playback instance!\n");
			goto error;
		}
	}

	rc = init_emphasis(&sender->estate, samplerate);
	if (rc < 0)
		goto error;

	*sender_tailp = sender;
	sender_tailp = &sender->next;

	return 0;
error:
	sender_destroy(sender);

	return rc;
}

/* Destroy transceiver instance and unlink from list. */
void sender_destroy(sender_t *sender)
{
	PDEBUG(DSENDER, DEBUG_DEBUG, "Destroying 'Sender' instance\n");

	sender_tailp = &sender_head;
	while (*sender_tailp) {
		if (sender == *sender_tailp)
			*sender_tailp = (*sender_tailp)->next;
		else
			sender_tailp = &((*sender_tailp)->next);
	}

	if (sender->slave)
		sender->slave->master = NULL;
	if (sender->master)
		sender->master->slave = NULL;

	if (sender->sound)
		sound_close(sender->sound);

	wave_destroy_record(&sender->wave_rec);
	wave_destroy_playback(&sender->wave_play);

	jitter_destroy(&sender->audio);
}

static void gen_pilotton(sender_t *sender, int16_t *samples, int length)
{
	double phaseshift, phase;
	int i;

	phaseshift = sender->pilotton_phaseshift;
	phase = sender->pilotton_phase;

	for (i = 0; i < length; i++) {
		if (phase < 0.5)
			*samples++ = 30000;
		else
			*samples++ = -30000;
		phase += phaseshift;
		if (phase >= 1.0)
			phase -= 1.0;
	}

	sender->pilotton_phase = phase;
}

static void gain_samples(int16_t *samples, int length, double gain)
{
	int i;
	int32_t sample;

	for (i = 0; i < length; i++) {
		sample = (int32_t)((double)(*samples) * gain);
		if (sample > 32767)
			sample = 32767;
		else if (sample < -32768)
			sample = -32768;
		*samples++ = sample;
	}
}

/* Handle audio streaming of one transceiver. */
static void process_sender_audio(sender_t *sender, int *quit, int latspl)
{
	sender_t *slave = sender->slave;
	int16_t samples[latspl], pilot[latspl], slave_samples[latspl];
	int rc, count;

	count = sound_get_inbuffer(sender->sound);
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
	if (count < latspl) {
		count = latspl - count;
		/* load TX data from audio loop or from sender instance */
		if (sender->loopback == 3)
			jitter_load(&sender->audio, samples, count);
		else
			sender_send(sender, samples, count);
		/* internal loopback: loop back TX audio to RX */
		if (sender->loopback == 1) {
			if (sender->wave_rec.fp)
				wave_write(&sender->wave_rec, samples, count);
			sender_receive(sender, samples, count);
		}
		/* do pre emphasis towards radio, not wave_write */
		if (sender->pre_emphasis)
			pre_emphasis(&sender->estate, samples, count);
		/* do above for audio slave, if set */
		if (slave) {
			if (slave->loopback == 3)
				jitter_load(&slave->audio, slave_samples, count);
			else
				sender_send(slave, slave_samples, count);
			/* internal loopback, if audio slave is set */
			if (slave && slave->loopback == 1) {
				if (slave->wave_rec.fp)
					wave_write(&slave->wave_rec, slave_samples, count);
				sender_receive(slave, slave_samples, count);
			}
			/* do pre emphasis towards radio, not wave_write */
			if (slave->pre_emphasis)
				pre_emphasis(&slave->estate, slave_samples, count);
		}
		switch (sender->use_pilot_signal) {
		case 2:
			/* tone if pilot signal is on */
			if (sender->pilot_on)
				gen_pilotton(sender, pilot, count);
			else
				memset(pilot, 0, count << 1);
			if (!sender->cross_channels)
				rc = sound_write(sender->sound, samples, pilot, count);
			else
				rc = sound_write(sender->sound, pilot, samples, count);
			break;
		case 1:
			/* positive signal if pilot signal is on */
			if (sender->pilot_on)
				memset(pilot, 127, count << 1);
			else
				memset(pilot, 128, count << 1);
			if (!sender->cross_channels)
				rc = sound_write(sender->sound, samples, pilot, count);
			else
				rc = sound_write(sender->sound, pilot, samples, count);
			break;
		case 0:
			/* negative signal if pilot signal is on */
			if (sender->pilot_on)
				memset(pilot, 128, count << 1);
			else
				memset(pilot, 127, count << 1);
			if (!sender->cross_channels)
				rc = sound_write(sender->sound, samples, pilot, count);
			else
				rc = sound_write(sender->sound, pilot, samples, count);
			break;
		default:
			/* if audio slave is set, write audio of both sender instances */
			if (slave) {
				if (!sender->cross_channels)
					rc = sound_write(sender->sound, samples, slave_samples, count);
				else
					rc = sound_write(sender->sound, slave_samples, samples, count);
			}else {
				/* use pilot tone buffer for silence */
				memset(pilot, 0, count << 1);
				if (!sender->cross_channels)
					rc = sound_write(sender->sound, samples, pilot, count);
				else
					rc = sound_write(sender->sound, pilot, samples, count);
			}
		}
		if (rc < 0) {
			PDEBUG(DSENDER, DEBUG_ERROR, "Failed to write TX data to sound device (rc = %d)\n", rc);
			if (rc == -EPIPE) {
				if (cant_recover)
					goto cant_recover;
				PDEBUG(DSENDER, DEBUG_ERROR, "Trying to recover!\n");
			}
			return;
		}
	}

	if (!sender->cross_channels)
		count = sound_read(sender->sound, samples, slave_samples, latspl);
	else
		count = sound_read(sender->sound, slave_samples, samples, latspl);
//printf("count=%d time= %.4f\n", count, (double)count * 1000 / sender->samplerate);
	if (count < 0) {
		PDEBUG(DSENDER, DEBUG_ERROR, "Failed to read from sound device (rc = %d)!\n", count);
		if (count == -EPIPE) {
			if (cant_recover)
				goto cant_recover;
			PDEBUG(DSENDER, DEBUG_ERROR, "Trying to recover!\n");
		}
		return;
	}
	if (count) {
		/* rx gain */
		if (sender->rx_gain != 1.0)
			gain_samples(samples, count, sender->rx_gain);
		/* do de emphasis from radio (then write_wave/wave_read), receive audio, process echo test */
		if (sender->de_emphasis)
			de_emphasis(&sender->estate, samples, count);
		if (sender->wave_play.fp)
			wave_read(&sender->wave_play, samples, count);
		if (sender->loopback != 1) {
			if (sender->wave_rec.fp)
				wave_write(&sender->wave_rec, samples, count);
			sender_receive(sender, samples, count);
		}
		if (sender->loopback == 3)
			jitter_save(&sender->audio, samples, count);
		/* do above for audio slave, if set */
		if (slave) {
			if (sender->rx_gain != 1.0)
				gain_samples(slave_samples, count, slave->rx_gain);
			if (slave->de_emphasis)
				de_emphasis(&slave->estate, slave_samples, count);
			if (slave->wave_play.fp)
				wave_read(&slave->wave_play, slave_samples, count);
			if (slave->loopback != 1) {
				if (slave->wave_rec.fp)
					wave_write(&slave->wave_rec, slave_samples, count);
				sender_receive(slave, slave_samples, count);
			}
			if (slave->loopback == 3)
				jitter_save(&slave->audio, slave_samples, count);
		}
	}
}

/* Loop through all transceiver instances of one network. */
void main_loop(int *quit, int latency)
{
	int latspl;
	sender_t *sender;
	double last_time = 0, now;

	while(!(*quit)) {
		/* process sound of all transceivers */
		for (sender = sender_head; sender; sender = sender->next) {
			/* do not process audio for an audio slave, since it is done by audio master */
			if (sender->master) /* if master is set, we are an audio slave */
				continue;
			latspl = sender->samplerate * latency / 1000;
			process_sender_audio(sender, quit, latspl);
		}

		/* process timers */
		process_timer();

		/* process audio for mncc call instances */
		now = get_time();
		if (now - last_time >= 0.1)
			last_time = now;
		if (now - last_time >= 0.020) {
			last_time += 0.020;
			/* call clock every 20ms */
			call_mncc_clock();
		}

		/* process audio of built-in call control */
		if (process_call())
			break;

		/* sleep a while */
		usleep(1000);
	}
}

