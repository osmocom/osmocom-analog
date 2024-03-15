/* main function
 *
 * (C) 2021 by Andreas Eversberg <jolly@eversberg.eu>
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

#ifndef ARDUINO

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include "../libsample/sample.h"
#include "../libsound/sound.h"
#include "../libwave/wave.h"
#include "../liblogging/logging.h"
#include "../liboptions/options.h"
#include "../libaaimage/aaimage.h"
#include "iso7811.h"

int num_kanal = 1;
static int quit = 0;
#ifdef HAVE_ALSA
static void *sound = NULL;
static int dsp_buffer = 50;
#endif
static int dsp_samplerate = 48000;
static const char *dsp_audiodev = "hw:0,0";
static const char *wave_file = NULL;
static int baudrate = 2666;
static const char *sicherung = "12345";

/* Measurements done in summer 2021 with an original card, applied with iron oxyde. */
/* Conforms to Track 3 (210 bpi) with 60 bits lead-in, 20 digits data, about 550 bits lead out */
/* Note that LEAD_OUT here is longer, because the switch must be manually pressed during lead-out. */
#define CNETZ_LEAD_IN		12	/* number of zero-digits before start sentinel (60 bits) */
#define CNETZ_LEAD_OUT		150	/* number of zero-digits after LRC sentinel */
#define CNETZ_SWITCH_ON		27	/* switch closing during lead-out, in digit-duration */ 
#define CNETZ_SWITCH_OFF	42	/* switch opening during lead-out, in digit-duration */ 

void print_help(const char *arg0)
{
	printf("Usage: %s [options] -a hw:0,0 <number> | service\n", arg0);
	/*      -                                                                             - */
	printf("General options:\n");
	printf(" -h --help\n");
	printf("        This help\n");
	printf(" --config [~/]<path to config file>\n");
	printf("        Give a config file to use. If it starts with '~/', path is at home dir.\n");
	printf("        Each line in config file is one option, '-' or '--' must not be given!\n");
	logging_print_help();
	printf(" -a --audio-device hw:<card>,<device>\n");
	printf("        Input audio from sound card's device number\n");
	printf(" -s --samplerate <sample rate>\n");
	printf("        Give audio device sample rate in Hz. (default = %d)\n", dsp_samplerate);
	printf(" -w --write-wave <filename>\n");
	printf("        Output sound as wave file\n");
	printf("\nMagnetic card simulator options:\n");
	printf(" -B --baud-rate <baud>\n");
	printf("        Playback baud rate (default = %d)\n", baudrate);
	printf(" -S --sicherung <security code>\n");
	printf("        Card's security code for simple authentication (default = '%s')\n", sicherung);
	printf("\n<number>: Give any valid 7 digit (optionally 8 digit) subscriber number. May\n");
	printf("        be prefixed with 0160.\n");
	printf("\n'service': BSA44 service card (to unlock phone after battery replacement)\n");
}

void add_options(void)
{
	option_add('h', "help", 0);
	option_add('v', "debug", 1);
	option_add('a', "audio-device", 1);
	option_add('s', "samplerate", 1);
	option_add('w', "write-wave", 1);
	option_add('B', "baud-rate", 1);
	option_add('S', "sicherung", 1);
};

int handle_options(int short_option, int argi, char **argv)
{
	int rc;

	switch (short_option) {
	case 'h':
		print_help(argv[0]);
		return 0;
	case 'v':
		rc = parse_logging_opt(argv[argi]);
		if (rc > 0)
			return 0;
		if (rc < 0) {
			fprintf(stderr, "Failed to parse logging option, please use -h for help.\n");
			return rc;
		}
		break;
	case 'a':
		dsp_audiodev = options_strdup(argv[argi]);
		break;
	case 's':
		dsp_samplerate = atof(argv[argi]);
		break;
	case 'w':
		wave_file = options_strdup(argv[argi]);
		break;
	case 'B':
		baudrate = atoi(argv[argi]);
		break;
	case 'S':
		sicherung = options_strdup(argv[argi]);
		break;
	default:
		return -EINVAL;
	}

	return 1;
}

