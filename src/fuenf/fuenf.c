/* 5-Ton-Folge call processing
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

#define CHAN fuenf->sender.kanal

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "../libsample/sample.h"
#include "../libdebug/debug.h"
#include "../libmobile/call.h"
#include "../libmobile/cause.h"
#include "../libosmocc/message.h"
#include "../liboptions/options.h"
#include "fuenf.h"
#include "dsp.h"

void bos_list_channels(void)
{
	printf("Channels\tBand\n");
	printf("------------------------\n");
	printf("101 - 125\t2 Meter\n");
	printf("  1 -  92\t2 Meter\n");
	printf("347 - 509\t4 Meter\n");
        printf("-> Give channel number or any frequency in MHz (using a dot, e.g. '169.810').\n");
        printf("\n");
}

/* Convert channel to frequency */
double bos_kanal2freq(const char *kanal)
{
        int k;

	if (strchr(kanal, '.'))
		return atof(kanal) * 1e6;

	k = atoi(kanal);

	if (k >= 101 && k <= 125)
		return 169.810e6 + 20e3 * (k - 101);

	if (k >= 1 && k <= 92)
		return 172.160e6 + 20e3 * (k - 1);

	if (k >= 347 && k <= 509)
		return 84.015e6 + 20e3 * (k - 347);

	return 0.0;
}

/* Convert frequency to channel, if possible */
const char *bos_freq2kanal(const char *freq)
{
	double f;
	char kanal[8];

	if (!strchr(freq, '.'))
		return options_strdup(freq);

	f = atof(freq) * 1e6;

	if (f >= 169.810e6 && f <= 170.290e6 && fmod(f - 169.810e6, 20e3) == 0.0) {
		sprintf(kanal, "%.0f", (f - 169.810e6) / 20e3 + 101);
		return options_strdup(kanal);
	}

	if (f >= 172.160e6 && f <= 173.980e6 && fmod(f - 172.160e6, 20e3) == 0.0) {
		sprintf(kanal, "%.0f", (f - 172.160e6) / 20e3 + 1);
		return options_strdup(kanal);
	}

	if (f >= 84.015e6 && f <= 87.255e6 && fmod(f - 84.015e6, 20e3) == 0.0) {
		sprintf(kanal, "%.0f", (f - 84.015e6) / 20e3 + 347);
		return options_strdup(kanal);
	}

	return options_strdup(freq);
}

const char *fuenf_state_name[] = {
	"IDLE",
	"RUF",
	"DURCHSAGE",
};

const char *fuenf_funktion_name[8] = {
	"Ruf",
	"Feueralarm",
	"Probealarm",
	"Warnung der Befoelkerung",
	"ABC-Alarm",
	"Entwarnung",
	"Katastrophenalarm",
	"Turbo-Scanner",
};

/* check if number is a valid pager ID */
const char *bos_number_valid(const char *number)
{
	/* assume that the number has valid length(s) and digits */

	if (number[5] && (number[5] < '0' || number[5] > '6'))
		return "Illegal 'Sirenenalarm' digit #6 (Use 1..6 only)";
	return NULL;
}

int fuenf_init(void)
{
	return 0;
}

void fuenf_exit(void)
{
}

static void fuenf_display_status(void)
{
	sender_t *sender;
	fuenf_t *fuenf;

	display_status_start();
	for (sender = sender_head; sender; sender = sender->next) {
		fuenf = (fuenf_t *) sender;
		display_status_channel(fuenf->sender.kanal, NULL, fuenf_state_name[fuenf->state]);
	}
	display_status_end();
}

void fuenf_new_state(fuenf_t *fuenf, enum fuenf_state new_state)
{
	if (fuenf->state == new_state)
		return;
	PDEBUG_CHAN(DFUENF, DEBUG_DEBUG, "State change: %s -> %s\n", fuenf_state_name[fuenf->state], fuenf_state_name[new_state]);
	fuenf->state = new_state;
	fuenf_display_status();
}

static int fuenf_scan_or_loopback(fuenf_t *fuenf)
{
	char rufzeichen[16];

	if (fuenf->scan_from < fuenf->scan_to) {
		sprintf(rufzeichen, "%05d", fuenf->scan_from++);
		PDEBUG_CHAN(DFUENF, DEBUG_NOTICE, "Transmitting ID '%s'.\n", rufzeichen);
		dsp_setup(fuenf, rufzeichen, fuenf->default_funktion);
		return 1;
	}

	if (fuenf->sender.loopback) {
		PDEBUG(DFUENF, DEBUG_INFO, "Sending 5-Ton-Ruf for loopback test.\n");
		dsp_setup(fuenf, "10357", FUENF_FUNKTION_FEUER);
		return 1;
	}

	return 0;
}

