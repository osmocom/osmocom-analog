/* Radiocom 2000 protocol handling
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

#define CHAN r2000->sender.kanal

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "../common/sample.h"
#include "../common/debug.h"
#include "../common/timer.h"
#include "../common/cause.h"
#include "r2000.h"
//#include "transaction.h"
#include "frame.h"
#include "dsp.h"

#define	CUT_OFF_EMPHASIS_R2000	300 //FIXME: use real cut-off / time constant

#define PAGE_TRIES		2	/* how many times trying to page */
#define IDENT_TIME		3.0	/* time to wait for identity response */
#define ALERT_TIME		60.0	/* time to wait for party to answer */
#define DIAL1_TIME		1.0	/* time to wait for party to dial digits 1..10 */
#define DIAL2_TIME		0.5	/* time to wait for party to dial digits 11..20 */
#define SUSPEND_TIME		1.0	/* time to wait for suspend response */
#define SUPER_TIME1		4.0	/* time to release if not receiving initial supervisory signal */
#define SUPER_TIME2		20.0	/* time to release after loosing supervisory signal */
#define RELEASE_TIME		2.0	/* time to wait for release response */

/* Call reference for calls from station mobile to network
   This offset of 0x400000000 is required for MNCC interface. */
static int new_callref = 0x40000000;

/* definiton of bands and channels */
#define CHANNEL_SPACING	0.0125

static struct r2000_bands {
	int		number;
	const char	*name;
	double		dl_f0;		/* first downlink channel (0) */
	int		channels;	/* number of channels (including 0) */
	double		duplex;		/* duplex distance (uplink below downlink) */
} r2000_bands[] = {
	{  1, "UHF",		424.8000,	256,	10.0 },
	{  3, "VHF A/B",	169.8000,	296,	 4.6 },
	{  4, "VHF 5/6/1",	176.5000,	176,	-8.0 },
	{  5, "VHF 5/6/2",	178.7000,	192,	-8.0 },
	{  6, "VHF 5/6/3",	181.1000,	192,	-8.0 },
	{  7, "VHF 7/8/1",	200.5000,	176,	 8.0 },
	{  8, "VHF 7/8/2",	202.7000,	192,	 8.0 },
	{  9, "VHF 7/8/3",	205.1000,	192,	 8.0 },
	{ 10, "VHF 9/10/1",	208.5000,	176,	-8.0 },
	{ 11, "VHF 9/10/2",	210.7000,	192,	-8.0 },
	{ 12, "VHF 9/10/3",	213.1000,	192,	-8.0 },
	{ 0, NULL, 0.0, 0, 0.0 }
};

void r2000_band_list(void)
{
	int i;

	printf("Bande\tName\t\tChannels\tDownlink\t\tUplink\n");
	printf("--------------------------------------------------------------------------\n");
	for (i = 0; r2000_bands[i].name; i++) {
		printf("%d\t%s%s\t0 .. %d\t%.4f..%.4f MHz\t%5.1f MHz\n",
			r2000_bands[i].number,
			r2000_bands[i].name,
			(strlen(r2000_bands[i].name) >= 8) ? "" : "\t",
			r2000_bands[i].channels - 1,
			r2000_bands[i].dl_f0,
			r2000_bands[i].dl_f0 + CHANNEL_SPACING * (double)(r2000_bands[i].channels - 1),
			-r2000_bands[i].duplex);
	}
}

/* Convert band+channel number to frequency number of base station.
   Set 'uplink' to 1 to get frequency of station mobile. */
double r2000_channel2freq(int band, int channel, int uplink)
{
	int i;
	double freq;

	for (i = 0; r2000_bands[i].name; i++) {
		if (r2000_bands[i].number == band)
			break;
	}
	
	if (!r2000_bands[i].name) {
		PDEBUG(DR2000, DEBUG_NOTICE, "Given band number is invalid! (use '-B list' for valid bands)\n");
		return 0.0;
	}

	if (channel < 0 || channel > r2000_bands[i].channels - 1) {
		PDEBUG(DR2000, DEBUG_NOTICE, "Given channel number %d invalid! (use '-B list' for valid channels)\n", channel);
		return 0.0;
	}

	freq = r2000_bands[i].dl_f0 + CHANNEL_SPACING * (double)(channel);
	if (uplink)
		freq -= r2000_bands[i].duplex;

	return freq * 1e6;
}

const char *r2000_state_name(enum r2000_state state)
{
	static char invalid[16];

	switch (state) {
	case STATE_NULL:
		return "(NULL)";
	case STATE_IDLE:
		return "IDLE";
	case STATE_INSCRIPTION:
		return "INSCRIPTION";
	case STATE_OUT_ASSIGN:
		return "OUT ASSIGN";
	case STATE_IN_ASSIGN:
		return "IN ASSIGN";
	case STATE_RECALL_ASSIGN:
		return "RECALL ASSIGN";
	case STATE_OUT_IDENT:
		return "OUT IDENT";
	case STATE_IN_IDENT:
		return "IN IDENT";
	case STATE_RECALL_IDENT:
		return "RECALL IDENT";
	case STATE_OUT_DIAL1:
		return "OUT DIAL1";
	case STATE_OUT_DIAL2:
		return "OUT DIAL2";
	case STATE_SUSPEND:
		return "SUSPEND";
	case STATE_RECALL_WAIT:
		return "RECALL WAIT";
	case STATE_IN_ALERT:
		return "IN ALERT";
	case STATE_OUT_ALERT:
		return "OUT ALERT";
	case STATE_RECALL_ALERT:
		return "RECALL ALERT";
	case STATE_ACTIVE:
		return "ACTIVE";
	case STATE_RELEASE_CC:
		return "RELEASE CC";
	case STATE_RELEASE_TC:
		return "RELEASE TC";
	}

	sprintf(invalid, "invalid(%d)", state);
	return invalid;
}

static const char *print_subscriber_subscr(r2000_subscriber_t *subscr);

void r2000_display_status(void)
{
	sender_t *sender;
	r2000_t *r2000;

	display_status_start();
	for (sender = sender_head; sender; sender = sender->next) {
		r2000 = (r2000_t *) sender;
		display_status_channel(r2000->sender.kanal, chan_type_short_name(r2000->sysinfo.chan_type), r2000_state_name(r2000->state));
		if (r2000->state != STATE_IDLE) {
			char result[32];
			sprintf(result, "%s", print_subscriber_subscr(&r2000->subscriber));
			display_status_subscriber(result, NULL);
		}
	}
	display_status_end();
}

static struct r2000_channels {
	enum r2000_chan_type chan_type;
	const char *short_name;
	const char *long_name;
} r2000_channels[] = {
	{ CHAN_TYPE_CC,		"CC",	"control channel" },
	{ CHAN_TYPE_TC,		"TC",	"taffic channel" },
	{ CHAN_TYPE_CC_TC,	"CC/TC","combined control & taffic" },
	{ 0, NULL, NULL }
};

void r2000_channel_list(void)
{
	int i;

	printf("Type\t\tDescription\n");
	printf("------------------------------------------------------------------------\n");
	for (i = 0; r2000_channels[i].long_name; i++)
		printf("%s%s\t%s\n", r2000_channels[i].short_name, (strlen(r2000_channels[i].short_name) >= 8) ? "" : "\t", r2000_channels[i].long_name);
}

