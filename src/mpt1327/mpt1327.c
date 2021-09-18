/* protocol handling
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

/*
 * How code-word scheduling works on downlink (tx_sched):
 *
 * The DSP mode can be set to CONTROL or TRAFFIC, depending on the mode the
 * channel is working. Depending on that, a function is called for every
 * code-word to be transmitted, one to schedule code-words on control channel,
 * one to schedule code-words on traffic channel.
 *
 * The scheduler uses a state that indicates what was last scheduled, i.e. what
 * is currently transmitted. If nothing is scheduled yet, an IDLE state is set.
 * When switching between CONTROL and TRAFFIC mode, the different states (for
 * each mode) are handled as they would be IDLE state, so that no reset to IDLE
 * state is required when changing DSP mode.
 *
 * An IDLE state results in a startup sequence on control channel (SYNC) or on
 * traffic channel (SYNT), whenever a message must be scheduled. The message to
 * be scheduled depends on the unit states. All units are queried for any
 * message to be scheduled. If no message on control channel need to be
 * scheduled, an ALH message is scheduled, so that random access is possible.
 * On control channel the address conde-words alternate with CCSC code-word.
 *
 * To prevent random access when a unit is requested to transmit more than two
 * code-word, a dummy frame counter is set. Then a dummy AHY message is
 * scheduled, to prevent random access by other units in that slot.
 */

/*
 * How code-word scheduling works on uplink (rx_sched):
 *
 * The DSP mode can be set to CONTROL or TRAFFIC, depending on the mode the
 * channel is working. Depending on that, a function is called for every
 * code-word received, one for code-words on control channel, one for
 * code-words on traffic channel.
 *
 * Most messages have an address code-word only, so the message type is defined
 * by the elements in the code-word. Additional data code-words (that may
 * follow an address code-word) do not have a message type, because they are
 * defined by the previous address code-word. If a message has additional data
 * code-words, a data word counter is set, so that subsequent data code-words
 * are parsed as defined by the address code-word. In case of a CRC error, the
 * message resets into un-synced state, i.e. waiting for next sync + address
 * code-word.
 */

#define CHAN mpt1327->sender.kanal

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <inttypes.h>
#include "../libsample/sample.h"
#include "../libdebug/debug.h"
#include "../libtimer/timer.h"
#include "../libmobile/call.h"
#include "../libmobile/cause.h"
#include "../libosmocc/message.h"
#include "mpt1327.h"
#include "dsp.h"
#include "message.h"

/* Timers and counters */
#define RESPONSE_TIMEOUT	1.0
#define REPEAT_GTC		1
#define REPEAT_AHY		1
#define REPEAT_AHYC		1
#define REPEAT_AHYX		3
#define REPEAT_CLEAR		3

/* Sysdef
 *
 */

static mpt1327_sysdef_t sysdef;

void init_sysdef (uint16_t sys, int wt, int per, int pon, int timeout)
{
	memset(&sysdef, 0, sizeof(sysdef));

	sysdef.sys = sys;
	sysdef.wt = wt;
	sysdef.per = per;
	sysdef.pon = pon;
	sysdef.timeout = timeout;
	sysdef.framelength = 3;
	sysdef.bcast_slots = 10; /* every seconds is good */
}

/*
 * Units handling
 */

static mpt1327_unit_t *unit_list = NULL;

#define	UNIT_IDLE		0
#define UNIT_REGISTER_ACK	(1 << 0)	/* need to ack registration */
#define UNIT_DIVERSION_REJ	(1 << 1)	/* need to nack diversion */
#define UNIT_CALLING_REJ	(1 << 2)	/* need to reject call */
#define UNIT_CALLING_AHYC	(1 << 3)	/* need to request SAMIS */
#define UNIT_CALLING_SAMIS	(1 << 4)	/* wait for SAMIS response */
#define UNIT_CALLED_AHY		(1 << 5)	/* need to request ACK */
#define UNIT_CALLED_AHYX	(1 << 6)	/* cancel call */
#define UNIT_CALLED_ACK		(1 << 7)	/* wait for AHY response */
#define UNIT_GTC_P		(1 << 8)	/* need to assign channel (same prefix) */
#define UNIT_GTC_A		(1 << 9)	/* need to assign channel (calling unit) */
#define UNIT_GTC_B		(1 << 10)	/* need to assign channel (called unit) */
#define UNIT_CALL		(1 << 11)	/* established call */
#define UNIT_CALL_CLEAR		(1 << 12)	/* established call */
#define UNIT_CANCEL_ACK		(1 << 13)	/* need to ack cancelation */

const char *unit_state_name(uint64_t state)
{
	static char invalid[32];

	switch (state) {
	case UNIT_IDLE:
		return "IDLE";
	case UNIT_REGISTER_ACK:
		return "REGISTER-ACK";
	case UNIT_DIVERSION_REJ:
		return "DIVERSION-REJ";
	case UNIT_CALLING_REJ:
		return "CALLING-REJ";
	case UNIT_CALLING_AHYC:
		return "CALLING-AHYC";
	case UNIT_CALLING_SAMIS:
		return "CALLING-SAMIS";
	case UNIT_CALLED_AHY:
		return "CALLED-AHY";
	case UNIT_CALLED_AHYX:
		return "CALLED-AHYX";
	case UNIT_CALLED_ACK:
		return "CALLED-ACK";
	case UNIT_GTC_P:
		return "GTC-BOTH";
	case UNIT_GTC_A:
		return "GTC-OTHER-UNIT";
	case UNIT_GTC_B:
		return "GTC-UNIT";
	case UNIT_CALL:
		return "CALL";
	case UNIT_CALL_CLEAR:
		return "CALL-CLEAR";
	case UNIT_CANCEL_ACK:
		return "CANCEL-ACK";
	}

	sprintf(invalid, "invalid(0x%" PRIx64 ")", state);
	return invalid;
}

void unit_new_state(mpt1327_unit_t *unit, uint64_t new_state)
{
	PDEBUG(DMPT1327, DEBUG_DEBUG, "Radio Unit (Prefix:%d Ident:%d) state: %s -> %s\n", unit->prefix, unit->ident, unit_state_name(unit->state), unit_state_name(new_state));
	unit->state = new_state;
}

static void unit_timeout(struct timer *timer);

mpt1327_unit_t *get_unit(uint8_t prefix, uint16_t ident)
{
	mpt1327_unit_t **unitp;

	for (unitp = &unit_list; *unitp; unitp = &((*unitp)->next)) {
		if ((*unitp)->prefix == prefix
		 && (*unitp)->ident == ident)
			break;
	}

	if (!(*unitp)) {
		PDEBUG(DDB, DEBUG_INFO, "Radio Unit (Prefix:%d Ident:%d) added to database\n", prefix, ident);
		*unitp = calloc(1, sizeof(mpt1327_unit_t));
		timer_init(&(*unitp)->timer, unit_timeout, (*unitp));
		(*unitp)->state = UNIT_IDLE;
		(*unitp)->prefix = prefix;
		(*unitp)->ident = ident;
	}

	return *unitp;
}

mpt1327_unit_t *find_unit_state(uint32_t state, mpt1327_t *tc)
{
	mpt1327_unit_t *unit;

	for (unit = unit_list; unit; unit = unit->next) {
		if (tc && unit->tc != tc)
			continue;
		if ((unit->state & state))
			break;
	}

	return unit;
}

mpt1327_unit_t *find_unit_callref(uint32_t callref)
{
	mpt1327_unit_t *unit;

	for (unit = unit_list; unit; unit = unit->next) {
		if ((unit->callref & callref))
			break;
	}

	return unit;
}

static void mpt1327_go_idle(mpt1327_t *mpt1327);
static void mpt1327_release(mpt1327_unit_t *unit);

/* Timeout handling */
static void unit_timeout(struct timer *timer)
{
	mpt1327_unit_t *unit = (mpt1327_unit_t *)timer->priv;

	// FIXME: do some retry
	switch (unit->state) {
	case UNIT_CALLING_SAMIS:
		if (unit->repeat) {
			--unit->repeat;
			PDEBUG(DMPT1327, DEBUG_INFO, "Resend AHYC, because unit timed out.\n");
			unit_new_state(unit, UNIT_CALLING_AHYC);
			break;
		}
		PDEBUG(DMPT1327, DEBUG_INFO, "Unit failed to respond to AHYC, releasing...\n");
		mpt1327_release(unit);
		if (unit->callref) {
			call_up_release(unit->callref, CAUSE_NORMAL);
			unit->callref = 0;
		}
		break;
	case UNIT_CALLED_ACK:
		if (unit->repeat) {
			--unit->repeat;
			PDEBUG(DMPT1327, DEBUG_INFO, "Resend AHY, because unit timed out.\n");
			unit_new_state(unit, UNIT_CALLED_AHY);
			break;
		}
		PDEBUG(DMPT1327, DEBUG_INFO, "Unit failed to respond to AHY, releasing...\n");
		mpt1327_release(unit);
		if (unit->callref) {
			call_up_release(unit->callref, CAUSE_NORMAL);
			unit->callref = 0;
		}
		break;
	case UNIT_CALL:
		PDEBUG(DMPT1327, DEBUG_NOTICE, "Release call, because unit timed out.\n");
		mpt1327_release(unit);
		if (unit->callref) {
			call_up_release(unit->callref, CAUSE_NORMAL);
			unit->callref = 0;
		}
		break;
	default:
		PDEBUG(DMPT1327, DEBUG_ERROR, "Unknown timeout at state 0x%" PRIx64 ", please fix!\n", unit->state);
		break;
	}

}