/* Create transceiver instance and link to a list. */
int fuenf_create(const char *kanal, double frequency, const char *device, int use_sdr, int samplerate, double rx_gain, double tx_gain, int tx, int rx, double max_deviation, double signal_deviation, enum fuenf_funktion funktion, uint32_t scan_from, uint32_t scan_to, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, const char *read_tx_wave, int loopback)
{
	fuenf_t *fuenf;
	int rc;

	fuenf = calloc(1, sizeof(*fuenf));
	if (!fuenf) {
		PDEBUG(DFUENF, DEBUG_ERROR, "No memory!\n");
		return -ENOMEM;
	}

	PDEBUG(DFUENF, DEBUG_DEBUG, "Creating '5-Ton-Folge' instance for 'Kanal' = %s (sample rate %d).\n", kanal, samplerate);

	/* init general part of transceiver */
	rc = sender_create(&fuenf->sender, kanal, frequency, frequency, device, use_sdr, samplerate, rx_gain, tx_gain, 0, 0, write_rx_wave, write_tx_wave, read_rx_wave, read_tx_wave, loopback, PAGING_SIGNAL_NONE);
	if (rc < 0) {
		PDEBUG(DFUENF, DEBUG_ERROR, "Failed to init transceiver process!\n");
		goto error;
	}

	/* init audio processing */
	rc = dsp_init_sender(fuenf, samplerate, max_deviation, signal_deviation);
	if (rc < 0) {
		PDEBUG(DFUENF, DEBUG_ERROR, "Failed to init audio processing!\n");
		goto error;
	}

	fuenf->tx = tx;
	fuenf->rx = rx;
	fuenf->default_funktion = funktion;
	fuenf->scan_from = scan_from;
	fuenf->scan_to = scan_to;

	fuenf_display_status();

	PDEBUG(DFUENF, DEBUG_NOTICE, "Created 'Kanal' %s\n", kanal);

	/* start scanning, if enabled, otherwise send loopback sequence, if enabled */
	fuenf_scan_or_loopback(fuenf);

	return 0;

error:
	fuenf_destroy(&fuenf->sender);

	return rc;
}

/* Destroy transceiver instance and unlink from list. */
void fuenf_destroy(sender_t *sender)
{
	fuenf_t *fuenf = (fuenf_t *) sender;

	PDEBUG(DFUENF, DEBUG_DEBUG, "Destroying '5-Ton-Folge' instance for 'Kanal' = %s.\n", sender->kanal);

	dsp_cleanup_sender(fuenf);
	sender_destroy(&fuenf->sender);
	free(fuenf);
}

/* call sign was transmitted */
void fuenf_tx_done(fuenf_t *fuenf)
{
	PDEBUG_CHAN(DFUENF, DEBUG_INFO, "Done sending 5-Ton-Ruf.\n");

	/* start scanning, if enabled, otherwise send loopback sequence, if enabled */
	if (fuenf_scan_or_loopback(fuenf)) {
		return;
	}

	/* go talker state */
	if (fuenf->callref && fuenf->tx_funktion == FUENF_FUNKTION_RUF) {
		PDEBUG_CHAN(DFUENF, DEBUG_INFO, "Caller may talk now.\n");
		fuenf_new_state(fuenf, FUENF_STATE_DURCHSAGE);
		return;
	}

	/* go idle */
	fuenf_new_state(fuenf, FUENF_STATE_IDLE);
	if (fuenf->callref) {
		PDEBUG_CHAN(DFUENF, DEBUG_INFO, "Releasing call toward network.\n");
		call_up_release(fuenf->callref, CAUSE_NORMAL);
	}
}

void fuenf_rx_callsign(fuenf_t *fuenf, const char *callsign)
{
	PDEBUG_CHAN(DFUENF, DEBUG_INFO, "Received 5-Ton-Ruf with call sign '%s'.\n", callsign);
}

void fuenf_rx_function(fuenf_t *fuenf, enum fuenf_funktion funktion)
{
	PDEBUG_CHAN(DFUENF, DEBUG_INFO, "Received function '%s'.\n", fuenf_funktion_name[funktion]);
}

