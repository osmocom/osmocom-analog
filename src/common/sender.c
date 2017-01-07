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
#include "debug.h"
#include "sender.h"

sender_t *sender_head = NULL;
static sender_t **sender_tailp = &sender_head;
int cant_recover = 0;

/* Init transceiver instance and link to list of transceivers. */
int sender_create(sender_t *sender, int kanal, double sendefrequenz, double empfangsfrequenz, const char *audiodev, int samplerate, double rx_gain, int pre_emphasis, int de_emphasis, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, int loopback, double loss_volume, enum paging_signal paging_signal)
{
	sender_t *master, *slave;
	int rc = 0;

	sender->kanal = kanal;
	sender->sendefrequenz = sendefrequenz;
	sender->empfangsfrequenz = empfangsfrequenz;
	sender->bandwidth = 4000; /* default is overwritten by dsp.c */
	sender->sample_deviation = 0.2; /* default is overwritten by dsp.c */
	strncpy(sender->audiodev, audiodev, sizeof(sender->audiodev) - 1);
	sender->samplerate = samplerate;
	sender->rx_gain = rx_gain;
	sender->pre_emphasis = pre_emphasis;
	sender->de_emphasis = de_emphasis;
	sender->loopback = loopback;
	sender->loss_volume = loss_volume;
	sender->paging_signal = paging_signal;

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
		if (!strcmp(master->audiodev, audiodev))
			break;
	}
	if (master) {
		if (master->paging_signal != PAGING_SIGNAL_NONE && !!strcmp(master->audiodev, "sdr")) {
			PDEBUG(DSENDER, DEBUG_ERROR, "Cannot share audio device with channel %d, because its second audio channel is used for paging signal! Use different audio device.\n", master->kanal);
			rc = -EBUSY;
			goto error;
		}
		if (paging_signal != PAGING_SIGNAL_NONE && !!strcmp(audiodev, "sdr")) {
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
		if (!strcmp(audiodev, "sdr")) {
			sender->audio_open = sdr_open;
			sender->audio_close = sdr_close;
			sender->audio_read = sdr_read;
			sender->audio_write = sdr_write;
			sender->audio_get_inbuffer = sdr_get_inbuffer;
		} else
#endif
		{
			sender->audio_open = sound_open;
			sender->audio_close = sound_close;
			sender->audio_read = sound_read;
			sender->audio_write = sound_write;
			sender->audio_get_inbuffer = sound_get_inbuffer;
		}
	}

	rc = init_samplerate(&sender->srstate, samplerate);
	if (rc < 0) {
		PDEBUG(DSENDER, DEBUG_ERROR, "Failed to init sample rate conversion!\n");
		goto error;
	}

	rc = jitter_create(&sender->dejitter, samplerate / 5);
	if (rc < 0) {
		PDEBUG(DSENDER, DEBUG_ERROR, "Failed to create and init audio buffer!\n");
		goto error;
	}

	if (write_rx_wave) {
		rc = wave_create_record(&sender->wave_rx_rec, write_rx_wave, samplerate);
		if (rc < 0) {
			PDEBUG(DSENDER, DEBUG_ERROR, "Failed to create WAVE recoding instance!\n");
			goto error;
		}
	}
	if (write_tx_wave) {
		rc = wave_create_record(&sender->wave_tx_rec, write_tx_wave, samplerate);
		if (rc < 0) {
			PDEBUG(DSENDER, DEBUG_ERROR, "Failed to create WAVE recoding instance!\n");
			goto error;
		}
	}
	if (read_rx_wave) {
		rc = wave_create_playback(&sender->wave_rx_play, read_rx_wave, samplerate);
		if (rc < 0) {
			PDEBUG(DSENDER, DEBUG_ERROR, "Failed to create WAVE playback instance!\n");
			goto error;
		}
	}

	rc = init_emphasis(&sender->estate, samplerate, CUT_OFF_EMPHASIS_DEFAULT);
	if (rc < 0)
		goto error;

	*sender_tailp = sender;
	sender_tailp = &sender->next;

	display_wave_init(sender, samplerate);

	return 0;
error:
	sender_destroy(sender);

	return rc;
}

