/* Eurosignal protocol handling
 *
 * (C) 2019 by Andreas Eversberg <jolly@eversberg.eu>
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

#define CHAN euro->sender.kanal

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "../libsample/sample.h"
#include "../liblogging/logging.h"
#include "../libmobile/call.h"
#include "../libmobile/cause.h"
#include <osmocom/cc/message.h>
#include "eurosignal.h"
#include "dsp.h"

/* announcement timers */
#define ANSWER_TIME		1,0		/* wait after answer */
#define OOO_TIME		3,800000	/* announcement 1.7 s, pause 2.1 s */
#define UNASSIGNED_TIME1	2,200000	/* announcement 2.2 s s */
#define UNASSIGNED_TIME2	2,900000	/* announcement 2.2 s, pause 0.7 s */
#define DEGRADED_TIME		4,950000	/* announcement 2.25 s, pause 2.7 s */
#define ACKNOWLEDGE_TIME1	2,800000	/* announcement 1.7 s, pause 1.1 s */
#define ACKNOWLEDGE_TIME2	4,600000	/* announcement 1.7 s, pause 2.9 s */
#define BEEP_TIME		4,000000	/* beep after answer */

/* these calls are not associated with a transmitter */
euro_call_t *ooo_call_list = NULL;

struct id_list {
	struct id_list *next;
	char id[7];
};

static struct id_list *id_list = NULL, **id_list_tail = &id_list;

/* add ID that is allowed to page */
void euro_add_id(const char *id)
{
	struct id_list *entry;

	entry = calloc(1, sizeof(struct id_list));
	if (!entry) {
		fprintf(stderr, "No mem!\n");
		abort();
	}
	strcpy(entry->id, id);
	(*id_list_tail) = entry;
	id_list_tail = &entry->next;
}

static int search_id(const char *id)
{
	struct id_list *entry = id_list;
	int count = 0;

	while (entry) {
		count++;
		if (!strcmp(entry->id, id))
			return count;
		entry = entry->next;
	}

	return 0;
}

static void euro_call_destroy(euro_call_t *call);

static void flush_id(void)
{
	struct id_list *entry;

	while (ooo_call_list)
		euro_call_destroy(ooo_call_list);
	while (id_list) {
		entry = id_list;
		id_list = entry->next;
		free(entry);
	}
	id_list_tail = &id_list;
}

static const char *call_state_name(enum euro_call_state state)
{
	static char invalid[16];

	switch (state) {
	case EURO_CALL_NULL:
		return "(NULL)";
	case EURO_CALL_ANSWER:
		return "ANSWER";
	case EURO_CALL_DEGRADED:
		return "DEGRADED";
	case EURO_CALL_ACKNOWLEDGE:
		return "ACKNOWLEDGE";
	case EURO_CALL_RELEASED:
		return "RELEASED";
	case EURO_CALL_UNASSIGNED:
		return "UNASSIGNED";
	case EURO_CALL_OUTOFORDER:
		return "OUT-OF-ORDER";
	case EURO_CALL_BEEPING:
		return "BEEPING";
	}

	sprintf(invalid, "invalid(%d)", state);
	return invalid;
}

static void euro_display_status(void)
{
	sender_t *sender;
	euro_t *euro;
	euro_call_t *call;

	display_status_start();
	for (call = ooo_call_list; call; call = call->next)
		display_status_subscriber(call->station_id, call_state_name(call->state));
	for (sender = sender_head; sender; sender = sender->next) {
		euro = (euro_t *) sender;
		display_status_channel(euro->sender.kanal, NULL, (euro->degraded) ? "Degraded" : "Working");
		for (call = euro->call_list; call; call = call->next)
			display_status_subscriber(call->station_id, call_state_name(call->state));
	}
	display_status_end();
}


static void call_new_state(euro_call_t *call, enum euro_call_state new_state)
{
	if (call->state == new_state)
		return;
	LOGP(DEURO, LOGL_DEBUG, "State change: %s -> %s\n", call_state_name(call->state), call_state_name(new_state));
	call->state = new_state;
	euro_display_status();
}

