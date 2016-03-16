/* NMT protocol handling
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../common/debug.h"
#include "../common/timer.h"
#include "../common/call.h"
#include "../common/cause.h"
#include "nmt.h"
#include "dsp.h"
#include "frame.h"

/* Call reference for calls from mobile station to network
   This offset of 0x400000000 is required for MNCC interface. */
static int new_callref = 0x40000000;

/* Timers */
#define RELEASE_TO	2.0	/* how long do we wait for release guard of the phone */
#define DIALING_TO	1.0	/* if we have a pause during dialing, we abort the call */
#define CHANNEL_TO	2.0	/* how long do we wait for phone to appear on assigned channel */
#define RINGING_TO	60.0	/* how long may the phone ring */
#define SUPERVISORY_TO1	3.0	/* 3 sec to detect after setup */
#define SUPERVISORY_TO2	20.0	/* 20 sec lost until abort */

/* Counters */
#define PAGE_TRIES	3	/* How many time do we try to page the phone */

const char *nmt_state_name(enum nmt_state state)
{
	static char invalid[16];

	switch (state) {
	case STATE_NULL:
		return "(NULL)";
	case STATE_IDLE:
		return "IDLE";
	case STATE_ROAMING_IDENT:
		return "ROAMING IDENT";
	case STATE_ROAMING_CONFIRM:
		return "ROAMING CONFIRM";
	case STATE_MO_IDENT:
		return "MO CALL IDENT";
	case STATE_MO_CONFIRM:
		return "MO CALL CONFIRM";
	case STATE_MO_DIALING:
		return "MO CALL DIALING";
	case STATE_MO_COMPLETE:
		return "MO CALL COMPLETE";
	case STATE_MT_PAGING:
		return "MT CALL PAGING";
	case STATE_MT_CHANNEL:
		return "MT CALL CHANNEL";
	case STATE_MT_IDENT:
		return "MT CALL IDENT";
	case STATE_MT_RINGING:
		return "MT CALL RINGING";
	case STATE_MT_COMPLETE:
		return "MT CALL COMPLETE";
	case STATE_ACTIVE:
		return "ACTIVE";
	case STATE_MO_RELEASE:
		return "RELEASE MTX->MS";
	case STATE_MT_RELEASE:
		return "RELEASE MTX->MS";
	}

	sprintf(invalid, "invalid(%d)", state);
	return invalid;
}

static void nmt_new_state(nmt_t *nmt, enum nmt_state new_state)
{
	if (nmt->state == new_state)
		return;
	PDEBUG(DNMT, DEBUG_DEBUG, "State change: %s -> %s\n", nmt_state_name(nmt->state), nmt_state_name(new_state));
	nmt->state = new_state;
}

static struct nmt_channels {
	enum nmt_chan_type chan_type;
	const char *short_name;
	const char *long_name;
} nmt_channels[] = {
	{ CHAN_TYPE_CC,		"CC",	"calling channel" },
	{ CHAN_TYPE_TC,		"TC",	"traffic channel" },
	{ CHAN_TYPE_CC_TC,	"CC/TC","combined calling & traffic channel" },
	{ CHAN_TYPE_TEST,	"TEST",	"calling channel" },
	{ 0, NULL, NULL }
};

void nmt_channel_list(void)
{
	int i;

	printf("Type\tDescription\n");
	printf("------------------------------------------------------------------------\n");
	for (i = 0; nmt_channels[i].long_name; i++)
		printf("%s\t%s\n", nmt_channels[i].short_name, nmt_channels[i].long_name);
}

int nmt_channel_by_short_name(const char *short_name)
{
	int i;

	for (i = 0; nmt_channels[i].short_name; i++) {
		if (!strcasecmp(nmt_channels[i].short_name, short_name)) {
			PDEBUG(DNMT, DEBUG_INFO, "Selecting channel '%s' = %s\n", nmt_channels[i].short_name, nmt_channels[i].long_name);
			return nmt_channels[i].chan_type;
		}
	}

	return -1;
}

const char *chan_type_short_name(enum nmt_chan_type chan_type)
{
	int i;

	for (i = 0; nmt_channels[i].short_name; i++) {
		if (nmt_channels[i].chan_type == chan_type)
			return nmt_channels[i].short_name;
	}

	return "invalid";
}

const char *chan_type_long_name(enum nmt_chan_type chan_type)
{
	int i;

	for (i = 0; nmt_channels[i].long_name; i++) {
		if (nmt_channels[i].chan_type == chan_type)
			return nmt_channels[i].long_name;
	}

	return "invalid";
}

const char *nmt_dir_name(enum nmt_direction dir)
{
	switch (dir) {
	case MTX_TO_MS:
		return "MTX->MS";
	case MTX_TO_BS:
		return "MTX->BS";
	case MTX_TO_XX:
		return "MTX->XX";
	case BS_TO_MTX:
		return "BS->MTX";
	case MS_TO_MTX:
		return "MS->MTX";
	case XX_TO_MTX:
		return "XX->MTX";
	}
	return "invalid";
}

/* Convert channel number to frequency number of base station.
   Set 'uplink' to 1 to get frequency of mobile station. */
double nmt_channel2freq(int channel, int uplink)
{
	double freq;

	if (channel < 1)
		return 0;
	else if (channel <= 180)
		freq = 463.000 + (channel - 1) * 0.025;
	else if (channel <= 200)
		freq = 462.500 + (channel - 181) * 0.025;
	else if (channel <= 380)
		freq = 463.000 + (channel - 201) * 0.025 + 0.0125;
	else if (channel <= 399) /* no channel 400, caused by interleaving and coding */
		freq = 462.500 + (channel - 381) * 0.025 + 0.0125;
	else
		return 0;

	if (uplink)
		freq -= 10.000;

	return freq;
}

/* convert 7-digits dial string to NMT number */
static int dialstring2number(const char *dialstring, char *ms_country, char *ms_number)
{
	if (strlen(dialstring) != 7) {
		PDEBUG(DNMT, DEBUG_NOTICE, "Wrong number of digits, use 7 digits: ZXXXXXX (Z=country, X=mobile number)\n");
		return -1;
	}
	if (dialstring[0] < '0' && dialstring[0] > '9') {
		PDEBUG(DNMT, DEBUG_NOTICE, "Invalid country digit (first digit) of dial string\n");
		return -1;
	}
	*ms_country = dialstring[0];
	memcpy(ms_number, dialstring + 1, 6);
	return 0;
}