int r2000_channel_by_short_name(const char *short_name)
{
	int i;

	for (i = 0; r2000_channels[i].short_name; i++) {
		if (!strcasecmp(r2000_channels[i].short_name, short_name)) {
			PDEBUG(DR2000, DEBUG_INFO, "Selecting channel '%s' = %s\n", r2000_channels[i].short_name, r2000_channels[i].long_name);
			return r2000_channels[i].chan_type;
		}
	}

	return -1;
}

const char *chan_type_short_name(enum r2000_chan_type chan_type)
{
	int i;

	for (i = 0; r2000_channels[i].short_name; i++) {
		if (r2000_channels[i].chan_type == chan_type)
			return r2000_channels[i].short_name;
	}

	return "invalid";
}

const char *chan_type_long_name(enum r2000_chan_type chan_type)
{
	int i;

	for (i = 0; r2000_channels[i].long_name; i++) {
		if (r2000_channels[i].chan_type == chan_type)
			return r2000_channels[i].long_name;
	}

	return "invalid";
}

static void r2000_new_state(r2000_t *r2000, enum r2000_state new_state)
{
	if (r2000->state == new_state)
		return;
	PDEBUG_CHAN(DR2000, DEBUG_DEBUG, "State change: %s -> %s\n", r2000_state_name(r2000->state), r2000_state_name(new_state));
	r2000->state = new_state;
	r2000_display_status();
	r2000->tx_frame_count = 0;
}

/* used to print station mobile data */
static const char *print_subscriber_frame(frame_t *frame)
{
	static char result[32];

	sprintf(result, "%d,%03d,%05d", frame->sm_type, frame->sm_relais, frame->sm_mor);

	return result;
}
static const char *print_subscriber_subscr(r2000_subscriber_t *subscr)
{
	static char result[32];

	sprintf(result, "%d,%03d,%05d", subscr->type, subscr->relais, subscr->mor);

	return result;
}

/* convert station mobile id to 9 digits caller data */
static const char *subscriber2string(r2000_subscriber_t *subscr)
{
	static char result[32];

	sprintf(result, "%d%03d%05d", subscr->type, subscr->relais, subscr->mor);

	return result;
}

/* convert 9-digits dial string to station mobile data */
static int string2subscriber(const char *dialstring, r2000_subscriber_t *subscr)
{
	char check[6];
	int type, relais, mor;
	int i;

	if (strlen(dialstring) != 9) {
		PDEBUG(DR2000, DEBUG_NOTICE, "Wrong number of digits, use 9 digits: TRRRXXXXX (T=type, R=relais, X=mobile number)\n");
		return -1;
	}

	for (i = 0; i < (int)strlen(dialstring); i++) {
		if (dialstring[i] < '0' || dialstring[i] > '9') {
			PDEBUG(DR2000, DEBUG_NOTICE, "Invalid digit in dial string, use only 0..9.\n");
			return -1;
		}
	}

	memcpy(check, dialstring, 1);
	check[1] = '\0';
	type = atoi(check);
	if (type < 1 || type > 511) {
		PDEBUG(DR2000, DEBUG_NOTICE, "Invalid station type in dial string, use 0..7 as station mobile type.\n");
		return -1;
	}

	memcpy(check, dialstring + 1, 3);
	check[3] = '\0';
	relais = atoi(check);
	if (relais < 1 || relais > 511) {
		PDEBUG(DR2000, DEBUG_NOTICE, "Invalid relais number in dial string, use 000..511 as relais number.\n");
		return -1;
	}

	memcpy(check, dialstring + 4, 5);
	check[5] = '\0';
	mor = atoi(check);
	if (mor > 65535) {
		PDEBUG(DR2000, DEBUG_NOTICE, "Invalid mobile number in dial string, use 00000..65535 as mobile number.\n");
		return -1;
	}

	subscr->type = type;
	subscr->relais = relais;
	subscr->mor = mor;
	return 0;
}

static int match_voie(r2000_t *r2000, frame_t *frame, uint8_t voie)
{
	if (frame->voie == 0 && voie == 1) {
		PDEBUG_CHAN(DR2000, DEBUG_NOTICE, "Frame for control channel, but expecting traffic channel, ignoring. (maybe radio noise)\n");
		return 0;
	}
	if (frame->voie == 1 && voie == 0) {
		PDEBUG_CHAN(DR2000, DEBUG_NOTICE, "Frame for traffic channel, but expecting control channel, ignoring. (maybe radio noise)\n");
		return 0;
	}

	return 1;
}

static int match_channel(r2000_t *r2000, frame_t *frame)
{
	if (frame->channel != r2000->sender.kanal) {
		PDEBUG_CHAN(DR2000, DEBUG_NOTICE, "Frame for different channel %d received, ignoring.\n", frame->channel);
		return 0;
	}

	return 1;
}

static int match_relais(r2000_t *r2000, frame_t *frame)
{
	if (frame->relais != r2000->sysinfo.relais) {
		PDEBUG_CHAN(DR2000, DEBUG_NOTICE, "Frame for different relais %d received, ignoring.\n", frame->relais);
		return 0;
	}

	return 1;
}

static int match_subscriber(r2000_t *r2000, frame_t *frame)
{
	/* ignore dialing messages, because subscriber info is not used in there  */
	if (frame->message == 19 || frame->message == 20)
		return 1;

	if (r2000->subscriber.relais != frame->sm_relais
	 || r2000->subscriber.mor != frame->sm_mor) {
		PDEBUG_CHAN(DR2000, DEBUG_NOTICE, "Frame for different subscriber '%s' received, ignoring.\n", print_subscriber_frame(frame));
		return 0;
	}

	return 1;
}

/* convert nconv to supervisory digit to be transmitted to phone */
uint8_t r2000_encode_super(r2000_t *r2000)
{
	uint8_t super, nconv, relais;

	nconv = r2000->sysinfo.nconv;
	relais = r2000->sysinfo.relais & 0xf;

	/* LSB first */
	super = ((nconv << 2) & 0x04)
	      | (nconv & 0x02)
	      | ((nconv >> 2) & 0x01)
	      | ((relais << 6) & 0x40)
	      | ((relais << 4) & 0x20)
	      | ((relais << 2) & 0x10)
	      | (relais & 0x08);

	PDEBUG_CHAN(DDSP, DEBUG_INFO, "TX Supervisory: NCONV: %d relais (4 lowest bits): %d\n", nconv, relais);

	return super ^ 0x7f;
}

static void r2000_timeout(struct timer *timer);

