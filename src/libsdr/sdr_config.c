/* Config for SDR
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
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <getopt.h>
#include "../libsample/sample.h"
#include "sdr.h"
#include "sdr_config.h"

static int got_init = 0;
extern int use_sdr;
sdr_config_t *sdr_config = NULL;

void sdr_config_init(void)
{
	sdr_config = calloc(1, sizeof(*sdr_config));
	memset(sdr_config, 0, sizeof(*sdr_config));
	sdr_config->device_args = "";
	sdr_config->stream_args = "";
	sdr_config->tune_args = "";

	got_init = 1;
}

void sdr_config_print_help(void)
{
	printf("\nSDR options:\n");
	/*      -                                                                             - */
#ifdef HAVE_UHD
	printf("    --sdr-uhd\n");
	printf("        Force UHD driver\n");
#endif
#ifdef HAVE_SOAPY
	printf("    --sdr-soapy\n");
	printf("        Force SoapySDR driver\n");
#endif
	printf("    --sdr-channel <channel #>\n");
	printf("        Give channel number for multi channel SDR device (default = %d)\n", sdr_config->channel);
	printf("    --sdr-device-args <args>\n");
	printf("    --sdr-stream-args <args>\n");
	printf("    --sdr-tune-args <args>\n");
	printf("        Optional SDR device arguments, seperated by comma\n");
	printf("        e.g. --sdr-device-args <key>=<value>[,<key>=<value>[,...]]\n");
	printf("    --sdr-samplerate <samplerate>\n");
	printf("        Sample rate to use with SDR. By default it equals the regular sample\n");
	printf("        rate.\n");
	printf("    --sdr-bandwidth <bandwidth>\n");
	printf("        Give IF filter bandwidth to use. If not, sample rate is used.\n");
	printf("    --sdr-rx-antenna <name>\n");
	printf("        SDR device's RX antenna name, use 'list' to get a list\n");
	printf("    --sdr-tx-antenna <name>\n");
	printf("        SDR device's TX antenna name, use 'list' to get a list\n");
	printf("    --sdr-rx-gain <gain>\n");
	printf("        SDR device's RX gain in dB (default = %.1f)\n", sdr_config->rx_gain);
	printf("    --sdr-tx-gain <gain>\n");
	printf("        SDR device's TX gain in dB (default = %.1f)\n", sdr_config->tx_gain);
	printf("    --write-iq-rx-wave <file>\n");
	printf("        Write received IQ data to given wave file.\n");
	printf("    --write-iq-tx-wave <file>\n");
	printf("        Write transmitted IQ data to given wave file.\n");
	printf("    --read-iq-rx-wave <file>\n");
	printf("        Replace received IQ data by given wave file.\n");
	printf("    --read-iq-tx-wave <file>\n");
	printf("        Replace transmitted IQ data by given wave file.\n");
	printf("    --sdr-swap-links\n");
	printf("        Swap RX and TX frequencies for loopback tests over the air.\n");
#ifdef HAVE_UHD
	printf("    --sdr-uhd-tx-timestamps\n");
	printf("        Use TX timestamps on UHD device. (May not work with some devices!)\n");
#endif
}

void sdr_config_print_hotkeys(void)
{
	printf("Press 'q' key to toggle display of RX I/Q vector.\n");
	printf("Press 's' key to toggle display of RX spectrum.\n");
	printf("Press shift + 'B' key to remove DC level.\n");
}

#define	OPT_SDR_UHD		1500
#define	OPT_SDR_SOAPY		1501
#define	OPT_SDR_CHANNEL		1502
#define	OPT_SDR_DEVICE_ARGS	1503
#define	OPT_SDR_STREAM_ARGS	1504
#define	OPT_SDR_TUNE_ARGS	1505
#define	OPT_SDR_RX_ANTENNA	1506
#define	OPT_SDR_TX_ANTENNA	1507
#define	OPT_SDR_RX_GAIN		1508
#define	OPT_SDR_TX_GAIN		1509
#define	OPT_SDR_SAMPLERATE	1510
#define	OPT_SDR_BANDWIDTH	1511
#define	OPT_WRITE_IQ_RX_WAVE	1512
#define	OPT_WRITE_IQ_TX_WAVE	1513
#define	OPT_READ_IQ_RX_WAVE	1514
#define	OPT_READ_IQ_TX_WAVE	1515
#define	OPT_SDR_SWAP_LINKS	1516
#define OPT_SDR_UHD_TX_TS	1517

