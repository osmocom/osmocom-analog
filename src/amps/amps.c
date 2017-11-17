/* AMPS protocol handling
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

/*
 * Notes on frames and scheduling:
 *
 * If the amps->dsp_mode is set to transmit frames, fsk_frame() at dsp.c code
 * requests frames, modulates them and forwards them to sound device.  Whenever
 * the dsp.c code requests frame (if no frame exists or frame had been sent),
 * it calls amps_encode_frame_focc() or amps_encode_frame_fvc() of frame.c.
 * There it generates a sequence of frames (message train). If no sequence is
 * transmitted or a new sequence starts, amps_tx_frame_focc() or
 * amps_tx_frame_fvc() of amps.c is called.  There it sets message data and
 * other states according to the current trans->state.
 *
 * If a frame is received by dsp.c code, amps_decode_frame() at frame.c is
 * called. There the bits are decoded and messages are assembled from multiple
 * frames.  Then amps_rx_recc() at amps.c is called, so the received messages
 * are processed.
 */

#define CHAN amps->sender.kanal

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../common/sample.h"
#include "../common/debug.h"
#include "../common/call.h"
#include "../libmncc/cause.h"
#include "amps.h"
#include "dsp.h"
#include "frame.h"
#include "stations.h"
#include "main.h"

/* Uncomment this to test SAT via loopback */
//#define DEBUG_VC

#define SAT_TO1		5.0	/* 5 sec to detect after setup */
#define SAT_TO2		5.0	/* 5 sec lost until abort (specs say 5) */
#define PAGE_TRIES	2	/* how many times to page the phone */
#define PAGE_TO1	8.0	/* max time to wait for paging reply */
#define PAGE_TO2	4.0	/* max time to wait for last paging reply */
#define ALERT_TO	60.0	/* max time to wait for answer */
#define RELEASE_TIMER	5.0	/* max time to send release messages */

/* Call reference for calls from mobile station to network
   This offset of 0x400000000 is required for MNCC interface. */
static int new_callref = 0x40000000;

/* Convert channel number to frequency number of base station.
   Set 'uplink' to 1 to get frequency of mobile station. */
double amps_channel2freq(int channel, int uplink)
{
	double freq;

	if (!tacs) {
		if (uplink == 2)
			return -45.000 * 1e6;

		/* 832 channels, 990 not used, see TIA/EIA-136-110 */
		if (channel < 1 || channel > 1023 || (channel > 799 && channel < 991))
			return 0;

		if (channel >= 990) // 990 is not used
			channel -= 1023;

		freq = 870.030 + (channel - 1) * 0.030;

		if (uplink)
			freq -= 45.000;
	} else if (!jtacs) {
		if (uplink == 2)
			return -45.000 * 1e6;

		/* 600 channels */
		if (channel < 1 || channel > 600)
			return 0;

		freq = 935.0125 + (channel - 1) * 0.025;

		if (uplink)
			freq -= 45.000;
	} else {
		if (uplink == 2)
			return -55.000 * 1e6;

		/* 799 channels */
		if (channel < 1 || channel > 799)
			return 0;

		freq = 860.0125 + (channel - 1) * 0.025;

		if (uplink)
			freq += 55.000;
	}

	return freq * 1e6;
}

enum amps_chan_type amps_channel2type(int channel)
{
	if (!tacs) {
		if (channel >= 313 && channel <= 354)
			return CHAN_TYPE_CC;
	} else {
		if (channel >= 23 && channel <= 43)
			return CHAN_TYPE_CC;
		if (channel >= 323 && channel <= 343)
			return CHAN_TYPE_CC;
	}

	return CHAN_TYPE_VC;
}

const char *amps_channel2band(int channel)
{
	if (!tacs) {
		if (channel >= 991 && channel <= 1023)
			return "A''";
		if (channel >= 1 && channel <= 333)
			return "A";
		if (channel >= 334 && channel <= 666)
			return "B";
		if (channel >= 667 && channel <= 716)
			return "A'";
		if (channel >= 717 && channel <= 799)
			return "B'";
	} else {
		if (channel >= 1 && channel <= 300)
			return "A";
		if (channel >= 301 && channel <= 600)
			return "B";
	}

	return "<invalid>";
}

static inline int digit2binary(int digit)
{
	if (digit == '0')
		return 10;
	return digit - '0';
}

static inline int binary2digit(int binary)
{
	if (binary == 10)
		return '0';
	return binary + '0';
}

/* AMPS: convert NPA-NXX-XXXX to MIN1 and MIN2
 * NPA = numbering plan area (MIN2)
 * NXX = mobile exchange code
 * XXXX = telephone number within the exchange
 */
/* TACS: convert AREA-XXXXXX to MIN1 and MIN2
 * AREA = 3 + 1 Digits
 * XXXXXX = telephone number
 */
void amps_number2min(const char *number, uint32_t *min1, uint16_t *min2)
{
	int nlen = strlen(number);
	int i;

	if (nlen != 10) {
		fprintf(stderr, "illegal lenght %d. Must be 10, aborting!", nlen);
		abort();
	}

	for (i = 0; i < nlen; i++) {
		if (number[i] < '0' || number[i] > '9') {
			fprintf(stderr, "illegal number %s. Must consists only of digits 0..9, aborting!", number);
			abort();
		}
	}

	/* MIN2 */
	if (nlen == 10) {
		*min2 = digit2binary(number[0]) * 100 + digit2binary(number[1]) * 10 + digit2binary(number[2]) - 111;
		number += 3;
		nlen -= 3;
	}

	if (!tacs) {
		/* MIN1 */
		*min1 = ((uint32_t)(digit2binary(number[0]) * 100 + digit2binary(number[1]) * 10 + digit2binary(number[2]) - 111)) << 14;
		*min1 |= digit2binary(number[3]) << 10;
		*min1 |= digit2binary(number[4]) * 100 + digit2binary(number[5]) * 10 + digit2binary(number[6]) - 111;
	} else {
		/* MIN1 */
		*min1 = (number[0] - '0') << 20;
		*min1 |= (digit2binary(number[1]) * 100 + digit2binary(number[2]) * 10 + digit2binary(number[3]) - 111) << 10;
		*min1 |= digit2binary(number[4]) * 100 + digit2binary(number[5]) * 10 + digit2binary(number[6]) - 111;
	}
}