/* country selector */
static struct nmt_country {
	int y;
	const char *short_name;
	const char *long_name;
	const char *provider_name;
} nmt_country[] = {
	{ 5, "DK",	"Denmark",		"Tele Danmark Mobile" },
	{ 6, "SE",	"Sweden",		"Telia Mobitel" },
	{ 7, "NO",	"Norway",		"Telenor Mobil" },
	{ 8, "FI",	"Finland",		"Telecom Finland" },
	{ 8, "IS",	"Iceland",		"Post & Telecom" },
	{ 5, "FO",	"Faroe Island",		"Faroese Telecom" },
	{ 7, "EE",	"Estonia",		"Eesti Mobiiltelefon" },
	{ 5, "LV",	"Latvia",		"Latvian Mobile Telephone" },
	{ 8, "LT",	"Lithuania",		"COMLIET" },
	{ 6, "BY",	"Belarus",		"Belcel" },
	{ 5, "MO",	"OSS/Moscow",		"Moscow Cellular Comm." },
	{ 6, "STP",	"OSS/St Petersburg",	"Delta Telecom" },
	{ 6, "STP",	"OSS/Leningrads Dist.",	"Delta Telecom" },
	{ 7, "CAR",	"OSS/Carelian Rep.",	"Telecom Finland" },
	{ 5, "MUR",	"OSS/Murmansk",		"Telecom Finland" },
	{ 5, "LED",	"OSS/Leningrads Dist.",	"Telecom Finland" },
	{ 5, "KAL",	"Kaliningrad",		"Telecom Finland" },
	{ 7, "PL",	"Poland",		"CENTERTEL" },
	{ 6, "BG",	"Bulgaria",		"MOBIFON" },
	{ 5, "RO",	"Romania",		"Telefonica Romania" },
	{ 6, "UA",	"Ukraine",		"Ukraine Mobile Comm." },
	{ 1, "RU1",	"",			"" },
	{ 2, "RU2",	"",			"" },
	{ 3, "RU3",	"",			"" },
	{ 4, "RU4",	"",			"" },
	{ 0, NULL, NULL, NULL }
};

void nmt_country_list(void)
{
	int i;

	printf("TA\tShort\tCountry (Provider)\n");
	printf("------------------------------------------------------------------------\n");
	for (i = 0; nmt_country[i].short_name; i++) {
		if (nmt_country[i].short_name[0])
			printf("%d\t%s\t%s (%s)\n", nmt_country[i].y, nmt_country[i].short_name, nmt_country[i].long_name, nmt_country[i].provider_name);
		else
			printf("%d\t%s\n", nmt_country[i].y, nmt_country[i].short_name);
	}
}

uint8_t nmt_country_by_short_name(const char *short_name)
{
	int i;

	for (i = 0; nmt_country[i].short_name; i++) {
		if (!strcasecmp(nmt_country[i].short_name, short_name)) {
			PDEBUG(DNMT, DEBUG_INFO, "Selecting country code %d of %s,%s (provider '%s')\n", nmt_country[i].y, nmt_country[i].short_name, nmt_country[i].long_name, nmt_country[i].provider_name);
			return nmt_country[i].y;
		}
	}

	return 0;
}

static void nmt_timeout(struct timer *timer);
static void nmt_go_idle(nmt_t *nmt);

/* Create transceiver instance and link to a list. */
int nmt_create(const char *sounddev, int samplerate, int channel, enum nmt_chan_type chan_type, uint8_t ms_power, uint8_t traffic_area, uint8_t area_no, int compander, int supervisory, int loopback)
{
	nmt_t *nmt;
	int rc;

	if (channel < 1 || channel > 399) {
		PDEBUG(DNMT, DEBUG_ERROR, "Channel number %d invalid.\n", channel);
		return -EINVAL;
	}

	if (channel >= 201) {
		PDEBUG(DNMT, DEBUG_NOTICE, "*** Channels numbers above 200 have been specified, but never used. These 'interleaved channels are probably not supports by the phone.\n");
	}

	if (channel >= 181 && channel <= 200) {
		PDEBUG(DNMT, DEBUG_NOTICE, "Extended channel numbers (181..200) have been specified, but never been supported for sure. There is no phone to test with, so don't use it!\n");
	}

	if (chan_type == CHAN_TYPE_CC) {
		PDEBUG(DNMT, DEBUG_NOTICE, "*** Selected channel can be used for calling only.\n");
		PDEBUG(DNMT, DEBUG_NOTICE, "*** No call from the mobile phone is possible.\n");
		PDEBUG(DNMT, DEBUG_NOTICE, "*** Use combined 'CC/TC' instead!\n");
	}
	if (chan_type == CHAN_TYPE_TC) {
		PDEBUG(DNMT, DEBUG_NOTICE, "*** Selected channel can be used for traffic only.\n");
		PDEBUG(DNMT, DEBUG_NOTICE, "*** No call to the mobile phone is possible.\n");
		PDEBUG(DNMT, DEBUG_NOTICE, "*** Use combined 'CC/TC' instead!\n");
	}
	if (chan_type == CHAN_TYPE_TEST && !loopback) {
		PDEBUG(DNMT, DEBUG_NOTICE, "*** Selected channel can be used for nothing but testing signal decoder.\n");
	}

	nmt = calloc(1, sizeof(nmt_t));
	if (!nmt) {
		PDEBUG(DNMT, DEBUG_ERROR, "No memory!\n");
		return -ENOMEM;
	}

	PDEBUG(DNMT, DEBUG_DEBUG, "Creating 'NMT' instance for channel = %d (sample rate %d).\n", channel, samplerate);

	/* init general part of transceiver */
	rc = sender_create(&nmt->sender, sounddev, samplerate, channel, loopback, 0, -1);
	if (rc < 0) {
		PDEBUG(DNMT, DEBUG_ERROR, "Failed to init transceiver process!\n");
		goto error;
	}

	/* init audio processing */
	rc = dsp_init_sender(nmt);
	if (rc < 0) {
		PDEBUG(DNMT, DEBUG_ERROR, "Failed to init audio processing!\n");
		goto error;
	}

	timer_init(&nmt->timer, nmt_timeout, nmt);
	nmt->sysinfo.chan_type = chan_type;
	nmt->sysinfo.ms_power = ms_power;
	nmt->sysinfo.traffic_area = traffic_area;
	nmt->sysinfo.area_no = area_no;
	nmt->compander = compander;
	nmt->supervisory = supervisory;

	/* go into idle state */
	nmt_go_idle(nmt);

	return 0;

error:
	nmt_destroy(&nmt->sender);

	return rc;
}

/* Destroy transceiver instance and unlink from list. */
void nmt_destroy(sender_t *sender)
{
	nmt_t *nmt = (nmt_t *) sender;

	PDEBUG(DNMT, DEBUG_DEBUG, "Destroying 'NMT' instance for channel = %d.\n", sender->kanal);
	dsp_cleanup_sender(nmt);
	timer_exit(&nmt->timer);
	sender_destroy(&nmt->sender);
	free(nmt);
}

/* Abort connection towards mobile station by sending idle digits. */
static void nmt_go_idle(nmt_t *nmt)
{
	timer_stop(&nmt->timer);

	PDEBUG(DNMT, DEBUG_INFO, "Entering IDLE state, sending idle frames on %s.\n", chan_type_long_name(nmt->sysinfo.chan_type));
	nmt_new_state(nmt, STATE_IDLE);
	nmt_set_dsp_mode(nmt, DSP_MODE_FRAME);
	memset(&nmt->subscriber, 0, sizeof(nmt->subscriber));
	memset(&nmt->dialing, 0, sizeof(nmt->dialing));
}