void sighandler(int sigset)
{
	if (sigset == SIGHUP)
		return;
	if (sigset == SIGPIPE)
		return;

	printf("Signal received: %d\n", sigset);

	quit = -1;
}

int main(int argc, char *argv[])
{
	const char *number;
	char string[19];
	uint8_t data[CNETZ_LEAD_IN + 21 + CNETZ_LEAD_OUT];
	int length;
	int rc, argi;
	int i, j;

	loglevel = LOGL_INFO;
	logging_init();

	add_options();
	rc = options_config_file(argc, argv, "~/.osmocom/analog/magnetic.conf", handle_options);
	if (rc < 0)
		return 0;

	/* parse command line */
	argi = options_command_line(argc, argv, handle_options);
	if (argi <= 0)
		return argi;

	if (argi >= argc) {
		fprintf(stderr, "Expecting phone number, use '-h' for help!\n");
		return 0;
	} else if (!strcmp(argv[argi], "service")) {
		bsa44_service(string);
	} else {
		number = argv[argi];
		/* remove prefix, if given */
		if (strlen(number) >= 10 && !strncmp(number, "0160", 4))
			number += 4;
		if (strlen(number) < 7 || strlen(number) > 8) {
			fprintf(stderr, "Expecting phone number to be 7 or 8 digits, use '-h' for help!\n");
			return 0;
		}
		for (i = 0; number[i]; i++) {
			if (number[0] < '0' || number[i] > '9') {
				fprintf(stderr, "Given phone number has invalid digits, use '-h' for help!\n");
				return 0;
			}
		}
		if (number[0] > '7') {
inval_number:
			fprintf(stderr, "Given digits of phone number are out of range for 'C-Netz', use '-h' for help!\n");
			return 0;
		}
		if (strlen(number) == 8) {
			if ((number[1] - '0') * 10 + (number[2] - '0') > 31)
				goto inval_number;
			if (atoi(number + 3) > 65535)
				goto inval_number;
		} else {
			if (atoi(number + 2) > 65535)
				goto inval_number;
		}
		for (i = 0; sicherung[i]; i++) {
			if (sicherung[0] < '0' || sicherung[i] > '9') {
				fprintf(stderr, "Given security code has invalid digits, use '-h' for help!\n");
				return 0;
			}
		}
		if (!sicherung[0] || (sicherung[0] == '0' && sicherung[1] == '0') || atoi(sicherung) > 65535) {
			fprintf(stderr, "Given security code is out of range, use '-h' for help!\n");
			return 0;
		}
		cnetz_card(string, number, sicherung);
	}

	length = encode_track(data, string, CNETZ_LEAD_IN, CNETZ_LEAD_OUT);
	if (length > (int)sizeof(data)) {
		fprintf(stderr, "Software error: Array too small, PLEASE FIX!\n");
		return -1;
	}

	/* alloc space depending on bit rate (length of half-bit: round up to next integer) */
	int samples_per_halfbit = (dsp_samplerate + (baudrate * 2) - 1) / (baudrate * 2);
	int total_samples = samples_per_halfbit * 2 * 5 * length;
	sample_t sample[total_samples], *samples[1], silence[dsp_samplerate], level = 1;
#ifdef HAVE_ALSA
	int switch_on_samples = samples_per_halfbit * 2 * 5 * (CNETZ_LEAD_IN + 21 + CNETZ_SWITCH_ON);
	int switch_off_samples = samples_per_halfbit * 2 * 5 * (CNETZ_LEAD_IN + 21 + CNETZ_SWITCH_OFF);
	int buffer_size = dsp_samplerate * dsp_buffer / 1000;
#endif

	/* generate sample */
	int s, ss = 0;
	for (i = 0; i < length; i++) {
		for (j = 0; j < 5; j++) {
			level = -level;
			for (s = 0; s < samples_per_halfbit; s++)
				sample[ss++] = level;
			if (((data[i] >> j) & 1))
				level = -level;
			for (s = 0; s < samples_per_halfbit; s++)
				sample[ss++] = level;
		}
	}
	memset(silence, 0, sizeof(silence));

	LOGP(DDSP, LOGL_INFO, "Total bits: %d\n", length * 5);
	LOGP(DDSP, LOGL_INFO, "Samples per bit: %d\n", samples_per_halfbit * 2);
	LOGP(DDSP, LOGL_INFO, "Total samples: %d (duration: %.3f seconds)\n", total_samples, (double)total_samples / (double)dsp_samplerate);

	if (wave_file) {
		wave_rec_t wave_rec;

		/* open wave file */
		rc = wave_create_record(&wave_rec, wave_file, dsp_samplerate, 1, 1.0);
		if (rc < 0) {
			LOGP(DRADIO, LOGL_ERROR, "Failed to create WAVE record instance!\n");
			goto error;
		}
		samples[0] = silence;
		wave_write(&wave_rec, samples, dsp_samplerate / 2);
		samples[0] = sample;
		wave_write(&wave_rec, samples, total_samples);
		samples[0] = silence;
		wave_write(&wave_rec, samples, dsp_samplerate / 2);
		wave_destroy_record(&wave_rec);
		goto done;
	}

#ifdef HAVE_ALSA
	/* open audio device */
	sound = sound_open(SOUND_DIR_PLAY, dsp_audiodev, NULL, NULL, NULL, 1, 0.0, dsp_samplerate, buffer_size, 1.0, 1.0, 0.0, 2.0);
	if (!sound) {
		rc = -EIO;
		LOGP(DRADIO, LOGL_ERROR, "Failed to open sound device!\n");
		goto error;
	}
#else
	rc = -ENOTSUP;
	LOGP(DRADIO, LOGL_ERROR, "No sound card support compiled in!\n");
	goto error;
#endif

	print_aaimage();
	printf("String to send: ;%s?\n", string);
	for (i = 0; i < 5; i++) {
		if (i < 4)
			printf("2^%d: ...", i);
		else
			printf("Par: ...");
		for (j = CNETZ_LEAD_IN - 4; j < CNETZ_LEAD_IN + 4 + 21; j++)
			printf(" %d", (data[j] >> i) & 1);
		printf(" ...\n");
	}

	/* catch signals */
	signal(SIGINT, sighandler);
	signal(SIGHUP, sighandler);
	signal(SIGTERM, sighandler);
	signal(SIGPIPE, sighandler);

#ifdef HAVE_ALSA
	sound_start(sound);

	int count;
	while (!quit) {
		ss = 0;
		while (!quit) {
			usleep(1000);
			count = sound_get_tosend(sound, buffer_size);
			if (count <= 0)
				continue;
			samples[0] = silence + ss;
			ss += count;
			if (ss > dsp_samplerate) {
				count -= ss - dsp_samplerate;
				ss = dsp_samplerate;
			}
			sound_write(sound, samples, NULL, count, NULL, NULL, 1);
			if (ss == dsp_samplerate)
				break;
		}
		printf("\033[0;32m  -> \033[1;31mTX\033[0;32m <-\033[0;39m\r"); fflush(stdout);
		ss = 0;
		while (!quit) {
			usleep(1000);
			count = sound_get_tosend(sound, buffer_size);
			if (count <= 0)
				continue;
			if ((ss >= 0 && ss < count) || (ss - switch_off_samples >= 0 && ss - switch_off_samples < count)) {
				printf("\033[0;32m  -> \033[1;31mTX\033[0;32m <-             \033[0;39m\r"); fflush(stdout);
			}
			if (ss - switch_on_samples >= 0 && ss - switch_on_samples < count) {
				printf("\033[0;32m  -> \033[1;31mTX\033[0;32m <-  -> \033[1;33mCLICK\033[0;32m <-\033[0;39m\r"); fflush(stdout);
			}
			samples[0] = sample + ss;
			ss += count;
			if (ss > total_samples) {
				count -= ss - total_samples;
				ss = total_samples;
			}
			sound_write(sound, samples, NULL, count, NULL, NULL, 1);
			if (ss == total_samples)
				break;
		}
		printf("                                 \r"); fflush(stdout);
	}
#endif

	/* reset signals */
	signal(SIGINT, SIG_DFL);
	signal(SIGHUP, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);

error:
#ifdef HAVE_ALSA
	if (sound)
		sound_close(sound);
#endif

done:
	options_free();

	return 0;
}

void osmo_cc_set_log_cat(void) {}

#endif /* ARDUINO */
