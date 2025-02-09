/* B-Netz dialer
 *
 * (C) 2018 by Andreas Eversberg <jolly@eversberg.eu>
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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "../libsample/sample.h"
#include "../libfsk/fsk.h"
#include "../libmobile/sender.h"
#include "../liblogging/logging.h"
#include "../liboptions/options.h"
#include "telegramm.h"

#define MAX_PAUSE	0.5	/* pause before and after dialing sequence */
#define F0		2070.0
#define F1		1950.0
#define BIT_RATE	100.0

/* presets */
char start_digit = 's';
const char *station_id = "50993";
const char *dialing;
const char *dsp_audiodev = "default";
int dsp_samplerate = 48000;
const char *write_tx_wave = NULL;
int dsp_buffer = 50;

/* states */
enum tx_mode {
	TX_MODE_SILENCE,
	TX_MODE_FSK,
	TX_MODE_DONE,
} tx_mode = TX_MODE_SILENCE;
int tx_silence_count = 0;
char funkwahl[128];
int digit_pos = 0;
const char *tx_telegramm = NULL;
int tx_telegramm_pos = 0;

/* instances */
fsk_mod_t fsk_mod;
#ifdef HAVE_ALSA
void *audio = NULL;
#endif
wave_rec_t wave_tx_rec;

/* dummy functions */
int num_kanal = 1; /* only one channel used for debugging */
sender_t *get_sender_by_empfangsfrequenz(double __attribute__((unused)) freq) { return NULL; }
dispmeasparam_t *display_measurements_add(dispmeas_t __attribute__((unused)) *disp, char __attribute__((unused)) *name, char __attribute__((unused)) *format, enum display_measurements_type __attribute__((unused)) type, enum display_measurements_bar __attribute__((unused)) bar, double __attribute__((unused)) min, double __attribute__((unused)) max, double __attribute__((unused)) mark) { return NULL; }
void display_measurements_update(dispmeasparam_t __attribute__((unused)) *param, double __attribute__((unused)) value, double __attribute__((unused)) value2) { }

#define OPT_METERING	1000
#define OPT_COIN_BOX	1001

static void print_help(const char *arg0)
{
	printf("Usage: %s [options] <number>\n\n", arg0);
	/*      -                                                                             - */
	printf("This program generates a dialing sequence to make a call via B-Netz base\n");
	printf("station using an amateur radio transceiver.\n");
	printf("Also it can write an audio file (wave) to be fed into B-Netz base station for\n");
	printf("showing how it simulates a B-Netz phone doing an outgoing call.\n\n");
	printf(" -h --help\n");
	printf("        This help\n");
	printf(" -i --station-id <station ID>\n");
	printf("        5 Digits of ID of mobile station (default = '%s')\n", station_id);
#ifdef HAVE_ALSA
	printf(" -a --audio-device hw:<card>,<device>\n");
	printf("        Sound card and device number (default = '%s')\n", dsp_audiodev);
#endif
	printf(" -s --samplerate <rate>\n");
	printf("        Sample rate of sound device (default = '%d')\n", dsp_samplerate);
	printf(" -w --write-tx-wave <file>\n");
	printf("        Write audio to given wave file also.\n");
	printf(" -g --gebuehenimpuls\n");
	printf(" -g --metering\n");
	printf("        Indicate to base station that we have a charge meter on board.\n");
	printf("        This will allow the base station to send billing tones during call.\n");
	printf(" -m --muenztelefon\n");
	printf(" -m --coin-box\n");
	printf("        Indicate to base station that we are a pay phone. ('Muenztelefon')\n");
}

static void add_options(void)
{
	option_add('h', "help", 0);
	option_add('i', "station-id", 1);
	option_add('a', "audio-device", 1);
	option_add('s', "samplerate", 1);
	option_add('w', "write-tx-wave", 1);
	option_add('g', "gebuehrenimpuls", 0);
	option_add(OPT_METERING, "metering", 0);
	option_add('m', "muenztelefon", 0);
	option_add(OPT_COIN_BOX, "coin-box", 0);
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
	case 'g':
	case OPT_METERING:
		start_digit = 'S';
		break;
	case 'm':
	case OPT_COIN_BOX:
		start_digit = 'M';
		break;
	default:
		return -EINVAL;
	}

	return 1;
}


/* process next fsk bit.
 * if the dial string terminates, change to SILENCE mode
 */