/* Create transceiver instance and link to a list. */
int r2000_create(int band, int channel, enum r2000_chan_type chan_type, const char *audiodev, int use_sdr, int samplerate, double rx_gain, int pre_emphasis, int de_emphasis, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, const char *read_tx_wave, uint16_t relais, uint8_t deport, uint8_t agi, uint8_t sm_power, uint8_t taxe, uint8_t crins, int destruction, uint8_t nconv, int recall, int loopback)
{
	sender_t *sender;
	r2000_t *r2000 = NULL;
	int rc;

	/* check channel matching and set deviation factor */
	if (r2000_channel2freq(band, channel, 0) == 0.0)
		return -EINVAL;

	for (sender = sender_head; sender; sender = sender->next) {
		r2000 = (r2000_t *)sender;
		if ((r2000->sysinfo.chan_type == CHAN_TYPE_CC || r2000->sysinfo.chan_type == CHAN_TYPE_CC_TC)
		 && (chan_type == CHAN_TYPE_CC || chan_type == CHAN_TYPE_CC_TC)) {
			PDEBUG(DCNETZ, DEBUG_NOTICE, "More than one control channel is not supported, please use traffic channels!\n");
			return -EINVAL;
		}
	}

	r2000 = calloc(1, sizeof(r2000_t));
	if (!r2000) {
		PDEBUG(DR2000, DEBUG_ERROR, "No memory!\n");
		return -ENOMEM;
	}

	PDEBUG(DR2000, DEBUG_DEBUG, "Creating 'Radiocom 2000' instance for channel = %d (sample rate %d).\n", channel, samplerate);

	/* init general part of transceiver */
	rc = sender_create(&r2000->sender, channel, r2000_channel2freq(band, channel, 0), r2000_channel2freq(band, channel, 1), audiodev, use_sdr, samplerate, rx_gain, 0, 0, write_rx_wave, write_tx_wave, read_rx_wave, read_tx_wave, loopback, 0, PAGING_SIGNAL_NONE);
	if (rc < 0) {
		PDEBUG(DR2000, DEBUG_ERROR, "Failed to init transceiver process!\n");
		goto error;
	}

	timer_init(&r2000->timer, r2000_timeout, r2000);
	r2000->sysinfo.relais = relais;
	r2000->sysinfo.chan_type = chan_type;
	r2000->sysinfo.deport = deport;
	r2000->sysinfo.agi = agi;
	r2000->sysinfo.sm_power = sm_power;
	r2000->sysinfo.taxe = taxe;
	r2000->sysinfo.crins = crins;
	r2000->sysinfo.nconv = nconv;
	r2000->sysinfo.recall = recall;
	if (crins == 3 && destruction != 2342) {
		PDEBUG(DR2000, DEBUG_ERROR, "Crins is 3, but destruction is not confirmed, please fix!\n");
		abort();
	}
	r2000->compandor = 1;

	r2000->pre_emphasis = pre_emphasis;
	r2000->de_emphasis = de_emphasis;
	rc = init_emphasis(&r2000->estate, samplerate, CUT_OFF_EMPHASIS_R2000);
	if (rc < 0)
		goto error;

	r2000->pre_emphasis = pre_emphasis;
	r2000->de_emphasis = de_emphasis;
	rc = init_emphasis(&r2000->estate, samplerate, CUT_OFF_EMPHASIS_R2000);
	if (rc < 0)
		goto error;

	/* init audio processing */
	rc = dsp_init_sender(r2000);
	if (rc < 0) {
		PDEBUG(DR2000, DEBUG_ERROR, "Failed to init audio processing!\n");
		goto error;
	}

	/* go into idle state */
	r2000_go_idle(r2000);

	PDEBUG(DR2000, DEBUG_NOTICE, "Created channel #%d of type '%s' = %s\n", channel, chan_type_short_name(chan_type), chan_type_long_name(chan_type));

	return 0;

error:
	r2000_destroy(&r2000->sender);

	return rc;
}

void r2000_check_channels(void)
{
	sender_t *sender;
	r2000_t *r2000;
	int cc = 0, tc = 0, combined = 0;
	int note = 0;

	for (sender = sender_head; sender; sender = sender->next) {
		r2000 = (r2000_t *) sender;
		if (r2000->sysinfo.chan_type == CHAN_TYPE_CC)
			cc = 1;
		if (r2000->sysinfo.chan_type == CHAN_TYPE_TC)
			tc = 1;
		if (r2000->sysinfo.chan_type == CHAN_TYPE_CC_TC) {
			cc = 1;
			tc = 1;
			combined = 1;
		}
	}
	if (cc && !tc) {
		if (note)
			PDEBUG(DNMT, DEBUG_NOTICE, "\n");
		PDEBUG(DNMT, DEBUG_NOTICE, "*** Selected channel(s) can be used for control only.\n");
		PDEBUG(DNMT, DEBUG_NOTICE, "*** No call from the mobile phone is possible on this channel.\n");
		PDEBUG(DNMT, DEBUG_NOTICE, "*** Use combined 'CC/TC' instead!\n");
		note = 1;
	}
	if (tc && !cc) {
		if (note)
			PDEBUG(DNMT, DEBUG_NOTICE, "\n");
		PDEBUG(DNMT, DEBUG_NOTICE, "*** Selected channel(s) can be used for traffic only.\n");
		PDEBUG(DNMT, DEBUG_NOTICE, "*** No call is possible at all!\n");
		PDEBUG(DNMT, DEBUG_NOTICE, "*** Use combined 'CC/TC' instead!\n");
		note = 1;
	}
	if (combined) {
		if (note)
			PDEBUG(DNMT, DEBUG_NOTICE, "\n");
		PDEBUG(DNMT, DEBUG_NOTICE, "*** Selected combined 'CC/TC' some phones might reject this.\n");
		note = 1;
	}
}

/* Destroy transceiver instance and unlink from list. */
void r2000_destroy(sender_t *sender)
{
	r2000_t *r2000 = (r2000_t *) sender;

	PDEBUG(DR2000, DEBUG_DEBUG, "Destroying 'Radiocom 2000' instance for channel = %d.\n", sender->kanal);
	dsp_cleanup_sender(r2000);
	timer_exit(&r2000->timer);
	sender_destroy(&r2000->sender);
	free(r2000);
}

/* go idle and return to frame mode */
void r2000_go_idle(r2000_t *r2000)
{
	timer_stop(&r2000->timer);

	if (r2000->callref) {
		PDEBUG(DR2000, DEBUG_ERROR, "Going idle, but still having callref, please fix!\n");
		call_in_release(r2000->callref, CAUSE_NORMAL);
		r2000->callref = 0;
	}

	if (r2000->sysinfo.chan_type == CHAN_TYPE_TC) {
		PDEBUG_CHAN(DR2000, DEBUG_INFO, "Entering IDLE state, no transmission at relais %d on %s.\n", r2000->sysinfo.relais, chan_type_long_name(r2000->sysinfo.chan_type));
		r2000_set_dsp_mode(r2000, DSP_MODE_OFF, -1);
	} else {
		PDEBUG_CHAN(DR2000, DEBUG_INFO, "Entering IDLE state, sending idle frames of relais %d on %s.\n", r2000->sysinfo.relais, chan_type_long_name(r2000->sysinfo.chan_type));
		r2000_set_dsp_mode(r2000, DSP_MODE_FRAME, (r2000->sender.loopback) ? r2000_encode_super(r2000) : -1);
	}
	r2000_new_state(r2000, STATE_IDLE);

//	r2000_set_dsp_mode(r2000, DSP_MODE_AUDIO, r2000_encode_super(r2000));
}