/* Convert channel number to frequency */
double euro_kanal2freq(const char *kanal, int fm)
{
	double freq = 87.34;
	char digit = kanal[0];

	if (strlen(kanal) != 1)
		return 0.0;

	if (digit >= 'a' && digit <= 'd')
		digit -= 'a';
	else if (digit >= 'A' && digit <= 'D')
		digit -= 'A';
	else
		return 0.0;

	if (fm)
		freq -= 0.0075;
		
	return (freq + 0.025 * (double)digit) * 1e6;
}

static struct channel_info {
	const char *kanal;
	const char *country;
	const char *station;
} channel_info[] = {
	{ "A",	"Germany", "Eurosignal Mitte" },
	{ NULL,	"France", "Centre Est" },
	{ "B",	"Germany", "Eurosignal Nord" },
	{ NULL,	"Germany", "Eurosignal Sued" },
	{ NULL,	"France", "North" },
	{ "C",	"France", "West" },
	{ NULL,	"France", "South East" },
	{ NULL,	"France", "East" },
	{ "D",	"France", "South West" },
	{ NULL,	"France", "South West (Corsica)" },
	{ NULL,	"Switzerland", "" },
	{ NULL, NULL, NULL },
};

/* list all channels */
void euro_list_channels(void)
{
	int i;

	printf("Channel\t\tFrequency\tCountry\t\tStation Name\n");
	printf("------------------------------------------------------------------------\n");
	for (i = 0; channel_info[i].country; i++) {
		if (channel_info[i].kanal)
			printf("%s\t\t%.3f MHz\t", channel_info[i].kanal, euro_kanal2freq(channel_info[i].kanal, 0) / 1e6);
		else
			printf("\t\t\t\t");
		printf("%s\t", channel_info[i].country);
		if (strlen(channel_info[i].country) < 8)
			printf("\t");
		printf("%s\n", channel_info[i].station);
	}
	printf("\n");
}

sample_t beep_tone[160];

/* global init */
int euro_init(void)
{
	int i;

	for (i = 0; i < 160; i++)
		beep_tone[i] = sin(2.0 * M_PI * (double)i * 2500.0 / 8000.0);

	return 0;
}

/* global exit */
void euro_exit(void)
{
	flush_id();
}

static void call_timeout(void *data);

/* Create transceiver instance and link to a list. */
int euro_create(const char *kanal, const char *device, int use_sdr, int samplerate, double rx_gain, double tx_gain, int fm, int tx, int rx, int repeat, int degraded, int random, uint32_t scan_from, uint32_t scan_to, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, const char *read_tx_wave, int loopback)
{
	euro_t *euro;
	int rc;

	if (euro_kanal2freq(kanal, 0) == 0.0) {
		LOGP(DEURO, LOGL_ERROR, "Channel ('Kanal') number %s invalid, use 'list' to get a list.\n", kanal);
		return -EINVAL;
	}

	euro = calloc(1, sizeof(*euro));
	if (!euro) {
		LOGP(DEURO, LOGL_ERROR, "No memory!\n");
		return -ENOMEM;
	}

	LOGP(DEURO, LOGL_DEBUG, "Creating 'Eurosignal' instance for 'Kanal' = %s (sample rate %d).\n", kanal, samplerate);

	/* init general part of transceiver */
	rc = sender_create(&euro->sender, kanal, euro_kanal2freq(kanal, fm), euro_kanal2freq(kanal, fm), device, use_sdr, samplerate, rx_gain, tx_gain, 0, 0, write_rx_wave, write_tx_wave, read_rx_wave, read_tx_wave, loopback, PAGING_SIGNAL_NONE);
	if (rc < 0) {
		LOGP(DEURO, LOGL_ERROR, "Failed to init transceiver process!\n");
		goto error;
	}

	/* init audio processing */
	rc = dsp_init_sender(euro, samplerate, fm);
	if (rc < 0) {
		LOGP(DEURO, LOGL_ERROR, "Failed to init audio processing!\n");
		goto error;
	}

	euro->tx = tx;
	euro->rx = rx;
	euro->repeat = repeat;
	euro->degraded = degraded;
	euro->random = random;
	euro->scan_from = scan_from;
	euro->scan_to = scan_to;

	euro_display_status();

	LOGP(DEURO, LOGL_NOTICE, "Created 'Kanal' %s\n", kanal);

	return 0;

error:
	euro_destroy(&euro->sender);

	return rc;
}

