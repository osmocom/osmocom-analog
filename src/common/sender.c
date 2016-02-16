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
int sender_create(sender_t *sender, const char *sounddev, int samplerate, int pre_emphasis, int de_emphasis, const char *write_wave, const char *read_wave, int kanal, int loopback, double loss_volume, int use_pilot_signal)
{
	int rc = 0;

	PDEBUG(DSENDER, DEBUG_DEBUG, "Creating 'Sender' instance\n");

	sender->samplerate = samplerate;
	sender->pre_emphasis = pre_emphasis;
	sender->de_emphasis = de_emphasis;
	sender->kanal = kanal;
	sender->loopback = loopback;
	sender->loss_volume = loss_volume;
	sender->use_pilot_signal = use_pilot_signal;
	sender->pilotton_phaseshift = 1.0 / ((double)samplerate / 1000.0);

	sender->sound = sound_open(sounddev, samplerate);
	if (!sender->sound) {
		PDEBUG(DSENDER, DEBUG_ERROR, "No sound device!\n");

		rc = -EIO;
		goto error;
	}

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
			*sender_tailp = sender->next;
		sender_tailp = &sender->next;
	}

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

/* Handle audio streaming of one transceiver. */
void process_sender(sender_t *sender, int *quit, int latspl)
{
	int16_t samples[latspl], pilot[latspl];
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
		if (sender->loopback == 3)
			jitter_load(&sender->audio, samples, count);
		else
			sender_send(sender, samples, count);
		if (sender->pre_emphasis)
			pre_emphasis(&sender->estate, samples, count);
		switch (sender->use_pilot_signal) {
		case 2:
			/* tone if pilot signal is on */
			if (sender->pilot_on)
				gen_pilotton(sender, pilot, count);
			else
				memset(pilot, 0, count << 1);
			rc = sound_write(sender->sound, samples, pilot, count);
			break;
		case 1:
			/* positive signal if pilot signal is on */
			if (sender->pilot_on)
				memset(pilot, 127, count << 1);
			else
				memset(pilot, 128, count << 1);
			rc = sound_write(sender->sound, samples, pilot, count);
			break;
		case 0:
			/* negative signal if pilot signal is on */
			if (sender->pilot_on)
				memset(pilot, 128, count << 1);
			else
				memset(pilot, 127, count << 1);
			rc = sound_write(sender->sound, samples, pilot, count);
			break;
		default:
			rc = sound_write(sender->sound, samples, samples, count);
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
		if (sender->loopback == 1) {
			if (sender->wave_rec.fp)
				wave_write(&sender->wave_rec, samples, count);
			sender_receive(sender, samples, count);
		}
	}

	count = sound_read(sender->sound, samples, latspl);
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
		if (sender->de_emphasis)
			de_emphasis(&sender->estate, samples, count);
		if (sender->wave_play.fp)
			wave_read(&sender->wave_play, samples, count);
		if (sender->loopback != 1) {
			if (sender->wave_rec.fp)
				wave_write(&sender->wave_rec, samples, count);
			sender_receive(sender, samples, count);
		}
		if (sender->loopback == 3) {
			jitter_save(&sender->audio, samples, count);
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
		sender = sender_head;
		while (sender) {
			latspl = sender->samplerate * latency / 1000;
			process_sender(sender, quit, latspl);
			sender = sender->next;
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