/* release towards station mobile */
void r2000_release(r2000_t *r2000)
{
	if (r2000->state == STATE_IDLE
	 || r2000->state == STATE_OUT_ASSIGN
	 || r2000->state == STATE_IN_ASSIGN
	 || r2000->state == STATE_RECALL_ASSIGN
	 || r2000->state == STATE_RECALL_WAIT) {
	 	/* release on CC */
		r2000_new_state(r2000, STATE_RELEASE_CC);
		timer_start(&r2000->timer, RELEASE_TIME);
	} else {
		/* release on TC */
		r2000_new_state(r2000, STATE_RELEASE_TC);
		timer_start(&r2000->timer, RELEASE_TIME);
	}
	r2000_set_dsp_mode(r2000, DSP_MODE_FRAME, -1);
}

static void r2000_page(r2000_t *r2000, int try, enum r2000_state state)
{
        PDEBUG_CHAN(DR2000, DEBUG_INFO, "Entering paging state (try %d), sending 'Appel' to '%s'.\n", try, print_subscriber_subscr(&r2000->subscriber));
        r2000_new_state(r2000, state);
        r2000->page_try = try;
}

static r2000_t *get_free_chan(enum r2000_chan_type chan_type)
{
	sender_t *sender;
	r2000_t *r2000, *combined = NULL;

	for (sender = sender_head; sender; sender = sender->next) {
		r2000 = (r2000_t *) sender;
		/* only search for idle channel */
		if (r2000->state != STATE_IDLE)
			continue;
		/* found exactly what we want */
		if (r2000->sysinfo.chan_type == chan_type)
			return r2000;
		/* use combined channel as alternative */
		if (!combined && r2000->sysinfo.chan_type == CHAN_TYPE_CC_TC)
			combined = r2000;
	}

	/* return alternative, if any */
	return combined;
}

/* try to move call to given channel, release callref, if not possible */
static r2000_t *move_call_to_chan(r2000_t *old_r2000, enum r2000_chan_type chan_type)
{
	r2000_t *new_r2000 = get_free_chan(chan_type);

	/* no free channel, reuse combined channel, if possible, or release call */
	if (!new_r2000 && old_r2000->sysinfo.chan_type == CHAN_TYPE_CC_TC) {
		PDEBUG(DR2000, DEBUG_NOTICE, "No %s found, straying on %s!\n", chan_type_long_name(chan_type), chan_type_long_name(old_r2000->sysinfo.chan_type));
		return old_r2000;
	}
	if (!new_r2000) {
		PDEBUG(DR2000, DEBUG_NOTICE, "Cannot move us to %s, because there is no free channel!\n", chan_type_long_name(chan_type));
		if (old_r2000->callref) {
			PDEBUG(DR2000, DEBUG_NOTICE, "Failed to assign channel, releasing towards network\n");
			call_in_release(old_r2000->callref, CAUSE_NOCHANNEL);
			old_r2000->callref = 0;
		}
		r2000_release(old_r2000);
		return NULL;
	}

	/* move subscriber */
	memcpy(&new_r2000->subscriber, &old_r2000->subscriber, sizeof(r2000_subscriber_t));

	/* move callref */
	new_r2000->callref = old_r2000->callref;

	/* move dsp mode */
	r2000_set_dsp_mode(new_r2000, old_r2000->dsp_mode, -1);

	/* move call state */
	r2000_new_state(new_r2000, old_r2000->state);

	/* cleanup old channel */
	old_r2000->callref = 0;
	r2000_go_idle(old_r2000);

	return new_r2000;
}

/*
 * idle process
 */

/* trasmit beacon */
static void tx_idle(r2000_t __attribute__((unused)) *r2000, frame_t *frame)
{
	frame->voie = 1;
	frame->message = 1;
}

/*
 * registration process
 */

/* receive registration */
static void rx_idle(r2000_t *r2000, frame_t *frame)
{
	if (!match_voie(r2000, frame, 0))
		return;
	if (!match_channel(r2000, frame))
		return;
	if (!match_relais(r2000, frame))
		return;

	switch(frame->message) {
	case 0:
		/* inscription */
		r2000->subscriber.type = frame->sm_type;
		r2000->subscriber.relais = frame->sm_relais;
		r2000->subscriber.mor = frame->sm_mor;

		PDEBUG_CHAN(DR2000, DEBUG_INFO, "Received inscription from station mobile '%s'\n", print_subscriber_subscr(&r2000->subscriber));
		PDEBUG_CHAN(DR2000, DEBUG_INFO, " -> Mobile Type: %d'\n", r2000->subscriber.type);
		PDEBUG_CHAN(DR2000, DEBUG_INFO, " -> Home Relais: %d'\n", r2000->subscriber.relais);
		PDEBUG_CHAN(DR2000, DEBUG_INFO, " -> Mobile ID: %d'\n", r2000->subscriber.mor);
		PDEBUG_CHAN(DR2000, DEBUG_INFO, " (Use '%s' as dial string to call the station mobile.)'\n", subscriber2string(&r2000->subscriber));

		r2000_new_state(r2000, STATE_INSCRIPTION);
		break;
	case 1:
	case 3:
		/* call request */
		r2000->subscriber.type = frame->sm_type;
		r2000->subscriber.relais = frame->sm_relais;
		r2000->subscriber.mor = frame->sm_mor;

		PDEBUG_CHAN(DR2000, DEBUG_INFO, "Received outgoing call from station mobile '%s'\n", print_subscriber_frame(frame));

		r2000_t *tc = get_free_chan(CHAN_TYPE_TC);
		if (!tc) {
			PDEBUG_CHAN(DR2000, DEBUG_NOTICE, "Rejecting mobile originated call, no free traffic channel\n");
			r2000_release(r2000);
			return;
		}
		r2000_new_state(r2000, STATE_OUT_ASSIGN);
		break;
	default:
		PDEBUG_CHAN(DR2000, DEBUG_DEBUG, "Dropping frame %s in state %s\n", r2000_frame_name(frame->message, SM_TO_REL), r2000_state_name(r2000->state));
	}
}

/* confirm registration */
static void tx_inscription(r2000_t *r2000, frame_t *frame)
{
	frame->voie = 1;
	frame->message = 0;
	frame->sm_type = r2000->subscriber.type;
	frame->sm_relais = r2000->subscriber.relais;
	frame->sm_mor = r2000->subscriber.mor;
	frame->crins = r2000->sysinfo.crins;

	PDEBUG_CHAN(DR2000, DEBUG_INFO, "Sending inscription acknowledge\n");

	r2000_go_idle(r2000);
}

/*
 * channel assignment process
 */

/* confirm dialing, assign outgoing call */
static void tx_out_assign(r2000_t *r2000, frame_t *frame)
{
	/* NOTE: We can only send this frame once, because afterwards we
	 * have moved to the new channel already!
	 */

	/* move us to tc */
	r2000_t *tc = move_call_to_chan(r2000, CHAN_TYPE_TC);
	if (!tc) {
		tx_idle(r2000, frame);
		return;
	}

	frame->voie = 1;
	frame->message = 5;
	frame->sm_type = r2000->subscriber.type;
	frame->sm_relais = r2000->subscriber.relais;
	frame->sm_mor = r2000->subscriber.mor;
	frame->chan_assign = tc->sender.kanal;

	PDEBUG_CHAN(DR2000, DEBUG_INFO, "Sending outgoing assignment from channel %d to %d\n", r2000->sender.kanal, tc->sender.kanal);

	r2000_new_state(tc, (tc->state == STATE_OUT_ASSIGN) ? STATE_OUT_IDENT : STATE_RECALL_IDENT);
	timer_start(&tc->timer, IDENT_TIME);
}