/* AMPS: convert MIN1 and MIN2 to NPA-NXX-XXXX
 */
/* TACS: convert MIN1 and MIN2 to AREA-XXXXXXX
 */
const char *amps_min22number(uint16_t min2)
{
	static char number[4];

	/* MIN2 */
	if (min2 > 999)
		strcpy(number, "???");
	else {
		number[0] = binary2digit((min2 / 100) + 1);
		number[1] = binary2digit(((min2 / 10) % 10) + 1);
		number[2] = binary2digit((min2 % 10) + 1);
	}
	number[3] = '\0';

	return number;
}

const char *amps_min12number(uint32_t min1)
{
	static char number[8];

	if (!tacs) {
		/* MIN1 */
		if ((min1 >> 14) > 999)
			strcpy(number, "???");
		else {
			number[0] = binary2digit(((min1 >> 14) / 100) + 1);
			number[1] = binary2digit((((min1 >> 14) / 10) % 10) + 1);
			number[2] = binary2digit(((min1 >> 14) % 10) + 1);
		}
		if (((min1 >> 10) & 0xf) < 1 || ((min1 >> 10) & 0xf) > 10)
			number[3] = '?';
		else
			number[3] = binary2digit((min1 >> 10) & 0xf);
		if ((min1 & 0x3ff) > 999)
			strcpy(number + 4, "???");
		else {
			number[4] = binary2digit(((min1 & 0x3ff) / 100) + 1);
			number[5] = binary2digit((((min1 & 0x3ff) / 10) % 10) + 1);
			number[6] = binary2digit(((min1 & 0x3ff) % 10) + 1);
		}
	} else {
		/* MIN1 */
		if ((min1 >> 20) > 9)
			number[0] = '?';
		else
			number[0] = '0' + (min1 >> 20);
		if (((min1 >> 10) & 0x3ff) > 999)
			strcpy(number +  1, "???");
		else {
			number[1] = binary2digit((((min1 >> 10) & 0x3ff) / 100) + 1);
			number[2] = binary2digit(((((min1 >> 10) & 0x3ff) / 10) % 10) + 1);
			number[3] = binary2digit((((min1 >> 10) & 0x3ff) % 10) + 1);
		}
		if ((min1 & 0x3ff) > 999)
			strcpy(number + 4, "???");
		else {
			number[4] = binary2digit(((min1 & 0x3ff) / 100) + 1);
			number[5] = binary2digit((((min1 & 0x3ff) / 10) % 10) + 1);
			number[6] = binary2digit(((min1 & 0x3ff) % 10) + 1);
		}
	}
	number[7] = '\0';

	return number;
}

const char *amps_min2number(uint32_t min1, uint16_t min2)
{
	static char number[11];

	sprintf(number, "%s%s", amps_min22number(min2), amps_min12number(min1));

	return number;
}

/* encode ESN */
void amps_encode_esn(uint32_t *esn, uint8_t mfr, uint32_t serial)
{
	*esn = (((uint32_t)mfr) << 24) | (serial & 0xffffff);
}

/* decode ESN */
void amps_decode_esn(uint32_t esn, uint8_t *mfr, uint32_t *serial)
{
	*mfr = esn >> 24;
	*serial = esn & 0xffffff;
}

const char *amps_scm(uint8_t scm)
{
	static char text[64];

	sprintf(text, "Class %d / %sontinuous / %d MHz", ((scm & 16) >> 2) + (scm & 3) + 1, (scm & 4) ? "Disc" : "C", (scm & 8) ? 25 : 20);

	return text;
}

const char *amps_mpci(uint8_t mpci)
{
	switch (mpci) {
	case 0:
		return "TIA/EIA-553 or IS-54A mobile station";
	case 1:
		return "TIA/EIA-627 dual-mode mobile station";
	case 2:
		return "reserved (see TIA/EIA IS-95)";
	case 3:
		return "EIATIA/EIA-136 dual-mode mobile station";
	default:
		return "MPCI INVALID, PLEASE FIX!";
	}

}

const char *amps_state_name(enum amps_state state)
{
	static char invalid[16];

	switch (state) {
	case STATE_NULL:
		return "(NULL)";
	case STATE_IDLE:
		return "IDLE";
	case STATE_BUSY:
		return "BUSY";
	}

	sprintf(invalid, "invalid(%d)", state);
	return invalid;
}

void amps_display_status(void)
{
	sender_t *sender;
	amps_t *amps;
	transaction_t *trans;

	display_status_start();
	for (sender = sender_head; sender; sender = sender->next) {
		amps = (amps_t *) sender;
		display_status_channel(amps->sender.kanal, chan_type_short_name(amps->chan_type), amps_state_name(amps->state));
		for (trans = amps->trans_list; trans; trans = trans->next)
			display_status_subscriber(amps_min2number(trans->min1, trans->min2), trans_short_state_name(trans->state));
	}
	display_status_end();
}

static void amps_new_state(amps_t *amps, enum amps_state new_state)
{
	if (amps->state == new_state)
		return;
	PDEBUG_CHAN(DAMPS, DEBUG_DEBUG, "State change: %s -> %s\n", amps_state_name(amps->state), amps_state_name(new_state));
	amps->state = new_state;
	amps_display_status();
}

static struct amps_channels {
	enum amps_chan_type chan_type;
	const char *short_name;
	const char *long_name;
} amps_channels[] = {
	{ CHAN_TYPE_CC,		"CC",	"control channel" },
	{ CHAN_TYPE_CC,		"PC",	"paging channel" },
	{ CHAN_TYPE_CC_PC,	"CC/PC","combined control & paging channel" },
	{ CHAN_TYPE_VC,		"VC",	"voice channel" },
	{ CHAN_TYPE_CC_PC_VC,	"CC/PC/VC","combined control & paging & voice channel" },
	{ 0, NULL, NULL }
};

