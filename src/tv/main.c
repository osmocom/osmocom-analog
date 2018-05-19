/* JollyTV main function
 *
 * (C) 2017 by Andreas Eversberg <jolly@eversberg.eu>
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

enum paging_signal;

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <math.h>
#include "../libsample/sample.h"
#include "../libfilter/iir_filter.h"
#include "../libfm/fm.h"
#include "../libwave/wave.h"
#include "../libimage/img.h"
#include "../libdebug/debug.h"
#ifdef HAVE_SDR
#include "../libsdr/sdr_config.h"
#include "../libsdr/sdr.h"
#endif
#include "../liboptions/options.h"
#include "bas.h"
#include "tv_modulate.h"
#include "channels.h"

#define DEFAULT_LO_OFFSET -3000000.0

void *sender_head = NULL;
int use_sdr = 0;
int num_kanal = 1; /* only one channel used for debugging */

void *get_sender_by_empfangsfrequenz() { return NULL; }

static double __attribute__((__unused__)) modulation = 0.7; /* level of modulation for I/Q amplitudes */
static double frequency = 0.0;
static int fbas = 1;
static int tone = 1;
static double circle_radius = 6.7;
static int color_bar = 0;
static int grid_only = 0;
static const char *station_id = "Jolly  Roger";
static int __attribute__((__unused__)) latency = 30;
static double samplerate = 10e6;
static const char *wave_file = NULL;

/* global variable to quit main loop */
int quit = 0;

void sighandler(int sigset)
{
	if (sigset == SIGHUP)
		return;
	if (sigset == SIGPIPE)
		return;

//	clear_console_text();
	printf("Signal received: %d\n", sigset);

	quit = 1;
}

void print_help(const char *arg0)

{
	printf("Usage: %s -f <frequency> | -c <channel>  <command>\n", arg0);
	/*      -                                                                             - */
	printf("\ncommand:\n");
	printf("        tx-fubk        Transmit FUBK test image (German PAL image)\n");
	printf("        tx-vcr         Transmit VCR calibration pattern\n");
	printf("        tx-img <image> Transmit given image file\n");
	printf("                       Use 4:3 image with 574 lines for best result.\n");
	printf("\ngeneral options:\n");
	printf(" -f --frequency <frequency>\n");
	printf("        Give frequency in Hertz.\n");
	printf(" -c --channel <channel>\n");
	printf("        Or give channel number.\n");
	printf("        Use 'list' to get a channel list.\n");
	printf(" -r --samplerate <frequency>\n");
	printf("        Give sample rate in Hertz.\n");
	printf(" -w --wave-file <filename>\n");
	printf("        Output to wave file instead of SDR\n");
	printf("\nsignal options:\n");
	printf(" -F --fbas 1 | 0\n");
	printf("        Turn color on or off. (default = %d)\n", fbas);
	printf(" -T --tone 1 | 0\n");
	printf("        Turn tone on or off. (default = %d)\n", tone);
	printf("\nFUBK options:\n");
	printf(" -R --circle-radius <radius> | 0\n");
	printf("        Use circle radius or 0 for off. (default = %.1f)\n", circle_radius);
	printf(" -C --color-bar 1 | 0\n");
	printf("        1 == Color bar on, 0 = Show complete middle field. (default = %d)\n", color_bar);
	printf("        For clean scope signal, also turn off circle by setting radius to 0.\n");
	printf(" -G --grid-only 1 | 0\n");
	printf("        1 == Show only the grid of the test image. (default = %d)\n", grid_only);
	printf(" -I --sation-id \"<text>\"\n");
	printf("        Give exactly 12 characters to display as Station ID.\n");
	printf("        (default = \"%s\")\n", station_id);
#ifdef HAVE_SDR
	printf("    --limesdr\n");
	printf("        Auto-select several required options for LimeSDR\n");
	sdr_config_print_help();
#endif
}

#define OPT_LIMESDR		1100

static void add_options(void)
{
	option_add('h', "help", 0);
	option_add('f', "frequency", 1);
	option_add('c', "channel", 1);
	option_add('r', "samplerate", 1);
	option_add('w', "wave-file", 1);
	option_add('F', "fbas", 1);
	option_add('T', "tone", 1);
	option_add('R', "circle-radius", 1);
	option_add('C', "color-bar", 1);
	option_add('G', "grid-only", 1);
	option_add('I', "station-id", 1);
#ifdef HAVE_SDR
	option_add(OPT_LIMESDR, "limesdr", 0);
	sdr_config_add_options();
#endif
}

