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
#include "../libsample/sample.h"
#include "../liblogging/logging.h"
#include "sender.h"
#include <osmocom/core/timer.h>
#ifdef HAVE_SDR
#include "../libsdr/sdr_config.h"
#endif

/* debug time consumption of audio processing */
//#define DEBUG_TIME_CONSUMPTION

sender_t *sender_head = NULL;
static sender_t **sender_tailp = &sender_head;
int cant_recover = 0;
int check_channel = 1;

/* Init transceiver instance and link to list of transceivers. */
int sender_create(sender_t *sender, const char *kanal, double sendefrequenz, double empfangsfrequenz, const char *device, int use_sdr, int samplerate, double rx_gain, double tx_gain, int pre_emphasis, int de_emphasis, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, const char *read_tx_wave, int loopback, enum paging_signal paging_signal)
{
	sender_t *master, *slave;
	int rc = 0;

	sender->kanal = kanal;
	sender->sendefrequenz = sendefrequenz;
	sender->empfangsfrequenz = (loopback) ? sendefrequenz : empfangsfrequenz;
	strncpy(sender->device, device, sizeof(sender->device) - 1);
	sender->samplerate = samplerate;
	sender->rx_gain = rx_gain;
	sender->tx_gain = tx_gain;
	sender->pre_emphasis = pre_emphasis;
	sender->de_emphasis = de_emphasis;
	sender->loopback = loopback;
	sender->paging_signal = paging_signal;
	sender->write_rx_wave = write_rx_wave;
	sender->write_tx_wave = write_tx_wave;
	sender->read_rx_wave = read_rx_wave;
	sender->read_tx_wave = read_tx_wave;

	/* no gain with SDR */
	if (use_sdr) {
		sender->rx_gain = 1.0;
		sender->tx_gain = 1.0;
	}

	if (samplerate < 8000) {
		LOGP(DSENDER, LOGL_NOTICE, "Given sample rate is below 8 KHz. Please use higher sample rate!\n");
		rc = -EINVAL;
		goto error;
	}

	LOGP_CHAN(DSENDER, LOGL_DEBUG, "Creating 'Sender' instance\n");

	/* if we find a channel that uses the same device as we do,
	 * we will link us as slave to this master channel. then we 
	 * receive and send audio via second channel of the device
	 * of the master channel.
	 */
	for (master = sender_head; master; master = master->next) {
		if (!strcmp(master->kanal, kanal)) {
			LOGP(DSENDER, LOGL_ERROR, "Channel %s may not be defined for multiple transceivers!\n", kanal);
			rc = -EIO;
			goto error;
		}
		if (check_channel && abs(atoi(master->kanal) - atoi(kanal)) == 1) {
			LOGP(DSENDER, LOGL_NOTICE, "------------------------------------------------------------------------\n");
			LOGP(DSENDER, LOGL_NOTICE, "NOTE: Channel %s is next to channel %s. This will cause interferences.\n", kanal, master->kanal);
			LOGP(DSENDER, LOGL_NOTICE, "Please use at least one channel distance to avoid that.\n");
			LOGP(DSENDER, LOGL_NOTICE, "------------------------------------------------------------------------\n");
		}
		if (!strcmp(master->device, device))
			break;
	}
	if (master) {
		if (master->paging_signal != PAGING_SIGNAL_NONE && !use_sdr) {
			LOGP(DSENDER, LOGL_ERROR, "Cannot share audio device with channel %s, because its second audio channel is used for paging signal! Use different audio device.\n", master->kanal);
			rc = -EBUSY;
			goto error;
		}
		if (paging_signal != PAGING_SIGNAL_NONE && !use_sdr) {
			LOGP(DSENDER, LOGL_ERROR, "Cannot share audio device with channel %s, because we need a second audio channel for paging signal! Use different audio device.\n", master->kanal);
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
#ifdef HAVE_ALSA
			sender->audio_open = sound_open;
			sender->audio_start = sound_start;
			sender->audio_close = sound_close;
			sender->audio_read = sound_read;
			sender->audio_write = sound_write;
			sender->audio_get_tosend = sound_get_tosend;
#else
			LOGP(DSENDER, LOGL_ERROR, "No sound card support compiled in!\n");
			rc = -ENOTSUP;
			goto error;
#endif
		}
	}

	rc = init_samplerate(&sender->srstate, 8000.0, (double)samplerate, 3300.0);
	if (rc < 0) {
		LOGP(DSENDER, LOGL_ERROR, "Failed to init sample rate conversion!\n");
		goto error;
	}

	rc = jitter_create(&sender->dejitter, sender->kanal, 8000, JITTER_AUDIO);
	if (rc < 0) {
		LOGP(DSENDER, LOGL_ERROR, "Failed to create and init audio buffer!\n");
		goto error;
	}

	rc = jitter_create(&sender->loop_dejitter, sender->kanal, samplerate, JITTER_AUDIO);
	if (rc < 0) {
		LOGP(DSENDER, LOGL_ERROR, "Failed to create and init loop audio buffer!\n");
		goto error;
	}

	rc = init_emphasis(&sender->estate, samplerate, CUT_OFF_EMPHASIS_DEFAULT, CUT_OFF_HIGHPASS_DEFAULT, CUT_OFF_LOWPASS_DEFAULT);
	if (rc < 0)
		goto error;

	*sender_tailp = sender;
	sender_tailp = &sender->next;

	display_wave_init(&sender->dispwav, samplerate, sender->kanal);
	display_measurements_init(&sender->dispmeas, samplerate, sender->kanal);

	return 0;
error:
	sender_destroy(sender);

	return rc;
}