/* release an ongoing connection, this is used by roaming update and release initiated by MTX */
static void nmt_release(nmt_t *nmt)
{
	timer_stop(&nmt->timer);

	PDEBUG(DNMT, DEBUG_INFO, "Releasing connection towards mobile station.\n");
	nmt_new_state(nmt, STATE_MT_RELEASE);
	nmt->tx_frame_count = 0;
	nmt_set_dsp_mode(nmt, DSP_MODE_FRAME);
	timer_start(&nmt->timer, RELEASE_TO);
}

/* Enter paging state and transmit phone's number on calling channel */
static void nmt_page(nmt_t *nmt, char ms_country, const char *ms_number, int try)
{
	PDEBUG(DNMT, DEBUG_INFO, "Entering paging state (try %d), sending call to '%c,%s'.\n", try, ms_country, ms_number);
	nmt->subscriber.country = ms_country;
	strcpy(nmt->subscriber.number, ms_number);
	nmt->page_try = try;
	nmt_new_state(nmt, STATE_MT_PAGING);
	nmt_set_dsp_mode(nmt, DSP_MODE_FRAME);
	nmt->tx_frame_count = 0;
}

/*
 * frame matching functions to check if channels is accessed correctly
 */

/* check match channel no, area no and traffic area */
static int match_channel(nmt_t *nmt, frame_t *frame)
{
	int channel, power;

	/* check channel match */
	nmt_decode_channel(frame->channel_no, &channel, &power);
	if (channel != nmt->sender.kanal) {
		PDEBUG(DNMT, DEBUG_NOTICE, "Frame for different channel %d received, ignoring.\n", channel);
		return 0;
	}

	return 1;
}

static int match_area(nmt_t *nmt, frame_t *frame)
{
	uint8_t area_no, traffic_area;

	/* old phones do not support ZY digits */
	if (frame->area_info + frame->traffic_area == 0)
		goto skip_area;

	area_no = frame->area_info >> 2;
	if (area_no == 0 && nmt->sysinfo.area_no != 0)
		area_no = 4;
	if (area_no != nmt->sysinfo.area_no) {
		PDEBUG(DNMT, DEBUG_NOTICE, "Received area no (%d) does not match the base station's area no (%d), ignoring.\n", area_no, nmt->sysinfo.area_no);
		return 0;
	}

	traffic_area = ((frame->area_info & 0x3) << 4) | frame->traffic_area;
	if (nmt->sysinfo.traffic_area != 0 && (nmt->sysinfo.traffic_area & 0x3f) != traffic_area) {
		PDEBUG(DNMT, DEBUG_NOTICE, "Received 6 bits of traffic area (0x%02x) does not match the 6 bits of base station's traffic area (0x%02x), ignoring.\n", nmt->sysinfo.traffic_area & 0x3f, traffic_area);
		return 0;
	}
skip_area:

	return 1;
}

/* check match subscriber number */
static int match_subscriber(nmt_t *nmt, frame_t *frame)
{
	if (nmt_digits2value(&nmt->subscriber.country, 1) != frame->ms_country) {
		PDEBUG(DNMT, DEBUG_NOTICE, "Received non matching subscriber counrtry, ignoring.\n");
		return 0;
	}
	if (nmt_digits2value(nmt->subscriber.number, 6) != frame->ms_number) {
		PDEBUG(DNMT, DEBUG_NOTICE, "Received non matching subscriber number, ignoring.\n");
		return 0;
	}

	return 1;
}

/*
 * handle idle channel
 */

static void tx_idle(nmt_t *nmt, frame_t *frame)
{
	switch (nmt->sysinfo.chan_type) {
	case CHAN_TYPE_CC:
		frame->index = NMT_MESSAGE_1a;
		break;
	case CHAN_TYPE_TC:
		frame->index = NMT_MESSAGE_4;
		break;
	case CHAN_TYPE_CC_TC:
		frame->index = NMT_MESSAGE_1b;
		break;
	case CHAN_TYPE_TEST:
		frame->index = NMT_MESSAGE_30;
		break;
	}
	frame->channel_no = nmt_encode_channel(nmt->sender.kanal, nmt->sysinfo.ms_power);
	frame->traffic_area = nmt->sysinfo.traffic_area;
	frame->additional_info = nmt_encode_area_no(nmt->sysinfo.area_no);
}

static void rx_idle(nmt_t *nmt, frame_t *frame)
{
	switch (frame->index) {
	case NMT_MESSAGE_11a: /* roaming update and seizure */
		if (!match_channel(nmt, frame))
			break;
		if (!match_area(nmt, frame))
			break;
		/* set subscriber */
		nmt_value2digits(frame->ms_country, &nmt->subscriber.country, 1);
		nmt_value2digits(frame->ms_number, nmt->subscriber.number, 6);
		nmt->subscriber.number[6] = '\0';

		PDEBUG(DNMT, DEBUG_INFO, "Received roaming seizure from subscriber %c,%s\n", nmt->subscriber.country, nmt->subscriber.number);
		/* change state */
		nmt_new_state(nmt, STATE_ROAMING_IDENT);
		nmt->rx_frame_count = 0;
		nmt->tx_frame_count = 0;
		break;
	case NMT_MESSAGE_10b: /* seizure from ordinary MS */
	case NMT_MESSAGE_12: /* seizure from coinbox MS */
		if (!match_channel(nmt, frame))
			break;
		if (!match_area(nmt, frame))
			break;
		/* set subscriber */
		nmt_value2digits(frame->ms_country, &nmt->subscriber.country, 1);
		nmt_value2digits(frame->ms_number, nmt->subscriber.number, 6);
		if (frame->index == NMT_MESSAGE_12)
			nmt->subscriber.coinbox = 1;
		nmt->subscriber.number[6] = '\0';

		PDEBUG(DNMT, DEBUG_INFO, "Received call from subscriber %c,%s%s\n", nmt->subscriber.country, nmt->subscriber.number, (nmt->subscriber.coinbox) ? " (coinbox)" : "");
		/* change state */
		nmt_new_state(nmt, STATE_MO_IDENT);
		nmt->rx_frame_count = 0;
		nmt->tx_frame_count = 0;
		break;
	/* signals after release */
	case NMT_MESSAGE_13a: /* line signal */
		break;
	default:
		PDEBUG(DNMT, DEBUG_DEBUG, "Dropping message %s in state %s\n", nmt_frame_name(frame->index), nmt_state_name(nmt->state));
	}
}

/*
 * handle roaming
 */

static void tx_ident(nmt_t *nmt, frame_t *frame)
{
	frame->index = NMT_MESSAGE_3b;
	frame->channel_no = nmt_encode_channel(nmt->sender.kanal, nmt->sysinfo.ms_power);
	frame->traffic_area = nmt->sysinfo.traffic_area;
	frame->ms_country = nmt_digits2value(&nmt->subscriber.country, 1);
	frame->ms_number = nmt_digits2value(nmt->subscriber.number, 6);
	frame->additional_info = nmt_encode_area_no(nmt->sysinfo.area_no);
}