/* Destroy transceiver instance and unlink from list. */
void euro_destroy(sender_t *sender)
{
	euro_t *euro = (euro_t *) sender;

	LOGP(DEURO, LOGL_DEBUG, "Destroying 'Eurosignal' instance for 'Kanal' = %s.\n", sender->kanal);

	while (euro->call_list)
		euro_call_destroy(euro->call_list);
	dsp_cleanup_sender(euro);
	sender_destroy(&euro->sender);
	free(euro);
}

/* Create call instance */
static euro_call_t *euro_call_create(euro_t *euro, uint32_t callref, const char *id)
{
	euro_call_t *call, **callp;

	LOGP(DEURO, LOGL_INFO, "Creating call instance to page ID '%s'.\n", id);

	/* create */
	call = calloc(1, sizeof(*call));
	if (!call) {
		LOGP(DEURO, LOGL_ERROR, "No mem!\n");
		abort();
	}

	/* init */
	call->callref = callref;
	strcpy(call->station_id, id);
	if (euro)
		call->page_count = euro->repeat;
	osmo_timer_setup(&call->timer, call_timeout, call);
	osmo_timer_schedule(&call->timer, ANSWER_TIME);

	/* link */
	call->euro = euro;
	callp = (euro) ? (&euro->call_list) : (&ooo_call_list);
	while ((*callp))
		callp = &(*callp)->next;
	(*callp) = call;

	return call;
}

/* Destroy call instance */
static void euro_call_destroy(euro_call_t *call)
{
	euro_call_t **callp;

	/* unlink */
	callp = (call->euro) ? (&call->euro->call_list) : (&ooo_call_list);
	while ((*callp) != call)
		callp = &(*callp)->next;
	(*callp) = call->next;

	/* cleanup */
	osmo_timer_del(&call->timer);

	/* destroy */
	free(call);

	/* update display */
	euro_display_status();
}

/* Return ID to page. If there is nothing scheduled, return idle pattern */
void euro_get_id(euro_t *euro, char *id)
{
	euro_call_t *call;
	int i;

	if (euro->scan_from < euro->scan_to) {
		sprintf(id, "%06d", euro->scan_from++);
		LOGP_CHAN(DEURO, LOGL_NOTICE, "Transmitting ID '%s'.\n", id);
		goto encode;
	}

	if (euro->sender.loopback) {
		LOGP_CHAN(DEURO, LOGL_NOTICE, "Transmitting test ID '123456'.\n");
		memcpy(id, "123456", 6);
		goto encode;
	}

	for (call = euro->call_list; call; call = call->next) {
		if ((call->state == EURO_CALL_ACKNOWLEDGE || call->state == EURO_CALL_RELEASED) && call->page_count) {
			call->page_count--;
			LOGP_CHAN(DEURO, LOGL_NOTICE, "Transmitting ID '%s'.\n", call->station_id);
			memcpy(id, call->station_id, 6);
			if (call->page_count == 0 && call->state == EURO_CALL_RELEASED)
				euro_call_destroy(call);
			/* reset random counter, so aferr call has been transmitted, the random mode continues with a new ID */
			euro->random_count = 0;
			break;
		}
	}

	if (!call && euro->random) {
		if (!euro->random_count) {
			if (((uint32_t)random() % 2) == 0 && euro->random_id[0] != 'R') {
				memcpy(euro->random_id, "RIIIII", 6);
				euro->random_count = (uint32_t)random() % 2 + 2;
			} else {
				sprintf(euro->random_id, "%06d", (uint32_t)(random() % 1000000));
				euro->random_count = (uint32_t)random() % 3 + 2;
			}
		}
		memcpy(id, euro->random_id, 6);
		euro->random_count--;
		if (id[0] == 'R') {
			LOGP_CHAN(DEURO, LOGL_NOTICE, "Randomly transmitting Idle sequence.\n");
			return;
		}
		LOGP_CHAN(DEURO, LOGL_NOTICE, "Randomly transmitting ID '%s'.\n", euro->random_id);
		goto encode;
	}

	if (!call) {
		LOGP_CHAN(DEURO, LOGL_DEBUG, "Transmitting Idle sequence.\n");
		memcpy(id, "RIIIII", 6);
		return;
	}

encode:
	/* return station ID (upper case) with repeat digit, when required */
	for (i = 0; i < 6; i++) {
		/* to upper case */
		if (id[i] >= 'a' && id[i] <= 'z')
			id[i] = id[i] - 'a' + 'A';
		/* repeat digit */
		if (i && id[i - 1] == id[i])
			id[i] = 'R';
	}
}