void amps_channel_list(void)
{
	int i;

	printf("Type\t\tDescription\n");
	printf("------------------------------------------------------------------------\n");
	for (i = 0; amps_channels[i].long_name; i++)
		printf("%s%s\t%s\n", amps_channels[i].short_name, (strlen(amps_channels[i].short_name) >= 8) ? "" : "\t", amps_channels[i].long_name);
}

int amps_channel_by_short_name(const char *short_name)
{
	int i;

	for (i = 0; amps_channels[i].short_name; i++) {
		if (!strcasecmp(amps_channels[i].short_name, short_name)) {
			PDEBUG(DAMPS, DEBUG_INFO, "Selecting channel '%s' = %s\n", amps_channels[i].short_name, amps_channels[i].long_name);
			return amps_channels[i].chan_type;
		}
	}

	return -1;
}

const char *chan_type_short_name(enum amps_chan_type chan_type)
{
	int i;

	for (i = 0; amps_channels[i].short_name; i++) {
		if (amps_channels[i].chan_type == chan_type)
			return amps_channels[i].short_name;
	}

	return "invalid";
}

const char *chan_type_long_name(enum amps_chan_type chan_type)
{
	int i;

	for (i = 0; amps_channels[i].long_name; i++) {
		if (amps_channels[i].chan_type == chan_type)
			return amps_channels[i].long_name;
	}

	return "invalid";
}

static amps_t *search_channel(int channel)
{
	sender_t *sender;
	amps_t *amps;

	for (sender = sender_head; sender; sender = sender->next) {
		if (sender->kanal != channel)
			continue;
		amps = (amps_t *) sender;
		if (amps->state == STATE_IDLE)
			return amps;
	}

	return NULL;
}

static amps_t *search_free_vc(void)
{
	sender_t *sender;
	amps_t *amps, *cc_pc_vc = NULL;

	for (sender = sender_head; sender; sender = sender->next) {
		amps = (amps_t *) sender;
		if (amps->state != STATE_IDLE)
			continue;
		/* return first free voice channel */
		if (amps->chan_type == CHAN_TYPE_VC)
			return amps;
		/* remember combined voice/control/paging channel as second alternative */
		if (amps->chan_type == CHAN_TYPE_CC_PC_VC)
			cc_pc_vc = amps;
	}

	return cc_pc_vc;
}

static amps_t *search_pc(void)
{
	sender_t *sender;
	amps_t *amps;

	for (sender = sender_head; sender; sender = sender->next) {
		amps = (amps_t *) sender;
		if (amps->state != STATE_IDLE)
			continue;
		if (amps->chan_type == CHAN_TYPE_PC)
			return amps;
		if (amps->chan_type == CHAN_TYPE_CC_PC)
			return amps;
		if (amps->chan_type == CHAN_TYPE_CC_PC_VC)
			return amps;
	}

	return NULL;
}