static int fsk_send_bit(void __attribute__((unused)) *inst)
{
	struct impulstelegramm *impulstelegramm;

	if (!tx_telegramm || tx_telegramm_pos == 16) {
		switch (funkwahl[digit_pos]) {
		case '\0':
			LOGP(DBNETZ, LOGL_INFO, "Done sending dialing sequence\n");
			tx_mode = TX_MODE_SILENCE;
			tx_silence_count = 0;
			return -1;
		case 'w':
			if (!tx_telegramm)
				LOGP(DBNETZ, LOGL_INFO, "Sending channel allocation tone ('Kanalbelegung')\n");
			tx_telegramm = "0000000000000000";
			tx_telegramm_pos = 0;
			digit_pos++;
			break;
		default:
			switch (funkwahl[digit_pos]) {
			case 's':
				LOGP(DBNETZ, LOGL_INFO, "Sending start digit (no charging meater on board)\n");
				break;
			case 'S':
				LOGP(DBNETZ, LOGL_INFO, "Sending start digit (with charging meater on board)\n");
				break;
			case 'M':
				LOGP(DBNETZ, LOGL_INFO, "Sending start digit (Phone is a coin box.)\n");
				break;
			case 'e':
				LOGP(DBNETZ, LOGL_INFO, "Sending stop digit\n");
				break;
			default:
				LOGP(DBNETZ, LOGL_INFO, "Sending digit '%c'\n", funkwahl[digit_pos]);
			}
			impulstelegramm = bnetz_digit2telegramm(funkwahl[digit_pos]);
			if (!impulstelegramm) {
				LOGP(DBNETZ, LOGL_ERROR, "Illegal digit '%c', please fix!\n", funkwahl[digit_pos]);
				abort();
			}
			tx_telegramm = impulstelegramm->sequence;
			tx_telegramm_pos = 0;
			digit_pos++;
		}
	}

	return tx_telegramm[tx_telegramm_pos++];
}

/* encode audio */
static void encode_audio(sample_t *samples, uint8_t *power, int length)
{
	int count;

	memset(power, 1, length);

again:
	switch (tx_mode) {
	case TX_MODE_SILENCE:
		memset(samples, 0, length * sizeof(*samples));
		tx_silence_count += length;
		if (tx_silence_count >= (int)((double)dsp_samplerate * MAX_PAUSE)) {
			if (funkwahl[digit_pos])
				tx_mode = TX_MODE_FSK;
			else
				tx_mode = TX_MODE_DONE;
		}
		break;
	case TX_MODE_FSK:
		/* send FSK until it stops, then fill with silence */
		count = fsk_mod_send(&fsk_mod, samples, length, 0);
		samples += count;
		length -= count;
		if (length)
			goto again;
		break;
	default:
		break;
	}
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

	while (tx_mode != TX_MODE_DONE) {
#ifdef HAVE_ALSA
		count = sound_get_tosend(audio, buffer_size);
#else
		count = dsp_samplerate / 1000;
#endif
		if (count < 0) {
			LOGP(DDSP, LOGL_ERROR, "Failed to get number of samples in buffer (rc = %d)!\n", count);
			break;
		}

		/* get fsk / silence */
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
	int i;
	int rc, argi;
	int buffer_size;

	/* init */
	bnetz_init_telegramm();
	memset(&fsk_mod, 0, sizeof(fsk_mod));

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
	if (strlen(station_id) != 5) {
		printf("Given station ID '%s' has too many digits!\n", station_id);
		goto exit;
	}
	for (i = 0; station_id[i]; i++) {
		if (station_id[i] < '0' || station_id[i] > '9') {
			printf("Given station ID '%s' has invalid digits!\n", station_id);
			goto exit;
		}
	}

	/* check for valid phone number */
	dialing = argv[argi];
	if (strlen(dialing) < 4) {
		printf("Given phone number '%s' has too few digits! (less than minimum of 4 digits)\n", dialing);
		goto exit;
	}
	if (strlen(dialing) > 14) {
		printf("Given phone number '%s' has too many digits! (more than allowed 14 digits)\n", dialing);
		goto exit;
	}
	for (i = 0; dialing[i]; i++) {
		if (dialing[i] < '0' || dialing[i] > '9') {
			printf("Given phone number '%s' has invalid digits!\n", dialing);
			goto exit;
		}
	}
	if (dialing[0] != '0') {
		printf("Given phone number '%s' does not start with 0!\n", dialing);
		goto exit;
	}

	/* dial string: 640 ms pause, 640 ms 2070 HZ, 2 * {start, station ID, number, stop}, 640 ms pause */
	sprintf(funkwahl, "wwww%c%s%se%c%s%se", start_digit, station_id, dialing + 1, start_digit, station_id, dialing + 1);

	/* init fsk */
	if (fsk_mod_init(&fsk_mod, NULL, fsk_send_bit, dsp_samplerate, BIT_RATE, F0, F1, 1.0, 0, 0) < 0) {
		LOGP(DDSP, LOGL_ERROR, "FSK init failed!\n");
		goto exit;
	}

#ifdef HAVE_ALSA
	/* init sound */
	audio = sound_open(SOUND_DIR_PLAY, dsp_audiodev, NULL, NULL, NULL, 1, 0.0, dsp_samplerate, buffer_size, 1.0, 1.0, 4000.0, 2.0);
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

	/* exit fsk */
	fsk_mod_cleanup(&fsk_mod);

	options_free();

	return 0;
}