/* A station ID was received. */
void euro_receive_id(euro_t *euro, char *id)
{
	int i;
	int count = 0;

	if (id[0] == 'R') {
		LOGP_CHAN(DEURO, LOGL_DEBUG, "Received Idle sequence'\n");
		return;
	}

	/* turn repeat digit to normal digit */
	for (i = 1; i < 6; i++) {
		if (id[i] == 'R')
			id[i] = id[i - 1];
	}

	/* loopback display */
	if (euro->sender.loopback) {
		LOGP_CHAN(DEURO, LOGL_NOTICE, "Received ID '%s'\n", id);
		return;
	}

	/* check for ID selection */
	if (id_list) {
		count = search_id(id);
		if (!count) {
			LOGP_CHAN(DEURO, LOGL_INFO, "Received ID '%s' is not for us.\n", id);
			return;
		}
	}

	LOGP_CHAN(DEURO, LOGL_NOTICE, "Received ID '%s'\n", id);

	/* we want to send beep via MNCC */
	if (id_list) {
		uint32_t callref;
		euro_call_t *call;
		char dialing[32];

		/* check if we already have a call that beeps */
		for (call = ooo_call_list; call; call = call->next) {
			if (!strcmp(call->station_id, id) && call->state == EURO_CALL_BEEPING)
				break;
		}
		/* if already beeping */
		if (call)
			return;

		/* create call and send setup */
		LOGP_CHAN(DEURO, LOGL_INFO, "Sending setup towards network.'\n");
		sprintf(dialing, "%d", count);
		callref = call_up_setup(call->station_id, dialing, OSMO_CC_NETWORK_EUROSIGNAL_NONE, "");
		call = euro_call_create(NULL, callref, id);
		call_new_state(call, EURO_CALL_BEEPING);
	}
}

int16_t *es_mitte_spl;
int es_mitte_size;
int16_t *es_ges_spl;
int es_ges_size;
int16_t *es_teilges_spl;
int es_teilges_size;
int16_t *es_kaudn_spl;
int es_kaudn_size;

/* play announcement for one call */
static void call_play_announcement(euro_call_t *call)
{
	int i;
	int16_t chunk[160];
	sample_t spl[160];

	for (i = 0; i < 160; i++) {
		/* announcement or silence, if finished or not set */
		if (call->announcement_index < call->announcement_size)
			chunk[i] = call->announcement_spl[call->announcement_index++] >> 2;
		else
			chunk[i] = 0.0;
	}
	int16_to_samples_speech(spl, chunk, 160);
	call_up_audio(call->callref, spl, 160);
}

/* play paging tone */
static void call_play_beep(euro_call_t *call)
{
	call_up_audio(call->callref, beep_tone, 160);
}