/* Create transceiver instance and link to a list. */
int amps_create(int channel, enum amps_chan_type chan_type, const char *audiodev, int use_sdr, int samplerate, double rx_gain, int pre_emphasis, int de_emphasis, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, const char *read_tx_wave, amps_si *si, uint16_t sid, uint8_t sat, int polarity, int tolerant, int loopback)
{
	sender_t *sender;
	amps_t *amps;
	int rc;
	enum amps_chan_type ct;
	const char *band;

	/* check for channel number */
	if (amps_channel2freq(channel, 0) == 0) {
		PDEBUG(DAMPS, DEBUG_ERROR, "Channel number %d invalid.\n", channel);
		return -EINVAL;
	}

	/* no paging channel (without control channel) support */
	if (chan_type == CHAN_TYPE_PC) {
		PDEBUG(DAMPS, DEBUG_ERROR, "Dedicated paging channel currently not supported. Please select CC/PC or CC/PC/VC instead.\n");
		return -EINVAL;
	}

	/* check if there is only one paging channel */
	if (chan_type == CHAN_TYPE_PC || chan_type == CHAN_TYPE_CC_PC || chan_type == CHAN_TYPE_CC_PC_VC) {
		for (sender = sender_head; sender; sender = sender->next) {
			amps = (amps_t *)sender;
			if (amps->chan_type == CHAN_TYPE_PC || amps->chan_type == CHAN_TYPE_CC_PC || amps->chan_type == CHAN_TYPE_CC_PC_VC) {
				PDEBUG(DAMPS, DEBUG_ERROR, "Only one paging channel is currently supported. Please check your channel types.\n");
				return -EINVAL;
			}
		}
	}

	/* check if channel type matches channel number */
	ct = amps_channel2type(channel);
	if (ct == CHAN_TYPE_CC && chan_type != CHAN_TYPE_PC && chan_type != CHAN_TYPE_CC_PC && chan_type != CHAN_TYPE_CC_PC_VC) {
		PDEBUG(DAMPS, DEBUG_NOTICE, "Channel number %d belongs to a control channel, but your channel type '%s' requires to be on a voice channel number. Some phone may reject this, but all my phones don't.\n", channel, chan_type_long_name(chan_type));
	}
	if (ct == CHAN_TYPE_VC && chan_type != CHAN_TYPE_VC) {
		PDEBUG(DAMPS, DEBUG_ERROR, "Channel number %d belongs to a voice channel, but your channel type '%s' requires to be on a control channel number. Please use correct channel.\n", channel, chan_type_long_name(chan_type));
		return -EINVAL;
	}

	/* check if sid machtes channel band */
	band = amps_channel2band(channel);
	if (band[0] == 'A' && (sid & 1) == 0 && chan_type != CHAN_TYPE_VC) {
		PDEBUG(DAMPS, DEBUG_ERROR, "Channel number %d belongs to system A, but your %s %d is even and belongs to system B. Please give odd %s.\n", channel, (!tacs) ? "SID" : "AID", sid, (!tacs) ? "SID" : "AID");
		return -EINVAL;
	}
	if (band[0] == 'B' && (sid & 1) == 1 && chan_type != CHAN_TYPE_VC) {
		PDEBUG(DAMPS, DEBUG_ERROR, "Channel number %d belongs to system B, but your %s %d is odd and belongs to system A. Please give even %s.\n", channel, (!tacs) ? "SID" : "AID", sid, (!tacs) ? "SID" : "AID");
		return -EINVAL;
	}

	/* check if we use combined voice channel hack */
	if (chan_type == CHAN_TYPE_CC_PC_VC) {
		PDEBUG(DAMPS, DEBUG_NOTICE, "You selected '%s'. This is a hack, but the only way to use control channel and voice channel on one transceiver. Some phones may reject this, but all my phones don't.\n", chan_type_long_name(chan_type));
	}

	/* check if we selected a voice channel that i outside 20 MHz band */
	if (chan_type == CHAN_TYPE_VC && channel > 666) {
		PDEBUG(DAMPS, DEBUG_NOTICE, "You selected '%s' on channel #%d. Older phones do not support channels above #666.\n", chan_type_long_name(chan_type), channel);
	}

	amps = calloc(1, sizeof(amps_t));
	if (!amps) {
		PDEBUG(DAMPS, DEBUG_ERROR, "No memory!\n");
		return -ENOMEM;
	}

	PDEBUG(DAMPS, DEBUG_DEBUG, "Creating 'AMPS' instance for channel = %d of band %s (sample rate %d).\n", channel, band, samplerate);

	/* init general part of transceiver */
	rc = sender_create(&amps->sender, channel, amps_channel2freq(channel, 0), amps_channel2freq(channel, 1), audiodev, use_sdr, samplerate, rx_gain, 0, 0, write_rx_wave, write_tx_wave, read_rx_wave, read_tx_wave, loopback, PAGING_SIGNAL_NONE);
	if (rc < 0) {
		PDEBUG(DAMPS, DEBUG_ERROR, "Failed to init transceiver process!\n");
		goto error;
	}

	/* init audio processing */
	rc = dsp_init_sender(amps, tolerant);
	if (rc < 0) {
		PDEBUG(DAMPS, DEBUG_ERROR, "Failed to init audio processing!\n");
		goto error;
	}

	if (polarity < 0)
		amps->flip_polarity = 1;

	amps->chan_type = chan_type;
	memcpy(&amps->si, si, sizeof(amps->si));
	amps->sat = sat;

	amps->pre_emphasis = pre_emphasis;
	amps->de_emphasis = de_emphasis;
	rc = init_emphasis(&amps->estate, samplerate, CUT_OFF_EMPHASIS_DEFAULT);
	if (rc < 0)
		goto error;

	/* go into idle state */
	amps_go_idle(amps);

#ifdef DEBUG_VC
	uint32_t min1;
	uint16_t min2;
	amps_number2min("1234567890", &min1, &min2);
	transaction_t __attribute__((__unused__)) *trans = create_transaction(amps, TRANS_CALL_MO_ASSIGN, min1, min2, 0, 0, 0, amps->sender.kanal);
//	amps_new_state(amps, STATE_BUSY);
#endif

	PDEBUG(DAMPS, DEBUG_NOTICE, "Created channel #%d (System %s) of type '%s' = %s\n", channel, band, chan_type_short_name(chan_type), chan_type_long_name(chan_type));

	return 0;

error:
	amps_destroy(&amps->sender);

	return rc;
}

/* Destroy transceiver instance and unlink from list. */
void amps_destroy(sender_t *sender)
{
	amps_t *amps = (amps_t *) sender;
	transaction_t *trans;

	PDEBUG(DAMPS, DEBUG_DEBUG, "Destroying 'AMPS' instance for channel = %d.\n", sender->kanal);

	while ((trans = amps->trans_list)) {
		const char *number = amps_min2number(trans->min1, trans->min2);
		PDEBUG(DAMPS, DEBUG_NOTICE, "Removing pending transaction for subscriber '%s'\n", number);
		destroy_transaction(trans);
	}

	dsp_cleanup_sender(amps);
	sender_destroy(&amps->sender);
	free(amps);
}

/* Abort connection towards mobile station by sending FOCC/FVC pattern. */
void amps_go_idle(amps_t *amps)
{
	int frame_length;

	if (amps->state == STATE_IDLE)
		return;

	if (amps->trans_list) {
		PDEBUG(DAMPS, DEBUG_ERROR, "Releasing but still having transaction, please fix!\n");
		if (amps->trans_list->callref)
			call_up_release(amps->trans_list->callref, CAUSE_NORMAL);
		destroy_transaction(amps->trans_list);
	}

	amps_new_state(amps, STATE_IDLE);

	if (amps->chan_type != CHAN_TYPE_VC) {
		PDEBUG_CHAN(DAMPS, DEBUG_INFO, "Entering IDLE state, sending Overhead/Filler frames on %s.\n", chan_type_long_name(amps->chan_type));
		if (amps->sender.loopback)
			frame_length = 441; /* bits after sync (FOCC) */
		else
			frame_length = 247; /* bits after sync (RECC) */
		amps_set_dsp_mode(amps, DSP_MODE_FRAME_RX_FRAME_TX, frame_length);
	} else {
		PDEBUG_CHAN(DAMPS, DEBUG_INFO, "Entering IDLE state, sending test tone on %s.\n", chan_type_long_name(amps->chan_type));
		amps_set_dsp_mode(amps, DSP_MODE_OFF, 0);
	}
}