void call_down_clock(void)
{
}

/* Call control starts call towards transmitter. */
int call_down_setup(int callref, const char __attribute__((unused)) *caller_id, enum number_type __attribute__((unused)) caller_type, const char *dialing)
{
	char channel = '\0';
	sender_t *sender;
	fuenf_t *fuenf;
	char rufzeichen[6];
	enum fuenf_funktion funktion;

	/* find transmitter */
	for (sender = sender_head; sender; sender = sender->next) {
		/* skip channels that are different than requested */
		if (channel && sender->kanal[0] != channel)
			continue;
		fuenf = (fuenf_t *) sender;
		if (fuenf->state != FUENF_STATE_IDLE)
			continue;
		/* check if base station cannot transmit */
		if (!fuenf->tx)
			continue;
		break;
	}
	if (!sender) {
		if (channel)
			PDEBUG(DFUENF, DEBUG_NOTICE, "Cannot page, because given station not available, rejecting!\n");
		else
			PDEBUG(DFUENF, DEBUG_NOTICE, "Cannot page, no trasmitting station idle, rejecting!\n");
		return -CAUSE_NOCHANNEL;
	}

	strncpy(rufzeichen, dialing, 5);
	rufzeichen[5] = '\0';
	switch (dialing[5]) {
	case '0':
		funktion = FUENF_FUNKTION_RUF;
		break;
	case '1':
		funktion = FUENF_FUNKTION_FEUER;
		break;
	case '2':
		funktion = FUENF_FUNKTION_PROBE;
		break;
	case '3':
		funktion = FUENF_FUNKTION_WARNUNG;
		break;
	case '4':
		funktion = FUENF_FUNKTION_ABC;
		break;
	case '5':
		funktion = FUENF_FUNKTION_ENTWARNUNG;
		break;
	case '6':
		funktion = FUENF_FUNKTION_KATASTROPHE;
		break;
	case '\0':
		funktion = fuenf->default_funktion;
		break;
	default:
		return -CAUSE_INVALNUMBER;
	}

	PDEBUG_CHAN(DFUENF, DEBUG_INFO, "Sending 5-Ton-Ruf with call sign '%s' and function '%s'.\n", rufzeichen, fuenf_funktion_name[funktion]);

	dsp_setup(fuenf, rufzeichen, funktion);

	fuenf_new_state(fuenf, FUENF_STATE_RUF);
	fuenf->callref = callref;
	/* must answer to hear paging tones. */
	call_up_answer(fuenf->callref, "");

	return 0;
}

void call_down_answer(int __attribute__((unused)) callref)
{
}


static void _release(int __attribute__((unused)) callref, int __attribute__((unused)) cause)
{
	sender_t *sender;
	fuenf_t *fuenf;

	PDEBUG(DFUENF, DEBUG_INFO, "Call has been disconnected by network.\n");

	for (sender = sender_head; sender; sender = sender->next) {
		fuenf = (fuenf_t *) sender;
		if (fuenf->callref == callref)
			break;
	}
	if (!sender) {
		PDEBUG(DBNETZ, DEBUG_NOTICE, "Outgoing release, but no callref!\n");
		/* don't send release, because caller already released */
		return;
        }

	/* remove call. go idle, if talking */
        fuenf->callref = 0;
	if (fuenf->state == FUENF_STATE_DURCHSAGE)
		fuenf_new_state(fuenf, FUENF_STATE_IDLE);
}

void call_down_disconnect(int callref, int cause)
{
	_release(callref, cause);

	call_up_release(callref, cause);
}

/* Call control releases call toward mobile station. */
void call_down_release(int callref, int cause)
{
	_release(callref, cause);
}

/* Receive audio from call instance. */
void call_down_audio(int callref, sample_t *samples, int count)
{
	sender_t *sender;
	fuenf_t *fuenf;

	for (sender = sender_head; sender; sender = sender->next) {
		fuenf = (fuenf_t *) sender;
		if (fuenf->callref == callref)
			break;
	}
	if (!sender)
		return;

	if (fuenf->state == FUENF_STATE_DURCHSAGE) {
		sample_t up[(int)((double)count * fuenf->sender.srstate.factor + 0.5) + 10];
		count = samplerate_upsample(&fuenf->sender.srstate, samples, count, up);
		jitter_save(&fuenf->sender.dejitter, up, count);
	}
}

void dump_info(void) {}

