/* IMTS dialer
 *
 * (C) 2019 by Andreas Eversberg <jolly@eversberg.eu>
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
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <errno.h>
#include "../libsample/sample.h"
#include "../libwave/wave.h"
#include "../liblogging/logging.h"
#ifdef HAVE_ALSA
#include "../libsound/sound.h"
#endif
#include "../liboptions/options.h"

/* presets */
const char *station_id = "6681739";
const char *dialing;
const char *dsp_audiodev = "hw:0,0";
int dsp_samplerate = 48000;
const char *write_tx_wave = NULL;
int dsp_buffer = 50;

#define TONE_DONE	-1
#define TONE_SILENCE	0
#define TONE_GUARD	2150
#define TONE_CONNECT	1633
#define TONE_DISCONNECT	1336

/* states */
static struct dial_string {
	char console;
	int tone;
	int length;
} dial_string[2048];
static int dial_pos = 0;

/* instances */
#ifdef HAVE_ALSA
void *audio = NULL;
#endif
wave_rec_t wave_tx_rec;

/* dummy functions */
int num_kanal = 1; /* only one channel used for debugging */
void *get_sender_by_empfangsfrequenz() { return NULL; }
void display_measurements_add() {}
void display_measurements_update() {}

static void print_help(const char *arg0)
{
	printf("Usage: %s [options] <number> | disconnect\n\n", arg0);
	/*      -                                                                             - */
	printf("This program generates a dialing sequence to make a call via IMTS base\n");
	printf("station using an amateur radio transceiver.\n");
	printf("Also it can write an audio file (wave) to be fed into IMTS base station for\n");
	printf("showing how it simulates an IMTS phone doing an outgoing call.\n\n");
	printf("Use 'disconnect' to generate a disconnect sequence.\n\n");
	printf(" -h --help\n");
	printf("        This help\n");
	printf(" -i --station-id <station ID>\n");
	printf("        7 Digits of ID of mobile station (default = '%s')\n", station_id);
#ifdef HAVE_ALSA
	printf(" -a --audio-device hw:<card>,<device>\n");
	printf("        Sound card and device number (default = '%s')\n", dsp_audiodev);
#endif
	printf(" -s --samplerate <rate>\n");
	printf("        Sample rate of sound device (default = '%d')\n", dsp_samplerate);
	printf(" -w --write-tx-wave <file>\n");
	printf("        Write audio to given wave file also.\n");
}

static void add_options(void)
{
	option_add('h', "help", 0);
	option_add('i', "station-id", 1);
	option_add('a', "audio-device", 1);
	option_add('s', "samplerate", 1);
	option_add('w', "write-tx-wave", 1);
}

static int handle_options(int short_option, int __attribute__((unused)) argi, char **argv)
{
	switch (short_option) {
	case 'h':
		print_help(argv[0]);
		return 0;
	case 'i':
		station_id = options_strdup(argv[argi]);
		break;
	case 'a':
		dsp_audiodev = options_strdup(argv[argi]);
		break;
	case 's':
		dsp_samplerate = atoi(argv[argi]);
		break;
	case 'w':
		write_tx_wave = options_strdup(argv[argi]);
		break;
	default:
		return -EINVAL;
	}

	return 1;
}

static double phase = 0;

/* encode audio */
static void encode_audio(sample_t *samples, uint8_t *power, int length)
{
	int count;
	int i;

	memset(power, 1, length);

again:
	count = length;
	if (dial_string[dial_pos].length && dial_string[dial_pos].length < count)
		count = dial_string[dial_pos].length;

	switch (dial_string[dial_pos].tone) {
	case TONE_DONE:
	case TONE_SILENCE:
		memset(samples, 0, count * sizeof(*samples));
		phase = 0;
		break;
	default:
		for (i = 0; i < count; i++) {
			samples[i] = cos(2.0 * M_PI * (double)dial_string[dial_pos].tone * phase);
			phase += 1.0 / dsp_samplerate;
		}
	}

	if (dial_string[dial_pos].length) {
		dial_string[dial_pos].length -= count;
		if (dial_string[dial_pos].length == 0) {
			if (dial_string[dial_pos].console) {
				printf("%c", dial_string[dial_pos].console);
				fflush(stdout);
			}
			dial_pos++;
		}
	}

	samples += count;
	length -= count;

	if (length)
		goto again;
}

/* loop that gets audio from encoder and forwards it to sound card.
 * alternatively a sound file is written.
 */
static void process_signal(int buffer_size)
{
	sample_t buff[buffer_size], *samples[1] = { buff };
        uint8_t pbuff[buffer_size], *power[1] = { pbuff };
	int count;
	int __attribute__((unused)) rc;

	while (dial_string[dial_pos].tone != TONE_DONE) {
#ifdef HAVE_ALSA
		count = sound_get_tosend(audio, buffer_size);
#else
		count = dsp_samplerate / 1000;
#endif
		if (count < 0) {
			LOGP(DDSP, LOGL_ERROR, "Failed to get number of samples in buffer (rc = %d)!\n", count);
			break;
		}

		/* encode dial_string of tones and lengths */
		encode_audio(samples[0], power[0], count);

		/* write wave, if open */
		if (wave_tx_rec.fp)
			wave_write(&wave_tx_rec, samples, count);

#ifdef HAVE_ALSA
		/* write audio */
		rc = sound_write(audio, samples, power, count, NULL, NULL, 1);
		if (rc < 0) {
			LOGP(DDSP, LOGL_ERROR, "Failed to write TX data to audio device (rc = %d)\n", rc);
			break;
		}
#endif

		/* sleep a while */
		usleep(1000);
	}
}