/* Abort connection towards mobile station by sending FOCC/FVC pattern. */
static void amps_release(transaction_t *trans, uint8_t cause)
{
	amps_t *amps = trans->amps;

	timer_stop(&trans->timer);
	timer_start(&trans->timer, RELEASE_TIMER);
	trans_new_state(trans, TRANS_CALL_RELEASE);
	trans->chan = 0;
	trans->msg_type = 0;
	trans->ordq = 0;
	trans->order = 3;
	/* release towards call control */
	if (trans->callref) {
		call_up_release(trans->callref, cause);
		trans->callref = 0;
	}
	/* change DSP mode to transmit release */
	if (amps->dsp_mode == DSP_MODE_AUDIO_RX_AUDIO_TX || amps->dsp_mode == DSP_MODE_OFF)
		amps_set_dsp_mode(amps, DSP_MODE_AUDIO_RX_FRAME_TX, 0);
}

/*
 * receive signaling
 */

void amps_rx_signaling_tone(amps_t *amps, int tone, double quality)
{
	transaction_t *trans = amps->trans_list;
	if (trans == NULL) {
		PDEBUG_CHAN(DAMPS, DEBUG_ERROR, "Signaling Tone without transaction, please fix!\n");
		return;
	}
	
	if (tone)
		PDEBUG_CHAN(DAMPS, DEBUG_INFO, "Detected Signaling Tone with quality=%.0f.\n", quality * 100.0);
	else
		PDEBUG_CHAN(DAMPS, DEBUG_INFO, "Lost Signaling Tone signal\n");

	switch (trans->state) {
	case TRANS_CALL:
		if (!tone)
			break;
		/* fall through */
	case TRANS_CALL_RELEASE:
	case TRANS_CALL_RELEASE_SEND:
		/* also loosing singaling tone indicates release confirm (after alerting) */
		timer_stop(&trans->timer);
		if (trans->callref)
			call_up_release(trans->callref, CAUSE_NORMAL);
		destroy_transaction(trans);
		amps_go_idle(amps);
		break;
	case TRANS_CALL_MT_ALERT:
		if (tone) {
			timer_stop(&trans->timer);
			call_up_alerting(trans->callref);
			amps_set_dsp_mode(amps, DSP_MODE_AUDIO_RX_AUDIO_TX, 0);
			trans_new_state(trans, TRANS_CALL_MT_ALERT_SEND);
			timer_start(&trans->timer, ALERT_TO);
		}
		break;
	case TRANS_CALL_MT_ALERT_SEND:
		if (!tone) {
			timer_stop(&trans->timer);
			if (!trans->sat_detected)
				timer_start(&trans->timer, SAT_TO1);
			call_up_answer(trans->callref, amps_min2number(trans->min1, trans->min2));
			trans_new_state(trans, TRANS_CALL);
		}
		break;
	default:
		PDEBUG_CHAN(DAMPS, DEBUG_ERROR, "Signaling Tone without active call, please fix!\n");
	}
}

void amps_rx_sat(amps_t *amps, int tone, double quality)
{
	transaction_t *trans = amps->trans_list;
	if (trans == NULL) {
		PDEBUG_CHAN(DAMPS, DEBUG_ERROR, "SAT signal without transaction, please fix!\n");
		return;
	}
	/* irgnoring SAT loss on release */
	if (trans->state == TRANS_CALL_RELEASE
	 || trans->state == TRANS_CALL_RELEASE_SEND)
		return;
	if (trans->state != TRANS_CALL
	 && trans->state != TRANS_CALL_MT_ALERT
	 && trans->state != TRANS_CALL_MT_ALERT_SEND) {
		PDEBUG_CHAN(DAMPS, DEBUG_ERROR, "SAT signal without active call, please fix!\n");
		return;
	}

	if (tone) {
		PDEBUG(DAMPS, DEBUG_INFO, "Detected SAT signal with quality=%.0f.\n", quality * 100.0);
		trans->sat_detected = 1;
	} else {
		PDEBUG(DAMPS, DEBUG_INFO, "Lost SAT signal\n");
		trans->sat_detected = 0;
	}

	/* no SAT during alerting */
	if (trans->state == TRANS_CALL_MT_ALERT
	 || trans->state == TRANS_CALL_MT_ALERT_SEND)
		return;

	if (tone) {
		timer_stop(&trans->timer);
	} else {
		if (!trans->dtx)
			timer_start(&trans->timer, SAT_TO2);
		else
			timer_stop(&trans->timer);
	}

	if (amps->sender.loopback)
		return;
}

static void timeout_sat(amps_t *amps, double duration)
{
	if (!amps->trans_list) {
		PDEBUG_CHAN(DAMPS, DEBUG_ERROR, "SAT timeout, but no transaction, please fix!\n");
		return;
	}
	if (duration == SAT_TO1)
		PDEBUG_CHAN(DAMPS, DEBUG_NOTICE, "Timeout after %.0f seconds not receiving SAT signal.\n", duration);
	else
		PDEBUG_CHAN(DAMPS, DEBUG_NOTICE, "Timeout after %.0f seconds loosing SAT signal.\n", duration);
	PDEBUG_CHAN(DAMPS, DEBUG_INFO, "Release call towards network.\n");
	amps_release(amps->trans_list, CAUSE_TEMPFAIL);
}