/* page phone, assign incoming call */
static void tx_in_assign(r2000_t *r2000, frame_t *frame)
{
	/* NOTE: We can only send this frame once, because afterwards we
	 * have moved to the new channel already!
	 */

	/* move us to tc */
	r2000_t *tc = move_call_to_chan(r2000, CHAN_TYPE_TC);
	if (!tc) {
		tx_idle(r2000, frame);
		return;
	}

	frame->voie = 1;
	frame->message = 3;
	frame->sm_type = r2000->subscriber.type;
	frame->sm_relais = r2000->subscriber.relais;
	frame->sm_mor = r2000->subscriber.mor;
	frame->chan_assign = tc->sender.kanal;

	PDEBUG_CHAN(DR2000, DEBUG_INFO, "Sending incoming assignment from channel %d to %d\n", r2000->sender.kanal, tc->sender.kanal);

	r2000_new_state(tc, STATE_IN_IDENT);
	timer_start(&tc->timer, IDENT_TIME);
}

/*
 * identity process
 */

/* identity request on assigned channel */
static void tx_ident(r2000_t *r2000, frame_t *frame)
{
	frame->voie = 0;
	frame->message = 16;
	frame->sm_type = r2000->subscriber.type;
	frame->sm_relais = r2000->subscriber.relais;
	frame->sm_mor = r2000->subscriber.mor;

	if (r2000->tx_frame_count == 1)
		PDEBUG_CHAN(DR2000, DEBUG_INFO, "Sending identity requrest\n");
}

/* receive identity response */
static void rx_ident(r2000_t *r2000, frame_t *frame)
{
	if (!match_voie(r2000, frame, 1))
		return;
	if (!match_channel(r2000, frame))
		return;
	if (!match_relais(r2000, frame))
		return;
	if (!match_subscriber(r2000, frame))
		return;

	switch(frame->message) {
	case 16:
		/* identity response */
		PDEBUG_CHAN(DR2000, DEBUG_INFO, "Received identity response from station mobile '%s'\n", print_subscriber_frame(frame));

		switch (r2000->state) {
		case STATE_IN_IDENT:
			/* alert the phone */
			r2000_new_state(r2000, STATE_IN_ALERT);
			timer_start(&r2000->timer, ALERT_TIME);
			call_in_alerting(r2000->callref);
			break;
		case STATE_RECALL_IDENT:
			/* alert the phone */
			r2000_new_state(r2000, STATE_RECALL_ALERT);
			timer_start(&r2000->timer, ALERT_TIME);
			break;
		case STATE_OUT_IDENT:
			/* request dial string */
			r2000_new_state(r2000, STATE_OUT_DIAL1);
			timer_start(&r2000->timer, DIAL1_TIME);
			break;
		default:
			break;
		}
		break;
	default:
		PDEBUG_CHAN(DR2000, DEBUG_DEBUG, "Dropping frame %s in state %s\n", r2000_frame_name(frame->message, SM_TO_REL), r2000_state_name(r2000->state));
	}
}

/* no identity response from phone */
static void timeout_out_ident(r2000_t *r2000)
{
	PDEBUG_CHAN(DR2000, DEBUG_INFO, "Timeout receiving identity (outgoing call)\n");

	r2000_go_idle(r2000);
}

static void timeout_in_ident(r2000_t *r2000)
{
	if (r2000->state == STATE_IN_IDENT)
		PDEBUG_CHAN(DR2000, DEBUG_INFO, "Timeout receiving identity (incoming call)\n");
	else
		PDEBUG_CHAN(DR2000, DEBUG_INFO, "Timeout receiving identity (recalling outgoing call)\n");

	/* move us back to cc */
	r2000 = move_call_to_chan(r2000, CHAN_TYPE_CC);
	if (!r2000)
		return;

	/* page again ... */
	if (--r2000->page_try) {
		r2000_page(r2000, r2000->page_try, (r2000->callref) ? STATE_IN_ASSIGN: STATE_RECALL_ASSIGN);
		return;
	}

	/* ... or release */
	PDEBUG_CHAN(DR2000, DEBUG_NOTICE, "Phone does not response, releasing towards network\n");
	call_in_release(r2000->callref, CAUSE_OUTOFORDER);
	r2000->callref = 0;
	r2000_release(r2000);
}

/*
 * alerting process (mobile rings)
 */

static void tx_invitation(r2000_t *r2000, frame_t *frame, uint16_t invitation, uint8_t nconv)
{
	frame->voie = 0;
	frame->message = 17;
	frame->sm_type = r2000->subscriber.type;
	frame->sm_relais = r2000->subscriber.relais;
	frame->sm_mor = r2000->subscriber.mor;
	frame->invitation = invitation;
	frame->nconv = nconv;
}

/* alert the phone */
static void tx_alert(r2000_t *r2000, frame_t *frame)
{
	tx_invitation(r2000, frame, 3, r2000->sysinfo.nconv);

	if (r2000->tx_frame_count == 1)
		PDEBUG_CHAN(DR2000, DEBUG_INFO, "Sending answer invitation to station mobile\n");
}

static int setup_call(r2000_t *r2000)
{
	int callref = ++new_callref;
	int rc;

	/* make call toward network */
	PDEBUG(DR2000, DEBUG_INFO, "Setup call to network.\n");
	rc = call_in_setup(callref, subscriber2string(&r2000->subscriber), r2000->subscriber.dialing);
	if (rc < 0) {
		PDEBUG(DR2000, DEBUG_NOTICE, "Call rejected (cause %d), releasing.\n", -rc);
		r2000_release(r2000);
		return rc;
	}
	r2000->callref = callref;

	return 0;
}

/* receive answer */
static void rx_alert(r2000_t *r2000, frame_t *frame)
{
	if (!match_voie(r2000, frame, 1))
		return;
	if (!match_channel(r2000, frame))
		return;
	if (!match_relais(r2000, frame))
		return;
	if (!match_subscriber(r2000, frame))
		return;

	switch(frame->message) {
	case 17:
		/* answer */
		PDEBUG_CHAN(DR2000, DEBUG_INFO, "Received answer from station mobile '%s'\n", print_subscriber_frame(frame));

		switch (r2000->state) {
		case STATE_IN_ALERT:
			/* answer incomming call */
			PDEBUG(DR2000, DEBUG_INFO, "Answer call to network.\n");
			call_in_answer(r2000->callref, subscriber2string(&r2000->subscriber));
			break;
		case STATE_OUT_ALERT:
			/* setup call, possible r2000_release() is called there! */
			if (setup_call(r2000) < 0)
				return;
			break;
		default:
			/* answer after recall, stop recall tone */
			call_tone_recall(r2000->callref, 0);
			break;
		}
		/* go active */
		timer_stop(&r2000->timer);
		r2000_new_state(r2000, STATE_ACTIVE);
		r2000_set_dsp_mode(r2000, DSP_MODE_AUDIO_TX, r2000_encode_super(r2000));
		/* start supervisory timer */
		timer_start(&r2000->timer, SUPER_TIME1);
		break;
	default:
		PDEBUG_CHAN(DR2000, DEBUG_DEBUG, "Dropping frame %s in state %s\n", r2000_frame_name(frame->message, SM_TO_REL), r2000_state_name(r2000->state));
	}
}

