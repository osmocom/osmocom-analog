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

#define CHAN nmt->sender.kanal

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "../libsample/sample.h"
#include "../liblogging/logging.h"
#include "../libmobile/cause.h"
#include "../libmobile/get_time.h"
#include "../libmobile/console.h"
#include <osmocom/cc/message.h>
#include "nmt.h"
#include "transaction.h"
#include "dsp.h"
#include "frame.h"
#include "countries.h"

/* How does paging on all channels work:
 *
 * Paging is performed on all free CC channels. The transaction pointer
 * is set towards transaction. (nmt->trans points to trans)
 *
 * After timeout, all channels with that transaction pointer are released.
 *
 * When paging was replied, other channels with the transaction pointer are
 * released, but the channel with the reply is linked to transaction in both
 * directions. (trans->nmt points to nmt, nmt->trans points to trans)
 *
 */

static int sms_ref = 0;

/* Timers */
#define PAGING_TO	1,0	/* wait for paging response: fictive value */
#define RELEASE_TO	2,0	/* how long do we wait for release guard of the phone */
#define DIALING_TO	1,0	/* if we have a pause during dialing, we abort the call */
#define CHANNEL_TO	2,0	/* how long do we wait for phone to appear on assigned channel */
#define RINGING_TO	60,0	/* how long may the phone ring */
#define SUPERVISORY_TO1	3	/* 3 sec to detect after setup */
#define SUPERVISORY_TO2	20,0	/* 20 sec lost until abort */
#define DTMF_DURATION	0.1	/* 100ms */

/* Counters */
#define PAGE_TRIES	3	/* How many time do we try to page the phone */

static const char *nmt_state_name(enum nmt_state state)
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
	case STATE_MT_AUTOANSWER:
		return "MT CALL AUTOANSWER";
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

static void nmt_display_status(void)
{
	sender_t *sender;
	nmt_t *nmt;

	display_status_start();
	for (sender = sender_head; sender; sender = sender->next) {
		nmt = (nmt_t *) sender;
		display_status_channel(nmt->sender.kanal, chan_type_short_name(nmt->sysinfo.system, nmt->sysinfo.chan_type), nmt_state_name(nmt->state));
		if (nmt->trans)
			display_status_subscriber(nmt->trans->subscriber.number, NULL);
	}
	display_status_end();
}

static void nmt_new_state(nmt_t *nmt, enum nmt_state new_state)
{
	if (nmt->state == new_state)
		return;
	LOGP_CHAN(DNMT, LOGL_DEBUG, "State change: %s -> %s\n", nmt_state_name(nmt->state), nmt_state_name(new_state));
	nmt->state = new_state;
	nmt_display_status();
}

static struct nmt_channels {
	int system;
	enum nmt_chan_type chan_type;
	const char *short_name;
	const char *long_name;
} nmt_channels[] = {
	{ 0,	CHAN_TYPE_CC,		"CC",	"calling channel (calls to mobile)" },
	{ 900,	CHAN_TYPE_CCA,		"CCA",	"calling channel for group A mobiles with odd secret key (calls to mobile)" },
	{ 900,	CHAN_TYPE_CCB,		"CCB",	"calling channel for group B mobiles with even secret key (calls to mobile)" },
	{ 0,	CHAN_TYPE_TC,		"TC",	"traffic channel (calls from mobile)" },
	{ 900,	CHAN_TYPE_AC_TC,	"AC/TC","combined access & traffic channel (calls from mobile)" },
	{ 0,	CHAN_TYPE_CC_TC,	"CC/TC","combined calling & traffic channel (both way calls)" },
	{ 0,	CHAN_TYPE_TEST,	"TEST",	"test channel" },
	{ 0,	0, NULL, NULL }
};

void nmt_channel_list(int nmt_system)
{
	int i;

	printf("Type\tDescription\n");
	printf("------------------------------------------------------------------------\n");
	for (i = 0; nmt_channels[i].long_name; i++) {
		if (nmt_channels[i].system != 0 && nmt_channels[i].system != nmt_system)
			continue;
		printf("%s\t%s\n", nmt_channels[i].short_name, nmt_channels[i].long_name);
	}
}

int nmt_channel_by_short_name(int nmt_system, const char *short_name)
{
	int i;

	for (i = 0; nmt_channels[i].short_name; i++) {
		if (nmt_channels[i].system != 0 && nmt_channels[i].system != nmt_system)
			continue;
		if (!strcasecmp(nmt_channels[i].short_name, short_name))
			return nmt_channels[i].chan_type;
	}

	return -1;
}

const char *chan_type_short_name(int nmt_system, enum nmt_chan_type chan_type)
{
	int i;

	for (i = 0; nmt_channels[i].short_name; i++) {
		if (nmt_channels[i].system != 0 && nmt_channels[i].system != nmt_system)
			continue;
		if (nmt_channels[i].chan_type == chan_type)
			return nmt_channels[i].short_name;
	}

	return "invalid";
}