static void set_line_signal(nmt_t *nmt, frame_t *frame, uint8_t signal)
{
	frame->index = NMT_MESSAGE_5a;
	frame->channel_no = nmt_encode_channel(nmt->sender.kanal, nmt->sysinfo.ms_power);
	frame->traffic_area = nmt->sysinfo.traffic_area;
	frame->ms_country = nmt_digits2value(&nmt->subscriber.country, 1);
	frame->ms_number = nmt_digits2value(nmt->subscriber.number, 6);
	frame->line_signal = (signal << 8) | (signal << 4) | signal;
}

static void tx_roaming_ident(nmt_t *nmt, frame_t *frame)
{
	if (++nmt->tx_frame_count == 1)
		PDEBUG(DNMT, DEBUG_INFO, "Sending identity request.\n");
	if (nmt->tx_frame_count == 8) {
		nmt_release(nmt);
	}
	tx_ident(nmt, frame);
}

static void rx_roaming_ident(nmt_t *nmt, frame_t *frame)
{
	switch (frame->index) {
	case NMT_MESSAGE_11a: /* roaming update */
		if (!match_channel(nmt, frame))
			break;
		if (!match_area(nmt, frame))
			break;
		if (!match_subscriber(nmt, frame))
			break;
		if (nmt->rx_frame_count < 2) {
			PDEBUG(DNMT, DEBUG_DEBUG, "Skipping second seizure frame\n");
			break;
		}
		nmt_value2digits(frame->ms_password, nmt->subscriber.password, 3);
		PDEBUG(DNMT, DEBUG_INFO, "Received identity confirm (password %s).\n", nmt->subscriber.password);
		nmt_new_state(nmt, STATE_ROAMING_CONFIRM);
		nmt->tx_frame_count = 0;
		break;
	default:
		PDEBUG(DNMT, DEBUG_DEBUG, "Dropping message %s in state %s\n", nmt_frame_name(frame->index), nmt_state_name(nmt->state));
	}
}

static void tx_roaming_confirm(nmt_t *nmt, frame_t *frame)
{
	set_line_signal(nmt, frame, 3);
	if (++nmt->tx_frame_count == 1)
		PDEBUG(DNMT, DEBUG_INFO, "Send 'ready to receive'.\n");
	if (nmt->tx_frame_count == 2)
		nmt_release(nmt); /* continue with this frame, then release */
}

static void rx_roaming_confirm(nmt_t *nmt, frame_t *frame)
{
	switch (frame->index) {
	default:
		PDEBUG(DNMT, DEBUG_DEBUG, "Dropping message %s in state %s\n", nmt_frame_name(frame->index), nmt_state_name(nmt->state));
	}
}

/*
 * handle call MS -> MTX
 */

static void tx_mo_ident(nmt_t *nmt, frame_t *frame)
{
	if (++nmt->tx_frame_count == 1)
		PDEBUG(DNMT, DEBUG_INFO, "Sending identity request.\n");
	if (nmt->tx_frame_count == 8) {
		nmt_release(nmt);
	}
	tx_ident(nmt, frame);
}

static void rx_mo_ident(nmt_t *nmt, frame_t *frame)
{
	switch (frame->index) {
	case NMT_MESSAGE_10b: /* seizure */
	case NMT_MESSAGE_12: /* seizure */
		if (!match_channel(nmt, frame))
			break;
		if (!match_subscriber(nmt, frame))
			break;
		if (nmt->rx_frame_count < 2) {
			PDEBUG(DNMT, DEBUG_DEBUG, "Skipping second seizure frame\n");
			break;
		}
		nmt_value2digits(frame->ms_password, nmt->subscriber.password, 3);
		PDEBUG(DNMT, DEBUG_INFO, "Received identity confirm (password %s).\n", nmt->subscriber.password);
		nmt_new_state(nmt, STATE_MO_CONFIRM);
		nmt->tx_frame_count = 0;
		break;
	default:
		PDEBUG(DNMT, DEBUG_DEBUG, "Dropping message %s in state %s\n", nmt_frame_name(frame->index), nmt_state_name(nmt->state));
	}
}

static void tx_mo_confirm(nmt_t *nmt, frame_t *frame)
{
	set_line_signal(nmt, frame, 3);
	if (++nmt->tx_frame_count <= 2) {
		if (nmt->tx_frame_count == 1)
			PDEBUG(DNMT, DEBUG_INFO, "Send 'ready to receive'.\n");
	} else {
		if (nmt->tx_frame_count == 3) {
			PDEBUG(DNMT, DEBUG_INFO, "Send dial tone.\n");
			nmt_new_state(nmt, STATE_MO_DIALING);
			nmt_set_dsp_mode(nmt, DSP_MODE_DIALTONE);
			timer_start(&nmt->timer, DIALING_TO);
		}
	}
}

static void rx_mo_dialing(nmt_t *nmt, frame_t *frame)
{
	int len = strlen(nmt->dialing);

	switch (frame->index) {
	case NMT_MESSAGE_14a: /* digits */
		if (!match_channel(nmt, frame))
			break;
		if (!match_area(nmt, frame))
			break;
		timer_start(&nmt->timer, DIALING_TO);
		/* max digits received */
		if (len + 1 == sizeof(nmt->dialing))
			break;
		/* received odd digit, but be already have odd number of digits */
		if ((len & 1)) {
			if (nmt->rx_frame_count > 1)
				goto missing_digit;
			break;
		} else if (len) {
			if (nmt->rx_frame_count > 3)
				goto missing_digit;
		}
		nmt->dialing[len] = nmt_value2digit(frame->digit);
		nmt->dialing[len + 1] = '\0';
		PDEBUG(DNMT, DEBUG_INFO, "Received (odd)  digit %c.\n", nmt->dialing[len]);
		nmt->rx_frame_count = 0;
		/* finish dial tone after first digit */
		if (!len)
			nmt_set_dsp_mode(nmt, DSP_MODE_AUDIO);
		break;
	case NMT_MESSAGE_14b: /* digits */
		if (!match_channel(nmt, frame))
			break;
		if (!match_area(nmt, frame))
			break;
		timer_start(&nmt->timer, DIALING_TO);
		/* max digits received */
		if (len + 1 == sizeof(nmt->dialing))
			break;
		/* received odd digit, but be already have odd number of digits */
		if (!(len & 1)) {
			if (len && nmt->rx_frame_count > 1)
				goto missing_digit;
			break;
		} else {
			if (nmt->rx_frame_count > 3)
				goto missing_digit;
		}
		nmt->dialing[len] = nmt_value2digit(frame->digit);
		nmt->dialing[len + 1] = '\0';
		PDEBUG(DNMT, DEBUG_INFO, "Received (even) digit %c.\n", nmt->dialing[len]);
		nmt->rx_frame_count = 0;
		break;
	case NMT_MESSAGE_15: /* idle */
		if (!len)
			break;
		PDEBUG(DNMT, DEBUG_INFO, "Dialing complete %s->%s, call established.\n", &nmt->subscriber.country, nmt->dialing);
		/* setup call */
		{
			int callref = ++new_callref;
			int rc;
			PDEBUG(DNMT, DEBUG_INFO, "Setup call to network.\n");
			rc = call_in_setup(callref, &nmt->subscriber.country, nmt->dialing);
			if (rc < 0) {
				PDEBUG(DNMT, DEBUG_NOTICE, "Call rejected (cause %d), releasing.\n", rc);
				nmt_release(nmt);
				return;
			}
			nmt->sender.callref = callref;
		}
		nmt_new_state(nmt, STATE_MO_COMPLETE);
		nmt_set_dsp_mode(nmt, DSP_MODE_FRAME);
		nmt->tx_frame_count = 0;
		break;
	default:
		PDEBUG(DNMT, DEBUG_DEBUG, "Dropping message %s in state %s\n", nmt_frame_name(frame->index), nmt_state_name(nmt->state));
	}

	return;

missing_digit:
	PDEBUG(DNMT, DEBUG_NOTICE, "Missing digit, aborting.\n");
	nmt_release(nmt);
}