void flush_units(void)
{
	mpt1327_unit_t *next;

	while (unit_list) {
		next = unit_list->next;
		timer_exit(&unit_list->timer);
		free(unit_list);
		unit_list = next;
	}
}

void dump_units(void)
{
	mpt1327_unit_t *unit = unit_list;

	PDEBUG(DDB, DEBUG_NOTICE, "Dump of Radio Unit list:\n");
	if (!unit) {
		PDEBUG(DDB, DEBUG_NOTICE, " - No Radio Unit seen yet!\n");
		return;
	}

	while (unit) {
		PDEBUG(DDB, DEBUG_NOTICE, " - Radio Unit (Prefix:%d Ident:%d) seen on this TSC.\n", unit->prefix, unit->ident);
		unit = unit->next;
	}
}

/*
 * bands and channels
 */

static struct mpt1327_band_def {
	const char *name;
	const char *description;
} mpt1327_band_def[] = {
	{ "MPT1343/1", "MPT1343 Sub Band 1"},
	{ "MPT1343/2", "MPT1343 Sub Band 2"},
	{ "Regionet43/1", "Regionet43 410-430 MHz (German band)" },
	{ "Regionet43/2", "Regionet43 445-448 MHz (rarely used in Germany)" },
	{ NULL, NULL }
};

const char *mpt1327_band_name(enum mpt1327_band band)
{
	return mpt1327_band_def[band].name;
}

int mpt1327_band_by_short_name(const char *name)
{
	int i;

	for (i = 0; mpt1327_band_def[i].name; i++) {
		if (!strcasecmp(mpt1327_band_def[i].name, name))
			return i;
	}

	return -1;
}

void mpt1327_band_list(void)
{
	int i;

	printf("Name\t\tDescription\n");
	printf("------------------------------------------------------------------------\n");
	for (i = 0; mpt1327_band_def[i].name; i++)
		printf("%s\t%s\n", mpt1327_band_def[i].name, mpt1327_band_def[i].description);
}

/* convert channel to frequency */
double mpt1327_channel2freq(enum mpt1327_band band, int channel, int uplink)
{
	double freq = 0, offset = 0; // make GCC happy
	int channels = 0;

	switch(band) {
	case BAND_MPT1343_SUB1:
		freq = 177.2125;
		offset = 8.0; /* that's right! */
		channel -= 58;
		channels = 503;
		break;
	case BAND_MPT1343_SUB2:
		freq = 201.2125;
		offset = -8.0;
		channel -= 58;
		channels = 503;
		break;
	case BAND_REGIONET43_SUB1:
		freq = 420.0125;
		offset = -10.0;
		channel -= 1;
		channels = 799;
		break;
	case BAND_REGIONET43_SUB2:
		freq = 445.0125;
		offset = -5.0;
		channel -= 1;
		channels = 239;
		break;
	}

	/* channel out of range */
	if (channel < 0 || channel > channels)
		return 0.0;

	if (uplink == 2)
		return offset * 1e6;

	freq += channel * 0.0125;
	if (uplink)
		freq += offset;

	return freq * 1e6;
}

/* convert channel to chan field */
uint16_t mpt1327_channel2chan(enum mpt1327_band band, int channel)
{
	uint16_t chan = 0;

	switch(band) {
	case BAND_MPT1343_SUB1:
		chan = channel - 58 + 513;
		break;
	case BAND_MPT1343_SUB2:
		chan = channel - 58 + 1;
		break;
	case BAND_REGIONET43_SUB1:
		chan = channel - 1 + 1;
		break;
	case BAND_REGIONET43_SUB2:
		chan = channel - 1 + 1; // works with DETEWE
		break;
	}

	return chan;
}

static struct mpt1327_channels {
	enum mpt1327_chan_type chan_type;
	const char *short_name;
	const char *long_name;
} mpt1327_channels[] = {
	{ CHAN_TYPE_CC,		"CC",	"control channel" },
	{ CHAN_TYPE_TC,		"TC",	"traffic channel" },
	{ CHAN_TYPE_CC_TC,	"CC/TC","combined control & traffic channel" },
	{ 0, NULL, NULL }
};

void mpt1327_channel_list(void)
{
	int i;

	printf("Type\t\tDescription\n");
	printf("------------------------------------------------------------------------\n");
	for (i = 0; mpt1327_channels[i].long_name; i++)
		printf("%s%s\t%s\n", mpt1327_channels[i].short_name, (strlen(mpt1327_channels[i].short_name) >= 8) ? "" : "\t", mpt1327_channels[i].long_name);
}

int mpt1327_channel_by_short_name(const char *short_name)
{
	int i;

	for (i = 0; mpt1327_channels[i].short_name; i++) {
		if (!strcasecmp(mpt1327_channels[i].short_name, short_name)) {
			PDEBUG(DMPT1327, DEBUG_INFO, "Selecting channel '%s' = %s\n", mpt1327_channels[i].short_name, mpt1327_channels[i].long_name);
			return mpt1327_channels[i].chan_type;
		}
	}

	return -1;
}

const char *chan_type_short_name(enum mpt1327_chan_type chan_type)
{
	int i;

	for (i = 0; mpt1327_channels[i].short_name; i++) {
		if (mpt1327_channels[i].chan_type == chan_type)
			return mpt1327_channels[i].short_name;
	}

	return "invalid";
}

const char *chan_type_long_name(enum mpt1327_chan_type chan_type)
{
	int i;

	for (i = 0; mpt1327_channels[i].long_name; i++) {
		if (mpt1327_channels[i].chan_type == chan_type)
			return mpt1327_channels[i].long_name;
	}

	return "invalid";
}

/*
 * MPT processing
 */

static mpt1327_t *search_free_tc(void)
{
	sender_t *sender;
	mpt1327_t *tc, *cc_tc = NULL;

	for (sender = sender_head; sender; sender = sender->next) {
		tc = (mpt1327_t *) sender;
		if (tc->state != STATE_IDLE)
			continue;
		/* remember combined voice/control/paging channel as second alternative */
		if (tc->chan_type == CHAN_TYPE_CC_TC)
			cc_tc = tc;
		if (tc->chan_type == CHAN_TYPE_TC)
			return tc;
	}

	return cc_tc;

}

static mpt1327_t *search_cc(void)
{
	sender_t *sender;
	mpt1327_t *cc = NULL;

	for (sender = sender_head; sender; sender = sender->next) {
		cc = (mpt1327_t *) sender;
		/* remember combined voice/control/paging channel as second alternative */
		if (cc->chan_type == CHAN_TYPE_CC_TC)
			return cc;
		if (cc->chan_type == CHAN_TYPE_CC)
			return cc;
	}

	return NULL;

}

const char *mpt1327_state_name(enum mpt1327_state state)
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

void mpt1327_display_status(void)
{
	sender_t *sender;
	mpt1327_t *mpt1327;

	display_status_start();
	for (sender = sender_head; sender; sender = sender->next) {
		mpt1327 = (mpt1327_t *) sender;
		display_status_channel(mpt1327->sender.kanal, chan_type_short_name(mpt1327->chan_type), mpt1327_state_name(mpt1327->state));
		if (mpt1327->unit) {
			char unit_id[32];
			sprintf(unit_id, "%d/%d", mpt1327->unit->prefix, mpt1327->unit->ident);
			display_status_subscriber(unit_id, NULL);
		}
	}
	display_status_end();
}

static void mpt1327_new_state(mpt1327_t *mpt1327, enum mpt1327_state new_state, mpt1327_unit_t *unit)
{
	if (mpt1327->state == new_state)
		return;
	PDEBUG_CHAN(DMPT1327, DEBUG_DEBUG, "State change: %s -> %s\n", mpt1327_state_name(mpt1327->state), mpt1327_state_name(new_state));

	/* unlink unit, if linked */
	if (mpt1327->unit) {
		mpt1327->unit->tc = NULL;
		mpt1327->unit = NULL;
	}

	/* link unit, if given */
	if (unit) {
		unit->tc = mpt1327;
		mpt1327->unit = unit;
	}

	mpt1327->state = new_state;
	mpt1327_display_status();
}

static void mpt1327_timeout(struct timer *timer);