/* loop through all calls and play the announcement */
void call_down_clock(void)
{
	sender_t *sender;
	euro_t *euro;
	euro_call_t *call;

	/* clock all calls without a transceiver */
	for (call = ooo_call_list; call; call = call->next) {
		/* no callref */
		if (!call->callref)
			continue;
		/* beep or announcement */
		if (call->state == EURO_CALL_BEEPING)
			call_play_beep(call);
		else
			call_play_announcement(call);
	}

	/* clock all calls that have a transceiver */
	for (sender = sender_head; sender; sender = sender->next) {
		euro = (euro_t *) sender;
		for (call = euro->call_list; call; call = call->next) {
			/* no callref */
			if (!call->callref)
				continue;
			/* announcement */
			call_play_announcement(call);
		}
	}
}

/* Timeout handling */
static void call_timeout(void *data)
{
	euro_call_t *call = data;

	switch (call->state) {
	case EURO_CALL_ANSWER:
		/* if no station is linked to the call, we are out-of-order */
		if (!call->euro) {
			LOGP(DEURO, LOGL_INFO, "Station is unavailable, playing announcement.\n");
			call->announcement_spl = es_ges_spl;
			call->announcement_size = es_ges_size;
			call->announcement_index = 0;
			osmo_timer_schedule(&call->timer, OOO_TIME);
			call_new_state(call, EURO_CALL_OUTOFORDER);
			break;
		}
		/* if subcriber list is available, but ID is not found, we are unassigned */
		if (id_list && !search_id(call->station_id)) {
			LOGP(DEURO, LOGL_INFO, "Subscriber unknown, playing announcement.\n");
			call->announcement_spl = es_kaudn_spl;
			call->announcement_size = es_kaudn_size;
			call->announcement_index = 0;
			call->announcement_count = 1;
			osmo_timer_schedule(&call->timer, UNASSIGNED_TIME1);
			call_new_state(call, EURO_CALL_UNASSIGNED);
			break;
		}
		/* if station is degraded, play that announcement */
		if (call->euro->degraded) {
			LOGP(DEURO, LOGL_INFO, "Station is degraded, playing announcement.\n");
			call->announcement_spl = es_teilges_spl;
			call->announcement_size = es_teilges_size;
			call->announcement_index = 0;
			osmo_timer_schedule(&call->timer, DEGRADED_TIME);
			call_new_state(call, EURO_CALL_DEGRADED);
			break;
		}
	/* fall through */
	case EURO_CALL_DEGRADED:
		LOGP(DEURO, LOGL_INFO, "Station acknowledges, playing announcement.\n");
		call->announcement_spl = es_mitte_spl;
		call->announcement_size = es_mitte_size;
		call->announcement_index = 0;
		call->announcement_count = 1;
		osmo_timer_schedule(&call->timer, ACKNOWLEDGE_TIME1);
		call_new_state(call, EURO_CALL_ACKNOWLEDGE);
		break;
	case EURO_CALL_ACKNOWLEDGE:
		if (call->announcement_count == 1) {
			call->announcement_spl = es_mitte_spl;
			call->announcement_size = es_mitte_size;
			call->announcement_index = 0;
			call->announcement_count = 2;
			osmo_timer_schedule(&call->timer, ACKNOWLEDGE_TIME2);
			break;
		}
		if (call->page_count) {
			LOGP(DEURO, LOGL_INFO, "Announcement played, receiver has not been paged yet, releasing call.\n");
			call_up_release(call->callref, CAUSE_NORMAL);
			call->callref = 0;
			call_new_state(call, EURO_CALL_RELEASED);
			break;
		}
		LOGP(DEURO, LOGL_INFO, "Announcement played, receiver has been paged, releasing call.\n");
		call_up_release(call->callref, CAUSE_NORMAL);
		euro_call_destroy(call);
		break;
	case EURO_CALL_OUTOFORDER:
		LOGP(DEURO, LOGL_INFO, "Announcement played, releasing call.\n");
		call_up_release(call->callref, CAUSE_NORMAL);
		euro_call_destroy(call);
		break;
	case EURO_CALL_UNASSIGNED:
		if (call->announcement_count == 1) {
			call->announcement_spl = es_kaudn_spl;
			call->announcement_size = es_kaudn_size;
			call->announcement_index = 0;
			call->announcement_count = 2;
			osmo_timer_schedule(&call->timer, UNASSIGNED_TIME2);
			break;
		}
		LOGP(DEURO, LOGL_INFO, "Announcement played, playing again.\n");
		call->announcement_spl = es_kaudn_spl;
		call->announcement_size = es_kaudn_size;
		call->announcement_index = 0;
		call->announcement_count = 1;
		osmo_timer_schedule(&call->timer, UNASSIGNED_TIME1);
		break;
	case EURO_CALL_BEEPING:
		LOGP(DEURO, LOGL_INFO, "Beep played, releasing.\n");
		call_up_release(call->callref, CAUSE_NORMAL);
		call->callref = 0;
		euro_call_destroy(call);
		break;
	default:
		break;
	}
}