static void tx_mo_complete(nmt_t *nmt, frame_t *frame)
{
	if (++nmt->tx_frame_count <= 4) {
		set_line_signal(nmt, frame, 6);
		if (nmt->tx_frame_count == 1)
			PDEBUG(DNMT, DEBUG_INFO, "Send 'addess complete'.\n");
	} else {
		if (nmt->compander) {
			set_line_signal(nmt, frame, 5);
			if (nmt->tx_frame_count == 5)
				PDEBUG(DNMT, DEBUG_INFO, "Send 'compander in'.\n");
		} else
			frame->index = NMT_MESSAGE_6;
		if (nmt->tx_frame_count == 9) {
			PDEBUG(DNMT, DEBUG_INFO, "Connect audio.\n");
			nmt_new_state(nmt, STATE_ACTIVE);
			nmt->active_state = ACTIVE_STATE_VOICE;
			nmt_set_dsp_mode(nmt, DSP_MODE_AUDIO);
			if (nmt->supervisory) {
				super_reset(nmt);
				timer_start(&nmt->timer, SUPERVISORY_TO1);
			}
		}
	}
}

static void timeout_mo_dialing(nmt_t *nmt)
{
	PDEBUG(DNMT, DEBUG_NOTICE, "Timeout while receiving digits.\n");
	nmt_release(nmt);
	PDEBUG(DNMT, DEBUG_INFO, "Release call towards network.\n");
	call_in_release(nmt->sender.callref, CAUSE_TEMPFAIL);
	nmt->sender.callref = 0;
}

/*
 * handle call MTX -> MS
 */
static void tx_mt_paging(nmt_t *nmt, frame_t *frame)
{
	frame->index = NMT_MESSAGE_2a;
	frame->channel_no = nmt_encode_channel(nmt->sender.kanal, nmt->sysinfo.ms_power);
	frame->traffic_area = nmt->sysinfo.traffic_area;
	frame->ms_country = nmt_digits2value(&nmt->subscriber.country, 1);
	frame->ms_number = nmt_digits2value(nmt->subscriber.number, 6);
	frame->additional_info = nmt_encode_area_no(nmt->sysinfo.area_no);
	if (++nmt->tx_frame_count == 1) {
		PDEBUG(DNMT, DEBUG_INFO, "Send call to mobile.\n");
	} else
		tx_idle(nmt, frame);
	/* wait some time to get answer. use more than one frame due to delay of audio processing */
	if (nmt->tx_frame_count == 5) {
		PDEBUG(DNMT, DEBUG_NOTICE, "No answer from mobile phone (try %d).\n", nmt->page_try);
		if (nmt->page_try == PAGE_TRIES) {
			PDEBUG(DNMT, DEBUG_INFO, "Release call towards network.\n");
			call_in_release(nmt->sender.callref, CAUSE_OUTOFORDER);
			nmt->sender.callref = 0;
			nmt_go_idle(nmt);
			return;
		}
		nmt_page(nmt, nmt->subscriber.country, nmt->subscriber.number, nmt->page_try + 1);
	}
}

static void rx_mt_paging(nmt_t *nmt, frame_t *frame)
{
	switch (frame->index) {
	case NMT_MESSAGE_10a: /* call acknowledgement */
		if (!match_channel(nmt, frame))
			break;
		if (!match_subscriber(nmt, frame))
			break;
		PDEBUG(DNMT, DEBUG_INFO, "Received call acknowledgement.\n");
		nmt_new_state(nmt, STATE_MT_CHANNEL);
		nmt->tx_frame_count = 0;
		break;
	default:
		PDEBUG(DNMT, DEBUG_DEBUG, "Dropping message %s in state %s\n", nmt_frame_name(frame->index), nmt_state_name(nmt->state));
	}
}

static void tx_mt_channel(nmt_t *nmt, frame_t *frame)
{
	frame->index = NMT_MESSAGE_2b;
	frame->channel_no = nmt_encode_channel(nmt->sender.kanal, nmt->sysinfo.ms_power);
	frame->traffic_area = nmt->sysinfo.traffic_area;
	frame->ms_country = nmt_digits2value(&nmt->subscriber.country, 1);
	frame->ms_number = nmt_digits2value(nmt->subscriber.number, 6);
	frame->tc_no = nmt_encode_channel(nmt->sender.kanal, nmt->sysinfo.ms_power);
	PDEBUG(DNMT, DEBUG_INFO, "Send channel activation to mobile.\n");
	nmt_new_state(nmt, STATE_MT_IDENT);
}

static void tx_mt_ident(nmt_t *nmt, frame_t *frame)
{
	if (++nmt->tx_frame_count == 1)
		PDEBUG(DNMT, DEBUG_INFO, "Sending identity request.\n");
	if (nmt->tx_frame_count == 8) {
		PDEBUG(DNMT, DEBUG_INFO, "Release call towards network.\n");
		call_in_release(nmt->sender.callref, CAUSE_TEMPFAIL);
		nmt->sender.callref = 0;
		nmt_go_idle(nmt);
	}
	tx_ident(nmt, frame);
}

static void rx_mt_ident(nmt_t *nmt, frame_t *frame)
{
	switch (frame->index) {
	case NMT_MESSAGE_10b: /* seizure */
		if (!match_subscriber(nmt, frame))
			break;
		nmt_value2digits(frame->ms_password, nmt->subscriber.password, 3);
		PDEBUG(DNMT, DEBUG_INFO, "Received identity (password %s).\n", nmt->subscriber.password);
		nmt_new_state(nmt, STATE_MT_RINGING);
		nmt->tx_frame_count = 0;
		timer_start(&nmt->timer, RINGING_TO);
		call_in_alerting(nmt->sender.callref);
		break;
	default:
		PDEBUG(DNMT, DEBUG_DEBUG, "Dropping message %s in state %s\n", nmt_frame_name(frame->index), nmt_state_name(nmt->state));
	}
}

