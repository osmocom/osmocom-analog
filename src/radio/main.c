/* Radio main function
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

enum paging_signal;

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sched.h>
#include <errno.h>
#include <math.h>
#include <termios.h>
#include <unistd.h>
#include "../libsample/sample.h"
#include "../libdebug/debug.h"
#include "../libsdr/sdr_config.h"
#include "../libsdr/sdr.h"
#include "../libdisplay/display.h"
#include "../liboptions/options.h"
#include "radio.h"

#define DEFAULT_LO_OFFSET -1000000.0

void *sender_head = NULL;
int use_sdr = 0;
int num_kanal = 1; /* only one channel used for debugging */
int rt_prio = 1;
int fast_math = 0;

void *get_sender_by_empfangsfrequenz() { return NULL; }

static double frequency = 0.0;
static int samplerate = 100000;
static int latency = 30;
static const char *tx_wave_file = NULL;
static const char *rx_wave_file = NULL;
static const char *tx_audiodev = NULL;
static const char *rx_audiodev = NULL;
static enum modulation modulation = MODULATION_NONE;
static int rx = 0, tx = 0;
static double bandwidth_am = 4500.0;
static double bandwidth_fm = 15000.0;
static double bandwidth = 0.0;
static double deviation = 75000.0;
static double modulation_index = 1.0;
static double time_constant_us = 50.0;
static double volume = 1.0;
static int stereo = 0;
static int rds = 0;
static int rds2 = 0;

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

static int get_char()
{
	struct timeval tv = {0, 0};
	fd_set fds;
	char c = 0;
	int __attribute__((__unused__)) rc;

	FD_ZERO(&fds);
	FD_SET(0, &fds);
	select(0+1, &fds, NULL, NULL, &tv);
	if (FD_ISSET(0, &fds)) {
		rc = read(0, &c, 1);
		return c;
	} else
		return -1;
}

void print_help(const char *arg0)

{
	printf("Usage: %s --sdr-soapy|--sdr-uhd <sdr options> -f <frequency> -M <modulation> -R|-T [options]\n", arg0);
	/*      -                                                                             - */
	printf("\noptions:\n");
	printf(" -f --frequency <frequency>\n");
	printf("        Give frequency in Hertz.\n");
	printf(" -s --samplerate <sample rate>\n");
	printf("        Give signal processing sample rate in Hz. (default = %d)\n", samplerate);
	printf("        This sample rate must be high enough for the signal's spectrum to fit.\n");
	printf("        I will inform you, if this bandwidth is too low.\n");
	printf(" -r --tx-wave-file <filename>\n");
	printf("        Input transmitted audio from wave file\n");
	printf(" -w --rx-wave-file <filename>\n");
	printf("        Output received audio to wave file\n");
	printf(" -a --audio-device hw:<card>,<device>\n");
	printf("        Input audio from sound card's device number\n");
	printf(" -M --modulation fm | am | usb | lsb\n");
	printf("        fm = Frequency modulation to be used for VHF.\n");
	printf("        am = Amplitude modulation to be used for long/medium/short wave.\n");
	printf("        usb = Amplitude modulation with upper side band only.\n");
	printf("        lsb = Amplitude modulation with lower side band only.\n");
	printf(" -R --rx\n");
	printf("        Receive radio signal.\n");
	printf(" -T --tx\n");
	printf("        Transmit radio signal.\n");
	printf(" -B --bandwidth\n");
	printf("        Give bandwidth of audio frequency. (default AM=%.0f FM=%.0f)\n", bandwidth_am, bandwidth_fm);
	printf(" -D --deviation\n");
	printf("        Give deviation of frequency modulated signal. (default %.0f)\n", deviation);
	printf(" -I --modulation-index 0..1\n");
	printf("        Give modulation index of amplitude modulated signal. (default %.0f)\n", modulation_index);
	printf(" -E --emphasis <uS> | 0\n");
	printf("        Use given time constant of pre- and de-emphasis for frequency\n");
	printf("        modulation. Give 0 to disbale. (default = %.0f uS)\n", time_constant_us);
	printf("        VHF broadcast 50 uS in Europe and 75 uS in the United States.\n");
	printf("        Other radio FM should use 530 uS, to cover complete speech spectrum.\n");
	printf(" -V --volume %.3f\n", volume);
	printf("        Change volume of radio side. (Gains transmission, dampens reception)\n");
	printf(" -S --stereo\n");
	printf("        Enables stereo carrier for frequency modulated UHF broadcast.\n");
	printf("        It uses the 'Pilot-tone' system.\n");
	printf("    --fast-math\n");
	printf("        Use fast math approximation for slow CPU / ARM based systems.\n");
	printf("    --limesdr\n");
	printf("        Auto-select several required options for LimeSDR\n");
	printf("    --limesdr-mini\n");
	printf("        Auto-select several required options for LimeSDR Mini\n");
	sdr_config_print_help();
}

