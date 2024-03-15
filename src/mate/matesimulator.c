/* Mate Simulator - This is a fun project that has been created on
 * chaos communication congress. It simulates the tone of a mate bottle
 * if you blow on it. It is used to stimulate the "mate-o-meter" of
 * eventphone.
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
static int dsp_buffer = 50;
static int dsp_samplerate = 48000;
static const char *dsp_audiodev = "hw:0,0";
const char *write_tx_wave = NULL;
double tone, fill;

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

/* mate fill table according to eventphone's research */
static double conversion[] = {
	/* percent, frequency */
	// 142, 1500,
	// 123, 1000,
	100, 910,
	98, 710,
	96, 608,
	94, 534,
	92, 490,
	90, 452,
	88, 418,
	86, 396,
	84, 372,
	82, 358,
	80, 340,
	78, 325,
	76, 310,
	74, 300,
	72, 294,
	70, 282,
	68, 270,
	66, 264,
	64, 256,
	62, 250,
	60, 244,
	58, 240,
	56, 234,
	54, 228,
	52, 224,
	50, 220,
	48, 216,
	46, 212,
	44, 208,
	42, 204,
	40, 202,
	38, 198,
	36, 194,
	34, 190,
	32, 187,
	30, 184,
	28, 182,
	26, 180,
	24, 178,
	22, 176,
	20, 174,
	18, 172,
	16, 170,
	14, 168,
	12, 166,
	10, 165,
	8, 164,
	6, 163,
	4, 162,
	2, 161,
	0, 160,
};

/* get the tone from the fill in percent, using conversion table */
static double convert(double fill)
{
	int i = 0;
	double t1,t2,f1,f2,offset;

	f2 = conversion[i++];
	t2 = conversion[i++];

	while (23) {
		f1 = f2;
		t1 = t2;
		f2 = conversion[i++];
		t2 = conversion[i++];

		if (fill >= f2)
			break;
	}

	/* interpolate between two entries */
	offset = (fill - f1) / (f2 - f1);
	return offset * (t2 - t1) + t1;
}

static void print_help(const char *arg0)
{
	printf("Usage: %s [options] <fill in percent>\n\n", arg0);
	/*      -                                                                             - */
	printf("36c3 Mate Bottle Simulator - a fun project. Don't take it seriously!\n\n");
	printf("This program simulates the bottle blowing of a Club Mate bottle.\n");
	printf("It will produce a tone that depends on the given fill of a bottle.\n");
	printf("Dial 6283 on Eventphone's network and play the tone into the phone.\n");
	printf(" -h --help\n");
	printf("        This help\n");
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
	case 'a':
		dsp_audiodev = strdup(argv[argi]);
		break;
	case 's':
		dsp_samplerate = atoi(argv[argi]);
		break;
	case 'w':
		write_tx_wave = strdup(argv[argi]);
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
	int i;

	memset(power, 1, length);

	for (i = 0; i < length; i++) {
		samples[i] = cos(2.0 * M_PI * tone * phase);
		phase += 1.0 / dsp_samplerate;
	}
}

/* loop that gets audio from encoder and fowards it to sound card.
 * alternatively a sound file is written.
 */
static void process_signal(int buffer_size)
{
	sample_t buff[buffer_size], *samples[1] = { buff };
	uint8_t pbuff[buffer_size], *power[1] = { pbuff };
	int count;
	int __attribute__((unused)) rc;

	while (!feof(stdin)) {
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
	int rc, argi;
	int buffer_size = dsp_samplerate * dsp_buffer / 1000;

	logging_init();

	/* handle options / config file */
	add_options();
	argi = options_command_line(argc, argv, handle_options);
	if (argi <= 0)
		return argi;

	if (argi >= argc) {
		printf("No fill of bottle given!\n\n");
		print_help(argv[0]);
		goto exit;
	}

	fill = atof(argv[argi]) / 100.0;
	if (fill < 0.0)
		fill = 0.0;
	if (fill > 1.0)
		fill = 1.0;
	tone = convert(fill * 100.0);


#ifdef HAVE_ALSA
	/* init sound */
	audio = sound_open(SOUND_DIR_PLAY, dsp_audiodev, NULL, NULL, NULL, 1, 0.0, dsp_samplerate, buffer_size, 1.0, 1.0, 4000.0, 2.0);
	if (!audio) {
		LOGP(DDSP, LOGL_ERROR, "No sound device!\n");
		goto exit;
	}
#endif

	/* open wave */
	if (write_tx_wave) {
		rc = wave_create_record(&wave_tx_rec, write_tx_wave, dsp_samplerate, 1, 1.0);
		if (rc < 0) {
			LOGP(DDSP, LOGL_ERROR, "Failed to create WAVE recoding instance!\n");
			goto exit;
		}
	}
#ifndef HAVE_ALSA
	else {
		LOGP(DDSP, LOGL_ERROR, "No sound support compiled in, so you need to write to a wave file. See help!\n");
		goto exit;
	}
#endif

#ifdef HAVE_ALSA
	/* start sound */
	sound_start(audio);
#endif

	printf("Sending Tone of %.1f Hz to simulate a Mate Bottle with %.0f%% fill.\n", tone, fill * 100.0);
	printf("Press CTRL+'c' to stop.\n");

	process_signal(buffer_size);

exit:
	/* close wave */
	wave_destroy_record(&wave_tx_rec);

#ifdef HAVE_ALSA
	/* exit sound */
	if (audio)
		sound_close(audio);
#endif

	return 0;
}