static void tx_mt_ringing(nmt_t *nmt, frame_t *frame)
{
	set_line_signal(nmt, frame, 9);
	if (++nmt->tx_frame_count == 1)
		PDEBUG(DNMT, DEBUG_INFO, "Send 'ringing order'.\n");
	if (nmt->tx_frame_count >= 4)
		frame->index = NMT_MESSAGE_6;
	/* repeat ringing after 5 seconds */
	if (nmt->tx_frame_count == 36)
		nmt->tx_frame_count = 0;
}

static void rx_mt_ringing(nmt_t *nmt, frame_t *frame)
{
	switch (frame->index) {
	case NMT_MESSAGE_13a: /* line signal */
		if (!match_subscriber(nmt, frame))
			break;
		if ((frame->line_signal & 0xf) != 14)
			break;
		PDEBUG(DNMT, DEBUG_INFO, "Received 'answer' from phone.\n");
		nmt_new_state(nmt, STATE_MT_COMPLETE);
		nmt->tx_frame_count = 0;
		call_in_answer(nmt->sender.callref, &nmt->subscriber.country);
		break;
	default:
		PDEBUG(DNMT, DEBUG_DEBUG, "Dropping message %s in state %s\n", nmt_frame_name(frame->index), nmt_state_name(nmt->state));
	}
}

static void tx_mt_complete(nmt_t *nmt, frame_t *frame)
{
	set_line_signal(nmt, frame, 5);
	++nmt->tx_frame_count;
	if (nmt->compander) {
		if (nmt->tx_frame_count == 1)
			PDEBUG(DNMT, DEBUG_INFO, "Send 'compander in'.\n");
	} else
		frame->index = NMT_MESSAGE_6;
	if (nmt->tx_frame_count == 5) {
		PDEBUG(DNMT, DEBUG_INFO, "Connect audio.\n");
		nmt_new_state(nmt, STATE_ACTIVE);
		nmt->active_state = ACTIVE_STATE_VOICE;
		nmt_set_dsp_mode(nmt, DSP_MODE_AUDIO);
		if (nmt->supervisory) {
			super_reset(nmt);
			timer_start(&nmt->timer, SUPERVISORY_TO1);
		}
	}
}

static void timeout_mt_ringing(nmt_t *nmt)
{
	PDEBUG(DNMT, DEBUG_NOTICE, "Timeout while waiting for answer of the phone.\n");
	nmt_release(nmt);
	PDEBUG(DNMT, DEBUG_INFO, "Release call towards network.\n");
	call_in_release(nmt->sender.callref, CAUSE_NOANSWER);
	nmt->sender.callref = 0;
}

/*
 * handle clearing towards MTX
 */

static void tx_mo_release(nmt_t *nmt, frame_t *frame)
{
	set_line_signal(nmt, frame, 15);
	if (++nmt->tx_frame_count == 1)
		PDEBUG(DNMT, DEBUG_INFO, "Send release.\n");
	if (nmt->tx_frame_count == 4)
		nmt_go_idle(nmt); /* continue with this frame, then go idle */
}

/*
 * handle clearing towards MS
 */

static void tx_mt_release(nmt_t *nmt, frame_t *frame)
{
	set_line_signal(nmt, frame, 15);
	if (++nmt->tx_frame_count == 1)
		PDEBUG(DNMT, DEBUG_INFO, "Send release.\n");
}

static void rx_mt_release(nmt_t *nmt, frame_t *frame)
{
	switch (frame->index) {
	case NMT_MESSAGE_13a: /* line signal */
		if (!match_channel(nmt, frame))
			break;
		if (!match_subscriber(nmt, frame))
			break;
		if ((frame->line_signal & 0xf) != 1)
			break;
		PDEBUG(DNMT, DEBUG_INFO, "Received release guard.\n");
		nmt_go_idle(nmt);
		break;
	default:
		PDEBUG(DNMT, DEBUG_DEBUG, "Dropping message %s in state %s\n", nmt_frame_name(frame->index), nmt_state_name(nmt->state));
	}
}

static void timeout_mt_release(nmt_t *nmt)
{
	PDEBUG(DNMT, DEBUG_NOTICE, "Timeout while releasing.\n");
	nmt_go_idle(nmt);
}

/*
 * handle call
 */

void nmt_rx_super(nmt_t *nmt, int tone, double quality)
{
	if (tone)
		PDEBUG(DNMT, DEBUG_INFO, "Detected supervisory signal with quality=%.0f.\n", quality * 100.0);
	else
		PDEBUG(DNMT, DEBUG_INFO, "Lost supervisory signal\n");

	if (nmt->sender.loopback)
		return;

	/* only detect supervisory signal during active call */
	if (nmt->state != STATE_ACTIVE || !nmt->supervisory)
		return;

	if (tone)
		timer_stop(&nmt->timer);
	else
		timer_start(&nmt->timer, SUPERVISORY_TO2);
}

static void timeout_active(nmt_t *nmt, double duration)
{
	if (duration == SUPERVISORY_TO1)
		PDEBUG(DNMT, DEBUG_NOTICE, "Timeout after %.0f seconds not receiving supervisory signal.\n", duration);
	else
		PDEBUG(DNMT, DEBUG_NOTICE, "Timeout after %.0f seconds loosing supervisory signal.\n", duration);
	nmt_release(nmt);
	PDEBUG(DNMT, DEBUG_INFO, "Release call towards network.\n");
	call_in_release(nmt->sender.callref, CAUSE_TEMPFAIL);
	nmt->sender.callref = 0;
}

static void rx_active(nmt_t *nmt, frame_t *frame)
{
	char digit;

	/* restart timer on every reception of frame */
	if (nmt->supervisory)
		timer_start(&nmt->timer, SUPERVISORY_TO2);

	switch (frame->index) {
	case NMT_MESSAGE_13a: /* line signal */
		if (!match_channel(nmt, frame))
			break;
		if (!match_subscriber(nmt, frame))
			break;
		switch ((frame->line_signal & 0xf)) {
		case 5:
			PDEBUG(DNMT, DEBUG_NOTICE, "Register Recall is not supported.\n");
			break;
		case 8:
			if (nmt->active_state != ACTIVE_STATE_VOICE)
				break;
			PDEBUG(DNMT, DEBUG_INFO, "Received 'MFT in' request.\n");
			nmt->active_state = ACTIVE_STATE_MFT_IN;
			nmt->tx_frame_count = 0;
			nmt_set_dsp_mode(nmt, DSP_MODE_FRAME);
			nmt->mft_num = 0;
			break;
		case 7:
			if (nmt->active_state != ACTIVE_STATE_MFT)
				break;
			PDEBUG(DNMT, DEBUG_INFO, "Received 'MFT out' request.\n");
			nmt->active_state = ACTIVE_STATE_MFT_OUT;
			nmt->tx_frame_count = 0;
			nmt_set_dsp_mode(nmt, DSP_MODE_FRAME);
			break;
		}
		break;
	case NMT_MESSAGE_14a: /* digits */
		if (!match_channel(nmt, frame))
			break;
		if (!match_area(nmt, frame))
			break;
		if (nmt->active_state != ACTIVE_STATE_MFT)
			break;
		if ((nmt->mft_num & 1))
			break;
		digit = nmt_value2digit(frame->digit);
		dtmf_set_tone(&nmt->dtmf, digit);
		PDEBUG(DNMT, DEBUG_INFO, "Received (odd)  digit %c.\n", digit);
		nmt->mft_num++;
		break;
	case NMT_MESSAGE_14b: /* digits */
		if (!match_channel(nmt, frame))
			break;
		if (!match_area(nmt, frame))
			break;
		if (nmt->active_state != ACTIVE_STATE_MFT)
			break;
		if (!(nmt->mft_num & 1))
			break;
		digit = nmt_value2digit(frame->digit);
		dtmf_set_tone(&nmt->dtmf, digit);
		PDEBUG(DNMT, DEBUG_INFO, "Received (even) digit %c.\n", digit);
		nmt->mft_num++;
		break;
	default:
		PDEBUG(DNMT, DEBUG_DEBUG, "Dropping message %s in state %s\n", nmt_frame_name(frame->index), nmt_state_name(nmt->state));
	}
}