/* no answer */
static void timeout_alert(r2000_t *r2000)
{
	PDEBUG_CHAN(DR2000, DEBUG_INFO, "Timeout while alerting\n");

	PDEBUG_CHAN(DR2000, DEBUG_NOTICE, "Phone does not response, releasing towards network\n");
	if (r2000->callref) {
		call_in_release(r2000->callref, CAUSE_NOANSWER);
		r2000->callref = 0;
	}
	r2000_release(r2000);
}

/*
 * dialing process (mobile dials)
 */

/* request digits from the phone */
static void tx_out_dial(r2000_t *r2000, frame_t *frame)
{
	tx_invitation(r2000, frame, 10, 0);

	if (r2000->tx_frame_count == 1)
		PDEBUG_CHAN(DR2000, DEBUG_INFO, "Sending dialing invitation to station mobile\n");
}

/* receive digits */
static void rx_out_dial1(r2000_t *r2000, frame_t *frame)
{
	int i;

	if (!match_voie(r2000, frame, 1))
		return;
	if (!match_channel(r2000, frame))
		return;
	if (!match_relais(r2000, frame))
		return;

	switch(frame->message) {
	case 19:
		/* digits */
		for (i = 0; i < 10; i++)
			r2000->subscriber.dialing[i] = frame->digit[i] + '0';
		r2000->subscriber.dialing[10] = '\0';

		PDEBUG_CHAN(DR2000, DEBUG_INFO, "Received digits 1..10 from station mobile: %s\n", r2000->subscriber.dialing);

		r2000_new_state(r2000, STATE_OUT_DIAL2);
		timer_start(&r2000->timer, DIAL2_TIME);
		break;
	default:
		PDEBUG_CHAN(DR2000, DEBUG_DEBUG, "Dropping frame %s in state %s\n", r2000_frame_name(frame->message, SM_TO_REL), r2000_state_name(r2000->state));
	}
}

/* no digits */
static void timeout_out_dial1(r2000_t *r2000)
{
	PDEBUG_CHAN(DR2000, DEBUG_INFO, "Timeout while receiving digits (outgoing call)\n");

	r2000_release(r2000);
}

/* receive digits */
static void rx_out_dial2(r2000_t *r2000, frame_t *frame)
{
	int i;

	if (!match_voie(r2000, frame, 1))
		return;
	if (!match_channel(r2000, frame))
		return;
	if (!match_relais(r2000, frame))
		return;

	switch(frame->message) {
	case 20:
		/* digits */
		for (i = 0; i < 10; i++)
			r2000->subscriber.dialing[i + 10] = frame->digit[i] + '0';
		r2000->subscriber.dialing[20] = '\0';

		PDEBUG_CHAN(DR2000, DEBUG_INFO, "Received digits 11..20 from station mobile: %s\n", r2000->subscriber.dialing);

		if (r2000->sysinfo.recall) {
			PDEBUG_CHAN(DR2000, DEBUG_INFO, "Suspending call until called party has answered\n");
			r2000_new_state(r2000, STATE_SUSPEND);
			timer_start(&r2000->timer, SUSPEND_TIME);
		} else {
			r2000_new_state(r2000, STATE_OUT_ALERT);
			timer_start(&r2000->timer, ALERT_TIME);
		}
		break;
	default:
		PDEBUG_CHAN(DR2000, DEBUG_DEBUG, "Dropping frame %s in state %s\n", r2000_frame_name(frame->message, SM_TO_REL), r2000_state_name(r2000->state));
	}
}

/* no additional digits */
static void timeout_out_dial2(r2000_t *r2000)
{
	PDEBUG_CHAN(DR2000, DEBUG_INFO, "Phone does not send digits 11..20\n");

	if (r2000->sysinfo.recall) {
		PDEBUG_CHAN(DR2000, DEBUG_INFO, "Suspending call until called party has answered\n");
		r2000_new_state(r2000, STATE_SUSPEND);
		timer_start(&r2000->timer, SUSPEND_TIME);
	} else {
		r2000_new_state(r2000, STATE_OUT_ALERT);
		timer_start(&r2000->timer, ALERT_TIME);
	}
}

/* release after dialing */
static void tx_suspend(r2000_t *r2000, frame_t *frame)
{
	frame->voie = 0;
	frame->message = 26;
	frame->sm_type = r2000->subscriber.type;
	frame->sm_relais = r2000->subscriber.relais;
	frame->sm_mor = r2000->subscriber.mor;

	if (r2000->tx_frame_count == 1)
		PDEBUG_CHAN(DR2000, DEBUG_INFO, "Sending suspend frame\n");
}

/* release response */
static void rx_suspend(r2000_t *r2000, frame_t *frame)
{
	if (!match_voie(r2000, frame, 1))
		return;
	if (!match_channel(r2000, frame))
		return;
	if (!match_relais(r2000, frame))
		return;
	if (!match_subscriber(r2000, frame))
		return;

	switch(frame->message) {
	case 26:
		/* suspend ack */
		PDEBUG_CHAN(DR2000, DEBUG_INFO, "Received suspend response from station mobile '%s'\n", print_subscriber_frame(frame));

		timer_stop(&r2000->timer);
		/* move us back to cc */
		r2000 = move_call_to_chan(r2000, CHAN_TYPE_CC);
		if (!r2000)
			return;
		r2000_new_state(r2000, STATE_RECALL_WAIT);
		/* setup call, possible r2000_release() is called there! */
		if (setup_call(r2000) < 0)
			return;
		break;
	default:
		PDEBUG_CHAN(DR2000, DEBUG_DEBUG, "Dropping frame %s in state %s\n", r2000_frame_name(frame->message, SM_TO_REL), r2000_state_name(r2000->state));
	}
}

/* response to accept frame */
static void timeout_suspend(r2000_t *r2000)
{
	PDEBUG_CHAN(DR2000, DEBUG_INFO, "Phone does not respond to suspend frame\n");

	r2000_release(r2000);
}

/*
 * process during active call
 */

static void timeout_active(r2000_t *r2000)
{
	PDEBUG_CHAN(DR2000, DEBUG_INFO, "Timeout after loosing supervisory signal, releasing call\n");

	call_in_release(r2000->callref, CAUSE_TEMPFAIL);
	r2000->callref = 0;
	r2000_release(r2000);
}

/*
 * release process
 */

static void tx_release_cc(r2000_t *r2000, frame_t *frame)
{
	frame->voie = 1;
	frame->message = 9;
	frame->sm_type = r2000->subscriber.type;
	frame->sm_relais = r2000->subscriber.relais;
	frame->sm_mor = r2000->subscriber.mor;

	if (r2000->tx_frame_count == 1)
		PDEBUG_CHAN(DR2000, DEBUG_INFO, "Sending release towards station mobile\n");
}

static void timeout_release_cc(r2000_t *r2000)
{
	PDEBUG_CHAN(DR2000, DEBUG_INFO, "Done sending release, going idle\n");

	r2000_go_idle(r2000);
}