/* Call control starts call towards paging network. */
int call_down_setup(int callref, const char __attribute__((unused)) *caller_id, enum number_type __attribute__((unused)) caller_type, const char *dialing)
{
	char channel = '\0';
	sender_t *sender;
	euro_t *euro;
	euro_call_t *call;

	/* find transmitter */
	for (sender = sender_head; sender; sender = sender->next) {
		/* skip channels that are different than requested */
		if (channel && sender->kanal[0] != channel)
			continue;
		euro = (euro_t *) sender;
		/* check if base station cannot transmit */
		if (!euro->tx)
			continue;
		break;
	}
	/* just (ab)use busy signal when no station is available */
	if (!sender) {
		if (channel)
			LOGP(DEURO, LOGL_NOTICE, "Cannot page receiver, because given station not available, rejecting!\n");
		else
			LOGP(DEURO, LOGL_NOTICE, "Cannot page receiver, no station not available, rejecting!\n");
		euro = NULL;
	}

	/* create call process to page station or send out-of-order message */
	call = euro_call_create(euro, callref, dialing);
	call_new_state(call, EURO_CALL_ANSWER);
	call_up_answer(callref, dialing);

	return 0;
}

void call_down_answer(int __attribute__((unused)) callref)
{
	euro_call_t *call;

	LOGP(DEURO, LOGL_INFO, "Call has been answered by network.\n");

	for (call = ooo_call_list; call; call = call->next) {
		if (call->callref == callref)
			break;
	}
	if (!call) {
		LOGP(DEURO, LOGL_NOTICE, "Answer from network, but no callref!\n");
		call_up_release(callref, CAUSE_INVALCALLREF);
		return;
	}

	osmo_timer_schedule(&call->timer, BEEP_TIME);
}

static void _release(int callref, int __attribute__((unused)) cause)
{
	sender_t *sender;
	euro_t *euro;
	euro_call_t *call;

	LOGP(DEURO, LOGL_INFO, "Call has been disconnected by network.\n");

	for (sender = sender_head; sender; sender = sender->next) {
		euro = (euro_t *) sender;
		for (call = euro->call_list; call; call = call->next) {
			if (call->callref == callref)
				break;
		}
		if (call)
			break;
	}
	if (!sender) {
		for (call = ooo_call_list; call; call = call->next) {
			if (call->callref == callref)
				break;
		}
	}
	if (!call) {
		LOGP(DEURO, LOGL_NOTICE, "Outgoing disconnect, but no callref!\n");
		call_up_release(callref, CAUSE_INVALCALLREF);
		return;
	}

	call->callref = 0;

	/* queued ID will keep in release state until transmission has finished */
	if (call->state == EURO_CALL_ACKNOWLEDGE && call->page_count) {
		call_new_state(call, EURO_CALL_RELEASED);
		return;
	}

	euro_call_destroy(call);
}

/* Call control sends disconnect.
 * A queued ID will be kept until transmitted by mobile station.
 */
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
void call_down_audio(int __attribute__((unused)) callref, uint16_t __attribute__((unused)) sequence, uint32_t __attribute__((unused)) timestamp, uint32_t __attribute__((unused)) ssrc, sample_t __attribute__((unused)) *samples, int __attribute__((unused)) count)
{
}

void dump_info(void) {}