static void tx_active(nmt_t *nmt, frame_t *frame)
{
	switch (nmt->active_state) {
	case ACTIVE_STATE_MFT_IN:
		set_line_signal(nmt, frame, 4);
		if (++nmt->tx_frame_count == 1)
			PDEBUG(DNMT, DEBUG_INFO, "Send 'MFT in acknowledge'.\n");
		if (nmt->tx_frame_count > 4) {
			nmt->active_state = ACTIVE_STATE_MFT;
			nmt_set_dsp_mode(nmt, DSP_MODE_DTMF);
		}
		break;
	case ACTIVE_STATE_MFT_OUT:
		set_line_signal(nmt, frame, 10);
		if (++nmt->tx_frame_count == 1)
			PDEBUG(DNMT, DEBUG_INFO, "Send 'MFT out acknowledge'.\n");
		if (nmt->tx_frame_count > 4) {
			nmt->active_state = ACTIVE_STATE_VOICE;
			nmt_set_dsp_mode(nmt, DSP_MODE_AUDIO);
			if (nmt->supervisory)
				super_reset(nmt);
		}
		break;
	default:
		;
	}
}

/*
 * general handlers to call sub handling
 */

void nmt_receive_frame(nmt_t *nmt, const char *bits, double quality, double level, double frames_elapsed)
{
	frame_t frame;
	int rc;

	rc = decode_frame(&frame, bits, (nmt->sender.loopback) ? MTX_TO_XX : XX_TO_MTX, (nmt->state == STATE_MT_PAGING));
	if (rc < 0) {
		PDEBUG(DNMT, (nmt->sender.loopback) ? DEBUG_NOTICE : DEBUG_DEBUG, "Received invalid frame. (quality=%.0f%% level=%.0f%%)\n", quality * 100.0, level * 100.0);
		return;
	}

	/* frame counter */
	nmt->rx_frame_count += (int)(frames_elapsed + 0.5);

	PDEBUG(DNMT, (nmt->sender.loopback) ? DEBUG_NOTICE : DEBUG_DEBUG, "Received frame %s (quality=%.0f%% level=%.0f%%)\n", nmt_frame_name(frame.index), quality * 100.0, level * 100.0);

	if (nmt->sender.loopback)
		return;

	/* MS releases, but this is not the acknowledge of MTX release */
	if (frame.index == NMT_MESSAGE_13a
	 && (frame.line_signal & 0xf) == 1
	 && nmt->state != STATE_MO_RELEASE
	 && nmt->state != STATE_MT_RELEASE) {
	 	/* drop packets after release */
	 	if (nmt->state == STATE_IDLE)
			return;
		if (!match_subscriber(nmt, &frame))
			return;
		PDEBUG(DNMT, DEBUG_INFO, "Received clearing by mobile phone in state %s.\n", nmt_state_name(nmt->state));
		nmt_new_state(nmt, STATE_MO_RELEASE);
		nmt->tx_frame_count = 0;
		nmt_set_dsp_mode(nmt, DSP_MODE_FRAME);
		if (nmt->sender.callref) {
			PDEBUG(DNMT, DEBUG_INFO, "Release call towards network.\n");
			call_in_release(nmt->sender.callref, CAUSE_NORMAL);
			nmt->sender.callref = 0;
		}
		return;
	}

	switch (nmt->state) {
	case STATE_IDLE:
		rx_idle(nmt, &frame);
		break;
	case STATE_ROAMING_IDENT:
		rx_roaming_ident(nmt, &frame);
		break;
	case STATE_ROAMING_CONFIRM:
		rx_roaming_confirm(nmt, &frame);
		break;
	case STATE_MO_IDENT:
		rx_mo_ident(nmt, &frame);
		break;
	case STATE_MO_CONFIRM:
	case STATE_MO_DIALING:
		rx_mo_dialing(nmt, &frame);
		break;
	case STATE_MO_RELEASE:
		break;
	case STATE_MT_PAGING:
		rx_mt_paging(nmt, &frame);
		break;
	case STATE_MT_IDENT:
		rx_mt_ident(nmt, &frame);
		break;
	case STATE_MT_RINGING:
		rx_mt_ringing(nmt, &frame);
		break;
	case STATE_MT_RELEASE:
		rx_mt_release(nmt, &frame);
		break;
	case STATE_ACTIVE:
		rx_active(nmt, &frame);
		break;
	default:
		PDEBUG(DNMT, DEBUG_DEBUG, "Dropping message %s in state %s\n", nmt_frame_name(frame.index), nmt_state_name(nmt->state));
	}
}

/* Timeout handling */
static void nmt_timeout(struct timer *timer)
{
	nmt_t *nmt = (nmt_t *)timer->priv;

	switch (nmt->state) {
	case STATE_MO_DIALING:
		timeout_mo_dialing(nmt);
		break;
	case STATE_MT_RINGING:
		timeout_mt_ringing(nmt);
		break;
	case STATE_MT_RELEASE:
		timeout_mt_release(nmt);
		break;
	case STATE_ACTIVE:
		timeout_active(nmt, timer->duration);
		break;
	default:
		break;
	}
}

/* FSK processing requests next frame after transmission of previous
   frame has been finished. */