int sender_open_audio(void)
{
	sender_t *master, *inst;
	int channels;
	double paging_frequency = 0.0;
	int i;

	for (master = sender_head; master; master = master->next) {
		/* skip audio slaves */
		if (master->master)
			continue;

		/* get list of frequencies */
		channels = 0;
		for (inst = master; inst; inst = inst->slave) {
			channels++;
		}
		double tx_f[channels], rx_f[channels];
		for (i = 0, inst = master; inst; i++, inst = inst->slave) {
			tx_f[i] = inst->sendefrequenz;
			if (inst->loopback)
				rx_f[i] = inst->sendefrequenz;
			else
				rx_f[i] = inst->empfangsfrequenz;
			if (inst->ruffrequenz)
				paging_frequency = inst->ruffrequenz;
		}

		/* open device */
		master->audio = master->audio_open(master->audiodev, tx_f, rx_f, channels, paging_frequency, master->samplerate, master->bandwidth, master->sample_deviation);
		if (!master->audio) {
			PDEBUG(DSENDER, DEBUG_ERROR, "No audio device!\n");
			return -EIO;
		}
	}

	return 0;
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

	jitter_destroy(&sender->dejitter);
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
void process_sender_audio(sender_t *sender, int *quit, int latspl)
{
	sender_t *inst;
	int rc, count;
	int num_chan, i;

	/* count instances for audio channel */
	for (num_chan = 0, inst = sender; inst; num_chan++, inst = inst->slave);
	int16_t buff[num_chan][latspl], *samples[num_chan];
	enum paging_signal paging_signal[num_chan];
	int on[num_chan];
	for (i = 0; i < num_chan; i++) {
		samples[i] = buff[i];
	}

	count = sender->audio_get_inbuffer(sender->audio);
	if (count < 0) {
		/* special case when the device is not yet ready to transmit packets */
		if (count == -EAGAIN) {
			goto transmit_later;
		}
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
		/* loop through all channels */
		for (i = 0, inst = sender; inst; i++, inst = inst->slave) {
			/* load TX data from audio loop or from sender instance */
			if (inst->loopback == 3)
				jitter_load(&inst->dejitter, samples[i], count);
			else
				sender_send(inst, samples[i], count);
			if (inst->wave_tx_rec.fp)
				wave_write(&inst->wave_tx_rec, samples[i], count);
			/* internal loopback: loop back TX audio to RX */
			if (inst->loopback == 1) {
				if (inst->wave_rx_rec.fp)
					wave_write(&inst->wave_rx_rec, samples[i], count);
				display_wave(inst, samples[i], count);
				sender_receive(inst, samples[i], count);
			}
			/* do pre emphasis towards radio, not wave_write */
			if (inst->pre_emphasis)
				pre_emphasis(&inst->estate, samples[i], count);
			/* set paging signal */
			paging_signal[i] = sender->paging_signal;
			on[i] = sender->paging_on;
		}

		rc = sender->audio_write(sender->audio, samples, count, paging_signal, on, num_chan);
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
transmit_later:

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
	if (count) {
		/* loop through all channels */
		for (i = 0, inst = sender; inst; i++, inst = inst->slave) {
			/* rx gain */
			if (inst->rx_gain != 1.0)
				gain_samples(samples[i], count, inst->rx_gain);
			/* do de emphasis from radio (then write_wave/wave_read), receive audio, process echo test */
			if (inst->de_emphasis)
				de_emphasis(&inst->estate, samples[i], count);
			if (inst->wave_rx_play.fp)
				wave_read(&inst->wave_rx_play, samples[i], count);
			if (inst->loopback != 1) {
				if (inst->wave_rx_rec.fp)
					wave_write(&inst->wave_rx_rec, samples[i], count);
				display_wave(inst, samples[i], count);
				sender_receive(inst, samples[i], count);
			}
			if (inst->loopback == 3)
				jitter_save(&inst->dejitter, samples[i], count);
		}
	}
}

void sender_paging(sender_t *sender, int on)
{
	sender->paging_on = on;
}