static int handle_options(int short_option, int argi, char **argv)
{
	switch (short_option) {
	case 'h':
		print_help(argv[0]);
		return 0;
	case 'f':
		frequency = atof(argv[argi]);
		break;
	case 'c':
		if (!strcmp(argv[argi], "list")) {
			list_tv_channels();
			return 0;
		}
		frequency = get_tv_video_frequency(atoi(argv[argi]));
		if (frequency == 0.0) {
			fprintf(stderr, "Given channel number unknown, use \"-c list\" to get a list.\n");
			return -EINVAL;
		}
		break;
	case 'r':
		samplerate = atof(argv[argi]);
		break;
	case 'w':
		wave_file = strdup(argv[argi]);
		break;
	case 'F':
		fbas = atoi(argv[argi]);
		break;
	case 'T':
		tone = atoi(argv[argi]);
		break;
	case 'R':
		circle_radius = atof(argv[argi]);
		break;
	case 'C':
		color_bar = atoi(argv[argi]);
		break;
	case 'G':
		grid_only = atoi(argv[argi]);
		break;
	case 'I':
		station_id = strdup(argv[argi]);
		if (strlen(station_id) != 12) {
			fprintf(stderr, "Given station ID must be exactly 12 charaters long. (Use spaces to fill it.)\n");
			return -EINVAL;
		}
		break;
#ifdef HAVE_SDR
	case OPT_LIMESDR:
		{
			char *argv_lime[] = { argv[0],
				"--sdr-soapy",
				"--sdr-tx-gain", "50",
				"--sdr-lo-offset", "-3000000",
				"--sdr-bandwidth", "60000000",
				"-r", "13750000",
			};
			int argc_lime = sizeof(argv_lime) / sizeof (*argv_lime);
			return options_command_line(argc_lime, argv_lime, handle_options);
		}
#endif
	default:
#ifdef HAVE_SDR
		return sdr_config_handle_options(short_option, argi, argv);
#else
		return -EINVAL;
#endif
	}

	return 1;
}

static void tx_bas(sample_t *sample_bas, __attribute__((__unused__)) sample_t *sample_tone, __attribute__((__unused__)) uint8_t *power_tone, int samples)
{
	/* catch signals */
	signal(SIGINT, sighandler);
	signal(SIGHUP, sighandler);
	signal(SIGTERM, sighandler);
	signal(SIGPIPE, sighandler);

	if (wave_file) {
		wave_rec_t rec;
		int rc;
		sample_t *buffers[1];

		rc = wave_create_record(&rec, wave_file, samplerate, 1, 1.0);
		if (rc < 0) {
			// FIXME cleanup
			exit(0);
		}

		buffers[0] = sample_bas;
		wave_write(&rec, buffers, samples);

		wave_destroy_record(&rec);
	} else {
#ifdef HAVE_SDR
		float *buff = NULL;
		void *sdr = NULL;
		int latspl = samplerate * latency / 1000;
		float *sendbuff = NULL;

		if ((sdr_config->uhd == 0 && sdr_config->soapy == 0)) {
			fprintf(stderr, "You must choose SDR API you want: --sdr-uhd or --sdr-soapy or -w <file> to generate wave file.\n");
			goto error;
		}

		sendbuff = calloc(latspl * 2, sizeof(*sendbuff));
		if (!sendbuff) {
			fprintf(stderr, "No mem!\n");
			goto error;
		}

		/* modulate */
		buff = calloc(samples + 10.0, sizeof(sample_t) * 2);
		if (!buff) {
			fprintf(stderr, "No mem!\n");
			goto error;
		}
		tv_modulate(buff, samples, sample_bas, modulation);

		if (sample_tone) {
			/* bandwidth is 2*(deviation + 2*f(sig)) = 2 * (50 + 2*15) = 160khz */
			fm_mod_t mod;
			fm_mod_init(&mod, samplerate, 5500000.0, modulation * 0.1);
			mod.state = MOD_STATE_ON; /* do not ramp up */
			fm_modulate_complex(&mod, sample_tone, power_tone, samples, buff);
		}

		double tx_frequencies[1], rx_frequencies[1];
		tx_frequencies[0] = frequency;
		rx_frequencies[0] = frequency;
		sdr = sdr_open(NULL, tx_frequencies, rx_frequencies, 1, 0.0, samplerate, latspl, 0.0, 0.0);
		if (!sdr)
			goto error;
		sdr_start(sdr);

		int pos = 0, max = samples * 2;
		int s, ss, tosend;
		while (!quit) {
			usleep(1000);
			sdr_read(sdr, (void *)sendbuff, latspl, 0, NULL);
			tosend = sdr_get_tosend(sdr, latspl);
			if (tosend > latspl / 10)
				tosend = latspl / 10;
			if (tosend == 0) {
				continue;
			}
			for (s = 0, ss = 0; s < tosend; s++) {
				sendbuff[ss++] = buff[pos++];
				sendbuff[ss++] = buff[pos++];
				if (pos == max) {
					pos = 0;
				}
			}
			sdr_write(sdr, (void *)sendbuff, NULL, tosend, NULL, NULL, 0);
		}

	error:
		free(sendbuff);
		free(buff);
		if (sdr)
			sdr_close(sdr);
#endif
	}

	/* reset signals */
	signal(SIGINT, SIG_DFL);
	signal(SIGHUP, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);
}