struct option sdr_config_long_options[] = {
	{"sdr-uhd", 0, 0, OPT_SDR_UHD},
	{"sdr-soapy", 0, 0, OPT_SDR_SOAPY},
	{"sdr-channel", 1, 0, OPT_SDR_CHANNEL},
	{"sdr-device-args", 1, 0, OPT_SDR_DEVICE_ARGS},
	{"sdr-stream-args", 1, 0, OPT_SDR_STREAM_ARGS},
	{"sdr-tune-args", 1, 0, OPT_SDR_TUNE_ARGS},
	{"sdr-samplerate", 1, 0, OPT_SDR_SAMPLERATE},
	{"sdr-bandwidth", 1, 0, OPT_SDR_BANDWIDTH},
	{"sdr-rx-antenna", 1, 0, OPT_SDR_RX_ANTENNA},
	{"sdr-tx-antenna", 1, 0, OPT_SDR_TX_ANTENNA},
	{"sdr-rx-gain", 1, 0, OPT_SDR_RX_GAIN},
	{"sdr-tx-gain", 1, 0, OPT_SDR_TX_GAIN},
	{"write-iq-rx-wave", 1, 0, OPT_WRITE_IQ_RX_WAVE},
	{"write-iq-tx-wave", 1, 0, OPT_WRITE_IQ_TX_WAVE},
	{"read-iq-rx-wave", 1, 0, OPT_READ_IQ_RX_WAVE},
	{"read-iq-tx-wave", 1, 0, OPT_READ_IQ_TX_WAVE},
	{"sdr-swap-links", 0, 0, OPT_SDR_SWAP_LINKS},
	{"sdr-uhd-tx-timestamps", 0, 0, OPT_SDR_UHD_TX_TS},
	{0, 0, 0, 0}
};

const char *sdr_config_optstring = "";

int sdr_config_opt_switch(int c, int *skip_args)
{
	switch (c) {
	case OPT_SDR_UHD:
#ifdef HAVE_UHD
		sdr_config->uhd = 1;
		use_sdr = 1;
#else
		fprintf(stderr, "UHD SDR support not compiled in!\n");
		exit(0);
#endif
		*skip_args += 1;
		break;
	case OPT_SDR_SOAPY:
#ifdef HAVE_SOAPY
		sdr_config->soapy = 1;
		use_sdr = 1;
#else
		fprintf(stderr, "SoapySDR support not compiled in!\n");
		exit(0);
#endif
		*skip_args += 1;
		break;
	case OPT_SDR_CHANNEL:
		sdr_config->channel = atoi(optarg);
		*skip_args += 2;
		break;
	case OPT_SDR_DEVICE_ARGS:
		sdr_config->device_args = strdup(optarg);
		*skip_args += 2;
		break;
	case OPT_SDR_STREAM_ARGS:
		sdr_config->stream_args = strdup(optarg);
		*skip_args += 2;
		break;
	case OPT_SDR_TUNE_ARGS:
		sdr_config->tune_args = strdup(optarg);
		*skip_args += 2;
		break;
	case OPT_SDR_SAMPLERATE:
		sdr_config->samplerate = atoi(optarg);
		*skip_args += 2;
		break;
	case OPT_SDR_BANDWIDTH:
		sdr_config->bandwidth = atof(optarg);
		*skip_args += 2;
		break;
	case OPT_SDR_RX_ANTENNA:
		sdr_config->rx_antenna = strdup(optarg);
		*skip_args += 2;
		break;
	case OPT_SDR_TX_ANTENNA:
		sdr_config->tx_antenna = strdup(optarg);
		*skip_args += 2;
		break;
	case OPT_SDR_RX_GAIN:
		sdr_config->rx_gain = atof(optarg);
		*skip_args += 2;
		break;
	case OPT_SDR_TX_GAIN:
		sdr_config->tx_gain = atof(optarg);
		*skip_args += 2;
		break;
	case OPT_WRITE_IQ_RX_WAVE:
		sdr_config->write_iq_rx_wave = strdup(optarg);
		*skip_args += 2;
		break;
	case OPT_WRITE_IQ_TX_WAVE:
		sdr_config->write_iq_tx_wave = strdup(optarg);
		*skip_args += 2;
		break;
	case OPT_READ_IQ_RX_WAVE:
		sdr_config->read_iq_rx_wave = strdup(optarg);
		*skip_args += 2;
		break;
	case OPT_READ_IQ_TX_WAVE:
		sdr_config->read_iq_tx_wave = strdup(optarg);
		*skip_args += 2;
		break;
	case OPT_SDR_SWAP_LINKS:
		sdr_config->swap_links = 1;
		*skip_args += 1;
		break;
	case OPT_SDR_UHD_TX_TS:
		sdr_config->uhd_tx_timestamps = 1;
		*skip_args += 1;
		break;
	default:
		return -1;
	}

	return 0;
}

int sdr_configure(int samplerate)
{
	if (!got_init) {
		fprintf(stderr, "sdr_config_init was not called, please fix!\n");
		abort();
	}

	/* no sdr selected -> return 0 */
	if (!sdr_config->uhd && !sdr_config->soapy)
		return 0;

	if ((sdr_config->uhd == 1 && sdr_config->soapy == 1)) {
		fprintf(stderr, "You must choose which one you want: --sdr-uhd or --sdr-soapy\n");
		exit(0);
	}

	if (sdr_config->samplerate == 0)
		sdr_config->samplerate = samplerate;
	if (sdr_config->bandwidth == 0.0)
		sdr_config->bandwidth = (double)sdr_config->samplerate;

	/* sdr selected -> return 1 */
	return 1;
}