int main(int argc, char *argv[])
{
	int i, d, p, pulses, tone = 0;
	int buffer_size;
	int rc, argi;

	memset(dial_string, 0, sizeof(dial_string));

	/* size of send buffer in samples */
	buffer_size = dsp_samplerate * dsp_buffer / 1000;

	/* handle options / config file */
	add_options();
	argi = options_command_line(argc, argv, handle_options);
	if (argi <= 0)
		return argi;

	if (argi >= argc) {
		printf("No phone number given!\n\n");
		print_help(argv[0]);
		goto exit;
	}

	/* check for valid station ID */
	if (strlen(station_id) != 7) {
		printf("Given station ID '%s' has invalid number of digits!\n", station_id);
		goto exit;
	}
	for (i = 0; station_id[i]; i++) {
		if (station_id[i] < '0' || station_id[i] > '9') {
			printf("Given station ID '%s' has invalid digits!\n", station_id);
			goto exit;
		}
	}

	dialing = argv[argi];
	d = 0;
	dial_string[d].tone = TONE_SILENCE; dial_string[d++].length = 0.600 * (double)dsp_samplerate; /* pause */
	if (!!strcasecmp(dialing, "disconnect")) {
		/* check for valid phone number */
		if (strlen(dialing) > 64) {
			printf("Given phone number '%s' has too many digits! (more than allowed 64 digits)\n", dialing);
			goto exit;
		}
		for (i = 0; dialing[i]; i++) {
			if (dialing[i] < '0' || dialing[i] > '9') {
				printf("Given phone number '%s' has invalid digits!\n", dialing);
				goto exit;
			}
		}

		dial_string[d].tone = TONE_GUARD; dial_string[d++].length = 0.350 * (double)dsp_samplerate; /* off-hook */
		dial_string[d].console = 's';
		dial_string[d].tone = TONE_CONNECT; dial_string[d++].length = 0.050 * (double)dsp_samplerate; /* seize */
		dial_string[d].console = '-';
		dial_string[d].tone = TONE_GUARD; dial_string[d++].length = 1.000 * (double)dsp_samplerate; /* pause */
		for (i = 0; station_id[i]; i++) {
			pulses = station_id[i] - '0';
			if (pulses == 0)
				pulses = 10;
			dial_string[d].console = station_id[i];
			for (p = 1; p <= pulses; p++) {
				if ((p & 1) == 1)
					tone = TONE_SILENCE;
				else
					tone = TONE_GUARD;
				dial_string[d].tone = TONE_CONNECT; dial_string[d++].length = 0.025 * (double)dsp_samplerate; /* mark */
				dial_string[d].tone = tone; dial_string[d++].length = 0.025 * (double)dsp_samplerate; /* space */
			}
			dial_string[d].tone = tone; dial_string[d++].length = 0.190 * (double)dsp_samplerate; /* after digit */
		}
		dial_string[d].console = '-';
		dial_string[d].tone = TONE_SILENCE; dial_string[d++].length = 2.000 * (double)dsp_samplerate; /* pause */
		for (i = 0; dialing[i]; i++) {
			pulses = dialing[i] - '0';
			if (pulses == 0)
				pulses = 10;
			dial_string[d].console = dialing[i];
			for (p = 1; p <= pulses; p++) {
				dial_string[d].tone = TONE_CONNECT; dial_string[d++].length = 0.060 * (double)dsp_samplerate; /* mark */
				dial_string[d].tone = TONE_GUARD; dial_string[d++].length = 0.040 * (double)dsp_samplerate; /* space */
			}
			dial_string[d].tone = TONE_GUARD; dial_string[d++].length = 0.400 * (double)dsp_samplerate; /* after digit */
		}
		dial_string[d].console = '\n';
	} else {
		for (i = 0; i < 750; i += 50) {
			dial_string[d].tone = TONE_DISCONNECT; dial_string[d++].length = 0.025 * (double)dsp_samplerate; /* mark */
			dial_string[d].tone = TONE_GUARD; dial_string[d++].length = 0.025 * (double)dsp_samplerate; /* space */
		}
	}
	dial_string[d].tone = TONE_SILENCE; dial_string[d++].length = 0.600 * (double)dsp_samplerate; /* pause */
	dial_string[d].tone = TONE_DONE; dial_string[d++].length = 0; /* end */

#ifdef HAVE_ALSA
	/* init sound */
	audio = sound_open(SOUND_DIR_PLAY, dsp_audiodev, NULL, NULL, NULL, 1, 0.0, dsp_samplerate, dsp_buffer, 1.0, 1.0, 4000.0, 2.0);
	if (!audio) {
		LOGP(DBNETZ, LOGL_ERROR, "No sound device!\n");
		goto exit;
	}
#endif

	/* open wave */
	if (write_tx_wave) {
		rc = wave_create_record(&wave_tx_rec, write_tx_wave, dsp_samplerate, 1, 1.0);
		if (rc < 0) {
			LOGP(DBNETZ, LOGL_ERROR, "Failed to create WAVE recoding instance!\n");
			goto exit;
		}
	}
#ifndef HAVE_ALSA
	else {
		LOGP(DBNETZ, LOGL_ERROR, "No sound support compiled in, so you need to write to a wave file. See help!\n");
		goto exit;
	}
#endif

#ifdef HAVE_ALSA
	/* start sound */
	sound_start(audio);
#endif

	LOGP(DBNETZ, LOGL_ERROR, "Start audio after pause...\n");

	process_signal(buffer_size);

exit:
	/* close wave */
	wave_destroy_record(&wave_tx_rec);

#ifdef HAVE_ALSA
	/* exit sound */
	if (audio)
		sound_close(audio);
#endif

	options_free();

	return 0;
}