/* receive message from phone on RECC */
void amps_rx_recc(amps_t *amps, uint8_t scm, uint8_t mpci, uint32_t esn, uint32_t min1, uint16_t min2, uint8_t msg_type, uint8_t ordq, uint8_t order, const char *dialing)
{
	amps_t *vc;
	transaction_t *trans;
	const char *callerid = amps_min2number(min1, min2);
	const char *carrier = NULL, *country = NULL, *national_number = NULL;

	/* check if we are busy, so we ignore all signaling */
	if (amps->state == STATE_BUSY) {
		PDEBUG_CHAN(DAMPS, DEBUG_NOTICE, "Ignoring RECC messages from phone while using this channel for voice.\n");
		return;
	}

	if (order == 13 && (ordq == 0 || ordq == 1 || ordq == 2 || ordq == 3) && msg_type == 0) {
		PDEBUG_CHAN(DAMPS, DEBUG_INFO, "Registration %s (ESN = %08x, %s, %s)\n", callerid, esn, amps_scm(scm), amps_mpci(mpci));
_register:
		numbering(callerid, &carrier, &country, &national_number);
		if (carrier)
			PDEBUG_CHAN(DAMPS, DEBUG_INFO, " -> Home carrier: %s\n", carrier);
		if (country)
			PDEBUG_CHAN(DAMPS, DEBUG_INFO, " -> Home country: %s\n", country);
		if (national_number)
			PDEBUG_CHAN(DAMPS, DEBUG_INFO, " -> Home number: %s\n", national_number);
		trans = create_transaction(amps, TRANS_REGISTER_ACK, min1, min2, msg_type, ordq, order, 0);
		if (!trans) {
			PDEBUG(DAMPS, DEBUG_ERROR, "Failed to create transaction\n");
			return;
		}
	} else
	if (order == 13 && ordq == 3 && msg_type == 1) {
		PDEBUG_CHAN(DAMPS, DEBUG_INFO, "Registration - Power Down %s (ESN = %08x, %s, %s)\n", callerid, esn, amps_scm(scm), amps_mpci(mpci));
		goto _register;
	} else
	if (order == 0 && ordq == 0 && msg_type == 0) {
		if (!dialing)
			PDEBUG_CHAN(DAMPS, DEBUG_INFO, "Paging reply %s (ESN = %08x, %s, %s)\n", callerid, esn, amps_scm(scm), amps_mpci(mpci));
		else
			PDEBUG_CHAN(DAMPS, DEBUG_INFO, "Call %s -> %s (ESN = %08x, %s, %s)\n", callerid, dialing, esn, amps_scm(scm), amps_mpci(mpci));
		trans = search_transaction_number(amps, min1, min2);
		if (!trans && !dialing) {
			PDEBUG(DAMPS, DEBUG_NOTICE, "Paging reply, but call is already gone, rejecting call\n");
			goto reject;
		}
		if (trans && dialing)
			PDEBUG(DAMPS, DEBUG_NOTICE, "There is already a transaction for this phone. Cloning?\n");
		vc = search_free_vc();
		if (!vc) {
			PDEBUG(DAMPS, DEBUG_NOTICE, "No free channel, rejecting call\n");
reject:
			if (!trans) {
				trans = create_transaction(amps, TRANS_CALL_REJECT, min1, min2, 0, 0, 3, 0);
				if (!trans) {
					PDEBUG(DAMPS, DEBUG_ERROR, "Failed to create transaction\n");
					return;
				}
			} else {
				trans_new_state(trans, TRANS_CALL_REJECT);
				trans->chan = 0;
				trans->msg_type = 0;
				trans->ordq = 0;
				trans->order = 3;
			}
			return;
		}
		if (!trans) {
			trans = create_transaction(amps, TRANS_CALL_MO_ASSIGN, min1, min2, 0, 0, 0, vc->sender.kanal);
			strncpy(trans->dialing, dialing, sizeof(trans->dialing) - 1);
			if (!trans) {
				PDEBUG(DAMPS, DEBUG_ERROR, "Failed to create transaction\n");
				return;
			}
		} else {
			trans_new_state(trans, TRANS_CALL_MT_ASSIGN);
			trans->chan = vc->sender.kanal;
		}
		/* if we support DTX and also the phone does, we set DTX state of transaction */
		if (amps->si.word2.dtx) {
			if ((scm & 4)) {
				PDEBUG(DAMPS, DEBUG_INFO, " -> Use DTX for this call\n");
				trans->dtx = 1;
			} else
				PDEBUG(DAMPS, DEBUG_INFO, " -> Requested DTX, but not supported by phone\n");
		}
	} else
		PDEBUG_CHAN(DAMPS, DEBUG_NOTICE, "Unsupported RECC messages: ORDER: %d ORDQ: %d MSG TYPE: %d (See Table 4 of specs.)\n", order, ordq, msg_type);
}

/*
 * call states received from call control
 */

/* Call control starts call towards mobile station. */
int call_down_setup(int callref, const char __attribute__((unused)) *caller_id, enum number_type __attribute__((unused)) caller_type, const char *dialing)
{
	sender_t *sender;
	amps_t *amps;
	transaction_t *trans;
	uint32_t min1;
	uint16_t min2;
	int i;

	/* 1. check if number is invalid, return INVALNUMBER */
	if (strlen(dialing) == 11 && !strncmp(dialing, "+", 1))
		dialing += 1;
	if (strlen(dialing) == 11 && !strncmp(dialing, "1", 1))
		dialing += 1;
	if (strlen(dialing) != 10) {
inval:
		PDEBUG(DAMPS, DEBUG_NOTICE, "Outgoing call to invalid number '%s', rejecting!\n", dialing);
		return -CAUSE_INVALNUMBER;
	}
	for (i = 0; i < 10; i++) {
		if (dialing[i] < '0' || dialing[i] > '9')
			goto inval;
	}

	amps_number2min(dialing, &min1, &min2);

	/* 2. check if the subscriber is attached */
//	if (!find_db(min1, min2)) {
//		PDEBUG(DAMPS, DEBUG_NOTICE, "Outgoing call to not attached subscriber, rejecting!\n");
//		return -CAUSE_OUTOFORDER;
//	}

	/* 3. check if given number is already in a call, return BUSY */
	for (sender = sender_head; sender; sender = sender->next) {
		amps = (amps_t *) sender;
		/* search transaction for this number */
		trans = search_transaction_number(amps, min1, min2);
		if (trans)
			break;
	}
	if (sender) {
		PDEBUG(DAMPS, DEBUG_NOTICE, "Outgoing call to busy number, rejecting!\n");
		return -CAUSE_BUSY;
	}

	/* 4. check if all senders are busy, return NOCHANNEL */
	if (!search_free_vc()) {
		PDEBUG(DAMPS, DEBUG_NOTICE, "Outgoing call, but no free channel, rejecting!\n");
		return -CAUSE_NOCHANNEL;
	}

	/* 5. check if we have (currently) no paging channel, return NOCHANNEL */
	amps = search_pc();
	if (!amps) {
		PDEBUG(DAMPS, DEBUG_NOTICE, "Outgoing call, but paging channel (control channel) is currently busy, rejecting!\n");
		return -CAUSE_NOCHANNEL;
	}

	PDEBUG_CHAN(DAMPS, DEBUG_INFO, "Call to mobile station, paging station id '%s'\n", dialing);

	/* 6. trying to page mobile station */
	trans = create_transaction(amps, TRANS_PAGE, min1, min2, 0, 0, 0, 0);
	if (!trans) {
		PDEBUG(DAMPS, DEBUG_ERROR, "Failed to create transaction\n");
		return -CAUSE_TEMPFAIL;
	}
	trans->callref = callref;
	trans->page_retry = 1;

	return 0;
}