/* Create transceiver instance and link to a list. */
int mpt1327_create(enum mpt1327_band band, const char *kanal, enum mpt1327_chan_type chan_type, const char *device, int use_sdr, int samplerate, double rx_gain, double tx_gain, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, const char *read_tx_wave, int loopback, double squelch_db)
{
	sender_t *sender;
	mpt1327_t *mpt1327;
	int rc;

	/* check channel matching and set deviation factor */
	if (mpt1327_channel2freq(band, atoi(kanal), 0) == 0.0)
		return -EINVAL;

	for (sender = sender_head; sender; sender = sender->next) {
		mpt1327 = (mpt1327_t *)sender;
		if ((mpt1327->chan_type == CHAN_TYPE_CC || mpt1327->chan_type == CHAN_TYPE_CC_TC)
		 && (chan_type == CHAN_TYPE_CC || chan_type == CHAN_TYPE_CC_TC)) {
			PDEBUG(DCNETZ, DEBUG_NOTICE, "More than one control channel is not supported, please define other channels as traffic channels!\n");
			return -EINVAL;
		}
	}
	mpt1327 = calloc(1, sizeof(mpt1327_t));
	if (!mpt1327) {
		PDEBUG(DMPT1327, DEBUG_ERROR, "No memory!\n");
		return -EIO;
	}

	PDEBUG(DMPT1327, DEBUG_DEBUG, "Creating 'MPT1327' instance for Channel %s on Band %s (sample rate %d).\n", kanal, mpt1327_band_def[band].name, samplerate);

	/* init general part of transceiver */
	rc = sender_create(&mpt1327->sender, kanal, mpt1327_channel2freq(band, atoi(kanal), 0), mpt1327_channel2freq(band, atoi(kanal), 1), device, use_sdr, samplerate, rx_gain, tx_gain, 0, 0, write_rx_wave, write_tx_wave, read_rx_wave, read_tx_wave, loopback, PAGING_SIGNAL_NONE);
	if (rc < 0) {
		PDEBUG(DMPT1327, DEBUG_ERROR, "Failed to init 'Sender' processing!\n");
		goto error;
	}

	/* init audio processing */
	rc = dsp_init_sender(mpt1327, squelch_db);
	if (rc < 0) {
		PDEBUG(DANETZ, DEBUG_ERROR, "Failed to init signal processing!\n");
		goto error;
	}

	/* timers */
	timer_init(&mpt1327->timer, mpt1327_timeout, mpt1327);

	mpt1327->band = band;
	mpt1327->chan_type = chan_type;

	/* only accept these valued */
	if (sysdef.framelength != 1 && sysdef.framelength != 3 && sysdef.framelength != 6) {
		PDEBUG(DMPT1327, DEBUG_ERROR, "Invalid frame length %d, please fix!\n", sysdef.framelength);
		abort();
	}
	if (sysdef.wt != 5 && sysdef.wt != 10 && sysdef.wt != 15) {
		PDEBUG(DMPT1327, DEBUG_ERROR, "Invalid WT value %d, please fix!\n", sysdef.wt);
		abort();
	}

	/* go into idle state */
	mpt1327_go_idle(mpt1327);

	PDEBUG(DMPT1327, DEBUG_NOTICE, "Created channel #%s of type '%s' = %s\n", kanal, chan_type_short_name(chan_type), chan_type_long_name(chan_type));

	return 0;

error:
	mpt1327_destroy(&mpt1327->sender);

	return rc;
}

void mpt1327_check_channels(void)
{
	sender_t *sender;
	mpt1327_t *mpt1327;
	int cc = 0, tc = 0;
	int note = 0;

	for (sender = sender_head; sender; sender = sender->next) {
		mpt1327 = (mpt1327_t *) sender;
		if (mpt1327->chan_type == CHAN_TYPE_CC)
			cc = 1;
		if (mpt1327->chan_type == CHAN_TYPE_TC)
			tc = 1;
		if (mpt1327->chan_type == CHAN_TYPE_CC_TC) {
			cc = 1;
			tc = 1;
		}
	}
	if (cc && !tc) {
		PDEBUG(DMPT1327, DEBUG_NOTICE, "\n");
		PDEBUG(DMPT1327, DEBUG_NOTICE, "*** Selected channel(s) can be used for control only.\n");
		PDEBUG(DMPT1327, DEBUG_NOTICE, "*** No call is possible.\n");
		PDEBUG(DMPT1327, DEBUG_NOTICE, "*** Use at least one 'TC'!\n");
		note = 1;
	}
	if (tc && !cc) {
		PDEBUG(DMPT1327, DEBUG_NOTICE, "\n");
		PDEBUG(DMPT1327, DEBUG_NOTICE, "*** Selected channel(s) can be used for traffic only.\n");
		PDEBUG(DMPT1327, DEBUG_NOTICE, "*** No call to the mobile phone is possible.\n");
		PDEBUG(DMPT1327, DEBUG_NOTICE, "*** Use one 'CC'!\n");
		note = 1;
	}
	if (note)
		PDEBUG(DMPT1327, DEBUG_NOTICE, "\n");
}

/* Destroy transceiver instance and unlink from list. */
void mpt1327_destroy(sender_t *sender)
{
	mpt1327_t *mpt1327 = (mpt1327_t *) sender;

	PDEBUG(DMPT1327, DEBUG_DEBUG, "Destroying 'MPT1327' instance for channel = %s.\n", sender->kanal);

	dsp_cleanup_sender(mpt1327);
	timer_exit(&mpt1327->timer);
	sender_destroy(&mpt1327->sender);
	free(sender);
}

/* Abort connection towards mobile station changing to IDLE state */
static void mpt1327_go_idle(mpt1327_t *mpt1327)
{
	timer_stop(&mpt1327->timer);
	mpt1327->pressel_on = 0;

	PDEBUG(DMPT1327, DEBUG_INFO, "Entering IDLE state on channel %s.\n", mpt1327->sender.kanal);
	mpt1327_new_state(mpt1327, STATE_IDLE, NULL);
	memset(&mpt1327->tx_sched, 0, sizeof(mpt1327->tx_sched));
	switch (mpt1327->chan_type) {
	case CHAN_TYPE_CC:
	case CHAN_TYPE_CC_TC:
		mpt1327->tx_sched.state = SCHED_STATE_CC_IDLE;
		mpt1327_set_dsp_mode(mpt1327, DSP_MODE_CONTROL, 0);
		break;
	case CHAN_TYPE_TC:
		mpt1327->tx_sched.state = SCHED_STATE_TC_IDLE;
		mpt1327_set_dsp_mode(mpt1327, DSP_MODE_OFF, 0);
		break;
	}
}

static void mpt1327_release(mpt1327_unit_t *unit)
{
	timer_stop(&unit->timer);

	if (unit->state == UNIT_CALL && unit->tc) {
		/* release all units on traffic channel */
		unit_new_state(unit, UNIT_CALL_CLEAR);
		unit->repeat = REPEAT_CLEAR;
	} else {
		/* release unit on control channel */
		unit_new_state(unit, UNIT_CALLED_AHYX);
		unit->repeat = REPEAT_AHYX;
	}
}

static int gtc_aloha_number(int length)
{
	switch (length) {
	case 1:
		return 1;
	case 3:
		return 2;
	case 6:
		return 3;
	default:
		return 0;
	}
}

/* schedule message on control channel
 *
 * on IDLE state a STARTUP sequence is sent, followed by alternating CCSC and
 * ADDR codewords. if a unit has nothing to send, ALH is transmitted. if a
 * unit wants to send a message, the message is scheduled. this message can
 * be repeated. afterwards, if no unit has something to send, the ALH is
 * scheduled.
 *
 * a dummy slot is used to allow radio unit to allow multi slot response to a
 * request from TSC.
 */