#define	OPT_FAST_MATH		1007
#define OPT_LIMESDR		1100
#define OPT_LIMESDR_MINI	1101

static void add_options(void)
{
	option_add('h', "help", 0);
	option_add('f', "frequency", 1);
	option_add('s', "samplerate", 1);
	option_add('r', "tx-wave-file", 1);
	option_add('w', "rx-wave-file", 1);
	option_add('a', "audio-device", 1);
	option_add('M', "modulation", 1);
	option_add('R', "rx", 0);
	option_add('T', "tx", 0);
	option_add('B', "bandwidth", 1);
	option_add('D', "deviation", 1);
	option_add('I', "modulation-index", 1);
	option_add('E', "emphasis", 1);
	option_add('V', "volume", 1);
	option_add('S', "stereo", 0);
	option_add(OPT_FAST_MATH, "fast-math", 0);
	option_add(OPT_LIMESDR, "limesdr", 0);
	option_add(OPT_LIMESDR_MINI, "limesdr-mini", 0);
        sdr_config_add_options();
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
	case 's':
		samplerate = atof(argv[argi]);
		break;
	case 'r':
		tx_wave_file = strdup(argv[argi]);
		break;
	case 'w':
		rx_wave_file = strdup(argv[argi]);
		break;
	case 'a':
		tx_audiodev = strdup(argv[argi]);
		rx_audiodev = strdup(argv[argi]);
		break;
	case 'M':
		if (!strcasecmp(argv[argi], "fm"))
			modulation = MODULATION_FM;
		else
		if (!strcasecmp(argv[argi], "am"))
			modulation = MODULATION_AM_DSB;
		else
		if (!strcasecmp(argv[argi], "usb"))
			modulation = MODULATION_AM_USB;
		else
		if (!strcasecmp(argv[argi], "lsb"))
			modulation = MODULATION_AM_LSB;
		else
		{
			fprintf(stderr, "Invalid modulation option, use '-h' for help!\n");
			return -EINVAL;
		}
		break;
	case 'R':
		rx = 1;
		break;
	case 'T':
		tx = 1;
		break;
	case 'B':
		bandwidth = atof(argv[argi]);
		break;
	case 'D':
		deviation = atof(argv[argi]);
		break;
	case 'I':
		modulation_index = atof(argv[argi]);
		if (modulation_index < 0.0 || modulation_index > 1.0) {
			fprintf(stderr, "Invalid modulation index, use '-h' for help!\n");
			return -EINVAL;
		}
		break;
	case 'E':
		time_constant_us = atof(argv[argi]);
		break;
	case 'V':
		volume = atof(argv[argi]);
		break;
	case 'S':
		stereo = 1;
		break;
	case OPT_FAST_MATH:
		fast_math = 1;
		break;
	case OPT_LIMESDR:
		{
			char *argv_lime[] = { argv[0],
				"--sdr-soapy",
				"--sdr-rx-antenna", "LNAL",
				"--sdr-rx-gain", "50",
				"--sdr-tx-gain", "50",
				"--sdr-samplerate", "5000000",
				"--sdr-bandwidth", "15000000",
			};
			int argc_lime = sizeof(argv_lime) / sizeof (*argv_lime);
			return options_command_line(argc_lime, argv_lime, handle_options);
		}
	case OPT_LIMESDR_MINI:
		{
			char *argv_lime[] = { argv[0],
				"--sdr-soapy",
				"--sdr-rx-antenna", "LNAW",
				"--sdr-tx-antenna", "BAND2",
				"--sdr-rx-gain", "50",
				"--sdr-tx-gain", "50",
				"--sdr-samplerate", "5000000",
				"--sdr-bandwidth", "15000000",
			};
			int argc_lime = sizeof(argv_lime) / sizeof (*argv_lime);
			return options_command_line(argc_lime, argv_lime, handle_options);
		}
	default:
		return sdr_config_handle_options(short_option, argi, argv);
	}

	return 1;
}