void call_down_answer(int __attribute__((unused)) callref)
{
}

/* Call control sends disconnect (with tones).
 * An active call stays active, so tones and annoucements can be received
 * by mobile station.
 */
void call_down_disconnect(int callref, int cause)
{
	sender_t *sender;
	amps_t *amps;
	transaction_t *trans;

	PDEBUG(DAMPS, DEBUG_INFO, "Call has been disconnected by network.\n");

	for (sender = sender_head; sender; sender = sender->next) {
		amps = (amps_t *) sender;
		/* search transaction for this callref */
		trans = search_transaction_callref(amps, callref);
		if (trans)
			break;
	}
	if (!sender) {
		PDEBUG(DAMPS, DEBUG_NOTICE, "Outgoing disconnect, but no callref!\n");
		call_up_release(callref, CAUSE_INVALCALLREF);
		return;
	}

	/* Release when not active */

	switch (amps->dsp_mode) {
	case DSP_MODE_AUDIO_RX_AUDIO_TX:
	case DSP_MODE_AUDIO_RX_FRAME_TX:
		if (trans->state == TRANS_CALL_MT_ALERT
		 || trans->state == TRANS_CALL_MT_ALERT_SEND) {
			PDEBUG_CHAN(DAMPS, DEBUG_INFO, "Call control disconnect on voice channel while alerting, releasing towards mobile station.\n");
			amps_release(trans, cause);
		}
		return;
	default:
		PDEBUG_CHAN(DAMPS, DEBUG_INFO, "Call control disconnects on control channel, removing transaction.\n");
		call_up_release(callref, cause);
		trans->callref = 0;
		destroy_transaction(trans);
		amps_go_idle(amps);
	}
}

/* Call control releases call toward mobile station. */
void call_down_release(int callref, int cause)
{
	sender_t *sender;
	amps_t *amps;
	transaction_t *trans;

	PDEBUG(DAMPS, DEBUG_INFO, "Call has been released by network, releasing call.\n");

	for (sender = sender_head; sender; sender = sender->next) {
		amps = (amps_t *) sender;
		/* search transaction for this callref */
		trans = search_transaction_callref(amps, callref);
		if (trans)
			break;
	}
	if (!sender) {
		PDEBUG(DAMPS, DEBUG_NOTICE, "Outgoing release, but no callref!\n");
		/* don't send release, because caller already released */
		return;
	}

	trans->callref = 0;

	switch (amps->dsp_mode) {
	case DSP_MODE_AUDIO_RX_AUDIO_TX:
	case DSP_MODE_AUDIO_RX_FRAME_TX:
		PDEBUG_CHAN(DAMPS, DEBUG_INFO, "Call control releases on voice channel, releasing towards mobile station.\n");
		amps_release(trans, cause);
		break;
	default:
		PDEBUG_CHAN(DAMPS, DEBUG_INFO, "Call control releases on control channel, removing transaction.\n");
		destroy_transaction(trans);
		amps_go_idle(amps);
	}
}

/* Receive audio from call instance. */
void call_down_audio(int callref, sample_t *samples, int count)
{
	sender_t *sender;
	amps_t *amps;

	for (sender = sender_head; sender; sender = sender->next) {
		amps = (amps_t *) sender;
		if (amps->trans_list && amps->trans_list->callref == callref)
			break;
	}
	if (!sender)
		return;

	if (amps->dsp_mode == DSP_MODE_AUDIO_RX_AUDIO_TX) {
		sample_t up[(int)((double)count * amps->sender.srstate.factor + 0.5) + 10];
		compress_audio(&amps->cstate, samples, count);
		count = samplerate_upsample(&amps->sender.srstate, samples, count, up);
		jitter_save(&amps->sender.dejitter, up, count);
	}
}

/* Timeout handling */
void transaction_timeout(struct timer *timer)
{
	transaction_t *trans = (transaction_t *)timer->priv;
	amps_t *amps = trans->amps;

	switch (trans->state) {
	case TRANS_CALL:
		timeout_sat(amps, timer->duration);
		break;
	case TRANS_CALL_RELEASE:
	case TRANS_CALL_RELEASE_SEND:
		PDEBUG_CHAN(DAMPS, DEBUG_NOTICE, "Release timeout, destroying transaction\n");
		destroy_transaction(trans);
		amps_go_idle(amps);
		break;
	case TRANS_CALL_MT_ALERT:
		amps_release(trans, CAUSE_TEMPFAIL);
		break;
	case TRANS_CALL_MT_ALERT_SEND:
		PDEBUG_CHAN(DAMPS, DEBUG_NOTICE, "Alerting timeout, destroying transaction\n");
		amps_release(trans, CAUSE_NOANSWER);
		break;
	case TRANS_PAGE_REPLY:
		if (trans->page_retry++ == PAGE_TRIES) {
			PDEBUG_CHAN(DAMPS, DEBUG_NOTICE, "Paging timeout, destroying transaction\n");
			amps_release(trans, CAUSE_OUTOFORDER);
		} else {
			PDEBUG_CHAN(DAMPS, DEBUG_NOTICE, "Paging timeout, retrying\n");
			trans_new_state(trans, TRANS_PAGE);
		}
		break;
	default:
		PDEBUG_CHAN(DAMPS, DEBUG_ERROR, "Timeout unhandled in state %d\n", trans->state);
	}
}