int mpt1327_send_codeword_control(mpt1327_t *mpt1327, mpt1327_codeword_t *codeword)
{
	mpt1327_unit_t *unit;

	/* CC scheduler, the sched_state is what we have sent */
	switch (mpt1327->tx_sched.state) {
	case SCHED_STATE_CC_ADDR:
		codeword->type = MPT_CCSC;
		codeword->params[MPT_SYS] = sysdef.sys;
		mpt1327->tx_sched.state = SCHED_STATE_CC_CCSC;
		break;
	case SCHED_STATE_CC_STARTUP:
	case SCHED_STATE_CC_CCSC:
		/* count slots for each broadcast */
		if (mpt1327->tx_sched.bcast_count < sysdef.bcast_slots) {
			mpt1327->tx_sched.bcast_count++;
		}
		/* count slots in frame */
		if (!mpt1327->tx_sched.frame_length || mpt1327->tx_sched.frame_count == mpt1327->tx_sched.frame_length) {
			mpt1327->tx_sched.frame_length = sysdef.framelength;
			mpt1327->tx_sched.frame_count = 0;
		}
		/* send out a dummy slot to prevent random access from other units */
		if (mpt1327->tx_sched.dummy_slot) {
			mpt1327->tx_sched.dummy_slot--;
			/* if this is a new frame, make it 1 slot long */
			if (mpt1327->tx_sched.frame_count == 0)
				mpt1327->tx_sched.frame_length = 1;
			codeword->type = MPT_AHY;
			codeword->params[MPT_PFIX] = 0x2a; /* just some alternating pattern (ignored) */
			codeword->params[MPT_IDENT1] = IDENT_DUMMYI;
			codeword->params[MPT_IDENT2] = IDENT_DUMMYI;
			PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Sending dummy AHY, to prevent random access while receiving SAMIS\n");
		} else {
			unit = find_unit_state(UNIT_REGISTER_ACK | UNIT_DIVERSION_REJ | UNIT_CALLING_REJ | UNIT_CALLING_AHYC | UNIT_CALLED_AHY | UNIT_GTC_P | UNIT_GTC_A | UNIT_GTC_B | UNIT_CANCEL_ACK | UNIT_CALLED_AHYX, NULL);
			if (!unit) {
				/* if we reached the slot count for broadcast message
				 * AND the frame is not frame 0 (with given length, as long es frame length > 1)
				 * if the frame length is 1 (implies count == 0), then make this message single slot frame
				 */
				if (mpt1327->tx_sched.bcast_count == sysdef.bcast_slots
				 && (mpt1327->tx_sched.frame_count > 0 || mpt1327->tx_sched.frame_length == 1)) {
					mpt1327->tx_sched.bcast_count = 0;
					/* if this is a new frame, make it 1 slot long */
					if (mpt1327->tx_sched.frame_count == 0)
						mpt1327->tx_sched.frame_length = 1;
					codeword->type = MPT_BCAST2;
					codeword->params[MPT_SYS] = sysdef.sys;
					codeword->params[MPT_IVAL] = sysdef.per;
					if (!sysdef.per)
						codeword->params[MPT_PER] = 1;
					if (!sysdef.pon)
					codeword->params[MPT_PON] = 1;
				} else {
					codeword->type = MPT_ALH;
					codeword->params[MPT_PFIX] = 0x2a; /* just some alternating pattern (ignored) */
					codeword->params[MPT_IDENT1] = 0x1555; /* dito */
					codeword->params[MPT_CHAN4] = mpt1327_channel2chan(mpt1327->band, atoi(mpt1327->sender.kanal)) & 0xf;
					codeword->params[MPT_WT] = (sysdef.wt < 5) ? sysdef.wt : sysdef.wt / 5 + 4;
				}
			} else switch (unit->state) {
			case UNIT_REGISTER_ACK: /* ack to register */
				codeword->type = MPT_ACK;
				codeword->params[MPT_PFIX] = unit->prefix;
				codeword->params[MPT_IDENT1] = IDENT_REGI;
				codeword->params[MPT_IDENT2] = unit->ident;
				codeword->params[MPT_QUAL] = 0;
				unit_new_state(unit, UNIT_IDLE);
				PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Sending acknowledge to Radio Unit (Prefix:%d Ident:%d)\n", unit->prefix, unit->ident);
				break;
			case UNIT_DIVERSION_REJ: /* reject diversion */
				codeword->type = MPT_ACKX;
				codeword->params[MPT_PFIX] = unit->prefix;
				codeword->params[MPT_IDENT1] = unit->called_ident;
				codeword->params[MPT_IDENT2] = unit->ident;
				codeword->params[MPT_QUAL] = 0;
				PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Sending negative acknowledge to Radio Unit (Prefix:%d Ident:%d)\n", unit->prefix, unit->ident);
				if (unit->repeat) {
					--unit->repeat;
					break;
				}
				unit_new_state(unit, UNIT_IDLE);
				if (unit->tc)
					mpt1327_go_idle(unit->tc);
				break;
			case UNIT_CALLING_REJ: /* outgoing call rejected, no channel available */
				codeword->type = MPT_ACKX;
				codeword->params[MPT_PFIX] = unit->prefix;
				codeword->params[MPT_IDENT1] = unit->called_ident;
				codeword->params[MPT_IDENT2] = unit->ident;
				codeword->params[MPT_QUAL] = 1;
				PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Sending negative acknowledge to Radio Unit (Prefix:%d Ident:%d)\n", unit->prefix, unit->ident);
				if (unit->repeat) {
					--unit->repeat;
					break;
				}
				unit_new_state(unit, UNIT_IDLE);
				if (unit->tc)
					mpt1327_go_idle(unit->tc);
				break;
			case UNIT_CALLING_AHYC: /* request SAMIS for dialing data */
				codeword->type = MPT_AHYC;
				codeword->params[MPT_PFIX] = unit->prefix;
				codeword->params[MPT_IDENT1] = unit->called_ident;
				codeword->params[MPT_IDENT2] = unit->ident; /* implies Mode 1 */
				switch (unit->called_type) {
				case CALLED_TYPE_INTERPFX:
					codeword->params[MPT_SLOTS] = 0x1;
					codeword->params[MPT_DESC] = 0x0;
					break;
				case CALLED_TYPE_PSTN_LONG1:
					codeword->params[MPT_SLOTS] = 0x1;
					codeword->params[MPT_DESC] = 0x1;
					break;
				case CALLED_TYPE_PSTN_LONG2:
					codeword->params[MPT_SLOTS] = 0x2;
					codeword->params[MPT_DESC] = 0x1;
					mpt1327->tx_sched.dummy_slot = 1;
					break;
				case CALLED_TYPE_PBX_LONG:
					codeword->params[MPT_SLOTS] = 0x1;
					codeword->params[MPT_DESC] = 0x2;
					break;
				default:
					PDEBUG_CHAN(DMPT1327, DEBUG_ERROR, "Want to send AHYC, but called_type not set correctly, please fix!\n");
					abort();
				}
				unit_new_state(unit, UNIT_CALLING_SAMIS);
				mpt1327->rx_sched.data_prefix = unit->prefix;
				mpt1327->rx_sched.data_ident = unit->ident;
				PDEBUG_CHAN(DMPT1327, DEBUG_DEBUG, "Starting timer, waiting for response\n");
				timer_start(&unit->timer, RESPONSE_TIMEOUT);
				PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Sending AHYC, to request SAMIS from Radio Unit (Prefix:%d Ident:%d)\n", unit->prefix, unit->ident);
				break;
			case UNIT_CALLED_AHY: /* call to unit and request ACK from unit */
				codeword->type = MPT_AHY;
				codeword->params[MPT_PFIX] = unit->prefix;
				codeword->params[MPT_IDENT1] = unit->ident;
				codeword->params[MPT_IDENT2] = IDENT_PABXI;
				codeword->params[MPT_D] = 0; /* speech call */
				codeword->params[MPT_POINT] = 0; /* demand ACK from ident1 */
				codeword->params[MPT_CHECK] = 1; /* unit is in contact and accepts calls */
				codeword->params[MPT_E] = 0; /* no emergency call */
				codeword->params[MPT_AD] = 0; /* no appended data */
				unit_new_state(unit, UNIT_CALLED_ACK);
				PDEBUG_CHAN(DMPT1327, DEBUG_DEBUG, "Starting timer, waiting for response\n");
				timer_start(&unit->timer, RESPONSE_TIMEOUT);
				PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Sending AHY, to request ACK from Radio Unit (Prefix:%d Ident:%d)\n", unit->prefix, unit->ident);
				break;
			case UNIT_GTC_P: /* channel assignment to unit itself and called unit */
				codeword->type = MPT_GTC;
				codeword->params[MPT_PFIX] = unit->prefix;
				codeword->params[MPT_IDENT1] = unit->called_ident;
				codeword->params[MPT_IDENT2] = unit->ident;
				codeword->params[MPT_CHAN] = mpt1327_channel2chan(unit->tc->band, atoi(unit->tc->sender.kanal));
				PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Sending channel assignment to calling and called Radio Units (Prefix:%d Ident:%d and Ident:%d)\n", unit->prefix, unit->ident, unit->called_ident);
				if (unit->repeat) {
					--unit->repeat;
					break;
				}
				unit_new_state(unit, UNIT_CALL);
				mpt1327_set_dsp_mode(unit->tc, DSP_MODE_TRAFFIC, 1);
				if (sysdef.timeout)
					timer_start(&unit->timer, sysdef.timeout);
				break;
			case UNIT_GTC_B: /* channel assignment to called unit */
				/* NOTE GTC to called unit must be sent before GTC to calling unit (1.3.5.3) */
				codeword->type = MPT_GTC;
				codeword->params[MPT_PFIX] = unit->prefix;
				codeword->params[MPT_IDENT1] = unit->called_ident;
				codeword->params[MPT_IDENT2] = IDENT_DUMMYI;
				codeword->params[MPT_CHAN] = mpt1327_channel2chan(unit->tc->band, atoi(unit->tc->sender.kanal));
				PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Sending channel assignment to called Radio Unit (Prefix:%d Ident:%d)\n", unit->prefix, unit->ident);
				if (unit->repeat) {
					--unit->repeat;
					break;
				}
				unit_new_state(unit, UNIT_GTC_A);
				unit->repeat = REPEAT_GTC;
				break;
			case UNIT_GTC_A: /* channel assignment unit itself */
				codeword->type = MPT_GTC;
				codeword->params[MPT_PFIX] = unit->prefix;
				codeword->params[MPT_IDENT1] = IDENT_DUMMYI;
				codeword->params[MPT_IDENT2] = unit->ident;
				codeword->params[MPT_CHAN] = mpt1327_channel2chan(unit->tc->band, atoi(unit->tc->sender.kanal));
				PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Sending channel assignment to calling Radio Unit (Prefix:%d Ident:%d)\n", unit->prefix, unit->ident);
				if (unit->repeat) {
					--unit->repeat;
					break;
				}
				unit_new_state(unit, UNIT_CALL);
				mpt1327_set_dsp_mode(unit->tc, DSP_MODE_TRAFFIC, 1);
				if (sysdef.timeout)
					timer_start(&unit->timer, sysdef.timeout);
				break;
			case UNIT_CANCEL_ACK:
				codeword->type = MPT_ACK;
				codeword->params[MPT_PFIX] = unit->called_prefix;
				codeword->params[MPT_IDENT1] = unit->called_ident;
				codeword->params[MPT_IDENT2] = unit->ident;
				codeword->params[MPT_QUAL] = 1;
				unit_new_state(unit, UNIT_IDLE);
				PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Sending acknowledge to Radio Unit (Prefix:%d Ident:%d)\n", unit->prefix, unit->ident);
				break;
			case UNIT_CALLED_AHYX: /* cancel call towards unit */
				codeword->type = MPT_AHYX;
				codeword->params[MPT_PFIX] = unit->prefix;
				codeword->params[MPT_IDENT1] = unit->ident;
				codeword->params[MPT_IDENT2] = IDENT_PABXI;
				PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Sending AHYX, to cancel call to Radio Unit (Prefix:%d Ident:%d)\n", unit->prefix, unit->ident);
				if (unit->repeat) {
					--unit->repeat;
					break;
				}
				unit_new_state(unit, UNIT_IDLE);
				if (unit->tc)
					mpt1327_go_idle(unit->tc);
				break;
			}
		}
		if (codeword->type == MPT_GTC)
			codeword->params[MPT_N] = (mpt1327->tx_sched.frame_count == 0) ? gtc_aloha_number(mpt1327->tx_sched.frame_length) : 0;
		else
			codeword->params[MPT_N] = (mpt1327->tx_sched.frame_count == 0) ? mpt1327->tx_sched.frame_length : 0;
		mpt1327->tx_sched.frame_count++;
		mpt1327->tx_sched.state = SCHED_STATE_CC_ADDR;
		break;
	default:
		/* on dirty state (e.g. changing from TC to CC), we start control channel framing */
		codeword->type = MPT_START_SYNC;
		mpt1327->tx_sched.frame_length = 0;
		mpt1327->tx_sched.state = SCHED_STATE_CC_STARTUP;
		break;
	}

	return 0;
}