static void tx_release_tc(r2000_t *r2000, frame_t *frame)
{
	frame->voie = 0;
	frame->message = 24;
	frame->sm_type = r2000->subscriber.type;
	frame->sm_relais = r2000->subscriber.relais;
	frame->sm_mor = r2000->subscriber.mor;

	if (r2000->tx_frame_count == 1)
		PDEBUG_CHAN(DR2000, DEBUG_INFO, "Sending release towards station mobile\n");

}

static void timeout_release_tc(r2000_t *r2000)
{
	PDEBUG_CHAN(DR2000, DEBUG_INFO, "Timeout while sending release, going idle\n");

	r2000_go_idle(r2000);
}

/* FSK processing requests next frame after transmission of previous
   frame has been finished. */
const char *r2000_get_frame(r2000_t *r2000)
{
	frame_t frame;
	const char *bits;
	int debug = 1;

	r2000->tx_frame_count++;

	memset(&frame, 0, sizeof(frame));
	frame.channel = r2000->sender.kanal;
	frame.relais = r2000->sysinfo.relais;
	frame.deport = r2000->sysinfo.deport;
	frame.agi = r2000->sysinfo.agi;
	frame.sm_power = r2000->sysinfo.sm_power;
	frame.taxe = r2000->sysinfo.taxe;

	switch (r2000->state) {
	case STATE_IDLE:
	case STATE_RECALL_WAIT:
		tx_idle(r2000, &frame);
		debug = 0;
		break;
	case STATE_INSCRIPTION:
		tx_inscription(r2000, &frame);
		break;
	case STATE_OUT_ASSIGN:
	case STATE_RECALL_ASSIGN:
		tx_out_assign(r2000, &frame);
		break;
	case STATE_IN_ASSIGN:
		tx_in_assign(r2000, &frame);
		break;
	case STATE_OUT_IDENT:
	case STATE_RECALL_IDENT:
	case STATE_IN_IDENT:
		tx_ident(r2000, &frame);
		break;
	case STATE_OUT_DIAL1:
	case STATE_OUT_DIAL2:
		tx_out_dial(r2000, &frame);
		break;
	case STATE_SUSPEND:
		tx_suspend(r2000, &frame);
		break;
	case STATE_IN_ALERT:
	case STATE_OUT_ALERT:
	case STATE_RECALL_ALERT:
		tx_alert(r2000, &frame);
		break;
	case STATE_RELEASE_CC:
		tx_release_cc(r2000, &frame);
		break;
	case STATE_RELEASE_TC:
		tx_release_tc(r2000, &frame);
		break;
	default:
		/* in case there is no handling, change to audio mode */
		/* this should not happen, but prevents an endless loop
		 * when the DSP tries to get next frame. */
		r2000_set_dsp_mode(r2000, DSP_MODE_AUDIO_TX_RX, -1);
	}

	/* frame sending aborted (e.g. due to audio) */
	if (r2000->dsp_mode != DSP_MODE_FRAME)
		return NULL;

	bits = encode_frame(&frame, debug);

	PDEBUG_CHAN(DR2000, DEBUG_DEBUG, "Sending frame %s.\n", r2000_frame_name(frame.message, REL_TO_SM));
	return bits;
}

void r2000_receive_frame(r2000_t *r2000, const char *bits, double quality, double level)
{
	frame_t frame;
	int rc;

	PDEBUG_CHAN(DDSP, DEBUG_INFO, "RX Level: %.0f%% Quality=%.0f\n", level * 100.0, quality * 100.0);

	rc = decode_frame(&frame, bits);
	if (rc < 0) {
		PDEBUG_CHAN(DR2000, (r2000->sender.loopback) ? DEBUG_NOTICE : DEBUG_DEBUG, "Received invalid frame.\n");
		return;
	}

	if (r2000->sender.loopback)
		PDEBUG_CHAN(DR2000, DEBUG_NOTICE, "Received frame %s\n", r2000_frame_name(frame.message, REL_TO_SM));
	else
		PDEBUG_CHAN(DR2000, DEBUG_DEBUG, "Received frame %s\n", r2000_frame_name(frame.message, SM_TO_REL));

	if (r2000->sender.loopback)
		return;

	/* release */
	if (frame.message == 6 || frame.message == 24) {
		if (r2000->state == STATE_IDLE)
			return;
		if (!match_voie(r2000, &frame, (frame.message < 16) ? 0 : 1))
			return;
		if (!match_channel(r2000, &frame))
			return;
		if (!match_relais(r2000, &frame))
			return;
		if (!match_subscriber(r2000, &frame))
			return;

		PDEBUG_CHAN(DR2000, DEBUG_INFO, "Received release from station mobile\n");

		if (r2000->callref) {
			call_in_release(r2000->callref, CAUSE_NORMAL);
			r2000->callref = 0;
		}
		r2000_go_idle(r2000);
		return;
	}

	switch (r2000->state) {
	case STATE_IDLE:
		rx_idle(r2000, &frame);
		break;
	case STATE_OUT_IDENT:
	case STATE_RECALL_IDENT:
	case STATE_IN_IDENT:
		rx_ident(r2000, &frame);
		break;
	case STATE_OUT_DIAL1:
		rx_out_dial1(r2000, &frame);
		break;
	case STATE_OUT_DIAL2:
		rx_out_dial2(r2000, &frame);
		break;
	case STATE_SUSPEND:
		rx_suspend(r2000, &frame);
		break;
	case STATE_IN_ALERT:
	case STATE_OUT_ALERT:
	case STATE_RECALL_ALERT:
		rx_alert(r2000, &frame);
		break;
	default:
		PDEBUG_CHAN(DR2000, DEBUG_DEBUG, "Dropping frame %s in state %s\n", r2000_frame_name(frame.message, SM_TO_REL), r2000_state_name(r2000->state));
	}
}

void r2000_receive_super(r2000_t *r2000, uint8_t super, double quality, double level)
{
	uint8_t nconv, relais;

	/* invert, if received from base station */
	if (r2000->sender.loopback)
		super ^= 0x7f;
		
	/* decode supervisory digit (nconv is LSB first) */
	nconv = ((super >> 2) & 0x01)
	      | (super & 0x02)
	      | ((super << 2) & 0x04);
	relais = ((super >> 6) & 0x01)
	      | ((super >> 4) & 0x02)
	      | ((super >> 2) & 0x04)
	      | (super & 0x08);

	PDEBUG_CHAN(DDSP, DEBUG_INFO, "RX Supervisory: NCONV: %d Relais (4 lowest bits): %d RX Level: %.0f%% Quality=%.0f\n", nconv, relais, level * 100.0, quality * 100.0);

	if (r2000->sender.loopback)
		return;
	if (r2000->state != STATE_ACTIVE)
		return;

	if (relais != (r2000->sysinfo.relais & 0xf)
	 || nconv != r2000->sysinfo.nconv)
		return;

	/* unmute RX audio if not already */
	r2000_set_dsp_mode(r2000, DSP_MODE_AUDIO_TX_RX, -1);

	/* reset supervisory timer */
	timer_start(&r2000->timer, SUPER_TIME2);
}