int sender_open_audio(int buffer_size, double interval)
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
		int am[channels];
		for (i = 0, inst = master; inst; i++, inst = inst->slave) {
			tx_f[i] = inst->sendefrequenz;
			rx_f[i] = inst->empfangsfrequenz;
			am[i] = inst->am;
			if (inst->ruffrequenz)
				paging_frequency = inst->ruffrequenz;
		}

		if (master->write_rx_wave) {
			rc = wave_create_record(&master->wave_rx_rec, master->write_rx_wave, master->samplerate, channels, (master->max_deviation) ?: 1.0);
			if (rc < 0) {
				LOGP(DSENDER, LOGL_ERROR, "Failed to create WAVE recoding instance!\n");
				return rc;
			}
		}
		if (master->write_tx_wave) {
			rc = wave_create_record(&master->wave_tx_rec, master->write_tx_wave, master->samplerate, channels, (master->max_deviation) ?: 1.0);
			if (rc < 0) {
				LOGP(DSENDER, LOGL_ERROR, "Failed to create WAVE recoding instance!\n");
				return rc;
			}
		}
		if (master->read_rx_wave) {
			rc = wave_create_playback(&master->wave_rx_play, master->read_rx_wave, &master->samplerate, &channels, (master->max_deviation) ?: 1.0);
			if (rc < 0) {
				LOGP(DSENDER, LOGL_ERROR, "Failed to create WAVE playback instance!\n");
				return rc;
			}
		}
		if (master->read_tx_wave) {
			rc = wave_create_playback(&master->wave_tx_play, master->read_tx_wave, &master->samplerate, &channels, (master->max_deviation) ?: 1.0);
			if (rc < 0) {
				LOGP(DSENDER, LOGL_ERROR, "Failed to create WAVE playback instance!\n");
				return rc;
			}
		}

		/* open device */
		master->audio = master->audio_open(master->device, tx_f, rx_f, am, channels, paging_frequency, master->samplerate, buffer_size, interval, (master->max_deviation) ?: 1.0, master->max_modulation, master->modulation_index);
		if (!master->audio) {
			LOGP(DSENDER, LOGL_ERROR, "No device for transceiver!\n");
			return -EIO;
		}
	}

#ifdef HAVE_SDR
	/* in case of initialized spectrum display (SDR), we add all channels.
	 * if spectrum display was not initialized (sound card), function call is ignored */
	for (inst = sender_head; inst; inst = inst->next)
		display_spectrum_add_mark(inst->kanal, (sdr_config->swap_links) ? inst->sendefrequenz : inst->empfangsfrequenz);
#endif

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
	LOGP_CHAN(DSENDER, LOGL_DEBUG, "Destroying 'Sender' instance\n");

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
	jitter_destroy(&sender->loop_dejitter);
}

/* set frequency modulation and parameters */
void sender_set_fm(sender_t *sender, double max_deviation, double max_modulation, double speech_deviation, double max_display)
{
	sender->max_deviation = max_deviation;
	sender->max_modulation = max_modulation;
	sender->speech_deviation = speech_deviation;
	sender->max_display = max_display;

	LOGP_CHAN(DSENDER, LOGL_DEBUG, "Maximum deviation: %.1f kHz, Maximum modulation: %.1f kHz\n", max_deviation / 1000.0, max_modulation / 1000.0);
	LOGP_CHAN(DSENDER, LOGL_DEBUG, "Deviation at speech level: %.1f kHz\n", speech_deviation / 1000.0);
}

/* set amplitude modulation and parameters */
void sender_set_am(sender_t *sender, double max_modulation, double speech_level, double max_display, double modulation_index)
{
	sender->am = 1;
	sender->max_deviation = 0;
	sender->max_modulation = max_modulation;
	sender->speech_deviation = speech_level;
	sender->max_display = max_display;
	sender->modulation_index = modulation_index;

	LOGP_CHAN(DSENDER, LOGL_DEBUG, "Modulation degree: %.0f %%, Maximum modulation: %.1f kHz\n", modulation_index / 100.0, max_modulation / 1000.0);
}