/* schedule messages on traffic channel
 *
 * when the unit has a message to send, it, the repeat counter is decreased and
 * a SYNC is sent the next request will send the an ADDR codeword. this will
 * repeat until the repeat counter reaches 0.
 */
int mpt1327_send_codeword_traffic(mpt1327_t *mpt1327, mpt1327_codeword_t __attribute__((unused)) *codeword)
{
	mpt1327_unit_t *unit;
	mpt1327_t *cc;

	/* TC scheduler */
	switch (mpt1327->tx_sched.state) {
	case SCHED_STATE_TC_IDLE:
	case SCHED_STATE_TC_ADDR:
		/* on idle state or after sending address, we search for a unit that wants to send a message */
		unit = find_unit_state(UNIT_CALL_CLEAR, mpt1327);
		if (!unit) {
			/* no message, so we have nothing to send */
			mpt1327->tx_sched.state = SCHED_STATE_TC_IDLE;
			return -1;
		}
		switch (unit->state) {
		case UNIT_CALL_CLEAR: /* release channel */
			if (!unit->repeat) {
				PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Done sending clear down on traffic channel, releasing\n");
				unit_new_state(unit, UNIT_IDLE);
				mpt1327_go_idle(mpt1327);
				return -1;
			}
			--unit->repeat;
			break;
		}
		codeword->type = MPT_START_SYNT;
		mpt1327->tx_sched.state = SCHED_STATE_TC_SYNT;
		break;
	case SCHED_STATE_TC_SYNT:
		/* after sending SYNT, we process message that unit wants to send */
		unit = find_unit_state(UNIT_CALL_CLEAR, mpt1327);
		if (!unit) {
			mpt1327->tx_sched.state = SCHED_STATE_TC_IDLE;
			return -1;
		}
		switch (unit->state) {
		case UNIT_CALL_CLEAR: /* release channel */
			codeword->type = MPT_CLEAR;
			codeword->params[MPT_CHAN] = mpt1327_channel2chan(mpt1327->band, atoi(mpt1327->sender.kanal));
			cc = search_cc();
			if (cc)
				codeword->params[MPT_CONT] = mpt1327_channel2chan(cc->band, atoi(cc->sender.kanal));
			PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Sending clear down on traffic channel\n");
		}
		mpt1327->tx_sched.state = SCHED_STATE_TC_ADDR;
		break;
	default:
		/* on dirty state (e.g. changing from CC to TC), we enter idle state */
		mpt1327->tx_sched.state = SCHED_STATE_TC_IDLE;
		return -1;
	}

	return 0;
}

int mpt1327_send_codeword(mpt1327_t *mpt1327, uint64_t *bits)
{
	mpt1327_codeword_t codeword;
	int rc = -1;

	memset(&codeword, 0, sizeof(codeword));

	switch (mpt1327->dsp_mode) {
	case DSP_MODE_CONTROL:
		rc = mpt1327_send_codeword_control(mpt1327, &codeword);
		break;
	case DSP_MODE_TRAFFIC:
		rc = mpt1327_send_codeword_traffic(mpt1327, &codeword);
		break;
	default:
		;
	}

	if (rc < 0)
		return 0;

	*bits = mpt1327_encode_codeword(&codeword);
	return 64;
}

static void out_setup(mpt1327_unit_t *unit, uint8_t network_type, int network_id)
{
	char caller_id[32], id[16];

	/* setup call */
	PDEBUG(DMPT1327, DEBUG_INFO, "Setup call to network.\n");
	sprintf(caller_id, "%03d%04d", unit->prefix, unit->ident);
	if (network_id)
		sprintf(id, "%d", network_id);
	else
		id[0] = '\0';
	unit->callref = call_up_setup(caller_id, unit->called_number, network_type, id);
}

static void _cancel_pending_call(mpt1327_t *mpt1327, mpt1327_unit_t *unit)
{
	PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "We are already in a call, the phone might have restarted, so we free old channel first.\n");
	mpt1327_go_idle(unit->tc);
	timer_stop(&unit->timer);
	if (unit->callref) {
		call_up_release(unit->callref, CAUSE_NORMAL);
		unit->callref = 0;
	}
}