const char *nmt_get_frame(nmt_t *nmt)
{
	frame_t frame;
	const char *bits;
	int debug = 1;

	memset(&frame, 0, sizeof(frame));

	switch(nmt->state) {
	case STATE_IDLE:
		tx_idle(nmt, &frame);
		break;
	case STATE_ROAMING_IDENT:
		tx_roaming_ident(nmt, &frame);
		break;
	case STATE_ROAMING_CONFIRM:
		tx_roaming_confirm(nmt, &frame);
		break;
	case STATE_MO_IDENT:
		tx_mo_ident(nmt, &frame);
		break;
	case STATE_MO_CONFIRM:
		tx_mo_confirm(nmt, &frame);
		break;
	case STATE_MO_COMPLETE:
		tx_mo_complete(nmt, &frame);
		break;
	case STATE_MO_RELEASE:
		tx_mo_release(nmt, &frame);
		break;
	case STATE_MT_PAGING:
		tx_mt_paging(nmt, &frame);
		break;
	case STATE_MT_CHANNEL:
		tx_mt_channel(nmt, &frame);
		break;
	case STATE_MT_IDENT:
		tx_mt_ident(nmt, &frame);
		break;
	case STATE_MT_RINGING:
		tx_mt_ringing(nmt, &frame);
		break;
	case STATE_MT_COMPLETE:
		tx_mt_complete(nmt, &frame);
		break;
	case STATE_MT_RELEASE:
		tx_mt_release(nmt, &frame);
		break;
	case STATE_ACTIVE:
		tx_active(nmt, &frame);
		break;
	default:
		break;
	}

	/* no encoding debug for certain (idle) frames */
	switch(frame.index) {
	case NMT_MESSAGE_1a:
	case NMT_MESSAGE_4:
	case NMT_MESSAGE_1b:
	case NMT_MESSAGE_30:
		debug = 0;
		break;
	}

	/* frame sending aborted (e.g. due to audio) */
	if (nmt->dsp_mode != DSP_MODE_FRAME)
		return NULL;

	bits = encode_frame(&frame, debug);

	PDEBUG(DNMT, DEBUG_DEBUG, "Sending frame %s.\n", nmt_frame_name(frame.index));
	return bits;
}

/*
 * call states received from call control
 */

/* Call control starts call towards mobile station. */
int call_out_setup(int callref, char *dialing)
{
	sender_t *sender;
	nmt_t *nmt;
	int i;
	char ms_country;
	char ms_number[7] = "000000";

	/* 1. check if number is invalid, return INVALNUMBER */
	if (dialstring2number(dialing, &ms_country, ms_number)) {
inval:
		PDEBUG(DNMT, DEBUG_NOTICE, "Outgoing call to invalid number '%s', rejecting!\n", dialing);
		return -CAUSE_INVALNUMBER;
	}
	for (i = 0; i < 6; i++) {
		if (ms_number[i] < '0' || ms_number[i] > '9')
			goto inval;
	}

	/* 2. check if given number is already in a call, return BUSY */
	for (sender = sender_head; sender; sender = sender->next) {
		nmt = (nmt_t *) sender;
		if (nmt->subscriber.country == ms_country && !strcmp(nmt->subscriber.number, ms_number))
			break;
	}
	if (sender) {
		PDEBUG(DNMT, DEBUG_NOTICE, "Outgoing call to busy number, rejecting!\n");
		return -CAUSE_BUSY;
	}

	/* 3. check if all senders are busy, return NOCHANNEL */
	for (sender = sender_head; sender; sender = sender->next) {
		nmt = (nmt_t *) sender;
		if (nmt->sysinfo.chan_type != CHAN_TYPE_CC
		 && nmt->sysinfo.chan_type != CHAN_TYPE_CC_TC)
		 	continue;
		if (nmt->state == STATE_IDLE)
			break;
	}
	if (!sender) {
		PDEBUG(DNMT, DEBUG_NOTICE, "Outgoing call, but no free calling channel, rejecting!\n");
		return -CAUSE_NOCHANNEL;
	}

	PDEBUG(DNMT, DEBUG_INFO, "Call to mobile station, paging station id '%c%s'\n", ms_country, ms_number);

	/* 4. trying to page mobile station */
	sender->callref = callref;
	nmt_page(nmt, ms_country, ms_number, 1);

	return 0;
}

/* Call control sends disconnect (with tones).
 * An active call stays active, so tones and annoucements can be received
 * by mobile station.
 */
void call_out_disconnect(int callref, int cause)
{
	sender_t *sender;
	nmt_t *nmt;

	PDEBUG(DNMT, DEBUG_INFO, "Call has been disconnected by network.\n");

	for (sender = sender_head; sender; sender = sender->next) {
		nmt = (nmt_t *) sender;
		if (sender->callref == callref)
			break;
	}
	if (!sender) {
		PDEBUG(DNMT, DEBUG_NOTICE, "Outgoing disconnect, but no callref!\n");
		call_in_release(callref, CAUSE_INVALCALLREF);
		return;
	}

	/* Release when not active */
	if (nmt->state == STATE_ACTIVE)
		return;
	switch (nmt->state) {
	case STATE_MT_RINGING:
		PDEBUG(DNMT, DEBUG_NOTICE, "Outgoing disconnect, during ringing, releasing!\n");
	 	nmt_release(nmt);
		break;
	default:
		PDEBUG(DNMT, DEBUG_NOTICE, "Outgoing disconnect, when phone is in call setup, releasing!\n");
	 	nmt_release(nmt);
		break;
	}

	call_in_release(callref, cause);

	sender->callref = 0;
}

/* Call control releases call toward mobile station. */
void call_out_release(int callref, int cause)
{
	sender_t *sender;
	nmt_t *nmt;

	PDEBUG(DNMT, DEBUG_INFO, "Call has been released by network, releasing call.\n");

	for (sender = sender_head; sender; sender = sender->next) {
		nmt = (nmt_t *) sender;
		if (sender->callref == callref)
			break;
	}
	if (!sender) {
		PDEBUG(DNMT, DEBUG_NOTICE, "Outgoing release, but no callref!\n");
		/* don't send release, because caller already released */
		return;
	}

	sender->callref = 0;

	switch (nmt->state) {
	case STATE_ACTIVE:
		PDEBUG(DNMT, DEBUG_NOTICE, "Outgoing release, during active call, releasing!\n");
	 	nmt_release(nmt);
		break;
	case STATE_MT_RINGING:
		PDEBUG(DNMT, DEBUG_NOTICE, "Outgoing release, during ringing, releasing!\n");
	 	nmt_release(nmt);
		break;
	default:
		PDEBUG(DNMT, DEBUG_NOTICE, "Outgoing release, when phone is in call setup, releasing!\n");
	 	nmt_release(nmt);
		break;
	}
}

/* Receive audio from call instance. */
void call_rx_audio(int callref, int16_t *samples, int count)
{
	sender_t *sender;
	nmt_t *nmt;

	for (sender = sender_head; sender; sender = sender->next) {
		nmt = (nmt_t *) sender;
		if (sender->callref == callref)
			break;
	}
	if (!sender)
		return;

	if (nmt->dsp_mode == DSP_MODE_AUDIO || nmt->dsp_mode == DSP_MODE_DTMF) {
		int16_t up[count * nmt->sender.srstate.factor];
		if (nmt->compander)
			compress_audio(&nmt->cstate, samples, count);
		count = samplerate_upsample(&nmt->sender.srstate, samples, count, up);
		jitter_save(&nmt->sender.audio, up, count);
	}
}