const char *chan_type_long_name(int nmt_system, enum nmt_chan_type chan_type)
{
	int i;

	for (i = 0; nmt_channels[i].long_name; i++) {
		if (nmt_channels[i].system != 0 && nmt_channels[i].system != nmt_system)
			continue;
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

/* convert 7-digits dial string to NMT number */
static int dialstring2number(const char *dialstring, char *ms_country, char *ms_number)
{
	if (strlen(dialstring) != 7) {
		LOGP(DNMT, LOGL_NOTICE, "Wrong number of digits, use 7 digits: ZXXXXXX (Z=country, X=mobile number)\n");
		return -1;
	}
	if (dialstring[0] < '0' || dialstring[0] > '9') {
		LOGP(DNMT, LOGL_NOTICE, "Invalid country digit (first digit) of dial string\n");
		return -1;
	}
	*ms_country = dialstring[0];
	memcpy(ms_number, dialstring + 1, 6);
	return 0;
}

static inline int is_chan_class_cc(enum nmt_chan_type chan_type)
{
	if (chan_type == CHAN_TYPE_CC
	 || chan_type == CHAN_TYPE_CCA
	 || chan_type == CHAN_TYPE_CCB
	 || chan_type == CHAN_TYPE_CC_TC)
		return 1;

	return 0;
}

static inline int is_chan_class_tc(enum nmt_chan_type chan_type)
{
	if (chan_type == CHAN_TYPE_TC
	 || chan_type == CHAN_TYPE_AC_TC
	 || chan_type == CHAN_TYPE_CC_TC)
		return 1;

	return 0;
}

static void nmt_timeout(void *data);

/* Create transceiver instance and link to a list. */
int nmt_create(int nmt_system, const char *country, const char *kanal, enum nmt_chan_type chan_type, const char *device, int use_sdr, int samplerate, double rx_gain, double tx_gain, int pre_emphasis, int de_emphasis, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, const char *read_tx_wave, uint8_t ms_power, uint8_t traffic_area, uint8_t area_no, int compandor, int supervisory, const char *smsc_number, int send_callerid, int send_clock, int loopback)
{
	nmt_t *nmt;
	int rc;
	double deviation_factor;
	int scandinavia;
	int tested;

	/* check channel matching and set deviation factor */
	if (nmt_channel2freq(nmt_system, country, atoi(kanal), 0, &deviation_factor, &scandinavia, &tested) == 0.0) {
		LOGP(DNMT, LOGL_NOTICE, "Channel number %s invalid, use '-Y list' to get a list of available channels.\n", kanal);
		return -EINVAL;
	}

	if (!tested) {
		LOGP(DNMT, LOGL_NOTICE, "*** The given NMT country has not been tested yet. Please tell the Author, if it works.\n");
	}

	if (scandinavia && atoi(kanal) >= 201) {
		LOGP(DNMT, LOGL_NOTICE, "*** Channels numbers above 200 have been specified, but never used. These 'interleaved channels are probably not supports by the phone.\n");
	}

	if (scandinavia && atoi(kanal) >= 181 && atoi(kanal) <= 200) {
		LOGP(DNMT, LOGL_NOTICE, "Extended channel numbers (181..200) have been specified, but never been supported for sure. There is no phone to test with, so don't use it!\n");
	}

	if (chan_type == CHAN_TYPE_TEST && !loopback) {
		LOGP(DNMT, LOGL_NOTICE, "*** Selected channel can be used for nothing but testing signal decoder.\n");
	}

	if (chan_type == CHAN_TYPE_CC_TC && send_clock) {
		LOGP(DNMT, LOGL_NOTICE, "*** Sending clock on combined CC + TC is not applicable.\n");
	}

	nmt = calloc(1, sizeof(nmt_t));
	if (!nmt) {
		LOGP(DNMT, LOGL_ERROR, "No memory!\n");
		return -ENOMEM;
	}

	LOGP(DNMT, LOGL_DEBUG, "Creating 'NMT' instance for channel = %s (sample rate %d).\n", kanal, samplerate);

	/* init general part of transceiver */
	rc = sender_create(&nmt->sender, kanal, nmt_channel2freq(nmt_system, country, atoi(kanal), 0, NULL, NULL, NULL), nmt_channel2freq(nmt_system, country, atoi(kanal), 1, NULL, NULL, NULL), device, use_sdr, samplerate, rx_gain, tx_gain, pre_emphasis, de_emphasis, write_rx_wave, write_tx_wave, read_rx_wave, read_tx_wave, loopback, PAGING_SIGNAL_NONE);
	if (rc < 0) {
		LOGP(DNMT, LOGL_ERROR, "Failed to init transceiver process!\n");
		goto error;
	}

	osmo_timer_setup(&nmt->timer, nmt_timeout, nmt);
	nmt->sysinfo.system = nmt_system;
	nmt->sysinfo.chan_type = chan_type;
	nmt->sysinfo.ms_power = ms_power;
	nmt->sysinfo.traffic_area = traffic_area;
	nmt->sysinfo.area_no = area_no;
	nmt->compandor = compandor;
	nmt->supervisory = supervisory;
	nmt->send_callerid = send_callerid;
	nmt->send_clock = send_clock;
	strncpy(nmt->smsc_number, smsc_number, sizeof(nmt->smsc_number) - 1);

	/* init audio processing */
	rc = dsp_init_sender(nmt, deviation_factor);
	if (rc < 0) {
		LOGP(DNMT, LOGL_ERROR, "Failed to init audio processing!\n");
		goto error;
	}

	/* init DMS processing */
	rc = dms_init_sender(nmt);
	if (rc < 0) {
		LOGP(DNMT, LOGL_ERROR, "Failed to init DMS processing!\n");
		goto error;
	}

	/* init SMS processing */
	rc = sms_init_sender(nmt);
	if (rc < 0) {
		LOGP(DNMT, LOGL_ERROR, "Failed to init SMS processing!\n");
		goto error;
	}

	/* go into idle state */
	nmt_go_idle(nmt);

	LOGP(DNMT, LOGL_NOTICE, "Created channel #%s of type '%s' = %s\n", kanal, chan_type_short_name(nmt_system, chan_type), chan_type_long_name(nmt_system, chan_type));
	if (nmt_long_name_by_short_name(nmt_system, country))
	LOGP(DNMT, LOGL_NOTICE, " -> Using country '%s'\n", nmt_long_name_by_short_name(nmt_system, country));
	LOGP(DNMT, LOGL_NOTICE, " -> Using traffic area %d,%d and area no %d\n", traffic_area >> 4, (nmt_system == 450) ? nmt_flip_ten((traffic_area & 0xf)) : (traffic_area & 0xf), area_no);
	if (nmt->supervisory)
		LOGP(DNMT, LOGL_NOTICE, " -> Using supervisory signal %d\n", supervisory);
	else
		LOGP(DNMT, LOGL_NOTICE, " -> Using no supervisory signal\n");

	return 0;

error:
	nmt_destroy(&nmt->sender);

	return rc;
}

void nmt_check_channels(int __attribute__((unused)) nmt_system)
{
	sender_t *sender;
	nmt_t *nmt;
	int cca = 0, ccb = 0, tc = 0;
	int note = 0;

	for (sender = sender_head; sender; sender = sender->next) {
		nmt = (nmt_t *) sender;
		if (nmt->sysinfo.chan_type == CHAN_TYPE_CC) {
			cca = 1;
			ccb = 1;
		}
		if (nmt->sysinfo.chan_type == CHAN_TYPE_CCA)
			cca = 1;
		if (nmt->sysinfo.chan_type == CHAN_TYPE_CCB)
			ccb = 1;
		if (nmt->sysinfo.chan_type == CHAN_TYPE_TC)
			tc = 1;
		if (nmt->sysinfo.chan_type == CHAN_TYPE_AC_TC)
			tc = 1;
		if (nmt->sysinfo.chan_type == CHAN_TYPE_CC_TC) {
			cca = 1;
			ccb = 1;
			tc = 1;
		}
	}
	if ((cca || ccb) && !tc) {
		LOGP(DNMT, LOGL_NOTICE, "\n");
		LOGP(DNMT, LOGL_NOTICE, "*** Selected channel(s) can be used for control only.\n");
		LOGP(DNMT, LOGL_NOTICE, "*** No registration and no call is possible.\n");
		LOGP(DNMT, LOGL_NOTICE, "*** Use at least one 'TC' or use combined 'CC/TC'!\n");
		note = 1;
	}
	if (tc && !(cca || ccb)) {
		LOGP(DNMT, LOGL_NOTICE, "\n");
		LOGP(DNMT, LOGL_NOTICE, "*** Selected channel(s) can be used for traffic only.\n");
		LOGP(DNMT, LOGL_NOTICE, "*** No call to the mobile phone is possible.\n");
		LOGP(DNMT, LOGL_NOTICE, "*** Use one 'CC' or use combined 'CC/TC'!\n");
		note = 1;
	}
	if (cca && !ccb) {
		LOGP(DNMT, LOGL_NOTICE, "\n");
		LOGP(DNMT, LOGL_NOTICE, "*** Selected channel(s) can be used for control of MS type A only.\n");
		LOGP(DNMT, LOGL_NOTICE, "*** No call to the MS type B phone is possible.\n");
		LOGP(DNMT, LOGL_NOTICE, "*** Use one 'CC' instead!\n");
		note = 1;
	}
	if (!cca && ccb) {
		LOGP(DNMT, LOGL_NOTICE, "\n");
		LOGP(DNMT, LOGL_NOTICE, "*** Selected channel(s) can be used for control of MS type B only.\n");
		LOGP(DNMT, LOGL_NOTICE, "*** No call to the MS type A phone is possible.\n");
		LOGP(DNMT, LOGL_NOTICE, "*** Use one 'CC' instead!\n");
		note = 1;
	}
	if (note)
		LOGP(DNMT, LOGL_NOTICE, "\n");
}

/* Destroy transceiver instance and unlink from list. */
void nmt_destroy(sender_t *sender)
{
	nmt_t *nmt = (nmt_t *) sender;

	LOGP(DNMT, LOGL_DEBUG, "Destroying 'NMT' instance for channel = %s.\n", sender->kanal);
	dsp_cleanup_sender(nmt);
	dms_cleanup_sender(nmt);
	sms_cleanup_sender(nmt);
	osmo_timer_del(&nmt->timer);
	sender_destroy(&nmt->sender);
	free(nmt);
}

/* Abort connection towards mobile station by sending idle digits. */
void nmt_go_idle(nmt_t *nmt)
{
	osmo_timer_del(&nmt->timer);
	dms_reset(nmt);
	sms_reset(nmt);

	LOGP_CHAN(DNMT, LOGL_INFO, "Entering IDLE state, sending idle frames on %s.\n", chan_type_long_name(nmt->sysinfo.system, nmt->sysinfo.chan_type));
	nmt->trans = NULL; /* remove transaction before state change, so status is shown correctly */
	nmt_new_state(nmt, STATE_IDLE);
	nmt_set_dsp_mode(nmt, DSP_MODE_FRAME);
	memset(&nmt->dialing, 0, sizeof(nmt->dialing));

#if 0
	/* go active for loopback tests */
	nmt_new_state(nmt, STATE_ACTIVE);
	nmt_set_dsp_mode(nmt, DSP_MODE_AUDIO);
#endif
}

/* release an ongoing connection, this is used by roaming update and release initiated by MTX */
static void nmt_release(nmt_t *nmt)
{
	transaction_t *trans = nmt->trans;

	osmo_timer_del(&nmt->timer);

	LOGP_CHAN(DNMT, LOGL_INFO, "Releasing connection towards mobile station.\n");
	if (trans->callref) {
		LOGP_CHAN(DNMT, LOGL_ERROR, "Callref already set, please fix!\n");
		abort();
	}
	nmt_new_state(nmt, STATE_MT_RELEASE);
	nmt->tx_frame_count = 0;
	nmt_set_dsp_mode(nmt, DSP_MODE_FRAME);
	osmo_timer_schedule(&nmt->timer, RELEASE_TO);
}

/* Enter paging state and transmit phone's number on calling channel */
static void nmt_page(transaction_t *trans, int try)
{
	sender_t *sender;
	nmt_t *nmt;

	LOGP(DNMT, LOGL_INFO, "Entering paging state (try %d), sending call to '%c,%s'.\n", try, trans->subscriber.country, trans->subscriber.number);
	trans->page_try = try;
	osmo_timer_schedule(&trans->timer, PAGING_TO);
	/* page on all CC (CC/TC) */
	for (sender = sender_head; sender; sender = sender->next) {
		nmt = (nmt_t *)sender;
		if (!is_chan_class_cc(nmt->sysinfo.chan_type))
			continue;
		/* page on all idle channels and on channels we previously paged */
		if (nmt->state != STATE_IDLE && nmt->trans != trans)
			continue;
		LOGP(DNMT, LOGL_INFO, "Paging on channel %s.\n", sender->kanal);
		nmt->trans = trans; /* add transaction before state change, so status is shown correctly */
		nmt_new_state(nmt, STATE_MT_PAGING);
		nmt_set_dsp_mode(nmt, DSP_MODE_FRAME);
		nmt->tx_frame_count = 0;
	}
}

static nmt_t *search_free_tc(nmt_t *own)
{
	sender_t *sender;
	nmt_t *nmt, *cc_tc = NULL;

	for (sender = sender_head; sender; sender = sender->next) {
		nmt = (nmt_t *) sender;
		/* if our CC is used, we don't care about busy state,
		 * because it can be used, if it is CC/TC type */
		if (nmt != own && nmt->state != STATE_IDLE)
			continue;
		/* remember combined voice/control/paging channel as second alternative */
		if (nmt->sysinfo.chan_type == CHAN_TYPE_CC_TC)
			cc_tc = nmt;
		else if (is_chan_class_tc(nmt->sysinfo.chan_type))
			return nmt;
	}

	return cc_tc;
}


/*
 * frame matching functions to check if channels is accessed correctly
 */

/* check match channel no, area no and traffic area */
static int match_channel(nmt_t *nmt, frame_t *frame)
{
	int channel, power;
	int rc;

	/* check channel match */
	rc = nmt_decode_channel(nmt->sysinfo.system, frame->channel_no, &channel, &power);
	if (rc < 0) {
		LOGP_CHAN(DNMT, LOGL_NOTICE, "Frame with illegal encoded channel received, ignoring.\n");
		return 0;
	}
	/* in case of interleaved channel, ignore the missing upper bit */
	if ((channel % 1024) != (atoi(nmt->sender.kanal) % 1024)) {
		LOGP_CHAN(DNMT, LOGL_NOTICE, "Frame for different channel %d received, ignoring.\n", channel);
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
		LOGP_CHAN(DNMT, LOGL_NOTICE, "Received area no (%d) does not match the base station's area no (%d), ignoring.\n", area_no, nmt->sysinfo.area_no);
		return 0;
	}

	traffic_area = ((frame->area_info & 0x3) << 4) | frame->traffic_area;
	if (nmt->sysinfo.traffic_area != 0 && (nmt->sysinfo.traffic_area & 0x3f) != traffic_area) {
		LOGP_CHAN(DNMT, LOGL_NOTICE, "Received 6 bits of traffic area (0x%02x) does not match the 6 bits of base station's traffic area (0x%02x), ignoring.\n", nmt->sysinfo.traffic_area & 0x3f, traffic_area);
		return 0;
	}
skip_area:

	return 1;
}

/* check match subscriber number */
static int match_subscriber(transaction_t *trans, frame_t *frame)
{
	if (nmt_digits2value(&trans->subscriber.country, 1) != frame->ms_country) {
		LOGP(DNMT, LOGL_NOTICE, "Received non matching subscriber counrtry, ignoring.\n");
		return 0;
	}
	if (nmt_digits2value(trans->subscriber.number, 6) != frame->ms_number) {
		LOGP(DNMT, LOGL_NOTICE, "Received non matching subscriber number, ignoring.\n");
		return 0;
	}

	return 1;
}

/*
 * helper functions to generate frames
 */

static void tx_ident(nmt_t *nmt, frame_t *frame)
{
	transaction_t *trans = nmt->trans;

	frame->mt = NMT_MESSAGE_3b;
	frame->channel_no = nmt_encode_channel(nmt->sysinfo.system, atoi(nmt->sender.kanal), nmt->sysinfo.ms_power);
	frame->traffic_area = nmt_encode_traffic_area(nmt->sysinfo.system, atoi(nmt->sender.kanal), nmt->sysinfo.traffic_area);
	frame->ms_country = nmt_digits2value(&trans->subscriber.country, 1);
	frame->ms_number = nmt_digits2value(trans->subscriber.number, 6);
	frame->additional_info = nmt_encode_area_no(nmt->sysinfo.area_no);
}

static void set_line_signal(nmt_t *nmt, frame_t *frame, uint8_t signal)
{
	transaction_t *trans = nmt->trans;

	frame->mt = NMT_MESSAGE_5a;
	frame->channel_no = nmt_encode_channel(nmt->sysinfo.system, atoi(nmt->sender.kanal), nmt->sysinfo.ms_power);
	frame->traffic_area = nmt_encode_traffic_area(nmt->sysinfo.system, atoi(nmt->sender.kanal), nmt->sysinfo.traffic_area);
	frame->ms_country = nmt_digits2value(&trans->subscriber.country, 1);
	frame->ms_number = nmt_digits2value(trans->subscriber.number, 6);
	frame->line_signal = (signal << 8) | (signal << 4) | signal;
}

/*
 * handle idle channel
 */

static void tx_idle(nmt_t *nmt, frame_t *frame)
{
	time_t time_sec;
	struct tm *tm;
	uint16_t clock;

	switch (nmt->sysinfo.chan_type) {
	case CHAN_TYPE_CC:
		frame->mt = NMT_MESSAGE_1a;
		break;
	case CHAN_TYPE_CCA:
		frame->mt = NMT_MESSAGE_1a_a;
		break;
	case CHAN_TYPE_CCB:
		frame->mt = NMT_MESSAGE_1a_b;
		break;
	case CHAN_TYPE_TC:
		frame->mt = NMT_MESSAGE_4;
		break;
	case CHAN_TYPE_AC_TC:
		frame->mt = NMT_MESSAGE_4b;
		break;
	case CHAN_TYPE_CC_TC:
		frame->mt = NMT_MESSAGE_1b;
		break;
	case CHAN_TYPE_TEST:
		frame->mt = NMT_MESSAGE_30;
		break;
	}

	frame->channel_no = nmt_encode_channel(nmt->sysinfo.system, atoi(nmt->sender.kanal), nmt->sysinfo.ms_power);
	frame->traffic_area = nmt_encode_traffic_area(nmt->sysinfo.system, atoi(nmt->sender.kanal), nmt->sysinfo.traffic_area);

	/* additional info */
	frame->additional_info = 0;
	if (frame->mt == NMT_MESSAGE_1a || frame->mt == NMT_MESSAGE_1a_a || frame->mt == NMT_MESSAGE_1a_b || frame->mt == NMT_MESSAGE_1b) {
		/* no battery saving, just use group 8 (all phones) with no saving period */
		frame->additional_info |= 0xeb00008000;
		/* phone is allowed to send overdecadic dialing digits */
		frame->additional_info |= 0x0000020000;
		/* no clock on combined CC+TC */
		if (nmt->send_clock && frame->mt != NMT_MESSAGE_1b) {
			/* send battery saving message including clock */
			time_sec = get_time();
			tm = localtime(&time_sec);
			clock = (1 << 11) | (tm->tm_hour << 6) | tm->tm_min;
			/* add clock with flag */
			frame->additional_info |= clock;
		}
	}
	if (frame->mt == NMT_MESSAGE_1b || frame->mt == NMT_MESSAGE_4 || frame->mt == NMT_MESSAGE_4b || frame->mt == NMT_MESSAGE_30) {
		/* sent area info on traffic channels; it is always H8H9H10, because all IEs are aligned 'to the right' */
		frame->additional_info |= nmt_encode_area_no(nmt->sysinfo.area_no);
	}
}

static void rx_idle(nmt_t *nmt, frame_t *frame)
{
	transaction_t *trans;
	nmt_subscriber_t subscr;

	switch (frame->mt) {
	case NMT_MESSAGE_11a: /* roaming update and seizure */
		if (!match_channel(nmt, frame))
			break;
		if (!match_area(nmt, frame))
			break;
		/* set subscriber */
		memset(&subscr, 0, sizeof(subscr));
		nmt_value2digits(frame->ms_country, &subscr.country, 1);
		nmt_value2digits(frame->ms_number, subscr.number, 6);

		LOGP_CHAN(DNMT, LOGL_INFO, "Received roaming seizure from subscriber %c,%s\n", subscr.country, subscr.number);

		/* create transaction */
		trans = create_transaction(&subscr);
		if (!trans) {
			LOGP(DNMT, LOGL_NOTICE, "Failed to create transaction!\n");
			break;
		}

		/* change state */
		nmt->trans = trans; /* add transaction before state change, so status is shown correctly */
		nmt_new_state(nmt, STATE_ROAMING_IDENT);
		trans->nmt = nmt;
		nmt->rx_frame_count = 0;
		nmt->tx_frame_count = 0;
		break;
	case NMT_MESSAGE_10b: /* seizure from ordinary MS */
	case NMT_MESSAGE_12: /* seizure from coinbox MS */
	case NMT_MESSAGE_10a: /* access signal */
		if (!match_channel(nmt, frame))
			break;
		if (!match_area(nmt, frame))
			break;
		/* set subscriber */
		memset(&subscr, 0, sizeof(subscr));
		nmt_value2digits(frame->ms_country, &subscr.country, 1);
		nmt_value2digits(frame->ms_number, subscr.number, 6);
		if (frame->mt == NMT_MESSAGE_12)
			subscr.coinbox = 1;

		LOGP_CHAN(DNMT, LOGL_INFO, "Received call from subscriber %c,%s%s\n", subscr.country, subscr.number, (subscr.coinbox) ? " (coinbox)" : "");

		/* create transaction */
		trans = create_transaction(&subscr);
		if (!trans) {
			LOGP(DNMT, LOGL_NOTICE, "Failed to create transaction!\n");
			break;
		}

		/* change state */
		nmt->trans = trans; /* add transaction before state change, so status is shown correctly */
		nmt_new_state(nmt, STATE_MO_IDENT);
		trans->nmt = nmt;
		nmt->rx_frame_count = 0;
		nmt->tx_frame_count = 0;
		break;
	/* signals after release */
	case NMT_MESSAGE_13a: /* line signal */
		break;
	default:
		LOGP_CHAN(DNMT, LOGL_DEBUG, "Dropping message %s in state %s\n", nmt_frame_name(frame->mt), nmt_state_name(nmt->state));
	}
}

/*
 * handle roaming
 */

static void tx_roaming_ident(nmt_t *nmt, frame_t *frame)
{
	if (++nmt->tx_frame_count == 1)
		LOGP_CHAN(DNMT, LOGL_INFO, "Sending identity request.\n");
	tx_ident(nmt, frame);
	if (nmt->tx_frame_count == 8) {
		LOGP_CHAN(DNMT, LOGL_NOTICE, "Timeout waiting for identity reply\n");
		nmt_release(nmt);
	}
}

static void rx_roaming_ident(nmt_t *nmt, frame_t *frame)
{
	transaction_t *trans = nmt->trans;

	switch (frame->mt) {
	case NMT_MESSAGE_11a: /* roaming update */
		if (!match_channel(nmt, frame))
			break;
		if (!match_area(nmt, frame))
			break;
		if (!match_subscriber(trans, frame))
			break;
		if (nmt->rx_frame_count < 2) {
			LOGP_CHAN(DNMT, LOGL_DEBUG, "Skipping second seizure frame\n");
			break;
		}
		nmt_value2digits(frame->ms_password, trans->subscriber.password, 3);
		LOGP_CHAN(DNMT, LOGL_INFO, "Received identity confirm (password %s).\n", trans->subscriber.password);
		nmt_new_state(nmt, STATE_ROAMING_CONFIRM);
		nmt->tx_frame_count = 0;
		console_inscription(&trans->subscriber.country);
		break;
	default:
		LOGP_CHAN(DNMT, LOGL_DEBUG, "Dropping message %s in state %s\n", nmt_frame_name(frame->mt), nmt_state_name(nmt->state));
	}
}

static void tx_roaming_confirm(nmt_t *nmt, frame_t *frame)
{
	set_line_signal(nmt, frame, 3);
	if (++nmt->tx_frame_count == 1)
		LOGP_CHAN(DNMT, LOGL_INFO, "Send 'Roaming updating confirmation'.\n");
	if (nmt->tx_frame_count == 2)
		nmt_release(nmt); /* continue with this frame, then release */
}

static void rx_roaming_confirm(nmt_t *nmt, frame_t *frame)
{
	switch (frame->mt) {
	default:
		LOGP_CHAN(DNMT, LOGL_DEBUG, "Dropping message %s in state %s\n", nmt_frame_name(frame->mt), nmt_state_name(nmt->state));
	}
}

/*
 * handle call MS -> MTX
 */

static void tx_mo_ident(nmt_t *nmt, frame_t *frame)
{
	if (++nmt->tx_frame_count == 1)
		LOGP_CHAN(DNMT, LOGL_INFO, "Sending identity request.\n");
	tx_ident(nmt, frame);
	if (nmt->tx_frame_count == 8) {
		LOGP_CHAN(DNMT, LOGL_NOTICE, "Timeout waiting for identity reply\n");
		nmt_release(nmt);
	}
}

static void rx_mo_ident(nmt_t *nmt, frame_t *frame)
{
	transaction_t *trans = nmt->trans;

	switch (frame->mt) {
	case NMT_MESSAGE_10b: /* seizure */
	case NMT_MESSAGE_12: /* seizure */
		if (!match_channel(nmt, frame))
			break;
		if (!match_subscriber(trans, frame))
			break;
		if (nmt->rx_frame_count < 2) {
			LOGP_CHAN(DNMT, LOGL_DEBUG, "Skipping second seizure frame\n");
			break;
		}
		nmt_value2digits(frame->ms_password, trans->subscriber.password, 3);
		LOGP_CHAN(DNMT, LOGL_INFO, "Received identity confirm (password %s).\n", trans->subscriber.password);
		nmt_new_state(nmt, STATE_MO_CONFIRM);
		nmt->tx_frame_count = 0;
		break;
	default:
		LOGP_CHAN(DNMT, LOGL_DEBUG, "Dropping message %s in state %s\n", nmt_frame_name(frame->mt), nmt_state_name(nmt->state));
	}
}

static void tx_mo_confirm(nmt_t *nmt, frame_t *frame)
{
	set_line_signal(nmt, frame, 3);
	if (++nmt->tx_frame_count <= 2) {
		if (nmt->tx_frame_count == 1)
			LOGP_CHAN(DNMT, LOGL_INFO, "Send 'Proceed to send'.\n");
	} else {
		if (nmt->tx_frame_count == 3) {
			LOGP_CHAN(DNMT, LOGL_INFO, "Send dial tone.\n");
			nmt_new_state(nmt, STATE_MO_DIALING);
			nmt_set_dsp_mode(nmt, DSP_MODE_DIALTONE);
			osmo_timer_schedule(&nmt->timer, DIALING_TO);
		}
	}
}

static void rx_mo_dialing(nmt_t *nmt, frame_t *frame)
{
	int len = strlen(nmt->dialing);
	transaction_t *trans = nmt->trans;

	switch (frame->mt) {
	case NMT_MESSAGE_14a: /* digits */
		if (!match_channel(nmt, frame))
			break;
		if (!match_area(nmt, frame))
			break;
		if (!match_subscriber(trans, frame))
			break;
		osmo_timer_schedule(&nmt->timer, DIALING_TO);
		/* max digits received */
		if (len + 1 == sizeof(nmt->dialing))
			break;
		if ((len & 1)) {
			/* received odd digit, but be already have odd number of digits */
			if (nmt->rx_frame_count > 1) /* we lost even digit */
				goto missing_digit;
			break;
		} else if (len) { /* complain only after first digit */
			/* received odd digit, and we have even number of digits */
			if (nmt->rx_frame_count > 3) /* we lost even digit */
				goto missing_digit;
		}
		if ((frame->digit >> 12) != 0x00) /* digit 0x0 0x0, x, x, x */
			goto not_right_position;
		if (((frame->digit >> 8) & 0xf) != ((frame->digit >> 4) & 0xf)
		 || ((frame->digit >> 4) & 0xf) != (frame->digit & 0xf))
			goto not_consistent_digit;
		nmt->dialing[len] = nmt_value2digit(frame->digit);
		nmt->dialing[len + 1] = '\0';
		LOGP_CHAN(DNMT, LOGL_INFO, "Received (odd)  digit %c.\n", nmt->dialing[len]);
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
		if (!match_subscriber(trans, frame))
			break;
		osmo_timer_schedule(&nmt->timer, DIALING_TO);
		/* max digits received */
		if (len + 1 == sizeof(nmt->dialing))
			break;
		/* received even digit, but no digit yet, so we lost first odd digit */
		if (!len)
			goto missing_digit;
		if (!(len & 1)) {
			/* received even digit, but be already have even number of digits */
			if (nmt->rx_frame_count > 1) /* we lost odd digit */
				goto missing_digit;
			break;
		} else {
			/* received even digit, and we have odd number of digits */
			if (nmt->rx_frame_count > 3) /* we lost odd digit */
				goto missing_digit;
		}
		if ((frame->digit >> 12) != 0xff) /* digit 0xf 0xf, x, x, x */
			goto not_right_position;
		if (((frame->digit >> 8) & 0xf) != ((frame->digit >> 4) & 0xf)
		 || ((frame->digit >> 4) & 0xf) != (frame->digit & 0xf))
			goto not_consistent_digit;
		nmt->dialing[len] = nmt_value2digit(frame->digit);
		nmt->dialing[len + 1] = '\0';
		LOGP_CHAN(DNMT, LOGL_INFO, "Received (even) digit %c.\n", nmt->dialing[len]);
		nmt->rx_frame_count = 0;
		break;
	case NMT_MESSAGE_15: /* idle */
		if (!len)
			break;
		if (nmt->dialing[0] == 'A') {
			nmt->dialing[0] = '+';
			LOGP_CHAN(DNMT, LOGL_INFO, "Dialing includes international '+' sign at the beginning.\n");
		}
		if (nmt->dialing[0] == 'B') {
			const char *code = NULL;
			switch (nmt->dialing[1]) {
			case '1':
				code = "general emergency number";
				break;
			case '2':
				code = "fire alarm";
				break;
			case '3':
				code = "police";
				break;
			case '4':
				code = "ambulance";
				break;
			case '5':
				code = "gas emergency";
				break;
			case '6':
				code = "directory inquiry (national)";
				break;
			case '7':
				code = "directory inquiry (international)";
				break;
			case '8':
				code = "operator assisted service (to make outgoing calls)";
				break;
			case '9':
				code = "local customer care";
				break;
			case 'B':
				code = "road service";
				break;
			case 'C':
				code = "weather";
				break;
			}
			if (code)
				LOGP_CHAN(DNMT, LOGL_INFO, "Dialing includes service code: '%c%c' = '%s'\n", nmt->dialing[0], nmt->dialing[1], code);
		}
		LOGP_CHAN(DNMT, LOGL_INFO, "Dialing complete %s->%s, call established.\n", &trans->subscriber.country, nmt->dialing);
		if (nmt->dialing[0] == 'B')
			nmt->dialing[0] = '+';
		/* setup call */
		if (!strcmp(nmt->dialing, nmt->smsc_number)) {
			/* SMS */
			LOGP(DNMT, LOGL_INFO, "Setup call to SMSC.\n");
			trans->dms_call = 1;
		} else {
			LOGP(DNMT, LOGL_INFO, "Setup call to network.\n");
			trans->callref = call_up_setup(&trans->subscriber.country, nmt->dialing, OSMO_CC_NETWORK_NMT_NONE, "");
		}
		osmo_timer_del(&nmt->timer);
		nmt_new_state(nmt, STATE_MO_COMPLETE);
		nmt_set_dsp_mode(nmt, DSP_MODE_FRAME);
		nmt->tx_frame_count = 0;
		break;
	default:
		LOGP_CHAN(DNMT, LOGL_DEBUG, "Dropping message %s in state %s\n", nmt_frame_name(frame->mt), nmt_state_name(nmt->state));
	}

	return;

missing_digit:
	LOGP_CHAN(DNMT, LOGL_NOTICE, "Missing digit, aborting.\n");
	nmt_release(nmt);
	return;

not_right_position:
	LOGP_CHAN(DNMT, LOGL_NOTICE, "Position information of digit does not match, ignoring due to corrupt frame.\n");
	return;

not_consistent_digit:
	LOGP_CHAN(DNMT, LOGL_NOTICE, "Digit repetition in frame does not match, ignoring due to corrupt frame.\n");
	return;
}

static void tx_mo_complete(nmt_t *nmt, frame_t *frame)
{
	transaction_t *trans = nmt->trans;

	if (++nmt->tx_frame_count <= 4) {
		set_line_signal(nmt, frame, 6);
		if (nmt->tx_frame_count == 1)
			LOGP_CHAN(DNMT, LOGL_INFO, "Send 'address complete'.\n");
	} else {
		if (nmt->compandor) {
			set_line_signal(nmt, frame, 5);
			if (nmt->tx_frame_count == 5)
				LOGP_CHAN(DNMT, LOGL_INFO, "Send 'compandor in'.\n");
		} else
			frame->mt = NMT_MESSAGE_6;
		if (nmt->tx_frame_count == 9) {
			LOGP_CHAN(DNMT, LOGL_INFO, "Connect audio.\n");
			nmt_new_state(nmt, STATE_ACTIVE);
			nmt->active_state = ACTIVE_STATE_VOICE;
			nmt_set_dsp_mode(nmt, DSP_MODE_AUDIO);
			if (nmt->supervisory && !trans->dms_call) {
				super_reset(nmt);
				osmo_timer_schedule(&nmt->timer, SUPERVISORY_TO1,0);
			}
		}
	}
}

static void timeout_mo_dialing(nmt_t *nmt)
{
	LOGP_CHAN(DNMT, LOGL_NOTICE, "Timeout while receiving digits.\n");
	nmt_release(nmt);
	LOGP(DNMT, LOGL_INFO, "Release call towards network.\n");
}

/*
 * handle call MTX -> MS
 */
static void tx_mt_paging(nmt_t *nmt, frame_t *frame)
{
	transaction_t *trans = nmt->trans;

	frame->mt = NMT_MESSAGE_2a;
	frame->channel_no = nmt_encode_channel(nmt->sysinfo.system, atoi(nmt->sender.kanal), nmt->sysinfo.ms_power);
	frame->traffic_area = nmt_encode_traffic_area(nmt->sysinfo.system, atoi(nmt->sender.kanal), nmt->sysinfo.traffic_area);
	frame->ms_country = nmt_digits2value(&trans->subscriber.country, 1);
	frame->ms_number = nmt_digits2value(trans->subscriber.number, 6);
	frame->additional_info = nmt_encode_area_no(nmt->sysinfo.area_no);
	if (++nmt->tx_frame_count == 1) {
		LOGP_CHAN(DNMT, LOGL_INFO, "Send call to mobile.\n");
	} else
		tx_idle(nmt, frame);
}

void timeout_mt_paging(transaction_t *trans)
{
	LOGP(DNMT, LOGL_NOTICE, "No answer from mobile phone (try %d).\n", trans->page_try);
	if (trans->page_try == PAGE_TRIES) {
		LOGP(DNMT, LOGL_INFO, "Release call towards network.\n");
		call_up_release(trans->callref, CAUSE_OUTOFORDER);
		destroy_transaction(trans);
		return;
	}
	nmt_page(trans, trans->page_try + 1);
}

static void rx_mt_paging(nmt_t *nmt, frame_t *frame)
{
	transaction_t *trans = nmt->trans;
	sender_t *sender;
	nmt_t *other;

	switch (frame->mt) {
	case NMT_MESSAGE_10a: /* call acknowledgment */
	case NMT_MESSAGE_10d: /* call ack on alternate type */
		if (!match_channel(nmt, frame))
			break;
		if (!match_subscriber(trans, frame))
			break;
		LOGP_CHAN(DNMT, LOGL_INFO, "Received call acknowledgment from subscriber %c,%s.\n", trans->subscriber.country, trans->subscriber.number);
		if (trans->sms_string[0])
			trans->dms_call = 1;
		osmo_timer_del(&trans->timer);
		nmt_new_state(nmt, STATE_MT_CHANNEL);
		trans->nmt = nmt;
		nmt->tx_frame_count = 0;
		/* release other channels */
		for (sender = sender_head; sender; sender = sender->next) {
			other = (nmt_t *)sender;
			if (other == nmt)
				continue;
			if (other->trans == trans)
				nmt_go_idle(other);
		}
		break;
	default:
		LOGP_CHAN(DNMT, LOGL_DEBUG, "Dropping message %s in state %s\n", nmt_frame_name(frame->mt), nmt_state_name(nmt->state));
	}
}

static void tx_mt_channel(nmt_t *nmt, frame_t *frame)
{
	transaction_t *trans = nmt->trans;
	nmt_t *tc;

	/* get free channel (after releasing all channels) */
	tc = search_free_tc(nmt);
	if (!tc) {
		LOGP_CHAN(DNMT, LOGL_NOTICE, "TC is not free anymore.\n");
		LOGP(DNMT, LOGL_INFO, "Release call towards network.\n");
		call_up_release(trans->callref, CAUSE_NOCHANNEL);
		trans->callref = 0;
		nmt_release(nmt);
		/* send idle for now, then continue with release */
		tx_idle(nmt, frame);
		return;
	}

	if (nmt != tc) {
		/* link trans and tc together, so we can continue with channel assignment */
		LOGP_CHAN(DNMT, LOGL_NOTICE, "Switching to TC channel #%s.\n", tc->sender.kanal);
		nmt_go_idle(nmt);
		tc->trans = trans;
		trans->nmt = tc;
	} else
		LOGP_CHAN(DNMT, LOGL_NOTICE, "Staying on CC/TC channel #%s.\n", tc->sender.kanal);
	nmt_new_state(tc, STATE_MT_IDENT);
	tc->tx_frame_count = 0;

	/* assign channel on 'nmt' to 'tc' */
	frame->mt = NMT_MESSAGE_2b;
	frame->channel_no = nmt_encode_channel(nmt->sysinfo.system, atoi(nmt->sender.kanal), nmt->sysinfo.ms_power);
	frame->traffic_area = nmt_encode_traffic_area(nmt->sysinfo.system, atoi(nmt->sender.kanal), nmt->sysinfo.traffic_area);
	frame->ms_country = nmt_digits2value(&trans->subscriber.country, 1);
	frame->ms_number = nmt_digits2value(trans->subscriber.number, 6);
	frame->tc_no = nmt_encode_tc(tc->sysinfo.system, atoi(tc->sender.kanal), tc->sysinfo.ms_power);
	LOGP_CHAN(DNMT, LOGL_INFO, "Send channel activation to mobile.\n");
}

static void tx_mt_ident(nmt_t *nmt, frame_t *frame)
{
	transaction_t *trans = nmt->trans;

	if (++nmt->tx_frame_count == 1)
		LOGP_CHAN(DNMT, LOGL_INFO, "Sending identity request.\n");
	tx_ident(nmt, frame);
	if (nmt->tx_frame_count == 8) {
		LOGP_CHAN(DNMT, LOGL_NOTICE, "Timeout waiting for identity reply\n");
		LOGP_CHAN(DNMT, LOGL_INFO, "Release call towards network.\n");
		call_up_release(trans->callref, CAUSE_TEMPFAIL);
		destroy_transaction(trans);
	}
}

static void rx_mt_ident(nmt_t *nmt, frame_t *frame)
{
	transaction_t *trans = nmt->trans;

	switch (frame->mt) {
	case NMT_MESSAGE_10b: /* seizure */
		if (!match_subscriber(trans, frame))
			break;
		nmt_value2digits(frame->ms_password, trans->subscriber.password, 3);
		LOGP_CHAN(DNMT, LOGL_INFO, "Received identity (password %s).\n", trans->subscriber.password);
		if (trans->dms_call) {
			nmt_new_state(nmt, STATE_MT_AUTOANSWER);
			nmt->wait_autoanswer = 1;
			nmt->tx_frame_count = 0;
		} else {
			nmt_new_state(nmt, STATE_MT_RINGING);
			/* start with caller ID before ringing */
			if (nmt->send_callerid) {
				nmt->tx_frame_count = 4;
				nmt->tx_callerid_count = 1;
			} else {
				nmt->tx_frame_count = 0;
				nmt->tx_callerid_count = 0;
			}
			osmo_timer_schedule(&nmt->timer, RINGING_TO);
			call_up_alerting(trans->callref);
		}
		break;
	default:
		LOGP_CHAN(DNMT, LOGL_DEBUG, "Dropping message %s in state %s\n", nmt_frame_name(frame->mt), nmt_state_name(nmt->state));
	}
}

static void tx_mt_autoanswer(nmt_t *nmt, frame_t *frame)
{
	transaction_t *trans = nmt->trans;

	/* first we need to wait for autoanswer */
	if (nmt->wait_autoanswer) {
		frame->mt = NMT_MESSAGE_6;
		return;
	}
	if (++nmt->tx_frame_count == 1)
		LOGP_CHAN(DNMT, LOGL_INFO, "Send 'autoanswer order'.\n");
	set_line_signal(nmt, frame, 12);
	if (nmt->tx_frame_count == 4) {
		LOGP_CHAN(DNMT, LOGL_INFO, "No reaction to autoanswer, proceed with ringing.\n");
		nmt_new_state(nmt, STATE_MT_RINGING);
		nmt->tx_frame_count = 0;
		nmt->tx_callerid_count = 0;
		osmo_timer_schedule(&nmt->timer, RINGING_TO);
		call_up_alerting(trans->callref);
	}
}

static void rx_mt_autoanswer(nmt_t *nmt, frame_t *frame)
{
	transaction_t *trans = nmt->trans;

	switch (frame->mt) {
	case NMT_MESSAGE_15: /* idle */
		nmt->wait_autoanswer = 0;
		break;
	case NMT_MESSAGE_13a: /* line signal */
		if (!match_channel(nmt, frame))
			break;
		if (!match_subscriber(trans, frame))
			break;
		if (((frame->line_signal >> 16) & 0xf) != ((frame->line_signal >> 12) & 0xf)
		 || ((frame->line_signal >> 12) & 0xf) != ((frame->line_signal >> 8) & 0xf)
		 || ((frame->line_signal >> 8) & 0xf) != ((frame->line_signal >> 4) & 0xf)
		 || ((frame->line_signal >> 4) & 0xf) != (frame->line_signal & 0xf)) {
			LOGP_CHAN(DNMT, LOGL_NOTICE, "Line signal repetition in frame does not match, ignoring due to corrupt frame.\n");
			break;
		}
		if ((frame->line_signal & 0xf) != 12)
			break;
		LOGP_CHAN(DNMT, LOGL_INFO, "Received acknowledge to autoanswer.\n");
		nmt_new_state(nmt, STATE_MT_COMPLETE);
		nmt->tx_frame_count = 0;
		call_up_answer(trans->callref, &trans->subscriber.country);
		break;
	default:
		LOGP_CHAN(DNMT, LOGL_DEBUG, "Dropping message %s in state %s\n", nmt_frame_name(frame->mt), nmt_state_name(nmt->state));
	}
}

static void tx_mt_ringing(nmt_t *nmt, frame_t *frame)
{
	transaction_t *trans = nmt->trans;

	set_line_signal(nmt, frame, 9);
	if (++nmt->tx_frame_count == 1)
		LOGP_CHAN(DNMT, LOGL_INFO, "Send 'ringing order'.\n");
	if (nmt->tx_frame_count >= 4) {
		if (nmt->tx_callerid_count) {
			if (nmt->tx_frame_count == 5)
				LOGP_CHAN(DNMT, LOGL_INFO, "Send 'A-number'.\n");
			nmt_encode_a_number(frame, nmt->tx_frame_count - 4, trans->caller_type, trans->caller_id, nmt->sysinfo.system, atoi(nmt->sender.kanal), nmt->sysinfo.ms_power, nmt->sysinfo.traffic_area);
		} else
			frame->mt = NMT_MESSAGE_6;
	}
	if (nmt->tx_callerid_count == 1) {
		/* start ringing after first caller ID of 6 frames */
		if (nmt->tx_frame_count == 10) {
			nmt->tx_frame_count = 0;
			nmt->tx_callerid_count++;
		}
	} else {
		/* repeat ringing after 5 seconds */
		if (nmt->tx_frame_count == 36) {
			nmt->tx_frame_count = 0;
		}
	}
}

static void rx_mt_ringing(nmt_t *nmt, frame_t *frame)
{
	transaction_t *trans = nmt->trans;

	switch (frame->mt) {
	case NMT_MESSAGE_13a: /* line signal */
		if (!match_channel(nmt, frame))
			break;
		if (!match_subscriber(trans, frame))
			break;
		if (((frame->line_signal >> 16) & 0xf) != ((frame->line_signal >> 12) & 0xf)
		 || ((frame->line_signal >> 12) & 0xf) != ((frame->line_signal >> 8) & 0xf)
		 || ((frame->line_signal >> 8) & 0xf) != ((frame->line_signal >> 4) & 0xf)
		 || ((frame->line_signal >> 4) & 0xf) != (frame->line_signal & 0xf)) {
			LOGP_CHAN(DNMT, LOGL_NOTICE, "Line signal repetition in frame does not match, ignoring due to corrupt frame.\n");
			break;
		}
		if ((frame->line_signal & 0xf) != 14)
			break;
		LOGP_CHAN(DNMT, LOGL_INFO, "Received 'answer' from phone.\n");
		nmt_new_state(nmt, STATE_MT_COMPLETE);
		nmt->tx_frame_count = 0;
		osmo_timer_del(&nmt->timer);
		call_up_answer(trans->callref, &trans->subscriber.country);
		break;
	default:
		LOGP_CHAN(DNMT, LOGL_DEBUG, "Dropping message %s in state %s\n", nmt_frame_name(frame->mt), nmt_state_name(nmt->state));
	}
}

static void tx_mt_complete(nmt_t *nmt, frame_t *frame)
{
	transaction_t *trans = nmt->trans;

	++nmt->tx_frame_count;
	if (nmt->compandor && !trans->dms_call) {
		if (nmt->tx_frame_count == 1)
			LOGP_CHAN(DNMT, LOGL_INFO, "Send 'compandor in'.\n");
		set_line_signal(nmt, frame, 5);
	} else
		frame->mt = NMT_MESSAGE_6;
	if (nmt->tx_frame_count == 5) {
		LOGP_CHAN(DNMT, LOGL_INFO, "Connect audio.\n");
		nmt_new_state(nmt, STATE_ACTIVE);
		nmt->active_state = ACTIVE_STATE_VOICE;
		nmt_set_dsp_mode(nmt, DSP_MODE_AUDIO);
		if (nmt->supervisory && !trans->dms_call) {
			super_reset(nmt);
			osmo_timer_schedule(&nmt->timer, SUPERVISORY_TO1,0);
		}
		if (trans->dms_call) {
			time_t ti = time(NULL);
			sms_deliver(nmt, sms_ref, trans->caller_id, trans->caller_type, SMS_PLAN_ISDN_TEL, ti, 1, trans->sms_string);
		}
	}
}

static void timeout_mt_ringing(nmt_t *nmt)
{
	transaction_t *trans = nmt->trans;

	LOGP_CHAN(DNMT, LOGL_NOTICE, "Timeout while waiting for answer of the phone.\n");
	LOGP(DNMT, LOGL_INFO, "Release call towards network.\n");
	call_up_release(trans->callref, CAUSE_NOANSWER);
	trans->callref = 0;
	nmt_release(nmt);
}

/*
 * handle clearing towards MTX
 */

static void tx_mo_release(nmt_t *nmt, frame_t *frame)
{
	transaction_t *trans = nmt->trans;

	set_line_signal(nmt, frame, 15);
	if (++nmt->tx_frame_count == 1)
		LOGP_CHAN(DNMT, LOGL_INFO, "Send release.\n");
	if (nmt->tx_frame_count == 4)
		destroy_transaction(trans); /* continue with this frame, then go idle */
}

/*
 * handle clearing towards MS
 */

static void tx_mt_release(nmt_t *nmt, frame_t *frame)
{
	set_line_signal(nmt, frame, 15);
	if (++nmt->tx_frame_count == 1)
		LOGP_CHAN(DNMT, LOGL_INFO, "Send release.\n");
}

static void rx_mt_release(nmt_t *nmt, frame_t *frame)
{
	transaction_t *trans = nmt->trans;

	switch (frame->mt) {
	case NMT_MESSAGE_13a: /* line signal */
		if (!match_channel(nmt, frame))
			break;
		if (!match_subscriber(trans, frame))
			break;
		if (((frame->line_signal >> 16) & 0xf) != ((frame->line_signal >> 12) & 0xf)
		 || ((frame->line_signal >> 12) & 0xf) != ((frame->line_signal >> 8) & 0xf)
		 || ((frame->line_signal >> 8) & 0xf) != ((frame->line_signal >> 4) & 0xf)
		 || ((frame->line_signal >> 4) & 0xf) != (frame->line_signal & 0xf)) {
			LOGP_CHAN(DNMT, LOGL_NOTICE, "Line signal repetition in frame does not match, ignoring due to corrupt frame.\n");
			break;
		}
		if ((frame->line_signal & 0xf) != 1)
			break;
		LOGP_CHAN(DNMT, LOGL_INFO, "Received release guard.\n");
		osmo_timer_del(&nmt->timer);
		destroy_transaction(trans);
		break;
	default:
		LOGP_CHAN(DNMT, LOGL_DEBUG, "Dropping message %s in state %s\n", nmt_frame_name(frame->mt), nmt_state_name(nmt->state));
	}
}

static void timeout_mt_release(nmt_t *nmt)
{
	transaction_t *trans = nmt->trans;

	LOGP_CHAN(DNMT, LOGL_NOTICE, "Timeout while releasing.\n");
	destroy_transaction(trans);
}

/*
 * handle call
 */

void nmt_rx_super(nmt_t *nmt, int tone, double quality)
{
	if (tone)
		LOGP_CHAN(DNMT, LOGL_INFO, "Detected supervisory signal with quality=%.0f.\n", quality * 100.0);
	else
		LOGP_CHAN(DNMT, LOGL_INFO, "Lost supervisory signal\n");

	if (nmt->sender.loopback)
		return;

	/* only detect supervisory signal during active call */
	if (nmt->state != STATE_ACTIVE || !nmt->supervisory)
		return;

	if (tone)
		osmo_timer_del(&nmt->timer);
	else
		osmo_timer_schedule(&nmt->timer, SUPERVISORY_TO2);
}

static void timeout_active(nmt_t *nmt, int duration)
{
	transaction_t *trans = nmt->trans;

	if (duration == SUPERVISORY_TO1)
		LOGP_CHAN(DNMT, LOGL_NOTICE, "Timeout after %d seconds not receiving supervisory signal.\n", duration);
	else
		LOGP_CHAN(DNMT, LOGL_NOTICE, "Timeout after %d seconds loosing supervisory signal.\n", duration);
	LOGP_CHAN(DNMT, LOGL_INFO, "Release call towards network.\n");
	call_up_release(trans->callref, CAUSE_TEMPFAIL);
	trans->callref = 0;
	nmt_release(nmt);
}

static void rx_active(nmt_t *nmt, frame_t *frame)
{
	transaction_t *trans = nmt->trans;
	char digit;

	/* restart timer on every reception of frame */
	if (nmt->supervisory)
		osmo_timer_schedule(&nmt->timer, SUPERVISORY_TO2);

	switch (frame->mt) {
	case NMT_MESSAGE_13a: /* line signal */
		if (!match_channel(nmt, frame))
			break;
		if (!match_subscriber(trans, frame))
			break;
		if (((frame->line_signal >> 16) & 0xf) != ((frame->line_signal >> 12) & 0xf)
		 || ((frame->line_signal >> 12) & 0xf) != ((frame->line_signal >> 8) & 0xf)
		 || ((frame->line_signal >> 8) & 0xf) != ((frame->line_signal >> 4) & 0xf)
		 || ((frame->line_signal >> 4) & 0xf) != (frame->line_signal & 0xf)) {
			LOGP_CHAN(DNMT, LOGL_NOTICE, "Line signal repetition in frame does not match, ignoring due to corrupt frame.\n");
			break;
		}
		switch ((frame->line_signal & 0xf)) {
		case 5:
			LOGP_CHAN(DNMT, LOGL_NOTICE, "Register Recall is not supported.\n");
			break;
		case 8:
			if (nmt->active_state != ACTIVE_STATE_VOICE)
				break;
			LOGP_CHAN(DNMT, LOGL_INFO, "Received 'MFT in' request.\n");
			nmt->active_state = ACTIVE_STATE_MFT_IN;
			nmt->tx_frame_count = 0;
			nmt_set_dsp_mode(nmt, DSP_MODE_FRAME);
			nmt->mft_num = 0;
			break;
		case 7:
			if (nmt->active_state != ACTIVE_STATE_MFT)
				break;
			LOGP_CHAN(DNMT, LOGL_INFO, "Received 'MFT out' request.\n");
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
		if ((frame->digit >> 12) != 0x00) {
			LOGP_CHAN(DNMT, LOGL_NOTICE, "Position information of digit does not match, ignoring due to corrupt frame.\n");
			break;
		}
		if (((frame->digit >> 8) & 0xf) != ((frame->digit >> 4) & 0xf)
		 || ((frame->digit >> 4) & 0xf) != (frame->digit & 0xf)) {
			LOGP_CHAN(DNMT, LOGL_NOTICE, "Digit repetition in frame does not match, ignoring due to corrupt frame.\n");
			break;
		}
		digit = nmt_value2digit(frame->digit);
		dtmf_encode_set_tone(&nmt->dtmf, digit, DTMF_DURATION, 0.0);
		LOGP_CHAN(DNMT, LOGL_INFO, "Received (odd)  digit %c.\n", digit);
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
		if ((frame->digit >> 12) != 0xff) {
			LOGP_CHAN(DNMT, LOGL_NOTICE, "Position information of digit does not match, ignoring due to corrupt frame.\n");
			break;
		}
		if (((frame->digit >> 8) & 0xf) != ((frame->digit >> 4) & 0xf)
		 || ((frame->digit >> 4) & 0xf) != (frame->digit & 0xf)) {
			LOGP_CHAN(DNMT, LOGL_NOTICE, "Digit repetition in frame does not match, ignoring due to corrupt frame.\n");
			break;
		}
		digit = nmt_value2digit(frame->digit);
		dtmf_encode_set_tone(&nmt->dtmf, digit, DTMF_DURATION, 0.0);
		LOGP_CHAN(DNMT, LOGL_INFO, "Received (even) digit %c.\n", digit);
		nmt->mft_num++;
		break;
	default:
		LOGP_CHAN(DNMT, LOGL_DEBUG, "Dropping message %s in state %s\n", nmt_frame_name(frame->mt), nmt_state_name(nmt->state));
	}
}

static void tx_active(nmt_t *nmt, frame_t *frame)
{
	switch (nmt->active_state) {
	case ACTIVE_STATE_MFT_IN:
		set_line_signal(nmt, frame, 4);
		if (++nmt->tx_frame_count == 1)
			LOGP_CHAN(DNMT, LOGL_INFO, "Send 'MFT in acknowledge'.\n");
		if (nmt->tx_frame_count > 4) {
			nmt->active_state = ACTIVE_STATE_MFT;
			nmt_set_dsp_mode(nmt, DSP_MODE_DTMF);
		}
		break;
	case ACTIVE_STATE_MFT_OUT:
		set_line_signal(nmt, frame, 10);
		if (++nmt->tx_frame_count == 1)
			LOGP_CHAN(DNMT, LOGL_INFO, "Send 'MFT out acknowledge'.\n");
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

void nmt_receive_frame(nmt_t *nmt, const char *bits, double quality, double level, int frames_elapsed)
{
	frame_t frame;
	int rc;

	LOGP_CHAN(DDSP, LOGL_INFO, "RX Level: %.0f%% Quality=%.0f%%\n", level * 100.0, quality * 100.0);

	rc = decode_frame(nmt->sysinfo.system, &frame, bits, (nmt->sender.loopback) ? MTX_TO_XX : XX_TO_MTX, (nmt->state == STATE_MT_PAGING));
	if (rc < 0) {
		LOGP_CHAN(DNMT, (nmt->sender.loopback) ? LOGL_NOTICE : LOGL_DEBUG, "Received invalid frame.\n");
		return;
	}

	/* frame counter */
	nmt->rx_frame_count += frames_elapsed;

	LOGP_CHAN(DNMT, (nmt->sender.loopback) ? LOGL_NOTICE : LOGL_DEBUG, "Received frame %s\n", nmt_frame_name(frame.mt));

	if (nmt->sender.loopback)
		return;

	/* MS releases, but this is not the acknowledge of MTX release */
	if (frame.mt == NMT_MESSAGE_13a
	 && (frame.line_signal & 0xf) == 1
	 && nmt->state != STATE_MO_RELEASE
	 && nmt->state != STATE_MT_RELEASE) {
	 	/* drop packets after release */
	 	if (nmt->state == STATE_IDLE)
			return;
		if (!match_subscriber(nmt->trans, &frame))
			return;
		if (((frame.line_signal >> 16) & 0xf) != ((frame.line_signal >> 12) & 0xf)
		 || ((frame.line_signal >> 12) & 0xf) != ((frame.line_signal >> 8) & 0xf)
		 || ((frame.line_signal >> 8) & 0xf) != ((frame.line_signal >> 4) & 0xf)
		 || ((frame.line_signal >> 4) & 0xf) != (frame.line_signal & 0xf)) {
			LOGP_CHAN(DNMT, LOGL_NOTICE, "Line signal repetition in frame does not match, ignoring due to corrupt frame.\n");
			return;
		}
		LOGP_CHAN(DNMT, LOGL_INFO, "Received clearing by mobile phone in state %s.\n", nmt_state_name(nmt->state));
		nmt_new_state(nmt, STATE_MO_RELEASE);
		nmt->tx_frame_count = 0;
		nmt_set_dsp_mode(nmt, DSP_MODE_FRAME);
		if (nmt->trans->callref) {
			LOGP(DNMT, LOGL_INFO, "Release call towards network.\n");
			call_up_release(nmt->trans->callref, CAUSE_NORMAL);
			nmt->trans->callref = 0;
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
	case STATE_MT_AUTOANSWER:
		rx_mt_autoanswer(nmt, &frame);
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
		LOGP_CHAN(DNMT, LOGL_DEBUG, "Dropping message %s in state %s\n", nmt_frame_name(frame.mt), nmt_state_name(nmt->state));
	}
}

/* Timeout handling */
static void nmt_timeout(void *data)
{
	nmt_t *nmt = data;

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
		timeout_active(nmt, nmt->timer.timeout.tv_sec);
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
	int last_frame_idle, debug = 1;

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
	case STATE_MT_AUTOANSWER:
		tx_mt_autoanswer(nmt, &frame);
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

	last_frame_idle = nmt->tx_last_frame_idle;
	nmt->tx_last_frame_idle = 0;

	/* no encoding debug for certain (idle) frames */
	switch(frame.mt) {
	case NMT_MESSAGE_1a:
	case NMT_MESSAGE_1a_a:
	case NMT_MESSAGE_1a_b:
	case NMT_MESSAGE_4:
	case NMT_MESSAGE_1b:
	case NMT_MESSAGE_30:
		if (last_frame_idle)
			debug = 0;
		nmt->tx_last_frame_idle = 1;
		break;
	default:
		break;
	}

	/* frame sending aborted (e.g. due to audio) */
	if (nmt->dsp_mode != DSP_MODE_FRAME)
		return NULL;

	bits = encode_frame(nmt->sysinfo.system, &frame, debug);

	if (debug)
		LOGP_CHAN(DNMT, LOGL_DEBUG, "Sending frame %s.\n", nmt_frame_name(frame.mt));
	if (debug && nmt->tx_last_frame_idle)
		LOGP_CHAN(DNMT, LOGL_DEBUG, "Subsequent IDLE frames are not shown, to prevent flooding the output.\n");
	return bits;
}

/*
 * call states received from call control
 */

/* Call control starts call towards mobile station. */
static int _out_setup(int callref, const char *caller_id, enum number_type caller_type, const char *dialing, const char *sms)
{
	sender_t *sender;
	nmt_t *nmt;
	nmt_subscriber_t subscr;
	transaction_t *trans;

	memset(&subscr, 0, sizeof(subscr));

	/* 1. split number into country and subscriber parts */
	if (dialstring2number(dialing, &subscr.country, subscr.number)) {
		LOGP(DNMT, LOGL_NOTICE, "Outgoing call to invalid number '%s', rejecting!\n", dialing);
		return -CAUSE_INVALNUMBER;
	}

	/* 2. check if given number is already in a call, return BUSY */
	trans = get_transaction_by_number(&subscr);
	if (trans) {
		LOGP(DNMT, LOGL_NOTICE, "Outgoing call to busy number, rejecting!\n");
		return -CAUSE_BUSY;
	}

	/* 3. check if all paging (calling) channels are busy, return NOCHANNEL */
	for (sender = sender_head; sender; sender = sender->next) {
		nmt = (nmt_t *) sender;
		if (!is_chan_class_cc(nmt->sysinfo.chan_type))
			continue;
		if (nmt->state == STATE_IDLE)
			break;
	}
	if (!sender) {
		LOGP(DNMT, LOGL_NOTICE, "Outgoing call, but no free calling channel, rejecting!\n");
		return -CAUSE_NOCHANNEL;
	}
	if (!search_free_tc(NULL)) {
		LOGP(DNMT, LOGL_NOTICE, "Outgoing call, but no free traffic channel, rejecting!\n");
		return -CAUSE_NOCHANNEL;
	}

	LOGP(DNMT, LOGL_INFO, "Call to mobile station, paging station id '%c%s'\n", subscr.country, subscr.number);

	/* 4. trying to page mobile station */
	trans = create_transaction(&subscr);
	if (!trans) {
		LOGP(DNMT, LOGL_NOTICE, "Failed to create transaction, rejecting!\n");
		return -CAUSE_TEMPFAIL;
	}
	trans->callref = callref;
	if (sms) {
		strncpy(trans->sms_string, sms, sizeof(trans->sms_string) - 1);
	}
	if (caller_type == TYPE_INTERNATIONAL) {
		trans->caller_id[0] = '+'; /* not done by phone */
		strncpy(trans->caller_id + 1, caller_id, sizeof(trans->caller_id) - 2);
	} else
		strncpy(trans->caller_id, caller_id, sizeof(trans->caller_id) - 1);
	trans->caller_type = caller_type;
	nmt_page(trans, 1);

	return 0;
}
int call_down_setup(int callref, const char *caller_id, enum number_type caller_type, const char *dialing)
{
	return _out_setup(callref, caller_id, caller_type, dialing, NULL);
}
static int sms_out_setup(char *dialing, const char *caller_id, enum number_type caller_type, const char *sms)
{
	return _out_setup(0, caller_id, caller_type, dialing, sms);
}

void call_down_answer(int __attribute__((unused)) callref, struct timeval __attribute__((unused)) *tv_meter)
{
}

/* Call control sends disconnect (with tones).
 * An active call stays active, so tones and annoucements can be received
 * by mobile station.
 */
void call_down_disconnect(int callref, int cause)
{
	transaction_t *trans;
	nmt_t *nmt;

	LOGP(DNMT, LOGL_INFO, "Call has been disconnected by network.\n");

	trans = get_transaction_by_callref(callref);
	if (!trans) {
		LOGP(DNMT, LOGL_NOTICE, "Outgoing disconnect, but no callref!\n");
		call_up_release(callref, CAUSE_INVALCALLREF);
		return;
	}
	nmt = trans->nmt;

	if (!nmt) {
		call_up_release(callref, cause);
		trans->callref = 0;
		destroy_transaction(trans);
		return;
	}

	/* Release when not active and not waiting for answer */
	if (nmt->state == STATE_ACTIVE || nmt->state == STATE_MO_COMPLETE)
		return;
	switch (nmt->state) {
	case STATE_MT_RINGING:
		LOGP_CHAN(DNMT, LOGL_NOTICE, "Outgoing disconnect, during ringing, releasing!\n");
		trans->callref = 0;
	 	nmt_release(nmt);
		break;
	default:
		LOGP_CHAN(DNMT, LOGL_NOTICE, "Outgoing disconnect, when phone is in call setup, releasing!\n");
		trans->callref = 0;
	 	nmt_release(nmt);
		break;
	}

	call_up_release(callref, cause);
}

/* Call control releases call toward mobile station. */
void call_down_release(int callref, int __attribute__((unused)) cause)
{
	transaction_t *trans;
	nmt_t *nmt;

	LOGP(DNMT, LOGL_INFO, "Call has been released by network, releasing call.\n");

	trans = get_transaction_by_callref(callref);
	if (!trans) {
		LOGP(DNMT, LOGL_NOTICE, "Outgoing release, but no callref!\n");
		/* don't send release, because caller already released */
		return;
	}
	nmt = trans->nmt;

	trans->callref = 0;

	if (!nmt) {
		destroy_transaction(trans);
		return;
	}

	switch (nmt->state) {
	case STATE_ACTIVE:
		LOGP_CHAN(DNMT, LOGL_NOTICE, "Outgoing release, during active call, releasing!\n");
	 	nmt_release(nmt);
		break;
	case STATE_MT_RINGING:
		LOGP_CHAN(DNMT, LOGL_NOTICE, "Outgoing release, during ringing, releasing!\n");
	 	nmt_release(nmt);
		break;
	default:
		LOGP_CHAN(DNMT, LOGL_NOTICE, "Outgoing release, when phone is in call setup, releasing!\n");
	 	nmt_release(nmt);
		break;
	}
}

/*
 * SMS layer messages
 */

/* SMS layer releases */
void sms_release(nmt_t *nmt)
{
	LOGP_CHAN(DNMT, LOGL_NOTICE, "Outgoing release, by SMS layer!\n");
 	nmt_release(nmt);
}

int sms_submit(nmt_t *nmt, uint8_t ref, const char *orig_address, uint8_t __attribute__((unused)) orig_type, uint8_t __attribute__((unused)) orig_plan, int __attribute__((unused)) msg_ref, const char *dest_address, uint8_t __attribute__((unused)) dest_type, uint8_t __attribute__((unused)) dest_plan, const char *message)
{
	char sms[512];

	if (!orig_address[0])
		orig_address = &nmt->trans->subscriber.country;

	LOGP_CHAN(DNMT, LOGL_NOTICE, "Received SMS from '%s' to '%s' (ref=%d)\n", orig_address, dest_address, ref);
	printf("SMS received '%s' -> '%s': %s\n", orig_address, dest_address, message);
	snprintf(sms, sizeof(sms) - 1, "%s,%s,%s", orig_address, dest_address, message);
	sms[sizeof(sms) - 1] = '\0';

	return submit_sms(sms);
}

void sms_deliver_report(nmt_t *nmt, uint8_t ref, int error, uint8_t cause)
{
	LOGP_CHAN(DNMT, LOGL_NOTICE, "Got SMS deliver report (ref=%d)\n", ref);
	if (error)
		printf("SMS failed! (cause=%d)\n", cause);
	else {
		sms_ref++;
		printf("SMS sent!\n");
	}
}

/* application sends ud a message, we need to deliver */
void deliver_sms(const char *sms)
{
	int rc;
	char buffer[strlen(sms) + 1], *p = buffer, *caller_id, *number, *message;
	enum number_type caller_type;

	strcpy(buffer, sms);
	caller_id = strsep(&p, ",");
	number = strsep(&p, ",");
	message = p;
	if (!caller_id || !number || !message) {
inval:
		LOGP(DNMT, LOGL_NOTICE, "Given SMS MUST be in the following format: [i|n|s|u]<caller ID>,<7 digits number>,<message with comma and spaces> (i, n, s, u indicate the type of number)\n");
		return;
	}
	if (strlen(number) != 7) {
		LOGP(DNMT, LOGL_NOTICE, "Given number must be 7 digits\n");
		goto inval;
	}

	switch(caller_id[0]) {
		case '\0':
			caller_type = TYPE_NOTAVAIL;
			break;
		case 'i':
			caller_type = TYPE_INTERNATIONAL;
			caller_id++;
			break;
		case 'n':
			caller_type = TYPE_NATIONAL;
			caller_id++;
			break;
		case 's':
			caller_type = TYPE_SUBSCRIBER;
			caller_id++;
			break;
		case 'u':
			caller_type = TYPE_UNKNOWN;
			caller_id++;
			break;
		default:
			caller_type = TYPE_UNKNOWN;
	}

	LOGP(DNMT, LOGL_INFO, "SMS from '%s' for subscriber '%s' with message '%s'\n", caller_id, number, message);
	printf("SMS sending '%s' -> '%s': %s\n", caller_id, number, message);

	rc = sms_out_setup(number, caller_id, caller_type, message);
	if (rc < 0) {
		LOGP(DNMT, LOGL_INFO, "SMS delivery failed with cause '%d'\n", -rc);
		return;
	}
}

void dump_info(void) {}