/* assigning voice channel and moving transaction+callref to that channel */
static amps_t *assign_voice_channel(transaction_t *trans)
{
	amps_t *amps = trans->amps, *vc;
	const char *callerid = amps_min2number(trans->min1, trans->min2);
	int callref = ++new_callref;
	int rc;

	vc = search_channel(trans->chan);
	if (!vc) {
		PDEBUG(DAMPS, DEBUG_NOTICE, "Channel %d is not free anymore, rejecting call\n", trans->chan);
		amps_release(trans, CAUSE_NOCHANNEL);
		return NULL;
	}

	if (vc == amps)
		PDEBUG(DAMPS, DEBUG_INFO, "Staying on combined control + voice channel %d\n", vc->sender.kanal);
	else
		PDEBUG(DAMPS, DEBUG_INFO, "Moving to voice channel %d\n", vc->sender.kanal);

	/* switch channel... */
	timer_start(&trans->timer, SAT_TO1);
	/* make channel busy */
	amps_new_state(vc, STATE_BUSY);
	/* relink */
	unlink_transaction(trans);
	link_transaction(trans, vc);
	/* flush all other transactions, if any (in case of combined VC + CC) */
	amps_flush_other_transactions(vc, trans);

	if (!trans->callref) {
		/* setup call */
		PDEBUG(DAMPS, DEBUG_INFO, "Setup call to network.\n");
		rc = call_up_setup(callref, callerid, trans->dialing);
		if (rc < 0) {
			PDEBUG(DAMPS, DEBUG_NOTICE, "Call rejected (cause %d), releasing.\n", rc);
			amps_release(trans, 0);
			return NULL;
		}
		trans->callref = callref;
	}

	return vc;
}

transaction_t *amps_tx_frame_focc(amps_t *amps)
{
	transaction_t *trans;
	amps_t *vc;
	
again:
	trans = amps->trans_list;
	if (!trans)
		return NULL;

	switch (trans->state) {
	case TRANS_REGISTER_ACK:
		PDEBUG_CHAN(DAMPS, DEBUG_INFO, "Sending Register acknowledge\n");
		trans_new_state(trans, TRANS_REGISTER_ACK_SEND);
		return trans;
	case TRANS_REGISTER_ACK_SEND:
		destroy_transaction(trans);
		goto again;
	case TRANS_CALL_REJECT:
		PDEBUG_CHAN(DAMPS, DEBUG_INFO, "Rejecting call from mobile station\n");
		trans_new_state(trans, TRANS_CALL_REJECT_SEND);
		return trans;
	case TRANS_CALL_REJECT_SEND:
		destroy_transaction(trans);
		goto again;
	case TRANS_CALL_MO_ASSIGN:
		PDEBUG_CHAN(DAMPS, DEBUG_INFO, "Assigning channel to call from mobile station\n");
		trans_new_state(trans, TRANS_CALL_MO_ASSIGN_SEND);
		return trans;
	case TRANS_CALL_MO_ASSIGN_SEND:
		vc = assign_voice_channel(trans);
		if (vc) {
			PDEBUG_CHAN(DAMPS, DEBUG_INFO, "Assignment complete, voice connected\n");
			trans_new_state(trans, TRANS_CALL);
			amps_set_dsp_mode(vc, DSP_MODE_AUDIO_RX_AUDIO_TX, 0);
		}
		return NULL;
	case TRANS_CALL_MT_ASSIGN:
		PDEBUG_CHAN(DAMPS, DEBUG_INFO, "Assigning channel to call to mobile station\n");
		trans_new_state(trans, TRANS_CALL_MT_ASSIGN_SEND);
		return trans;
	case TRANS_CALL_MT_ASSIGN_SEND:
		vc = assign_voice_channel(trans);
		if (vc) {
			PDEBUG_CHAN(DAMPS, DEBUG_INFO, "Assignment complete, next: sending alerting on VC\n");
			trans->chan = 0;
			trans->msg_type = 0;
			trans->ordq = 0;
			trans->order = 1;
			trans_new_state(trans, TRANS_CALL_MT_ALERT);
			amps_set_dsp_mode(vc, DSP_MODE_AUDIO_RX_FRAME_TX, 0);
		}
		return NULL;
	case TRANS_PAGE:
		PDEBUG_CHAN(DAMPS, DEBUG_INFO, "Paging the phone\n");
		trans_new_state(trans, TRANS_PAGE_SEND);
		return trans;
	case TRANS_PAGE_SEND:
		trans_new_state(trans, TRANS_PAGE_REPLY);
		timer_start(&trans->timer, (trans->page_retry == PAGE_TRIES) ? PAGE_TO2 : PAGE_TO1);
		return NULL;
	default:
		return NULL;
	}
}

transaction_t *amps_tx_frame_fvc(amps_t *amps)
{
	transaction_t *trans = amps->trans_list;

	trans = amps->trans_list;
	if (!trans)
		return NULL;

	switch (trans->state) {
	case TRANS_CALL_RELEASE:
		PDEBUG_CHAN(DAMPS, DEBUG_INFO, "Releasing call towards mobile station\n");
		trans_new_state(trans, TRANS_CALL_RELEASE_SEND);
		return trans;
	case TRANS_CALL_RELEASE_SEND:
		PDEBUG_CHAN(DAMPS, DEBUG_INFO, "Release call was sent, continue sending release\n");
		return trans;
	case TRANS_CALL_MT_ALERT:
		PDEBUG_CHAN(DAMPS, DEBUG_INFO, "Sending alerting\n");
		return trans;
	default:
		return NULL;
	}
}

void dump_info(void) {}