static void gain_samples(sample_t *samples, int length, double gain)
{
	int i;

	for (i = 0; i < length; i++)
		*samples++ *= gain;
}

/* Handle audio streaming of one transceiver. */
void process_sender_audio(sender_t *sender, int *quit, sample_t **samples, uint8_t **power, int buffer_size)
{
	sender_t *inst;
	int rc, count;
	int num_chan, i;
#ifdef DEBUG_TIME_CONSUMPTION
	static double t1, t2, t3, t4, t5, d1 = 0, d2 = 0, d3 = 0, d4 = 0, s = 0;
#endif

	/* count instances for audio channel */
	for (num_chan = 0, inst = sender; inst; num_chan++, inst = inst->slave);
	enum paging_signal paging_signal[num_chan];
	int on[num_chan];
	double rf_level_db[num_chan];

#ifdef DEBUG_TIME_CONSUMPTION
	t1 = get_time();
#endif
	count = sender->audio_get_tosend(sender->audio, buffer_size);
	if (count < 0) {
		LOGP_CHAN(DSENDER, LOGL_ERROR, "Failed to get number of samples in buffer (rc = %d)!\n", count);
		if (count == -EPIPE) {
			if (cant_recover) {
cant_recover:
				LOGP(DSENDER, LOGL_ERROR, "Cannot recover due to measurements, quitting!\n");
				*quit = 1;
				return;
			}
			LOGP(DSENDER, LOGL_ERROR, "Trying to recover!\n");
		}
		return;
	}
#ifdef DEBUG_TIME_CONSUMPTION
	t2 = get_time();
#endif
	if (count > 0) {
		/* limit to our buffer */
		if (count > buffer_size)
			count = buffer_size;
		/* loop through all channels */
		for (i = 0, inst = sender; inst; i++, inst = inst->slave) {
			/* load TX data from audio loop or from sender instance */
			if (inst->loopback == 3)
				jitter_load_samples(&inst->loop_dejitter, (uint8_t *)samples[i], count, sizeof(*(samples[i])), NULL, NULL);
			else
				sender_send(inst, samples[i], power[i], count);
			/* internal loopback: loop back TX audio to RX */
			if (inst->loopback == 1) {
				display_wave(&inst->dispwav, samples[i], count, inst->max_display);
				sender_receive(inst, samples[i], count, 0.0);
			}
			/* do pre emphasis towards radio */
			if (inst->pre_emphasis)
				pre_emphasis(&inst->estate, samples[i], count);
			/* tx gain */
			if (inst->tx_gain != 1.0)
				gain_samples(samples[i], count, inst->tx_gain);
			/* normal level to frequency deviation of speech level */
			gain_samples(samples[i], count, inst->speech_deviation);
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
			LOGP(DSENDER, LOGL_ERROR, "Failed to write TX data to audio device (rc = %d)\n", rc);
			if (rc == -EPIPE) {
				if (cant_recover)
					goto cant_recover;
				LOGP(DSENDER, LOGL_ERROR, "Trying to recover!\n");
			}
			return;
		}
	}
#ifdef DEBUG_TIME_CONSUMPTION
	t3 = get_time();
#endif

	count = sender->audio_read(sender->audio, samples, buffer_size, num_chan, rf_level_db);
	if (count < 0) {
		/* special case when audio_read wants us to quit */
		if (count == -EPERM) {
			*quit = 1;
			return;
		}
		LOGP(DSENDER, LOGL_ERROR, "Failed to read from audio device (rc = %d)!\n", count);
		if (count == -EPIPE) {
			if (cant_recover)
				goto cant_recover;
			LOGP(DSENDER, LOGL_ERROR, "Trying to recover!\n");
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
			/* frequency deviation of speech level to normal level */
			gain_samples(samples[i], count, 1.0 / inst->speech_deviation);
			/* rx gain */
			if (inst->rx_gain != 1.0)
				gain_samples(samples[i], count, inst->rx_gain);
			/* do filter and de-emphasis from radio receive audio, process echo test */
			if (inst->de_emphasis) {
				dc_filter(&inst->estate, samples[i], count);
				de_emphasis(&inst->estate, samples[i], count);
			}
			if (inst->loopback != 1) {
				display_wave(&inst->dispwav, samples[i], count, inst->max_display);
				sender_receive(inst, samples[i], count, rf_level_db[i]);
			}
			if (inst->loopback == 3) {
				jitter_frame_t *jf;
				jf = jitter_frame_alloc(NULL, NULL, (uint8_t *)samples[i], count * sizeof(*(samples[i])), 0, inst->loop_sequence, inst->loop_timestamp, 123);
				if (jf)
					jitter_save(&inst->loop_dejitter, jf);
				inst->loop_sequence += 1;
				inst->loop_timestamp += count;
			}
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