int main(int argc, char *argv[])
{
	int rc, argi;
	radio_t radio;
	struct termios term, term_orig;
	int c;
	int latspl;

	debuglevel = 0;

	sdr_config_init(DEFAULT_LO_OFFSET);

	/* handle options / config file */
	add_options();
	rc = options_config_file("~/.osmocom/analog/radio.conf", handle_options);
	if (rc < 0)
		return 0;
	argi = options_command_line(argc, argv, handle_options);
	if (argi <= 0)
		return argi;

	if (frequency == 0.0) {
		printf("No frequency given, I suggest to use 100000000 (100 MHz) and FM\n\n");
		print_help(argv[0]);
		exit(0);
	}

	/* global inits */
	fm_init(fast_math);
	am_init(fast_math);

	rc = sdr_configure(samplerate);
	if (rc < 0)
		return rc;
	if (rc == 0) {
		fprintf(stderr, "Please select SDR, use '-h' for help!\n");
		exit(0);
	}

	if (modulation == MODULATION_NONE) {
		fprintf(stderr, "Please select modulation, use '-h' for help!\n");
		exit(0);
	}

	if (bandwidth == 0) {
		if (modulation == MODULATION_FM)
			bandwidth = bandwidth_fm;
		else
			bandwidth = bandwidth_am;
	}

	if (stereo && modulation != MODULATION_FM) {
		fprintf(stderr, "Stereo works with FM only, use '-h' for help!\n");
		exit(0);
	}
	if (!rx && !tx) {
		fprintf(stderr, "You need to specify --rx (receiver) and/or --tx (transmitter), use '-h' for help!\n");
		exit(0);
	}
	if (stereo && bandwidth != 15000.0) {
		fprintf(stderr, "Warning: Stereo works with bandwidth of 15 KHz only, using this bandwidth!\n");
	}
	if (stereo && time_constant_us != 75.0 && time_constant_us != 50.0) {
		fprintf(stderr, "Stereo works with time constant of 50 uS or 75 uS only, use '-h' for help!\n");
		exit(0);
	}

	/* now we have latency and sample rate */
	latspl = samplerate * latency / 1000;

	rc = radio_init(&radio, latspl, samplerate, tx_wave_file, rx_wave_file, (tx) ? tx_audiodev : NULL, (rx) ? rx_audiodev : NULL, modulation, bandwidth, deviation, modulation_index, time_constant_us, volume, stereo, rds, rds2);
	if (rc < 0) {
		fprintf(stderr, "Failed to initialize radio with given options, exitting!\n");
		exit(0);
	}

	void *sdr = NULL;
	float *sendbuff = NULL;

	sendbuff = calloc(latspl * 2, sizeof(*sendbuff));
	if (!sendbuff) {
		fprintf(stderr, "No mem!\n");
		goto error;
	}

	/* real time priority */
	if (rt_prio > 0) {
		struct sched_param schedp;
		int rc;

		memset(&schedp, 0, sizeof(schedp));
		schedp.sched_priority = rt_prio;
		rc = sched_setscheduler(0, SCHED_RR, &schedp);
		if (rc)
			fprintf(stderr, "Error setting SCHED_RR with prio %d\n", rt_prio);
	}

	double tx_frequencies[1], rx_frequencies[1];
	tx_frequencies[0] = frequency;
	rx_frequencies[0] = frequency;
	sdr = sdr_open(NULL, tx_frequencies, rx_frequencies, 1, 0.0, samplerate, latspl, 0.0, 0.0);
	if (!sdr)
		goto error;
	sdr_start(sdr);

	/* prepare terminal */
	tcgetattr(0, &term_orig);
	term = term_orig;
	term.c_lflag &= ~(ISIG|ICANON|ECHO);
	term.c_cc[VMIN]=1;
	term.c_cc[VTIME]=2;
	tcsetattr(0, TCSANOW, &term);

	/* catch signals */
	signal(SIGINT, sighandler);
	signal(SIGHUP, sighandler);
	signal(SIGTERM, sighandler);
	signal(SIGPIPE, sighandler);

	printf("Starting radio...\n");
	rc = radio_start(&radio);
	if (rc < 0) {
		fprintf(stderr, "Failed to start radio's streaming, exitting!\n");
		goto error;
	}

	int tosend, got;
	while (!quit) {
		usleep(1000);
		got = sdr_read(sdr, (void *)sendbuff, latspl, 0, NULL);
		if (rx) {
			got = radio_rx(&radio, sendbuff, got);
			if (got < 0)
				break;
		}
		tosend = sdr_get_tosend(sdr, latspl);
		if (tosend > latspl / 10)
			tosend = latspl / 10;
		if (tosend == 0) {
			continue;
		}
		/* perform radio modulation */
		if (tx)
			tosend = radio_tx(&radio, sendbuff, tosend);
		else
			memset(sendbuff, 0, tosend * sizeof(*sendbuff) * 2);
		if (tosend < 0)
			break;
		/* write to SDR */
		sdr_write(sdr, (void *)sendbuff, NULL, tosend, NULL, NULL, 0);

		/* process keyboard input */
next_char:
		c = get_char();
		switch (c) {
		case 3:
			/* quit */
//			if (clear_console_text)
//				clear_console_text();
			printf("CTRL+c received, quitting!\n");
			quit = 1;
			goto next_char;
#if 0
- carrier frequency
- deviation
- modulation index
- stereo pilot
		case 'm':
			/* toggle measurements display */
			display_iq_on(0);
			display_spectrum_on(0);
			display_measurements_on(-1);
			goto next_char;
#endif
		case 'q':
			/* toggle IQ display */
			display_measurements_on(0);
			display_spectrum_on(0);
			display_iq_on(-1);
			goto next_char;
		case 's':
			/* toggle spectrum display */
			display_measurements_on(0);
			display_iq_on(0);
			display_spectrum_on(-1);
			goto next_char;
		case 'B':
			calibrate_bias();
			goto next_char;
		}
	}

	/* reset signals */
	signal(SIGINT, SIG_DFL);
	signal(SIGHUP, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);

	/* reset terminal */
	tcsetattr(0, TCSANOW, &term_orig);
	
error:
	/* reset real time prio */
	if (rt_prio > 0) {
		struct sched_param schedp;

		memset(&schedp, 0, sizeof(schedp));
		schedp.sched_priority = 0;
		sched_setscheduler(0, SCHED_OTHER, &schedp);
	}

	free(sendbuff);
	if (sdr)
		sdr_close(sdr);
	radio_exit(&radio);

	/* global exits */
	fm_exit();
	am_exit();

	return 0;
}