static int tx_test_picture(enum bas_type type)
{
	bas_t bas;
	sample_t *test_bas = NULL;
	sample_t *test_tone = NULL;
	uint8_t *test_power = NULL;
	int i;
	int ret = -1;
	int count;

	/* test image, add some samples in case of overflow due to rounding errors */
	test_bas = calloc(samplerate / 25.0 * 4.0 + 10.0, sizeof(sample_t));
	if (!test_bas) {
		fprintf(stderr, "No mem!\n");
		goto error;
	}
	bas_init(&bas, samplerate, type, fbas, circle_radius, color_bar, grid_only, station_id, NULL, 0, 0);
	count = bas_generate(&bas, test_bas);
	count += bas_generate(&bas, test_bas + count);
	count += bas_generate(&bas, test_bas + count);
	count += bas_generate(&bas, test_bas + count);

	if (tone) {
		/* for more about audio modulation on tv, see: http://elektroniktutor.de/geraetetechnik/fston.html */
		test_tone = calloc(count, sizeof(sample_t));
		if (!test_tone) {
			fprintf(stderr, "No mem!\n");
			goto error;
		}
		test_power = calloc(count, sizeof(uint8_t));
		if (!test_power) {
			fprintf(stderr, "No mem!\n");
			goto error;
		}
		/* emphasis 50us, but 1000Hz does not change level */
		for (i = 0; i < count; i++)
			test_tone[i] = sin((double)i * 2.0 * M_PI * 1000.0 / samplerate) * 50000;
		memset(test_power, 1, count);
	}

	tx_bas(test_bas, test_tone, test_power, count);

	ret = 0;
error:
	free(test_bas);
	free(test_tone);
	free(test_power);
	return ret;
}

static int tx_img(const char *filename)
{
	unsigned short *img = NULL;
	int width, height;
	bas_t bas;
	sample_t *img_bas = NULL;
	int ret = -1;
	int count;

	img = load_img(&width, &height, filename, 0);
	if (!img) {
		fprintf(stderr, "Failed to load grey image '%s'\n", filename);
		return -1;
	}

	/* test image, add some samples in case of overflow due to rounding errors */
	img_bas = calloc(samplerate / 25.0 * 4.0 + 10.0, sizeof(sample_t));
	if (!img_bas) {
		fprintf(stderr, "No mem!\n");
		goto error;
	}
	bas_init(&bas, samplerate, BAS_IMAGE, fbas, circle_radius, color_bar, grid_only, NULL, img, width, height);
	count = bas_generate(&bas, img_bas);
	count += bas_generate(&bas, img_bas + count);
	count += bas_generate(&bas, img_bas + count);
	count += bas_generate(&bas, img_bas + count);

	tx_bas(img_bas, NULL, NULL, count);

	ret = 0;
error:
	free(img_bas);
	free(img);
	return ret;
}

int main(int argc, char *argv[])
{
	int __attribute__((__unused__)) rc, argi;

	debuglevel = 0;

#ifdef HAVE_SDR
	sdr_config_init(DEFAULT_LO_OFFSET);
#endif

	/* handle options / config file */
	add_options();
	rc = options_config_file("~/.osmocom/analog/osmotv.conf", handle_options);
	if (rc < 0)
		return 0;
	argi = options_command_line(argc, argv, handle_options);
	if (argi <= 0)
		return argi;

	if (frequency == 0.0 && !wave_file) {
		print_help(argv[0]);
		exit(0);
	}

	if (!wave_file) {
#ifdef HAVE_SDR
		rc = sdr_configure(samplerate);
		if (rc < 0)
			return rc;
#endif
	}

	if (argi >= argc) {
		fprintf(stderr, "Expecting command, use '-h' for help!\n");
		exit(0);
	} else if (!strcmp(argv[argi], "tx-fubk")) {
		tx_test_picture(BAS_FUBK);
	} else if (!strcmp(argv[argi], "tx-vcr")) {
		tx_test_picture(BAS_VCR);
	} else if (!strcmp(argv[argi], "tx-img")) {
		if (argi + 1 >= argc) {
			fprintf(stderr, "Expecting image file, use '-h' for help!\n");
			return -EINVAL;
		}
		tx_img(argv[argi + 1]);
	} else {
		fprintf(stderr, "Unknown command '%s', use '-h' for help!\n", argv[argi]);
		return -EINVAL;
	}

	return 0;
}