void mpt1327_receive_codeword_control(mpt1327_t *mpt1327, mpt1327_codeword_t *codeword)
{
	mpt1327_unit_t *unit;
	mpt1327_t *tc;

	switch (codeword->type) {
	case MPT_RQR: /* register */
		mpt1327_reset_sync(mpt1327); /* message complete */
		unit = get_unit(codeword->params[MPT_PFIX], codeword->params[MPT_IDENT1]);
		PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Radio Unit (Prefix:%d Ident:%d) registers\n", unit->prefix, unit->ident);
		if (unit->tc)
			_cancel_pending_call(mpt1327, unit);
		unit_new_state(unit, UNIT_REGISTER_ACK);
		break;
	case MPT_RQT: /* diversion */
		mpt1327_reset_sync(mpt1327); /* message complete */
		unit = get_unit(codeword->params[MPT_PFIX], codeword->params[MPT_IDENT2]);
		unit->called_ident = codeword->params[MPT_IDENT1];
		PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Radio Unit (Prefix:%d Ident:%d) requests diversion\n", unit->prefix, unit->ident);
		if (unit->tc)
			_cancel_pending_call(mpt1327, unit);
		PDEBUG_CHAN(DMPT1327, DEBUG_NOTICE, "Diversion not supported by TSC, rejecting...\n");
		unit_new_state(unit, UNIT_DIVERSION_REJ);
		break;
	case MPT_RQS: /* simple call */
	case MPT_RQE: /* emergency call */
		mpt1327_reset_sync(mpt1327); /* message complete */
		unit = get_unit(codeword->params[MPT_PFIX], codeword->params[MPT_IDENT2]);
		unit->called_ident = codeword->params[MPT_IDENT1];
		PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Radio Unit (Prefix:%d Ident:%d) calls Ident:%d%s\n", unit->prefix, unit->ident, unit->called_ident, (codeword->type == MPT_RQE) ? " (emergency)" : "");
		if (unit->tc)
			_cancel_pending_call(mpt1327, unit);
		tc = search_free_tc();
		if (!tc) {
			unit_new_state(unit, UNIT_CALLING_REJ);
			PDEBUG_CHAN(DMPT1327, DEBUG_NOTICE, "No free Traffic Channel, call is rejected.\n");
			break;
		}
		if (codeword->params[MPT_EXT]) {
			int exchange;
			unit->called_type = CALLED_TYPE_PBX_SHORT;
			sprintf(unit->called_number, "%d", unit->called_ident);
			exchange = ((codeword->params[MPT_FLAG1] << 1) | codeword->params[MPT_FLAG2]) + 1;
			PDEBUG_CHAN(DMPT1327, DEBUG_INFO, " -> Call to PBX exchange %d, Number %s\n", exchange, unit->called_number);
			unit_new_state(unit, UNIT_GTC_A);
			unit->repeat = REPEAT_GTC;
			out_setup(unit, OSMO_CC_NETWORK_MPT1327_PBX, exchange);
		} else if (unit->called_ident >= IDENT_PSTNSI1 && unit->called_ident < IDENT_PSTNSI1 + 15) {
			unit->called_type = CALLED_TYPE_PSTN_PRE;
			sprintf(unit->called_number, "%d", unit->called_ident - IDENT_PSTNSI1 + 1);
			PDEBUG_CHAN(DMPT1327, DEBUG_INFO, " -> Call to PSTN with pre-arranged Number %s\n", unit->called_number);
			unit_new_state(unit, UNIT_GTC_A);
			unit->repeat = REPEAT_GTC;
			out_setup(unit, OSMO_CC_NETWORK_MPT1327_PSTN, 0);
		} else switch (unit->called_ident) {
		case IDENT_IPFIXI:
			unit->called_type = CALLED_TYPE_INTERPFX;
			PDEBUG_CHAN(DMPT1327, DEBUG_INFO, " -> Call to Unit/Group %d with different Prefix\n", unit->called_ident);
			unit_new_state(unit, UNIT_CALLING_AHYC);
			unit->repeat = REPEAT_AHYC;
			break;
		case IDENT_ALLI:
			unit->called_type = CALLED_TYPE_SYSTEM;
			PDEBUG_CHAN(DMPT1327, DEBUG_INFO, " -> System wide Call\n");
			unit_new_state(unit, UNIT_GTC_P);
			unit->repeat = REPEAT_GTC;
			break;
		case IDENT_PSTNGI:
			if (codeword->params[MPT_FLAG1]) {
				unit->called_type = CALLED_TYPE_PSTN_LONG2;
				PDEBUG_CHAN(DMPT1327, DEBUG_INFO, " -> Call to PSTN with long Number (10..31 Digits)\n");
			} else {
				unit->called_type = CALLED_TYPE_PSTN_LONG1;
				PDEBUG_CHAN(DMPT1327, DEBUG_INFO, " -> Call to PSTN with long Number (1..9 Digits)\n");
			}
			unit_new_state(unit, UNIT_CALLING_AHYC);
			unit->repeat = REPEAT_AHYC;
			break;
		case IDENT_PABXI:
			unit->called_type = CALLED_TYPE_PBX_LONG;
			PDEBUG_CHAN(DMPT1327, DEBUG_INFO, " -> Call to PBX (long number)\n");
			unit_new_state(unit, UNIT_CALLING_AHYC);
			unit->repeat = REPEAT_AHYC;
			break;
		default:
			unit->called_type = CALLED_TYPE_UNIT;
			unit->called_prefix = unit->prefix;
			PDEBUG_CHAN(DMPT1327, DEBUG_INFO, " -> Call to Unit/Group %d (same Prefix)\n", unit->called_ident);
			unit_new_state(unit, UNIT_GTC_P);
			unit->repeat = REPEAT_GTC;
		}
		PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Allocating Traffic Channel %s\n", tc->sender.kanal);
		mpt1327_new_state(tc, STATE_BUSY, unit);
		break;
	case MPT_SAMIS: /* SAMIS response */
		unit = get_unit(mpt1327->rx_sched.data_prefix, mpt1327->rx_sched.data_ident);
		if (unit->state != UNIT_CALLING_SAMIS) {
			PDEBUG_CHAN(DMPT1327, DEBUG_ERROR, "Radio Unit (Prefix:%d Ident:%d) sends SAMIS, but not requested\n", unit->prefix, unit->ident);
			break;
		}
		switch (unit->called_type) {
		case CALLED_TYPE_INTERPFX:
			if (codeword->params[MPT_DESC] != 0x0) {
				PDEBUG_CHAN(DMPT1327, DEBUG_ERROR, "Expecting DESC=%d from Radio Unit, but got DESC=%d, dropping!\n", 0x0, (int)codeword->params[MPT_DESC]);
				return;
			}
			unit->called_prefix = codeword->params[MPT_PARAMETERS1] >> 13;
			unit->called_ident = codeword->params[MPT_PARAMETERS1] & 0x1fff;
			PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Radio Unit (Prefix:%d Ident:%d) calls Prefix:%d Ident:%d\n", unit->prefix, unit->ident, unit->called_prefix, unit->called_ident);
			unit_new_state(unit, UNIT_GTC_B);
			unit->repeat = REPEAT_GTC;
			break;
		case CALLED_TYPE_PSTN_LONG1:
		case CALLED_TYPE_PSTN_LONG2:
			if (codeword->params[MPT_DESC] != 0x1) {
				PDEBUG_CHAN(DMPT1327, DEBUG_ERROR, "Expecting DESC=%d from Radio Unit, but got DESC=%d, dropping!\n", 0x1, (int)codeword->params[MPT_DESC]);
				return;
			}
			unit->called_number[0] = '0';
			unit->called_number[1] = mpt1327_bcd[(codeword->params[MPT_PARAMETERS1] >> 16) & 0xf];
			unit->called_number[2] = mpt1327_bcd[(codeword->params[MPT_PARAMETERS1] >> 12) & 0xf];
			unit->called_number[3] = mpt1327_bcd[(codeword->params[MPT_PARAMETERS1] >> 8) & 0xf];
			unit->called_number[4] = mpt1327_bcd[(codeword->params[MPT_PARAMETERS1] >> 4) & 0xf];
			unit->called_number[5] = mpt1327_bcd[(codeword->params[MPT_PARAMETERS1] >> 0) & 0xf];
			unit->called_number[6] = mpt1327_bcd[(codeword->params[MPT_PARAMETERS2] >> 12) & 0xf];
			unit->called_number[7] = mpt1327_bcd[(codeword->params[MPT_PARAMETERS2] >> 8) & 0xf];
			unit->called_number[8] = mpt1327_bcd[(codeword->params[MPT_PARAMETERS2] >> 4) & 0xf];
			unit->called_number[9] = mpt1327_bcd[(codeword->params[MPT_PARAMETERS2] >> 0) & 0xf];
			unit->called_number[10] = '\0';
			/* schedule reception of one or two words */
			mpt1327->rx_sched.data_num = codeword->params[MPT_PARAMETERS2] >> 16;
			mpt1327->rx_sched.data_count = 0;
			mpt1327->rx_sched.data_word = MPT_SAMIS_DT;
			if (mpt1327->rx_sched.data_num == 0) {
				timer_stop(&unit->timer);
				PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Radio Unit (Prefix:%d Ident:%d) calls Number %s\n", unit->prefix, unit->ident, unit->called_number);
				out_setup(unit, OSMO_CC_NETWORK_MPT1327_PSTN, 0);
				unit_new_state(unit, UNIT_GTC_A);
				unit->repeat = REPEAT_GTC;
			}
			break;
		case CALLED_TYPE_PBX_LONG:
			if (codeword->params[MPT_DESC] != 0x2) {
				PDEBUG_CHAN(DMPT1327, DEBUG_ERROR, "Expecting DESC=%d from Radio Unit, but got DESC=%d, dropping!\n", 0x2, (int)codeword->params[MPT_DESC]);
				return;
			}
			unit->called_number[0] = '0';
			unit->called_number[1] = mpt1327_bcd[(codeword->params[MPT_PARAMETERS1] >> 16) & 0xf];
			unit->called_number[2] = mpt1327_bcd[(codeword->params[MPT_PARAMETERS1] >> 12) & 0xf];
			unit->called_number[3] = mpt1327_bcd[(codeword->params[MPT_PARAMETERS1] >> 8) & 0xf];
			unit->called_number[4] = mpt1327_bcd[(codeword->params[MPT_PARAMETERS1] >> 4) & 0xf];
			unit->called_number[5] = mpt1327_bcd[(codeword->params[MPT_PARAMETERS1] >> 0) & 0xf];
			unit->called_number[6] = mpt1327_bcd[(codeword->params[MPT_PARAMETERS2] >> 12) & 0xf];
			unit->called_number[7] = mpt1327_bcd[(codeword->params[MPT_PARAMETERS2] >> 8) & 0xf];
			unit->called_number[8] = mpt1327_bcd[(codeword->params[MPT_PARAMETERS2] >> 4) & 0xf];
			unit->called_number[9] = mpt1327_bcd[(codeword->params[MPT_PARAMETERS2] >> 0) & 0xf];
			unit->called_number[10] = '\0';
			PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Radio Unit (Prefix:%d Ident:%d) calls Number %s\n", unit->prefix, unit->ident, unit->called_number);
			timer_stop(&unit->timer);
			out_setup(unit, OSMO_CC_NETWORK_MPT1327_PBX, 0);
			unit_new_state(unit, UNIT_GTC_A);
			unit->repeat = REPEAT_GTC;
			break;
		default:
			PDEBUG_CHAN(DMPT1327, DEBUG_ERROR, "Want to receive SAMIS, but called_type not set correctly, please fix!\n");
			abort();
		}
		break;
	case MPT_SAMIS_DT:
		unit = get_unit(mpt1327->rx_sched.data_prefix, mpt1327->rx_sched.data_ident);
		if (unit->state != UNIT_CALLING_SAMIS) {
			PDEBUG_CHAN(DMPT1327, DEBUG_ERROR, "Radio Unit (Prefix:%d Ident:%d) sends SAMIS, but not requested\n", unit->prefix, unit->ident);
			break;
		}
		if (mpt1327->rx_sched.data_count == 1) {
			unit->called_number[10] = mpt1327_bcd[(codeword->params[MPT_BCD11] >> 40) & 0xf];
			unit->called_number[11] = mpt1327_bcd[(codeword->params[MPT_BCD11] >> 36) & 0xf];
			unit->called_number[12] = mpt1327_bcd[(codeword->params[MPT_BCD11] >> 32) & 0xf];
			unit->called_number[13] = mpt1327_bcd[(codeword->params[MPT_BCD11] >> 28) & 0xf];
			unit->called_number[14] = mpt1327_bcd[(codeword->params[MPT_BCD11] >> 24) & 0xf];
			unit->called_number[15] = mpt1327_bcd[(codeword->params[MPT_BCD11] >> 20) & 0xf];
			unit->called_number[16] = mpt1327_bcd[(codeword->params[MPT_BCD11] >> 16) & 0xf];
			unit->called_number[17] = mpt1327_bcd[(codeword->params[MPT_BCD11] >> 12) & 0xf];
			unit->called_number[18] = mpt1327_bcd[(codeword->params[MPT_BCD11] >> 8) & 0xf];
			unit->called_number[19] = mpt1327_bcd[(codeword->params[MPT_BCD11] >> 4) & 0xf];
			unit->called_number[20] = mpt1327_bcd[(codeword->params[MPT_BCD11] >> 0) & 0xf];
			unit->called_number[21] = '\0';
			if (mpt1327->rx_sched.data_num == 1) {
				timer_stop(&unit->timer);
				PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Radio Unit (Prefix:%d Ident:%d) calls Number %s\n", unit->prefix, unit->ident, unit->called_number);
				out_setup(unit, OSMO_CC_NETWORK_MPT1327_PSTN, 0);
				unit_new_state(unit, UNIT_GTC_A);
				unit->repeat = REPEAT_GTC;
			}
		} else {
			unit->called_number[21] = mpt1327_bcd[(codeword->params[MPT_BCD11] >> 40) & 0xf];
			unit->called_number[22] = mpt1327_bcd[(codeword->params[MPT_BCD11] >> 36) & 0xf];
			unit->called_number[23] = mpt1327_bcd[(codeword->params[MPT_BCD11] >> 32) & 0xf];
			unit->called_number[24] = mpt1327_bcd[(codeword->params[MPT_BCD11] >> 28) & 0xf];
			unit->called_number[25] = mpt1327_bcd[(codeword->params[MPT_BCD11] >> 24) & 0xf];
			unit->called_number[26] = mpt1327_bcd[(codeword->params[MPT_BCD11] >> 20) & 0xf];
			unit->called_number[27] = mpt1327_bcd[(codeword->params[MPT_BCD11] >> 16) & 0xf];
			unit->called_number[28] = mpt1327_bcd[(codeword->params[MPT_BCD11] >> 12) & 0xf];
			unit->called_number[29] = mpt1327_bcd[(codeword->params[MPT_BCD11] >> 8) & 0xf];
			unit->called_number[30] = mpt1327_bcd[(codeword->params[MPT_BCD11] >> 4) & 0xf];
			unit->called_number[31] = mpt1327_bcd[(codeword->params[MPT_BCD11] >> 0) & 0xf];
			unit->called_number[32] = '\0';
			mpt1327->rx_sched.data_num = 0; /* just in case it is more than 2 data words */
			timer_stop(&unit->timer);
			PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Radio Unit (Prefix:%d Ident:%d) calls Number %s\n", unit->prefix, unit->ident, unit->called_number);
			out_setup(unit, OSMO_CC_NETWORK_MPT1327_PSTN, 0);
			unit_new_state(unit, UNIT_GTC_A);
			unit->repeat = REPEAT_GTC;
		}
		break;
	case MPT_RQX: /* call cancel */
		unit = get_unit(codeword->params[MPT_PFIX], codeword->params[MPT_IDENT2]);
		unit->called_ident = codeword->params[MPT_IDENT1];
		timer_stop(&unit->timer);
		PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Radio Unit (Prefix:%d Ident:%d) cancels call to %d\n", unit->prefix, unit->ident, unit->called_ident);
		unit_new_state(unit, UNIT_CANCEL_ACK);
		if (unit->tc) {
			PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Free Traffic Channel %s, because unit cancels on control channel\n", unit->tc->sender.kanal);
			mpt1327_go_idle(unit->tc);
		}
		break;
	case MPT_ACKI: /* ack from unit (not ready, wait for RQQ) */
		unit = get_unit(codeword->params[MPT_PFIX], codeword->params[MPT_IDENT1]);
		timer_stop(&unit->timer);
		if (unit->state == UNIT_CALLED_ACK) {
			PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Radio Unit (Prefix:%d Ident:%d) acknowledges call (not yet ready, waiting for RQQ\n", unit->prefix, unit->ident);
			if (unit->callref)
				call_up_alerting(unit->callref);
			break;
		}
		PDEBUG_CHAN(DMPT1327, DEBUG_ERROR, "Radio Unit (Prefix:%d Ident:%d) acknowledges, no call\n", unit->prefix, unit->ident);
		break;
	case MPT_ACK: /* ack from unit */
		unit = get_unit(codeword->params[MPT_PFIX], codeword->params[MPT_IDENT1]);
		timer_stop(&unit->timer);
		if (unit->state == UNIT_CALLED_ACK) {
			PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Radio Unit (Prefix:%d Ident:%d) acknowledges call\n", unit->prefix, unit->ident);
answer:
			if (unit->callref) {
				char connected_id[32];
				sprintf(connected_id, "%03d%04d", unit->prefix, unit->ident);
				call_up_answer(unit->callref, connected_id);
			}
			unit_new_state(unit, UNIT_GTC_B);
			unit->repeat = REPEAT_GTC;
			break;
		}
		PDEBUG_CHAN(DMPT1327, DEBUG_ERROR, "Radio Unit (Prefix:%d Ident:%d) acknowledges, no call\n", unit->prefix, unit->ident);
		break;
	case MPT_RQQ: /* status from radio */
		unit = get_unit(codeword->params[MPT_PFIX], codeword->params[MPT_IDENT2]);
		timer_stop(&unit->timer);
		PDEBUG_CHAN(DMPT1327, DEBUG_ERROR, "Radio Unit (Prefix:%d Ident:%d) sends RRQ with STATUS=%d\n", unit->prefix, unit->ident, (int)codeword->params[MPT_STATUS]);
		switch (codeword->params[MPT_STATUS]) {
		case 0x00:
			if (unit->state == UNIT_CALLED_ACK) {
				PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Radio Unit (Prefix:%d Ident:%d) answers call\n", unit->prefix, unit->ident);
				// NOTE: GTC acknowledges RQQ
				goto answer;
			}
			break;
		case 0x1f:
			PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Radio Unit (Prefix:%d Ident:%d) rejects call, releasing\n", unit->prefix, unit->ident);
			// NOTE: AHYX acknowledges RQQ
			mpt1327_release(unit);
			if (unit->callref) {
				call_up_release(unit->callref, CAUSE_NORMAL);
				unit->callref = 0;
			}
			break;
		}
		break;
	case MPT_ALH: /* control channel Aloha for loopback mode */
	case MPT_ALHS:
	case MPT_ALHD:
	case MPT_ALHE:
	case MPT_ALHR:
	case MPT_ALHX:
	case MPT_ALHF:
		/* schedule reception of CCSC word */
		mpt1327->rx_sched.data_num = 1;
		mpt1327->rx_sched.data_count = 0;
		mpt1327->rx_sched.data_word = MPT_CCSC;
		break;
	default:
		if (mpt1327->sender.loopback)
			return;
		PDEBUG_CHAN(DMPT1327, DEBUG_NOTICE, "Received unsupported codeword '%s' = '%s' on control channel\n", codeword->short_name, codeword->long_name);
	}
}

void mpt1327_receive_codeword_traffic(mpt1327_t *mpt1327, mpt1327_codeword_t *codeword)
{
	mpt1327_unit_t *unit;

	switch (codeword->type) {
	case MPT_MAINT: /* maintenance message */
		unit = get_unit(codeword->params[MPT_PFIX], codeword->params[MPT_IDENT1]);
		if (codeword->params[MPT_CHAN] != mpt1327_channel2chan(mpt1327->band, atoi(mpt1327->sender.kanal))) {
			PDEBUG_CHAN(DMPT1327, DEBUG_NOTICE, "Radio Unit (Prefix:%d Ident:%d) sends maintenance message on wrong channel %d, ignoring!\n", unit->prefix, unit->ident, (int)codeword->params[MPT_CHAN]);
			return;
		}
		if (!unit->tc) {
			PDEBUG_CHAN(DMPT1327, DEBUG_NOTICE, "Radio Unit (Prefix:%d Ident:%d) sends maintenance, but it has no channel assigned, ignoring!\n", unit->prefix, unit->ident);
			return;
		}
		switch (codeword->params[MPT_OPER]) {
		case OPER_PRESSEL_ON:
			if (sysdef.timeout)
				timer_start(&unit->timer, sysdef.timeout);
			mpt1327->pressel_on = 1;
			PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Radio Unit (Prefix:%d Ident:%d) starts transmission\n", unit->prefix, unit->ident);
			break;
		case OPER_PRESSEL_OFF:
			if (sysdef.timeout)
				timer_start(&unit->timer, sysdef.timeout);
			mpt1327->pressel_on = 0;
			PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Radio Unit (Prefix:%d Ident:%d) stops transmission\n", unit->prefix, unit->ident);
			break;
		case OPER_DISCONNECT:
			/* ignore while we send clear message */
			if (unit->state == UNIT_CALL_CLEAR)
				return;
			PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Radio Unit (Prefix:%d Ident:%d) disconnects from channel\n", unit->prefix, unit->ident);
			if (unit->state == UNIT_CALL) {
				timer_stop(&unit->timer);
				PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Free Traffic Channel %s, because the initiator goes on-hook\n", unit->tc->sender.kanal);
				mpt1327_go_idle(unit->tc);
				if (unit->callref) {
					call_up_release(unit->callref, CAUSE_NORMAL);
					unit->callref = 0;
				}
			}
			unit_new_state(unit, UNIT_IDLE);
			break;
		case OPER_PERIODIC:
			if (sysdef.timeout)
				timer_start(&unit->timer, sysdef.timeout);
			mpt1327->pressel_on = 1;
			PDEBUG_CHAN(DMPT1327, DEBUG_INFO, "Radio Unit (Prefix:%d Ident:%d) sends periodic message\n", unit->prefix, unit->ident);
			break;
		}
		break;
	default:
		if (mpt1327->sender.loopback)
			return;
		PDEBUG_CHAN(DMPT1327, DEBUG_NOTICE, "Received unsupported codeword '%s' = '%s' on traffic channel\n", codeword->short_name, codeword->long_name);
	}
}

void mpt1327_receive_codeword(mpt1327_t *mpt1327, uint64_t bits, double quality, double level)
{
	mpt1327_codeword_t codeword;
	int rc;

	PDEBUG_CHAN(DDSP, DEBUG_INFO, "RX Level: %.0f%% Quality=%.0f%%\n", level * 100.0, quality * 100.0);

	rc = mpt1327_decode_codeword(&codeword, (mpt1327->rx_sched.data_num) ? mpt1327->rx_sched.data_word : -1, (mpt1327->sender.loopback) ? MPT_DOWN : MPT_UP, bits);
	if (rc < 0) {
		mpt1327->rx_sched.data_num = 0;
		mpt1327_reset_sync(mpt1327); /* message complete */
		return;
	}

	/* count if we have data words */
	if (mpt1327->rx_sched.data_num)
		mpt1327->rx_sched.data_count++;

	switch (mpt1327->dsp_mode) {
	case DSP_MODE_CONTROL:
		mpt1327_receive_codeword_control(mpt1327, &codeword);
		break;
	case DSP_MODE_TRAFFIC:
		mpt1327_receive_codeword_traffic(mpt1327, &codeword);
		break;
	default:
		;
	}

	/* if all data words are received */
	if (mpt1327->rx_sched.data_num && mpt1327->rx_sched.data_count == mpt1327->rx_sched.data_num)
		mpt1327->rx_sched.data_num = 0;

	/* reset receiver unless there is a pending data word to be received */
	if (mpt1327->rx_sched.data_num == 0)
		mpt1327_reset_sync(mpt1327); /* message complete */
}

void mpt1327_signal_indication(mpt1327_t *mpt1327)
{
	/* restart timer, if enabled */
	if (mpt1327->unit && mpt1327->unit->state == UNIT_CALL) {
		if (sysdef.timeout)
			timer_start(&mpt1327->unit->timer, sysdef.timeout);
	}
}

/* Timeout handling */
static void mpt1327_timeout(struct timer *timer)
{
	mpt1327_t *mpt1327 = (mpt1327_t *)timer->priv;

	switch (mpt1327->state) {
	default:
		break;
	}
}

/*
 * call control (from upper layer)
 */

/* Call control starts call towards mobile station. */
int call_down_setup(int callref, const char __attribute__((unused)) *caller_id, enum number_type __attribute__((unused)) caller_type, const char *dialing)
{
	mpt1327_unit_t *unit;
	mpt1327_t *tc;
	uint8_t prefix;
	uint16_t ident;
	int i;

	/* 1. check if number is invalid, return INVALNUMBER */
	if (strlen(dialing) != 7) {
inval:
		PDEBUG(DMPT1327, DEBUG_NOTICE, "Outgoing call to invalid number '%s', rejecting!\n", dialing);
		return -CAUSE_INVALNUMBER;
	}
	for (i = 0; i < 7; i++) {
		if (dialing[i] < '0' || dialing[i] > '9')
			goto inval;
	}
	prefix = (dialing[0] - '0') * 100 + (dialing[1] - '0') * 10 + (dialing[2] - '0');
	if (prefix > 127) {
		PDEBUG(DMPT1327, DEBUG_NOTICE, "Outgoing call to invalid Prefix '%03d' in number '%s', rejecting! (Prefix must be 000..127)\n", prefix, dialing);
		return -CAUSE_INVALNUMBER;
	}
	ident = atoi(dialing + 3);
	if (ident > 8100 || ident < 1) {
		PDEBUG(DMPT1327, DEBUG_NOTICE, "Outgoing call to invalid Ident '%04d' in number '%s', rejecting! (Ident must be 0001..8100)\n", ident, dialing);
		return -CAUSE_INVALNUMBER;
	}

	/* 2. check if given number is already in a call, return BUSY */
	unit = get_unit(prefix, ident);
	if (unit->state != UNIT_IDLE) {
		PDEBUG(DMPT1327, DEBUG_NOTICE, "Outgoing call to busy Radio Unit, rejecting!\n");
		return -CAUSE_BUSY;
	}

	/* 3. check if all channels are busy, return NOCHANNEL */
	tc = search_free_tc();
	if (!tc) {
		PDEBUG(DMPT1327, DEBUG_NOTICE, "Outgoing call, but no free channel, rejecting!\n");
		return -CAUSE_NOCHANNEL;
	}

	PDEBUG(DMPT1327, DEBUG_INFO, "Outgoing call to Radio Unit (Prefix:%d Ident:%d)\n", unit->prefix, unit->ident);

	/* 4. trying to reach radio unit */
	unit->callref = callref;
	mpt1327_new_state(tc, STATE_BUSY, unit);
	unit_new_state(unit, UNIT_CALLED_AHY);
	unit->repeat = REPEAT_AHY;
	unit->called_prefix = unit->prefix;
	unit->called_ident = unit->ident;

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
	mpt1327_unit_t *unit;

	PDEBUG(DMPT1327, DEBUG_INFO, "Call has been disconnected by network.\n");

	unit = find_unit_callref(callref);
	if (!unit) {
		PDEBUG(DMPT1327, DEBUG_NOTICE, "Outgoing disconnect, but no unit for callref!\n");
		call_up_release(callref, CAUSE_INVALCALLREF);
		return;
	}

	/* Release when not active */
	if (unit->state == UNIT_CALL)
		return;
	PDEBUG(DMPT1327, DEBUG_NOTICE, "Outgoing disconnect, but no call, releasing!\n");
	mpt1327_release(unit);
	unit->callref = 0;

	call_up_release(callref, cause);
}

/* Call control releases call toward mobile station. */
void call_down_release(int callref, __attribute__((unused)) int cause)
{
	mpt1327_unit_t *unit;

	PDEBUG(DMPT1327, DEBUG_INFO, "Call has been released by network, releasing call.\n");

	unit = find_unit_callref(callref);
	if (!unit) {
		PDEBUG(DMPT1327, DEBUG_NOTICE, "Outgoing release, but no unit for callref!\n");
		/* don't send release, because caller already released */
		return;
	}

	PDEBUG(DMPT1327, DEBUG_NOTICE, "Outgoing release, releasing!\n");
	mpt1327_release(unit);
	unit->callref = 0;
}

/* Receive audio from call instance. */
void call_down_audio(int callref, sample_t *samples, int count)
{
	mpt1327_unit_t *unit;

	unit = find_unit_callref(callref);
	if (!unit)
		return;
	if (!unit->tc)
		return;

	if (unit->tc->state == STATE_BUSY && unit->tc->dsp_mode == DSP_MODE_TRAFFIC) {
		sample_t up[(int)((double)count * unit->tc->sender.srstate.factor + 0.5) + 10];
		count = samplerate_upsample(&unit->tc->sender.srstate, samples, count, up);
		jitter_save(&unit->tc->sender.dejitter, up, count);
	}
}

void dump_info(void)
{
	dump_units();
}

void call_down_clock(void) {}