/* Timeout handling */
static void r2000_timeout(struct timer *timer)
{
	r2000_t *r2000 = (r2000_t *)timer->priv;

	switch (r2000->state) {
	case STATE_OUT_IDENT:
		timeout_out_ident(r2000);
		break;
	case STATE_IN_IDENT:
	case STATE_RECALL_IDENT:
		timeout_in_ident(r2000);
		break;
	case STATE_OUT_DIAL1:
		timeout_out_dial1(r2000);
		break;
	case STATE_OUT_DIAL2:
		timeout_out_dial2(r2000);
		break;
	case STATE_SUSPEND:
		timeout_suspend(r2000);
		break;
	case STATE_IN_ALERT:
	case STATE_OUT_ALERT:
	case STATE_RECALL_ALERT:
		timeout_alert(r2000);
		break;
	case STATE_ACTIVE:
		timeout_active(r2000);
		break;
	case STATE_RELEASE_CC:
		timeout_release_cc(r2000);
		break;
	case STATE_RELEASE_TC:
		timeout_release_tc(r2000);
		break;
	default:
		break;
	}
}

/*
 * call states received from call control
 */

/* Call control starts call towards station mobile. */
int call_out_setup(int callref, const char __attribute__((unused)) *caller_id, enum number_type __attribute__((unused)) caller_type, const char *dialing)
{
	sender_t *sender;
	r2000_t *r2000, *tc;
	r2000_subscriber_t subscr;

	memset(&subscr, 0, sizeof(subscr));

	/* 1. convert number to station mobile identification, return INVALNUMBER */
	if (string2subscriber(dialing, &subscr)) {
		PDEBUG(DR2000, DEBUG_NOTICE, "Outgoing call to invalid number '%s', rejecting!\n", dialing);
		return -CAUSE_INVALNUMBER;
	}

	/* 2. check if given number is already in a call, return BUSY */
	for (sender = sender_head; sender; sender = sender->next) {
		r2000 = (r2000_t *) sender;
		if (r2000->state != STATE_IDLE
		 && r2000->subscriber.relais == subscr.relais
		 && r2000->subscriber.mor == subscr.mor)
		 	break;
	}
	if (sender) {
		PDEBUG(DR2000, DEBUG_NOTICE, "Outgoing call to busy number, rejecting!\n");
		return -CAUSE_BUSY;
	}

	/* 3. check if all paging (control) channels are busy, return NOCHANNEL */
	r2000 = get_free_chan(CHAN_TYPE_CC);
	if (!r2000) {
		PDEBUG(DR2000, DEBUG_NOTICE, "Outgoing call, but no free control channel, rejecting!\n");
		return -CAUSE_NOCHANNEL;
	}
	tc = get_free_chan(CHAN_TYPE_TC);
	if (!tc) {
		PDEBUG(DR2000, DEBUG_NOTICE, "Outgoing call, but no free traffic channel, rejecting!\n");
		return -CAUSE_NOCHANNEL;
	}

	PDEBUG(DR2000, DEBUG_INFO, "Call to station mobile, paging station id '%s'\n", print_subscriber_subscr(&subscr));

	/* 4. trying to page station mobile */
	memcpy(&r2000->subscriber, &subscr, sizeof(r2000_subscriber_t));
	r2000->callref = callref;
	r2000_page(r2000, PAGE_TRIES, STATE_IN_ASSIGN);

	return 0;
}

/* Call control answers call toward station mobile. */
void call_out_answer(int callref)
{
	sender_t *sender;
	r2000_t *r2000;

	for (sender = sender_head; sender; sender = sender->next) {
		r2000 = (r2000_t *) sender;
	       	if (r2000->callref == callref)
			break;
	}
	if (!sender) {
		PDEBUG(DR2000, DEBUG_NOTICE, "Outgoing answer, but no callref!\n");
		call_in_release(callref, CAUSE_INVALCALLREF);
		return;
	}

	switch (r2000->state) {
	case STATE_RECALL_WAIT:
		PDEBUG_CHAN(DR2000, DEBUG_INFO, "Call has been answered by network, recalling station mobile.\n");
		r2000_page(r2000, PAGE_TRIES, STATE_RECALL_ASSIGN);
		call_tone_recall(callref, 1);
		break;
	default:
		break;
	}
}

/* Call control sends disconnect (with tones).
 * An active call stays active, so tones and annoucements can be received
 * by station mobile.
 */
void call_out_disconnect(int callref, int __attribute__((unused)) cause)
{
	sender_t *sender;
	r2000_t *r2000;

	PDEBUG(DR2000, DEBUG_INFO, "Call has been disconnected by network.\n");

	for (sender = sender_head; sender; sender = sender->next) {
		r2000 = (r2000_t *) sender;
	       	if (r2000->callref == callref)
			break;
	}
	if (!sender) {
		PDEBUG(DR2000, DEBUG_NOTICE, "Outgoing disconnect, but no callref!\n");
		call_in_release(callref, CAUSE_INVALCALLREF);
		return;
	}

	/* Release when not active and not waiting for answer */
	if (r2000->state == STATE_ACTIVE)
		return;
	switch (r2000->state) {
	default:
		PDEBUG_CHAN(DR2000, DEBUG_NOTICE, "Outgoing disconnect, during call setup, releasing!\n");
		r2000->callref = 0;
	 	r2000_release(r2000);
		break;
	}

	call_in_release(callref, cause);
}

/* Call control releases call toward station mobile. */
void call_out_release(int callref, int __attribute__((unused)) cause)
{
	sender_t *sender;
	r2000_t *r2000;

	PDEBUG(DR2000, DEBUG_INFO, "Call has been released by network, releasing call.\n");

	for (sender = sender_head; sender; sender = sender->next) {
		r2000 = (r2000_t *) sender;
	       	if (r2000->callref == callref)
			break;
	}
	if (!sender) {
		PDEBUG(DR2000, DEBUG_NOTICE, "Outgoing release, but no callref!\n");
		/* don't send release, because caller already released */
		return;
	}

	r2000->callref = 0;

	switch (r2000->state) {
	case STATE_ACTIVE:
		PDEBUG_CHAN(DR2000, DEBUG_NOTICE, "Outgoing release, during ringing, releasing!\n");
	 	r2000_release(r2000);
		break;
	default:
		PDEBUG_CHAN(DR2000, DEBUG_NOTICE, "Outgoing release, during call setup, releasing!\n");
	 	r2000_release(r2000);
		break;
	}
}

/* Receive audio from call instance. */
void call_rx_audio(int callref, sample_t *samples, int count)
{
	sender_t *sender;
	r2000_t *r2000;

	for (sender = sender_head; sender; sender = sender->next) {
		r2000 = (r2000_t *) sender;
	       	if (r2000->callref == callref)
			break;
	}
	if (!sender)
		return;

	if (r2000->dsp_mode == DSP_MODE_AUDIO_TX
	 || r2000->dsp_mode == DSP_MODE_AUDIO_TX_RX) {
		sample_t up[(int)((double)count * r2000->sender.srstate.factor + 0.5) + 10];
		if (r2000->compandor)
			compress_audio(&r2000->cstate, samples, count);
		count = samplerate_upsample(&r2000->sender.srstate, samples, count, up);
		jitter_save(&r2000->sender.dejitter, up, count);
	}
}

void dump_info(void) {}

