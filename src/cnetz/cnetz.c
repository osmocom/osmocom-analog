/* C-Netz protocol handling
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

/* Notes on channel state an callref:
 *
 * If channel state is busy, it is used as SpK, also it has callref.
 *
 * Each transaction has callref, but callref is only assigned to channel
 * in SpK mode, so voice frames are routed there.
 *
 */

/* Notes on transaction state:
 *
 * The state is used to define what is scheduled next, what message is awaited,
 * what is done when timeout. The event (scheduler, message, timeout) is
 * processed then and the state may change.
 */

/* Call control process:
 *
 * If an MT (mobile terminating) call is made, a transaction with callref is
 * created. The transaction is linked to OgK. When the scheduler schedules
 * VAK(R), the SpK is allocated and the transaction is linked to it.
 *
 * If no SpK is available, the call is rejected. If queue (Warteschlange) is
 * enabled, WSK(R) is scheduled. After transmission, the state changes to
 * TRANS_MT_QUEUE. Upon timeout (no channel becomes available), the call is
 * rejected by scheduling VA(R). Upon available channel the call proceeds with
 * VAK(R) as described above.
 *
 * If an MO (mobile originating) call is made (received VWG(K)), a transaction
 * with callref is created. The transaction is linked to OgK. When the
 * scheduler schedules WAF(M), the process waits for WUE(M). If not received,
 * the process is repeated. After N times WBN(R) is scheduled and transaction
 * is destroyed.  If WUE(M) is received, the scheduler schedules WBP(R) and
 * then schedules VAG(R), the SpK is allocated and the transaction is linked to
 * it.
 *
 * If no SpK is available, the call is rejected by scheduling WBN(R). If queue
 * (Warteschlange) is enabled, WWBP(R) is scheduled. After transmission, the
 * state is changed to TRANS_MO_QUEUE. Upon timeout (no channel becomes
 * available), the call is rejected by scheduling VA(R). Upon available channel
 * the call proceeds with VAG(R) as described above.
 *
 * Switching to SpK is performed two time slots after transmitting VAK(R) or
 * VAG(R). The timer is started.  The schedulers schedules 8 times BQ(K) and
 * awaits at least one BEL(K). If BEK(K) is received, the timer is stopped. If
 * BQ(K) was sent at least 8 times and if timer is stopped, the scheduler
 * schedules VHQ(K).  If no BEL(K) was received, AFK(K) is scheduled N_AFKT
 * times, then the process on OgK (WBP+VAG or VAK) is repeated N times.
 *
 * Similar to BQ/BEL the DS/DSQ handing is performed. For MT calls, the BQ/BEL
 * is followed by RTA/RTAQ handling. If the phone answers, the AT(K) is
 * received and DS/DSQ handling is performed.
 *
 * After DS/DSQ handling, the SpK changes to distributed signalling mode.
 * VHQ1/VHQ2(V) is transmitted and VH(V) is received. If VH(V) is not received
 * F_VHQ times, the connection is terminated by sending AF(K) N_AFKT times.
 * Transaction is released.
 *
 * If AT(K) or AT(V) is received, AF(K) or AF(V) is sent once and transaction
 * is released.
 *
 * If call is released by upper layer, AF(K) is sent N_AFKT times or AF(V) is
 * sent N_AFV times. The transaction is released.
 *
 * More details about the process can be read from the source code.
 *
 * Special timings and correct scheduling is defined in source code and can be
 * read also in the C-Netz specs.
 */

/*
 * Notes on switching from OgK to SpK
 *
 * Upon transmission of TRANS_VAG and TRANS_VAK, the SpK channel is allocated,
 * set to busy, scheduled to switch to SpK mode after two frames. The trans-
 * action is relinked from OgK to SpK.
 *
 * In case of a combined OgK+SpK, the channel stays the same, but will change.
 *
 * See below for detailed processing.
 */

/*
 * Notes on database (subscriber)
 *
 * If a subscriber registers (transaction is created), an instance of the
 * subscriber database is created. A timer is running for each instance, so
 * the subscriber is paged to check availability of the phone. If the paging
 * fails, a retry counter is decreased until the subscriber is removed from
 * database.
 *
 * See database.c for more information.
 */

/*
 * Notes on the combined channel hack:
 *
 * For combined SpK+OgK hack, the channel is used as SpK as last choice. This
 * allows to use only one transceiver for making C-Netz to work. Also it allows
 * to use all transceivers for simultanious phone calls. Some phones may not
 * work with that.
 */

/*
 * Notes on sync:
 *
 * The encoder generates a precise clocked signal using correction value given
 * by command line. For multichannel, the second sound card's channel (slave) is
 * synced to the first one (master), if calculation of signal phase might drift
 * due to rounding errors.
 *
 * The decoder is synced to the phone, whenever it receives a valid frame.
 *
 * See dsp.c and fsk_fm_demod.c for code about syncing.
 */

#define CHAN cnetz->sender.kanal

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <inttypes.h>
#include "../libsample/sample.h"
#include "../libdebug/debug.h"
#include "../libmobile/call.h"
#include "../libmobile/cause.h"
#include "../libosmocc/message.h"
#include "cnetz.h"
#include "database.h"
#include "sysinfo.h"
#include "telegramm.h"
#include "dsp.h"
#include "stations.h"

/* uncomment this to do echo debugging (-l) on Speech Channel */
//#define DEBUG_SPK

#define CUT_OFF_EMPHASIS_CNETZ	796.0 /* 200 uS time constant */

/* OgK list of alternative channels, NOT including 131 */
cnetz_t *ogk_list[16];
int ogk_list_count = 0;
int ogk_list_index = 0;


/* Convert channel number to frequency number of base station.
   Set 'unterband' to 1 to get frequency of mobile station. */
double cnetz_kanal2freq(int kanal, int unterband)
{
	double freq = 465.750;

	if (unterband == 2)
		return -10.000 * 1e6;

	if ((kanal & 1))
		freq -= (double)(kanal + 1) / 2.0 * 0.010;
	else
		freq -= (double)kanal / 2.0 * 0.0125;
	if (unterband)
		freq -= 10.000;

	return freq * 1e6;
}

/* check if number is a valid station ID */
const char *cnetz_number_valid(const char *number)
{
	/* assume that the number has valid length(s) and digits */

	if (number[0] > '7')
		return "Digit 1 (mobile country code) exceeds 7.";
	if (number[7]) {
		if ((number[1] - '0') == 0)
			return "Digit 2 and 3 (mobile network code) of 8-digit number must be at least 10.";
		if ((number[1] - '0') * 10 + (number[2] - '0') > 31)
			return "Digit 2 and 3 (mobile network code) of 8-digit number exceed 31.";
		if (atoi(number + 3) > 65535)
			return "Digit 4 to 8 (mobile subscriber suffix) of 8-digit number exceed 65535.";
	} else {
		if (atoi(number + 2) > 65535)
			return "Digit 3 to 7 (mobile subscriber suffix) of 7-digit number exceed 65535.";
	}

	return NULL;
}

/* convert power level to P-bits by selecting next higher level */
static uint8_t cnetz_power2bits(int power)
{
	switch (power) {
	case 1:
		return 3;
	case 2:
	case 3:
		return 2;
	case 4:
	case 5:
		return 1;
	case 6:
	case 7:
	case 8:
	default:
		return 0;
	}
}

const char *cnetz_state_name(enum cnetz_state state)
{
	static char invalid[16];

	switch (state) {
	case CNETZ_NULL:
		return "(NULL)";
	case CNETZ_IDLE:
		return "IDLE";
	case CNETZ_BUSY:
		return "BUSY";
	}

	sprintf(invalid, "invalid(%d)", state);
	return invalid;
}

void cnetz_display_status(void)
{
	sender_t *sender;
	cnetz_t *cnetz;
	transaction_t *trans;

	display_status_start();
	for (sender = sender_head; sender; sender = sender->next) {
		cnetz = (cnetz_t *) sender;
		display_status_channel(cnetz->sender.kanal, chan_type_short_name(cnetz->chan_type), cnetz_state_name(cnetz->state));
		for (trans = cnetz->trans_list; trans; trans = trans->next)
			display_status_subscriber(transaction2rufnummer(trans), trans_short_state_name(trans->state));
	}
	display_status_end();
}

static void cnetz_new_state(cnetz_t *cnetz, enum cnetz_state new_state)
{
	if (cnetz->state == new_state)
		return;
	PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "State change: %s -> %s\n", cnetz_state_name(cnetz->state), cnetz_state_name(new_state));
	cnetz->state = new_state;
	cnetz_display_status();
}

/* Convert ISDN cause to 'Ausloesegrund' of C-Netz mobile station */
uint8_t cnetz_cause_isdn2cnetz(int cause)
{
	switch (cause) {
	case CAUSE_NORMAL:
	case CAUSE_BUSY:
	case CAUSE_NOANSWER:
		return CNETZ_CAUSE_TEILNEHMERBESETZT;
	case CAUSE_OUTOFORDER:
	case CAUSE_INVALNUMBER:
	case CAUSE_NOCHANNEL:
	case CAUSE_TEMPFAIL:
	default:
		return CNETZ_CAUSE_GASSENBESETZT;
	}
}

/* global init */
int cnetz_init(void)
{
	return 0;
}

/* Create transceiver instance and link to a list. */
int cnetz_create(const char *kanal_name, enum cnetz_chan_type chan_type, const char *device, int use_sdr, enum demod_type demod, int samplerate, double rx_gain, double tx_gain, int challenge_valid, uint64_t challenge, int response_valid, uint64_t response, int warteschlange, int metering, double speech_deviation, int ms_power, int measure_speed, double clock_speed[2], int polarity, int pre_emphasis, int de_emphasis, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, const char *read_tx_wave, int loopback)
{
	sender_t *sender;
	cnetz_t *cnetz;
	int kanal;
	int rc;

	kanal = atoi(kanal_name);
	if ((kanal & 1) && (kanal < 3 || kanal > 1147)) {
		PDEBUG(DCNETZ, DEBUG_ERROR, "Channel ('Kanal') number %d invalid. For odd channel numbers, use channel 3 ... 1147.\n", kanal);
		return -EINVAL;
	}
	if ((kanal & 1) && kanal > 947) {
		PDEBUG(DCNETZ, DEBUG_NOTICE, "You defined an extended frequency channel %d, only newer phones support this!\n", kanal);
	}
	if (!(kanal & 1) && (kanal < 4 || kanal > 918)) {
		PDEBUG(DCNETZ, DEBUG_ERROR, "Channel ('Kanal') number %d invalid. For even channel numbers, use channel 4 ... 918.\n", kanal);
		return -EINVAL;
	}
	if (!(kanal & 1) && kanal > 758) {
		PDEBUG(DCNETZ, DEBUG_NOTICE, "You defined an extended frequency channel %d, only newer phones support this!\n", kanal);
	}

	/* SpK must be on channel other than 131 */
	if (chan_type == CHAN_TYPE_SPK && kanal == CNETZ_STD_OGK_KANAL) {
		PDEBUG(DCNETZ, DEBUG_NOTICE, "You must not use channel %d for traffic channel!\n", CNETZ_STD_OGK_KANAL);
		return -EINVAL;
	}

	/* warn if we combine SpK and OgK, this is not supported by standard */
	if (chan_type == CHAN_TYPE_OGK_SPK) {
		PDEBUG(DCNETZ, DEBUG_NOTICE, "You selected channel %d ('Orga-Kanal') for combined control + traffic channel. Some phones will reject this.\n", kanal);
	}

	for (sender = sender_head; sender; sender = sender->next) {
		cnetz = (cnetz_t *)sender;
		if (!!strcmp(sender->device, device)) {
			PDEBUG(DCNETZ, DEBUG_NOTICE, "To be able to sync multiple channels, all channels must be on the same sound device!\n");
			return -EINVAL;
		}
	}

	cnetz = calloc(1, sizeof(cnetz_t));
	if (!cnetz) {
		PDEBUG(DCNETZ, DEBUG_ERROR, "No memory!\n");
		return -ENOMEM;
	}

	PDEBUG(DCNETZ, DEBUG_DEBUG, "Creating 'C-Netz' instance for 'Kanal' = %d (sample rate %d).\n", kanal, samplerate);

	cnetz->kanal = kanal;
	if ((chan_type == CHAN_TYPE_OGK || chan_type == CHAN_TYPE_OGK_SPK) && kanal != CNETZ_STD_OGK_KANAL) {
		if (ogk_list_count == 16) {
			PDEBUG(DCNETZ, DEBUG_ERROR, "No more than 16 non-standard OGK are allowed!\n");
			rc = -EINVAL;
			goto error;
		}
		ogk_list[ogk_list_count++] = cnetz;
	}

	/* init general part of transceiver */
	/* do not enable emphasis, since it is done by cnetz code, not by common sender code */
	rc = sender_create(&cnetz->sender, kanal_name, cnetz_kanal2freq(kanal, 0), cnetz_kanal2freq(kanal, 1), device, use_sdr, samplerate, rx_gain, tx_gain, 0, 0, write_rx_wave, write_tx_wave, read_rx_wave, read_tx_wave, loopback, PAGING_SIGNAL_NONE);
	if (rc < 0) {
		PDEBUG(DCNETZ, DEBUG_ERROR, "Failed to init transceiver process!\n");
		goto error;
	}

#if 0
	#warning hacking: applying different clock to slave
	if (&cnetz->sender != sender_head) {
		clock_speed[0] = -3;
		clock_speed[1] = -3;
	}
#endif

	/* init audio processing */
	rc = dsp_init_sender(cnetz, measure_speed, clock_speed, demod, speech_deviation);
	if (rc < 0) {
		PDEBUG(DCNETZ, DEBUG_ERROR, "Failed to init signal processing!\n");
		goto error;
	}

	cnetz->chan_type = chan_type;
	cnetz->challenge_valid = challenge_valid;
	cnetz->challenge = challenge;
	cnetz->response_valid = response_valid;
	cnetz->response = response;
	cnetz->warteschlange = warteschlange;
	cnetz->metering = metering;
	cnetz->ms_power = ms_power;

	switch (polarity) {
	case 1:
		/* select cell 0 for positive polarity */
		cnetz->negative_polarity = 0;
		cnetz->auto_polarity = 0;
		break;
	case -1:
		/* select cell 1 for negative polarity */
		cnetz->negative_polarity = 1;
		cnetz->auto_polarity = 0;
		break;
	default:
		/* send two cells and select by the first message from mobile */
		cnetz->negative_polarity = 0;
		cnetz->auto_polarity = 1;
	}

	cnetz->pre_emphasis = pre_emphasis;
	cnetz->de_emphasis = de_emphasis;
	rc = init_emphasis(&cnetz->estate, samplerate, CUT_OFF_EMPHASIS_CNETZ, CUT_OFF_HIGHPASS_DEFAULT, CUT_OFF_LOWPASS_DEFAULT);
	if (rc < 0)
		goto error;

	/* go into idle state */
	if (chan_type == CHAN_TYPE_OGK || chan_type == CHAN_TYPE_OGK_SPK)
		cnetz_set_dsp_mode(cnetz, DSP_MODE_OGK);
	else
		cnetz_set_dsp_mode(cnetz, DSP_MODE_OFF);
	cnetz_go_idle(cnetz);

#ifdef DEBUG_SPK
	transaction_t *trans = create_transaction(cnetz, TRANS_DS, 2, 2, 22002, -1, -1, NAN);
	trans->mo_call = 1;
	cnetz_set_sched_dsp_mode(cnetz, DSP_MODE_SPK_K, (cnetz->sched_ts + 2) & 31);
#else
	/* create transaction for speech channel loopback */
	if (loopback && chan_type == CHAN_TYPE_SPK) {
		transaction_t *trans = create_transaction(cnetz, TRANS_VHQ_K, 2, 2, 22002, -1, -1, NAN);
		trans->mo_call = 1;
		cnetz_set_dsp_mode(cnetz, DSP_MODE_SPK_K);
		cnetz_set_sched_dsp_mode(cnetz, DSP_MODE_SPK_K, (cnetz->sched_ts + 1) & 31);
	}
#endif

#if 0
	/* debug flushing transactions */
	transaction_t *trans1, *trans2;
	trans1 = create_transaction(cnetz, 99, 6, 2, 15784, -1, -1, NAN);
	destroy_transaction(trans1);
	trans1 = create_transaction(cnetz, 99, 6, 2, 15784, -1, -1, NAN);
	destroy_transaction(trans1);
	trans1 = create_transaction(cnetz, 99, 6, 2, 15784, -1, -1, NAN);
	trans2 = create_transaction(cnetz, 99, 2, 2, 22002, -1, -1, NAN);
	unlink_transaction(trans1);
	link_transaction(trans1, cnetz);
	cnetz_flush_other_transactions(cnetz, trans1);
	trans2 = create_transaction(cnetz, 99, 2, 2, 22002, -1, -1, NAN);
	cnetz_flush_other_transactions(cnetz, trans2);
#endif

	PDEBUG(DCNETZ, DEBUG_NOTICE, "Created 'Kanal' #%d of type '%s' = %s\n", kanal, chan_type_short_name(chan_type), chan_type_long_name(chan_type));
	const char *name, *station;
	name = get_station_name(si.fuz_nat, si.fuz_fuvst, si.fuz_rest, &station);
	PDEBUG(DNMT, DEBUG_NOTICE, " -> Using cell ID: Nat=%d FuVst=%d Rest=%d Name='%s' (%s)\n", si.fuz_nat, si.fuz_fuvst, si.fuz_rest, name, station);

	return 0;

error:
	cnetz_destroy(&cnetz->sender);

	return rc;
}

/* Destroy transceiver instance and unlink from list. */
void cnetz_destroy(sender_t *sender)
{
	cnetz_t *cnetz = (cnetz_t *) sender;
	transaction_t *trans;

	PDEBUG(DCNETZ, DEBUG_DEBUG, "Destroying 'C-Netz' instance for 'Kanal' = %s.\n", sender->kanal);

	while ((trans = search_transaction(cnetz, ~0))) {
		const char *rufnummer = transaction2rufnummer(trans);
		PDEBUG(DCNETZ, DEBUG_NOTICE, "Removing pending transaction for subscriber '%s'\n", rufnummer);
		destroy_transaction(trans);
	}

	dsp_cleanup_sender(cnetz);
	sender_destroy(&cnetz->sender);
	free(cnetz);
}

static cnetz_t *search_free_spk(int extended)
{
	sender_t *sender;
	cnetz_t *cnetz, *ogk_spk = NULL;

	for (sender = sender_head; sender; sender = sender->next) {
		cnetz = (cnetz_t *) sender;
		/* ignore extended frequency, if not supported */
		if (!extended) {
			if ((cnetz->kanal & 1) && cnetz->kanal > 947)
				continue;
			if (!(cnetz->kanal & 1) && cnetz->kanal > 758)
				continue;
		}
		/* ignore busy channel */
		if (cnetz->state != CNETZ_IDLE)
			continue;
		/* return first free SpK */
		if (cnetz->chan_type == CHAN_TYPE_SPK)
			return cnetz;
		/* remember OgK/SpK combined channel as second alternative */
		if (cnetz->chan_type == CHAN_TYPE_OGK_SPK)
			ogk_spk = cnetz;
	}

	return ogk_spk;
}

static cnetz_t *search_ogk(int kanal)
{
	sender_t *sender;
	cnetz_t *cnetz;

	for (sender = sender_head; sender; sender = sender->next) {
		cnetz = (cnetz_t *) sender;
		/* ignore busy channel */
		if (cnetz->state != CNETZ_IDLE)
			continue;
		if (cnetz->kanal != kanal)
			continue;
		if (cnetz->chan_type == CHAN_TYPE_OGK)
			return cnetz;
		if (cnetz->chan_type == CHAN_TYPE_OGK_SPK)
			return cnetz;
	}

	return NULL;
}

/* Abort connection, if any and send idle broadcast */
void cnetz_go_idle(cnetz_t *cnetz)
{
	transaction_t *trans;

	if (cnetz->state == CNETZ_IDLE)
		return;

	if (cnetz->trans_list) {
		PDEBUG(DCNETZ, DEBUG_ERROR, "Releasing but still having transaction, please fix!\n");
		if (cnetz->trans_list->callref)
			call_up_release(cnetz->trans_list->callref, CAUSE_NORMAL);
		destroy_transaction(cnetz->trans_list);
	}

	PDEBUG(DCNETZ, DEBUG_INFO, "Entering IDLE state on channel %s.\n", cnetz->sender.kanal);
	cnetz_new_state(cnetz, CNETZ_IDLE);
	cnetz->sched_lr_debugged = 0;
	cnetz->sched_mlr_debugged = 0;

	/* set scheduler to OgK or turn off SpK */
	if (cnetz->dsp_mode == DSP_MODE_SPK_K || cnetz->dsp_mode == DSP_MODE_SPK_V) {
		/* switch next frame after distributed signaling boundary (multiple of 8 slots) */
		cnetz_set_sched_dsp_mode(cnetz, (cnetz->chan_type == CHAN_TYPE_OGK || cnetz->chan_type == CHAN_TYPE_OGK_SPK) ? DSP_MODE_OGK : DSP_MODE_OFF, (cnetz->sched_ts + 8) & 24);
	} else {
		/* switch next frame */
		cnetz_set_sched_dsp_mode(cnetz, (cnetz->chan_type == CHAN_TYPE_OGK || cnetz->chan_type == CHAN_TYPE_OGK_SPK) ? DSP_MODE_OGK : DSP_MODE_OFF, (cnetz->sched_ts + 1) & 31);
		cnetz_set_dsp_mode(cnetz, (cnetz->chan_type == CHAN_TYPE_OGK || cnetz->chan_type == CHAN_TYPE_OGK_SPK) ? DSP_MODE_OGK : DSP_MODE_OFF);
	}

	/* check for first phone in queue and trigger completion of call (becoming idle means that SpK is now available)  */
	trans = search_transaction_queue();
	if (trans) {
		PDEBUG(DCNETZ, DEBUG_NOTICE, "Now channel is available for queued subscriber '%s'.\n", transaction2rufnummer(trans));
		trans_new_state(trans, (trans->state == TRANS_MT_QUEUE) ? TRANS_MT_DELAY : TRANS_MO_DELAY);
		timer_stop(&trans->timer);
		timer_start(&trans->timer, 3.0); /* Wait at least one frame cycles until timeout */
	}
}

/* Initiate release connection on speech channel */
static void cnetz_release(transaction_t *trans, uint8_t cause)
{
	trans_new_state(trans, (trans->cnetz->dsp_mode == DSP_MODE_OGK) ? TRANS_VA : TRANS_AF);
	trans->repeat = 0;
	trans->release_cause = cause;
	trans->cnetz->sched_dsp_mode_ts = -1;
	timer_stop(&trans->timer);
}

/* Receive audio from call instance. */
void call_down_audio(int callref, sample_t *samples, int count)
{
	sender_t *sender;
	cnetz_t *cnetz;

	for (sender = sender_head; sender; sender = sender->next) {
		cnetz = (cnetz_t *) sender;
		if (cnetz->trans_list && cnetz->trans_list->callref == callref)
			break;
	}
	if (!sender)
		return;

	if (cnetz->dsp_mode == DSP_MODE_SPK_V) {
		/* store as is, since we convert rate when processing FSK frames */
		jitter_save(&cnetz->sender.dejitter, samples, count);
	}
}

void call_down_clock(void) {}

int call_down_setup(int callref, const char __attribute__((unused)) *caller_id, enum number_type __attribute__((unused)) caller_type, const char *dialing)
{
	cnetz_t *ogk, *spk;
	int rc;
	int extended;
	transaction_t *trans;
	uint8_t futln_nat;
	uint8_t futln_fuvst;
	int futln_rest; /* use int for checking size > 65535 */
	int ogk_kanal;

	/* 1. split number into elements */
	futln_nat = dialing[0] - '0';
	if (dialing[7]) {
		futln_fuvst = (dialing[1] - '0') * 10 + (dialing[2] - '0');
		futln_rest = atoi(dialing + 3);
	} else {
		futln_fuvst = dialing[1] - '0';
		futln_rest = atoi(dialing + 2);
	}

	/* 2. check if the subscriber is attached */
	rc = find_db(futln_nat, futln_fuvst, futln_rest, &ogk_kanal, NULL, &extended);
	if (rc < 0) {
		PDEBUG(DCNETZ, DEBUG_NOTICE, "Outgoing call to not attached subscriber, rejecting!\n");
		return -CAUSE_OUTOFORDER;
	}

	/* 3. check if given number is already in a call, return BUSY */
	trans = search_transaction_number_global(futln_nat, futln_fuvst, futln_rest);
	if (trans) {
		PDEBUG(DCNETZ, DEBUG_NOTICE, "Outgoing call to busy number, rejecting!\n");
		return -CAUSE_BUSY;
	}

	/* 4. check if we have no OgK, return NOCHANNEL */
	ogk = search_ogk(ogk_kanal);
	if (!ogk) {
		PDEBUG(DCNETZ, DEBUG_NOTICE, "Outgoing call, but OgK is currently busy, rejecting!\n");
		return -CAUSE_NOCHANNEL;
	}

	/* 5. check if all senders are busy, return NOCHANNEL */
	spk = search_free_spk(extended);
	if (!spk) {
		if (!ogk->warteschlange) {
			PDEBUG(DCNETZ, DEBUG_NOTICE, "Outgoing call, but no free channel, rejecting!\n");
			return -CAUSE_NOCHANNEL;
		} else
			PDEBUG(DCNETZ, DEBUG_NOTICE, "Outgoing call, but no free channel, queuing call!\n");
	}

	PDEBUG(DCNETZ, DEBUG_INFO, "Call to mobile station, paging station id '%s'\n", dialing);

	/* 6. trying to page mobile station */
	trans = create_transaction(ogk, (spk) ? TRANS_VAK : TRANS_WSK, futln_nat, futln_fuvst, futln_rest, -1, -1, NAN);
	if (!trans) {
		PDEBUG(DCNETZ, DEBUG_ERROR, "Failed to create transaction\n");
		return -CAUSE_TEMPFAIL;
	}
	trans->callref = callref;
	trans->try = 1;

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
	cnetz_t *cnetz;
	transaction_t *trans;

	PDEBUG(DCNETZ, DEBUG_INFO, "Call has been disconnected by network.\n");

	for (sender = sender_head; sender; sender = sender->next) {
		cnetz = (cnetz_t *) sender;
		/* search transaction for this callref */
		trans = search_transaction_callref(cnetz, callref);
		if (trans)
			break;
	}
	if (!sender) {
		PDEBUG(DCNETZ, DEBUG_NOTICE, "Outgoing disconnect, but no callref!\n");
		call_up_release(callref, CAUSE_INVALCALLREF);
		return;
	}

	/* Release when not active */

	switch (cnetz->dsp_mode) {
	case DSP_MODE_SPK_V:
		return;
	case DSP_MODE_SPK_K:
		PDEBUG(DCNETZ, DEBUG_INFO, "Call control disconnects on speech channel, releasing towards mobile station.\n");
		cnetz_release(trans, cnetz_cause_isdn2cnetz(cause));
		call_up_release(callref, cause);
		trans->callref = 0;
		break;
	default:
		PDEBUG(DCNETZ, DEBUG_INFO, "Call control disconnects on organisation channel, removing transaction.\n");
		call_up_release(callref, cause);
		trans->callref = 0;
		if (trans->state == TRANS_MT_QUEUE || trans->state == TRANS_MT_DELAY) {
			cnetz_release(trans, cnetz_cause_isdn2cnetz(cause));
		} else {
			destroy_transaction(trans);
			cnetz_go_idle(cnetz);
		}
	}

}

/* Call control releases call toward mobile station. */
void call_down_release(int callref, int cause)
{
	sender_t *sender;
	cnetz_t *cnetz;
	transaction_t *trans;

	PDEBUG(DCNETZ, DEBUG_INFO, "Call has been released by network, releasing call.\n");

	for (sender = sender_head; sender; sender = sender->next) {
		cnetz = (cnetz_t *) sender;
		/* search transaction for this callref */
		trans = search_transaction_callref(cnetz, callref);
		if (trans)
			break;
	}
	if (!sender) {
		PDEBUG(DCNETZ, DEBUG_NOTICE, "Outgoing release, but no callref!\n");
		/* don't send release, because caller already released */
		return;
	}

	trans->callref = 0;

	switch (cnetz->dsp_mode) {
	case DSP_MODE_SPK_K:
	case DSP_MODE_SPK_V:
		PDEBUG(DCNETZ, DEBUG_INFO, "Call control releases on speech channel, releasing towards mobile station.\n");
		cnetz_release(trans, cnetz_cause_isdn2cnetz(cause));
		break;
	default:
		PDEBUG(DCNETZ, DEBUG_INFO, "Call control releases on organisation channel, removing transaction.\n");
		if (trans->state == TRANS_MT_QUEUE) {
			cnetz_release(trans, cnetz_cause_isdn2cnetz(cause));
		} else {
			destroy_transaction(trans);
			cnetz_go_idle(cnetz);
		}
	}
}

int cnetz_meldeaufruf(uint8_t futln_nat, uint8_t futln_fuvst, uint16_t futln_rest, int ogk_kanal)
{
	cnetz_t *cnetz;
	transaction_t *trans;

	cnetz = search_ogk(ogk_kanal);
	if (!cnetz) {
		PDEBUG(DCNETZ, DEBUG_NOTICE, "'Meldeaufruf', but OgK is currently busy!\n");
		return -CAUSE_NOCHANNEL;
	}
	trans = create_transaction(cnetz, TRANS_MA, futln_nat, futln_fuvst, futln_rest, -1, -1, NAN);
	if (!trans) {
		PDEBUG(DCNETZ, DEBUG_ERROR, "Failed to create transaction\n");
		return -CAUSE_TEMPFAIL;
	}

	return 0;
}

static struct cnetz_channels {
	enum cnetz_chan_type chan_type;
	const char *short_name;
	const char *long_name;
} cnetz_channels[] = {
	{ CHAN_TYPE_OGK_SPK,	"OgK/SpK","combined control & voice channel" },
	{ CHAN_TYPE_OGK,	"OgK",	"control channel" },
	{ CHAN_TYPE_SPK,	"SpK",	"voice channel" },
	{ 0, NULL, NULL }
};

void cnetz_channel_list(void)
{
	int i;

	printf("Type\tDescription\n");
	printf("------------------------------------------------------------------------\n");
	for (i = 0; cnetz_channels[i].long_name; i++)
		printf("%s\t%s\n", cnetz_channels[i].short_name, cnetz_channels[i].long_name);
}

int cnetz_channel_by_short_name(const char *short_name)
{
	int i;

	for (i = 0; cnetz_channels[i].short_name; i++) {
		if (!strcasecmp(cnetz_channels[i].short_name, short_name))
			return cnetz_channels[i].chan_type;
	}

	return -1;
}

const char *chan_type_short_name(enum cnetz_chan_type chan_type)
{
	int i;

	for (i = 0; cnetz_channels[i].short_name; i++) {
		if (cnetz_channels[i].chan_type == chan_type)
			return cnetz_channels[i].short_name;
	}

	return "invalid";
}

const char *chan_type_long_name(enum cnetz_chan_type chan_type)
{
	int i;

	for (i = 0; cnetz_channels[i].long_name; i++) {
		if (cnetz_channels[i].chan_type == chan_type)
			return cnetz_channels[i].long_name;
	}

	return "invalid";
}

/* Timeout handling */
void transaction_timeout(struct timer *timer)
{
	transaction_t *trans = (transaction_t *)timer->priv;
	cnetz_t *cnetz = trans->cnetz;

	switch (trans->state) {
	case TRANS_MT_QUEUE:
		PDEBUG_CHAN(DCNETZ, DEBUG_NOTICE, "Phone in queue, but still no channel available, releasing call!\n");
		call_up_release(trans->callref, CAUSE_NOCHANNEL);
		trans->callref = 0;
		cnetz_release(trans, CNETZ_CAUSE_GASSENBESETZT);
		break;
	case TRANS_MO_QUEUE:
		PDEBUG_CHAN(DCNETZ, DEBUG_NOTICE, "Phone in queue, but still no channel available, releasing!\n");
		cnetz_release(trans, CNETZ_CAUSE_GASSENBESETZT);
		break;
	case TRANS_MT_DELAY:
		trans_new_state(trans, TRANS_VAK);
		break;
	case TRANS_MO_DELAY:
		trans_new_state(trans, TRANS_VAG);
		break;
	case TRANS_BQ:
		PDEBUG_CHAN(DCNETZ, DEBUG_NOTICE, "No response after channel allocation 'Belegung Quittung'\n");
		trans_new_state(trans, TRANS_AF);
		trans->repeat = 0;
		break;
	case TRANS_ZFZ:
		PDEBUG_CHAN(DCNETZ, DEBUG_NOTICE, "No response after sending random number 'Zufallszahl'\n");
		if (trans->callref) {
			call_up_release(trans->callref, CAUSE_TEMPFAIL);
			trans->callref = 0;
		}
		cnetz_release(trans, CNETZ_CAUSE_FUNKTECHNISCH);
		break;
	case TRANS_AP:
		PDEBUG_CHAN(DCNETZ, DEBUG_NOTICE, "No response after waiting for challenge response 'Autorisierungsparameter'\n");
		if (trans->callref) {
			call_up_release(trans->callref, CAUSE_TEMPFAIL);
			trans->callref = 0;
		}
		cnetz_release(trans, CNETZ_CAUSE_FUNKTECHNISCH);
		break;
	case TRANS_VHQ_K:
	case TRANS_VHQ_V:
		if (cnetz->dsp_mode != DSP_MODE_SPK_V)
			PDEBUG_CHAN(DCNETZ, DEBUG_NOTICE, "No response while holding call 'Quittung Verbindung halten'\n");
		else
			PDEBUG_CHAN(DCNETZ, DEBUG_NOTICE, "Lost signal from 'FuTln' (mobile station)\n");
		if (trans->callref) {
			call_up_release(trans->callref, CAUSE_TEMPFAIL);
			trans->callref = 0;
		}
		cnetz_release(trans, CNETZ_CAUSE_FUNKTECHNISCH);
		break;
	case TRANS_DS:
		PDEBUG_CHAN(DCNETZ, DEBUG_NOTICE, "No response after connect 'Durchschalten'\n");
		call_up_release(trans->callref, CAUSE_TEMPFAIL);
		trans->callref = 0;
		cnetz_release(trans, CNETZ_CAUSE_FUNKTECHNISCH);
		break;
	case TRANS_RTA:
		PDEBUG_CHAN(DCNETZ, DEBUG_NOTICE, "No response after ringing order 'Rufton anschalten'\n");
		call_up_release(trans->callref, CAUSE_TEMPFAIL);
		trans->callref = 0;
		cnetz_release(trans, CNETZ_CAUSE_FUNKTECHNISCH);
		break;
	case TRANS_AHQ:
		PDEBUG_CHAN(DCNETZ, DEBUG_NOTICE, "No response after answer 'Abhebequittung'\n");
		call_up_release(trans->callref, CAUSE_TEMPFAIL);
		trans->callref = 0;
		cnetz_release(trans, CNETZ_CAUSE_FUNKTECHNISCH);
		break;
	default:
		PDEBUG_CHAN(DCNETZ, DEBUG_ERROR, "Timeout unhandled in state %" PRIu64 "\n", trans->state);
	}
}

/*
 * sync to phone
 *
 * because we don't know the actual delay on sound card, we need to sync
 * to the phone, that is synced to us.
 *
 * if block is given, we can set sync to absolute position in super frame.
 * if not, we just sync to the nearest block.
 */

void cnetz_sync_frame(cnetz_t *cnetz, double sync, int block)
{
	double offset;

	if (block >= 0) {
		/* offset is the actual sync relative to bit_time */
		offset = fmod(sync - BITS_PER_BLOCK * (double)block + BITS_PER_SUPERFRAME, BITS_PER_SUPERFRAME);
		if (offset > BITS_PER_SUPERFRAME / 2)
			offset -= BITS_PER_SUPERFRAME;
	} else {
		/* sync to the nearest block */
		/* offset is the actual sync relative to bit_time */
		offset = fmod(sync, BITS_PER_BLOCK);
		if (offset > BITS_PER_BLOCK / 2)
			offset -= BITS_PER_BLOCK;
	}
	/* if more than +- one bit out of sync */
	if (offset < -0.5 || offset > 0.5) {
		PDEBUG_CHAN(DCNETZ, DEBUG_NOTICE, "Frame sync offset = %.2f, correcting!\n", offset);
		fsk_correct_sync(&cnetz->fsk_demod, offset);
		return;
	}

	/* resync by some fraction of received sync error */
	PDEBUG_CHAN(DCNETZ, DEBUG_DEBUG, "Frame sync offset = %.2f, correcting.\n", offset);
	fsk_correct_sync(&cnetz->fsk_demod, offset / 2.0);
}

/*
 * OgK handling
 */

/* transmit rufblock */
const telegramm_t *cnetz_transmit_telegramm_rufblock(cnetz_t *cnetz)
{
	static telegramm_t telegramm;
	transaction_t *trans;
	cnetz_t *spk;

	memset(&telegramm, 0, sizeof(telegramm));

	telegramm.opcode = OPCODE_LR_R;
	telegramm.max_sendeleistung = cnetz_power2bits(cnetz->ms_power);
	telegramm.bedingte_genauigkeit_der_fufst = si.genauigkeit;
	telegramm.zeitschlitz_nr = cnetz->sched_ts;
	telegramm.grenzwert_fuer_einbuchen_und_umbuchen = si.grenz_einbuchen;
	telegramm.authentifikationsbit = si.authentifikationsbit;
	telegramm.vermittlungstechnische_sperren = si.vermittlungstechnische_sperren;
	telegramm.ws_kennung = si.ws_kennung;
	telegramm.reduzierungsfaktor = si.reduzierung;
	telegramm.fuz_nationalitaet = si.fuz_nat;
	telegramm.fuz_fuvst_nr = si.fuz_fuvst;
	telegramm.fuz_rest_nr = si.fuz_rest;
	telegramm.kennung_fufst = si.kennung_fufst;
	telegramm.bahn_bs = si.bahn_bs;
	telegramm.nachbarschafts_prioritaets_bit = si.nachbar_prio;
	telegramm.bewertung_nach_pegel_und_entfernung = si.bewertung;
	telegramm.entfernungsangabe_der_fufst = si.entfernung;
	telegramm.mittelungsfaktor_fuer_ausloesen = si.mittel_ausloesen;
	telegramm.mittelungsfaktor_fuer_umschalten = si.mittel_umschalten;
	telegramm.grenzwert_fuer_umschalten = si.grenz_umschalten;
	telegramm.grenze_fuer_ausloesen = si.grenz_ausloesen;
	
	trans = search_transaction(cnetz, TRANS_EM | TRANS_UM | TRANS_WBN | TRANS_WBP | TRANS_VAG | TRANS_VAK | TRANS_ATQ | TRANS_ATQ_IDLE | TRANS_VA | TRANS_WSK);
	if (trans) {
		telegramm.futln_nationalitaet = trans->futln_nat;
		telegramm.futln_heimat_fuvst_nr = trans->futln_fuvst;
		telegramm.futln_rest_nr = trans->futln_rest;
		telegramm.ausloesegrund = trans->release_cause;
		switch (trans->state) {
		case TRANS_EM:
			PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Sending acknowledgment 'Einbuchquittung' to Attachment request.\n");
			telegramm.opcode = OPCODE_EBQ_R;
			destroy_transaction(trans);
			break;
		case TRANS_UM:
			PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Sending acknowledgment 'Umbuchquittung' to Roaming requuest.\n");
			telegramm.opcode = OPCODE_UBQ_R;
			destroy_transaction(trans);
			break;
		case TRANS_WBN:
wbn:
			PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Sending call reject 'Wahlbestaetigung negativ'.\n");
			telegramm.opcode = OPCODE_WBN_R;
			destroy_transaction(trans);
			cnetz_go_idle(cnetz);
			break;
		case TRANS_WBP:
			spk = search_free_spk(trans->extended);
			/* Accept call if channel available, otherwise reject or queue call */
			if (spk) {
				PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Sending call accept 'Wahlbestaetigung positiv'.\n");
				telegramm.opcode = OPCODE_WBP_R;
				trans_new_state(trans, TRANS_VAG);
			} else if (cnetz->warteschlange) {
				/* queue call if no channel is available, but queue allowed */
				PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "No free channel, sending call accept in queue 'Wahlbestaetigung positiv in Warteschlage'.\n");
				telegramm.opcode = OPCODE_WWBP_R;
				trans_new_state(trans, TRANS_MO_QUEUE);
				timer_start(&trans->timer, T_VAG2); /* Maximum time to hold queue */
			} else {
				PDEBUG(DCNETZ, DEBUG_NOTICE, "No free channel anymore, rejecting call!\n");
				trans_new_state(trans, TRANS_WBN);
				goto wbn;
			}
			break;
		case TRANS_VAG:
		case TRANS_VAK:
vak:
			if (trans->state == TRANS_VAG) {
				PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Sending channel assignment 'Verbindungsaufbau gehend'.\n");
				telegramm.opcode = OPCODE_VAG_R;
			} else {
				PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Sending channel assignment 'Verbindungsaufbau kommend'.\n");
				telegramm.opcode = OPCODE_VAK_R;
			}
			trans_new_state(trans, TRANS_BQ);
			trans->repeat = 0;
			timer_start(&trans->timer, 0.150 + 0.0375 * F_BQ); /* two slots + F_BQ frames */
			/* select channel */
			spk = search_free_spk(trans->extended);
			if (!spk) {
				PDEBUG(DCNETZ, DEBUG_NOTICE, "No free channel anymore, rejecting call!\n");
				destroy_transaction(trans);
				cnetz_go_idle(cnetz);
				break;
			}
			if (spk == cnetz) {
				PDEBUG(DCNETZ, DEBUG_INFO, "Staying on combined control + traffic channel %s\n", spk->sender.kanal);
			} else {
				PDEBUG(DCNETZ, DEBUG_INFO, "Assigning phone to traffic channel %s\n", spk->sender.kanal);
				/* sync RX time to current OgK time */
				fsk_copy_sync(&spk->fsk_demod, &cnetz->fsk_demod);
			}
			/* set channel */
			telegramm.frequenz_nr = spk->kanal;
			/* change state to busy */
			cnetz_new_state(spk, CNETZ_BUSY);
			/* schedule switching two slots ahead */
			cnetz_set_sched_dsp_mode(spk, DSP_MODE_SPK_K, (cnetz->sched_ts + 2) & 31);
			/* relink */
			unlink_transaction(trans);
			link_transaction(trans, spk);
			/* flush all other transactions, if any (in case of OgK/SpK) */
			cnetz_flush_other_transactions(spk, trans);
			break;
		case TRANS_ATQ:
		case TRANS_ATQ_IDLE:
			PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Sending acknowledgment 'Quittung fuer Ausloesen des FuTelG im OgK-Betrieb' to release request.\n");
			telegramm.opcode = OPCODE_ATQ_R;
			destroy_transaction(trans);
			break;
		case TRANS_VA:
			PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Sending 'Vorzeitiges Ausloesen' to queued mobile station\n");
			telegramm.opcode = OPCODE_VA_R;
			destroy_transaction(trans);
			break;
		case TRANS_WSK:
			spk = search_free_spk(trans->extended);
			/* if channel becomes free before we send the queue information, we proceed with channel assignment */
			if (spk) {
				trans_new_state(trans, TRANS_VAK);
				goto vak;
			}
			PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "No free channel, sending incoming call in queue 'Warteschglange kommend'.\n");
			telegramm.opcode = OPCODE_WSK_R;
			trans_new_state(trans, TRANS_MT_QUEUE);
			timer_start(&trans->timer, T_VAK); /* Maximum time to hold queue */
			call_up_alerting(trans->callref);
		default:
			; /* LR */
		}
	}

	return &telegramm;
}

/* transmit meldeblock */
const telegramm_t *cnetz_transmit_telegramm_meldeblock(cnetz_t *cnetz)
{
	static telegramm_t telegramm;
	transaction_t *trans;

	memset(&telegramm, 0, sizeof(telegramm));
	telegramm.opcode = OPCODE_MLR_M;
	telegramm.max_sendeleistung = cnetz_power2bits(cnetz->ms_power);
	telegramm.ogk_verkehrsanteil = 0; /* must be 0 or some phone might not respond to messages in different slots */
	telegramm.teilnehmergruppensperre = si.teilnehmergruppensperre;
	telegramm.anzahl_gesperrter_teilnehmergruppen = si.anzahl_gesperrter_teilnehmergruppen;
	if (ogk_list_count) {
		/* if we have alternative OGKs, we cycle through the list and indicate their channels */
		telegramm.ogk_vorschlag = ogk_list[ogk_list_index]->kanal;
		if (++ogk_list_index == ogk_list_count)
			ogk_list_index = 0;
	} else
		telegramm.ogk_vorschlag = CNETZ_STD_OGK_KANAL;
	telegramm.fuz_rest_nr = si.fuz_rest;

next_candidate:
	trans = search_transaction(cnetz, TRANS_VWG | TRANS_WAF | TRANS_MA | TRANS_MFT);
	if (trans) {
		switch (trans->state) {
		case TRANS_WAF:
			/* no response to dial request (try again or drop connection) */
			PDEBUG_CHAN(DCNETZ, DEBUG_NOTICE, "No response after dialing request 'Wahlaufforderung'\n");
			if (trans->try == N) {
				trans_new_state(trans, TRANS_WBN);
				goto next_candidate;
			}
			trans->try++;
			trans_new_state(trans, TRANS_VWG);
			/* FALLTHRU */
		case TRANS_VWG:
			PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Sending acknowledgment 'Wahlaufforderung' to outging call\n");
			telegramm.opcode = OPCODE_WAF_M;
			telegramm.futln_nationalitaet = trans->futln_nat;
			telegramm.futln_heimat_fuvst_nr = trans->futln_fuvst;
			telegramm.futln_rest_nr = trans->futln_rest;
			trans_new_state(trans, TRANS_WAF);
			break;
		case TRANS_MA:
			PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Sending keepalive request 'Meldeaufruf'\n");
			telegramm.opcode = OPCODE_MA_M;
			telegramm.futln_nationalitaet = trans->futln_nat;
			telegramm.futln_heimat_fuvst_nr = trans->futln_fuvst;
			telegramm.futln_rest_nr = trans->futln_rest;
			trans_new_state(trans, TRANS_MFT);
			break;
		case TRANS_MFT:
			/* no response to availability check */
			PDEBUG_CHAN(DCNETZ, DEBUG_NOTICE, "No response after keepalive order 'Meldeaufruf'\n");
			trans->page_failed = 1;
			destroy_transaction(trans);
			goto next_candidate;
		default:
			; /* MLR */
		}
	}

	return &telegramm;
}

void cnetz_receive_telegramm_ogk(cnetz_t *cnetz, telegramm_t *telegramm, int block)
{
	uint8_t opcode = telegramm->opcode;
	int valid_frame = 0;
	transaction_t *trans;
	const char *rufnummer;

	switch (opcode) {
	case OPCODE_EM_R:
		if (!match_fuz(telegramm))
			break;
		rufnummer = telegramm2rufnummer(telegramm);
		if (si.authentifikationsbit && telegramm->chipkarten_futelg_bit)
			PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Received Attachment 'Einbuchen' message from Subscriber '%s' with chip card's ID %d (vendor id %d, hardware version %d, software version %d)\n", rufnummer, telegramm->kartenkennung, telegramm->herstellerkennung, telegramm->hardware_des_futelg, telegramm->software_des_futelg);
		else
			PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Received Attachment 'Einbuchen' message from Subscriber '%s' with %s card's security code %d\n", rufnummer, (telegramm->chipkarten_futelg_bit) ? "chip":"magnet", telegramm->sicherungs_code);
		if (telegramm->erweitertes_frequenzbandbit)
			PDEBUG(DCNETZ, DEBUG_INFO, " -> Phone supports extended frequency band\n");
		if (cnetz->state != CNETZ_IDLE) {
			PDEBUG(DCNETZ, DEBUG_NOTICE, "Ignoring Attachment from subscriber '%s', because we are busy becoming SpK.\n", rufnummer);
			break;
		}
		trans = create_transaction(cnetz, TRANS_EM, telegramm->futln_nationalitaet, telegramm->futln_heimat_fuvst_nr, telegramm->futln_rest_nr, telegramm->chipkarten_futelg_bit, telegramm->erweitertes_frequenzbandbit, cnetz->rf_level_db);
		if (!trans) {
			PDEBUG(DCNETZ, DEBUG_ERROR, "Failed to create transaction\n");
			break;
		}
		cnetz = trans->cnetz; /* cnetz may change, due to stronger reception on different OgK */
		valid_frame = 1;
		break;
	case OPCODE_UM_R:
		if (!match_fuz(telegramm))
			break;
		rufnummer = telegramm2rufnummer(telegramm);
		if (si.authentifikationsbit && telegramm->chipkarten_futelg_bit)
			PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Received Roaming 'Umbuchen' message from Subscriber '%s' with chip card's ID %d (vendor id %d, hardware version %d, software version %d)\n", rufnummer, telegramm->kartenkennung, telegramm->herstellerkennung, telegramm->hardware_des_futelg, telegramm->software_des_futelg);
		else
			PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Received Roaming 'Umbuchen' message from Subscriber '%s' with %s card's security code %d\n", rufnummer, (telegramm->chipkarten_futelg_bit) ? "chip":"magnet", telegramm->sicherungs_code);
		if (telegramm->erweitertes_frequenzbandbit)
			PDEBUG(DCNETZ, DEBUG_INFO, " -> Phone supports extended frequency band\n");
		if (cnetz->state != CNETZ_IDLE) {
			PDEBUG(DCNETZ, DEBUG_NOTICE, "Ignoring Roaming from subscriber '%s', because we are busy becoming SpK.\n", rufnummer);
			break;
		}
		trans = create_transaction(cnetz, TRANS_UM, telegramm->futln_nationalitaet, telegramm->futln_heimat_fuvst_nr, telegramm->futln_rest_nr, telegramm->chipkarten_futelg_bit, telegramm->erweitertes_frequenzbandbit, cnetz->rf_level_db);
		if (!trans) {
			PDEBUG(DCNETZ, DEBUG_ERROR, "Failed to create transaction\n");
			break;
		}
		cnetz = trans->cnetz; /* cnetz may change, due to stronger reception on different OgK */
		valid_frame = 1;
		break;
	case OPCODE_UWG_R:
	case OPCODE_UWK_R:
		if (!match_fuz(telegramm))
			break;
		rufnummer = telegramm2rufnummer(telegramm);
		PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Received Roaming request 'Umbuchantrag' message from Subscriber '%s' on queue\n", rufnummer);
		break;
	case OPCODE_VWG_R:
	case OPCODE_SRG_R:
	case OPCODE_NUG_R:
		if (!match_fuz(telegramm))
			break;
		rufnummer = telegramm2rufnummer(telegramm);
		if (opcode == OPCODE_VWG_R)
			PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Received outgoing Call 'Verbindungswunsch gehend' message from Subscriber '%s'\n", rufnummer);
		else if (opcode == OPCODE_SRG_R)
			PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Received outgoing emergency Call 'Sonderruf gehend' message from Subscriber '%s'\n", rufnummer);
		else
			PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Received outgoing Call 'Verbindungswunsch gehend bei Nachbarschaftsunterstuetzung' message from Subscriber '%s'\n", rufnummer);
		if (cnetz->state != CNETZ_IDLE) {
			PDEBUG(DCNETZ, DEBUG_NOTICE, "Ignoring Call from subscriber '%s', because we are busy becoming SpK.\n", rufnummer);
			break;
		}
		trans = create_transaction(cnetz, TRANS_VWG, telegramm->futln_nationalitaet, telegramm->futln_heimat_fuvst_nr, telegramm->futln_rest_nr, -1, telegramm->erweitertes_frequenzbandbit, cnetz->rf_level_db);
		if (!trans) {
			PDEBUG(DCNETZ, DEBUG_ERROR, "Failed to create transaction\n");
			break;
		}
		cnetz = trans->cnetz; /* cnetz may change, due to stronger reception on different OgK */
		trans->try = 1;
		valid_frame = 1;
		break;
	case OPCODE_WUE_M:
		trans = search_transaction(cnetz, TRANS_WAF | TRANS_WBP | TRANS_VAG | TRANS_MO_QUEUE);
		if (!trans) {
			PDEBUG_CHAN(DCNETZ, DEBUG_NOTICE, "Received dialing digits 'Wahluebertragung' message without transaction (on this OgK), ignoring!\n");
			break;
		}
		rufnummer = transaction2rufnummer(trans);
		strncpy(trans->dialing, telegramm->wahlziffern, sizeof(trans->dialing) - 1);
		PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Received dialing digits 'Wahluebertragung' message from Subscriber '%s' to Number '%s'\n", rufnummer, trans->dialing);
		timer_stop(&trans->timer);
		trans_new_state(trans, TRANS_WBP);
		trans->try = 1; /* try */
		valid_frame = 1;
		break;
	case OPCODE_ATO_R:
		if (!match_fuz(telegramm))
			break;
		rufnummer = telegramm2rufnummer(telegramm);
		PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Received release 'Ausloesen des FuTelG im OgK-Betrieb bei WS' message from Subscriber '%s'\n", rufnummer);
		trans = search_transaction_number_global(telegramm->futln_nationalitaet, telegramm->futln_heimat_fuvst_nr, telegramm->futln_rest_nr);
		if (!trans) {
			PDEBUG_CHAN(DCNETZ, DEBUG_NOTICE, "There is no transaction, so we assume that the phone did not receive previous release.\n");
			/* create transaction, in case the phone repeats the release after we have acked it */
			trans = create_transaction(cnetz, TRANS_ATQ_IDLE, telegramm->futln_nationalitaet, telegramm->futln_heimat_fuvst_nr, telegramm->futln_rest_nr, -1, -1, cnetz->rf_level_db);
			if (!trans) {
				PDEBUG(DCNETZ, DEBUG_ERROR, "Failed to create transaction\n");
				break;
			}
			cnetz = trans->cnetz; /* cnetz may change, due to stronger reception on different OgK */
		} else {
			if (cnetz == trans->cnetz) {
				timer_stop(&trans->timer);
				trans_new_state(trans, TRANS_ATQ);
			} else
			if (trans->state == TRANS_ATQ_IDLE) {
				trans = create_transaction(cnetz, TRANS_ATQ_IDLE, telegramm->futln_nationalitaet, telegramm->futln_heimat_fuvst_nr, telegramm->futln_rest_nr, -1, -1, cnetz->rf_level_db);
				if (!trans) {
					PDEBUG(DCNETZ, DEBUG_ERROR, "Failed to create transaction\n");
					break;
				}
				cnetz = trans->cnetz; /* cnetz may change, due to stronger reception on different OgK */
			} else
				PDEBUG_CHAN(DCNETZ, DEBUG_NOTICE, "Received release 'Ausloesen des FuTelG im OgK-Betrieb bei WS' message without transaction (on this OgK), ignoring!\n");
		}
		valid_frame = 1;
		break;
	case OPCODE_MFT_M:
		if (!match_fuz(telegramm))
			break;
		trans = search_transaction_number(cnetz, telegramm->futln_nationalitaet, telegramm->futln_heimat_fuvst_nr, telegramm->futln_rest_nr);
		if (!trans) {
			PDEBUG_CHAN(DCNETZ, DEBUG_NOTICE, "Received acknowledge 'Meldung Funktelefonteilnehmer' message without transaction (on this OgK), ignoring!\n");
			break;
		}
		rufnummer = transaction2rufnummer(trans);
		PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Received acknowledge 'Meldung Funktelefonteilnehmer' message from Subscriber '%s'\n", rufnummer);
		destroy_transaction(trans);
		valid_frame = 1;
		break;
	default:
		PDEBUG_CHAN(DCNETZ, DEBUG_NOTICE, "Received unexpected Telegramm (opcode %d = %s)\n", opcode, telegramm_name(opcode));
	}

	if (cnetz->sender.loopback) {
		fprintf(stderr, "we don't know TS here, but we are in loopback mode. in loopback mode call to this function shall never happen. please fix or find a way to know when the time slot was received!\n");
		abort();
	}

	if (valid_frame)
		cnetz_sync_frame(cnetz, telegramm->sync_time, block);
}

/*
 * SpK handling
 */

/* transmit concentrated messages */
const telegramm_t *cnetz_transmit_telegramm_spk_k(cnetz_t *cnetz)
{
	static telegramm_t telegramm;
	transaction_t *trans = cnetz->trans_list;
	int ogk_kanal;
	cnetz_t *ogk;
	int rc;

	if (!trans)
		return NULL;

	memset(&telegramm, 0, sizeof(telegramm));

	telegramm.max_sendeleistung = cnetz_power2bits(cnetz->ms_power);
	telegramm.sendeleistungsanpassung = (cnetz->ms_power < 8) ? 1 : 0;
	telegramm.entfernung = si.entfernung;
	telegramm.fuz_nationalitaet = si.fuz_nat;
	telegramm.fuz_fuvst_nr = si.fuz_fuvst;
	telegramm.fuz_rest_nr = si.fuz_rest;
	telegramm.futln_nationalitaet = trans->futln_nat;
	telegramm.futln_heimat_fuvst_nr = trans->futln_fuvst;
	telegramm.futln_rest_nr = trans->futln_rest;
	telegramm.frequenz_nr = cnetz->kanal;
	telegramm.bedingte_genauigkeit_der_fufst = si.genauigkeit;
	telegramm.zufallszahl = cnetz->challenge;
	telegramm.bahn_bs = si.bahn_bs;

	switch (trans->state) {
	case TRANS_BQ:
		PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Sending 'Belegungsquittung' on traffic channel\n");
		telegramm.opcode = OPCODE_BQ_K;
		if (++trans->repeat >= 8 && !timer_running(&trans->timer)) {
			if (cnetz->challenge_valid) {
				if (si.authentifikationsbit == 0) {
					PDEBUG_CHAN(DCNETZ, DEBUG_NOTICE, "Cannot authenticate, because base station does not support it. (Authentication disabled in sysinfo.)\n");
					goto no_auth;
				}
				if (trans->futelg_bit == 0) {
					PDEBUG_CHAN(DCNETZ, DEBUG_NOTICE, "Cannot authenticate, because mobile station does not support it. (Mobile station has magnetic card.)\n");
					goto no_auth;
				}
				PDEBUG_CHAN(DCNETZ, DEBUG_NOTICE, "Perform authentication with subscriber's card, use challenge: 0x%016" PRIx64 "\n", telegramm.zufallszahl);
				trans_new_state(trans, TRANS_ZFZ);
				timer_start(&trans->timer, 0.0375 * F_ZFZ); /* F_ZFZ frames */
			} else {
no_auth:
				trans_new_state(trans, TRANS_VHQ_K);
				timer_start(&trans->timer, 0.0375 * F_VHQK); /* F_VHQK frames */
			}
			trans->repeat = 0;
		}
		break;
	case TRANS_ZFZ:
		PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Sending 'Zufallszahl' on traffic channel (0x%016" PRIx64 ").\n", telegramm.zufallszahl);
		telegramm.opcode = OPCODE_ZFZ_K;
		break;
	case TRANS_AP:
		PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Sending 'Quittung Verbindung halten' on traffic channel\n");
		telegramm.opcode = OPCODE_VHQ_K;
		break;
	case TRANS_VHQ_K:
		if (!cnetz->sender.loopback)
			PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Sending 'Quittung Verbindung halten' on traffic channel\n");
		telegramm.opcode = OPCODE_VHQ_K;
		/* continue until next sub frame, so we send DS from first block of next sub frame. */
		if (!cnetz->sender.loopback && (cnetz->sched_ts & 7) == 7 && cnetz->sched_r_m && !timer_running(&trans->timer)) {
			/* next sub frame */
			if (trans->mo_call) {
				trans->callref = call_up_setup(transaction2rufnummer(trans), trans->dialing, OSMO_CC_NETWORK_CNETZ_NONE, "");
				trans_new_state(trans, TRANS_DS);
				trans->repeat = 0;
				timer_start(&trans->timer, 0.0375 * F_DS); /* F_DS frames */
			}
			if (trans->mt_call) {
				trans_new_state(trans, TRANS_RTA);
				timer_start(&trans->timer, 0.0375 * F_RTA); /* F_RTA frames */
				trans->repeat = 0;
				call_up_alerting(trans->callref);
			}
		}
		break;
	case TRANS_DS:
		PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Sending 'Durchschalten' on traffic channel\n");
		telegramm.opcode = OPCODE_DSB_K;
		/* send exactly a sub frame (8 time slots) */
		if ((cnetz->sched_ts & 7) == 7 && cnetz->sched_r_m && !timer_running(&trans->timer)) {
			/* next sub frame */
			trans_new_state(trans, TRANS_VHQ_V);
			trans->repeat = 0;
			cnetz_set_sched_dsp_mode(cnetz, DSP_MODE_SPK_V, (cnetz->sched_ts + 1) & 31);
#ifndef DEBUG_SPK
			timer_start(&trans->timer, 0.075 + 0.6 * F_VHQ); /* one slot + F_VHQ frames */
#endif
		}
		break;
	case TRANS_RTA:
		PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Sending 'Rufton anschalten' on traffic channel\n");
		telegramm.opcode = OPCODE_RTA_K;
		break;
	case TRANS_AHQ:
		PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Sending 'Abhebe Quittung' on traffic channel\n");
		telegramm.opcode = OPCODE_AHQ_K;
		if ((cnetz->sched_ts & 7) == 7 && cnetz->sched_r_m) {
			/* next sub frame */
			trans_new_state(trans, TRANS_VHQ_V);
			trans->repeat = 0;
			cnetz_set_sched_dsp_mode(cnetz, DSP_MODE_SPK_V, (cnetz->sched_ts + 1) & 31);
			timer_start(&trans->timer, 0.075 + 0.6 * F_VHQ); /* one slot + F_VHQ frames */
		}
		break;
	case TRANS_AF:
		PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Sending 'Ausloesen durch FuFSt' on traffic channel\n");
		telegramm.opcode = OPCODE_AF_K;
		if (++trans->repeat < N_AFKT)
			break;
		if (!trans->try) {
			/* no retry */
			destroy_transaction(trans);
			cnetz_go_idle(cnetz);
			break;
		}
		if (trans->try == N) {
			PDEBUG(DCNETZ, DEBUG_INFO, "Maximum retries, removing transaction\n");
			/* no response to incomming call */
			trans->page_failed = 1;
			cnetz_release(trans, CNETZ_CAUSE_FUNKTECHNISCH);
			if (trans->callref)
				call_up_release(trans->callref, CAUSE_TEMPFAIL);
			/* must destroy transaction after cnetz_release */
			destroy_transaction(trans);
			cnetz_go_idle(cnetz);
			break;
		}
		/* remove call from SpK (or OgK+SpK) */
		unlink_transaction(trans);
		/* idle channel */
		cnetz_go_idle(cnetz);
		/* alloc ogk again */
		rc = find_db(trans->futln_nat, trans->futln_fuvst, trans->futln_rest, &ogk_kanal, NULL, NULL);
		if (rc < 0) {
			PDEBUG(DCNETZ, DEBUG_NOTICE, "Cannot find subscriber in database anymore, releasing!\n");
			goto no_ogk;
		}
		ogk = search_ogk(ogk_kanal);
		if (!ogk) {
			PDEBUG(DCNETZ, DEBUG_NOTICE, "Cannot retry, because currently no OgK available (busy)\n");
no_ogk:
			cnetz_release(trans, CNETZ_CAUSE_FUNKTECHNISCH);
			if (trans->callref)
				call_up_release(trans->callref, CAUSE_NOCHANNEL);
			/* must destroy transaction after cnetz_release */
			destroy_transaction(trans);
			break;
		}
		PDEBUG(DCNETZ, DEBUG_INFO, "Retry to assign channel.\n");
		/* attach call to OgK */
		link_transaction(trans, ogk);
		/* change state */
		if (trans->mo_call)
			trans_new_state(trans, TRANS_WBP);
		if (trans->mt_call)
			trans_new_state(trans, TRANS_VAK);
		trans->try++;
		break;
	case TRANS_AT:
		PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Sending 'Auslosen durch FuFst' on traffic channel\n");
		telegramm.opcode = OPCODE_AF_K;
		if (++trans->repeat == 1) {
			destroy_transaction(trans);
			cnetz_go_idle(cnetz);
		}
		break;
	}

	return &telegramm;
}

/* receive concentrated messages */
void cnetz_receive_telegramm_spk_k(cnetz_t *cnetz, telegramm_t *telegramm)
{
	uint8_t opcode = telegramm->opcode;
	int valid_frame = 0;
	transaction_t *trans = cnetz->trans_list;

	if (!trans)
		return;

	switch (opcode) {
	case OPCODE_BEL_K:
		if (!match_fuz(telegramm)) {
			break;
		}
		if (!match_futln(telegramm, trans->futln_nat, trans->futln_fuvst, trans->futln_rest)) {
			break;
		}
		PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Received allocation 'Belegung' message.\n");
		valid_frame = 1;
		if (trans->state != TRANS_BQ)
			break;
		timer_stop(&trans->timer);
		trans->try = 0;
		break;
	case OPCODE_DSQ_K:
		if (!match_fuz(telegramm)) {
			break;
		}
		if (!match_futln(telegramm, trans->futln_nat, trans->futln_fuvst, trans->futln_rest)) {
			break;
		}
		PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Received assignment confirm 'Durchschaltung Quittung' message.\n");
		valid_frame = 1;
		if (trans->state != TRANS_DS)
			break;
		cnetz->scrambler = telegramm->betriebs_art;
		cnetz->scrambler_switch = 0;
		timer_stop(&trans->timer);
		break;
	case OPCODE_ZFZQ_K:
		PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Received random number acknowledge 'Zufallszahlquittung' message.\n");
		valid_frame = 1;
		if (trans->state != TRANS_ZFZ)
			break;
		if (cnetz->challenge != telegramm->zufallszahl) {
			PDEBUG_CHAN(DCNETZ, DEBUG_NOTICE, "Received random number acknowledge (0x%016" PRIx64 ") does not match the transmitted one (0x%016" PRIx64 "), ignoring!\n", telegramm->zufallszahl, cnetz->challenge);
			break;
		}
		timer_stop(&trans->timer);
		trans_new_state(trans, TRANS_AP);
		timer_start(&trans->timer, T_AP); /* 750 milliseconds */
		break;
	case OPCODE_AP_K:
		PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Received challenge response 'Autorisierungsparameter' message (0x%016" PRIx64 ").\n", telegramm->authorisierungsparameter);
		valid_frame = 1;
		if (trans->state != TRANS_AP)
			break;
		/* if authentication response from card does not match */
		if (cnetz->response_valid && telegramm->authorisierungsparameter != cnetz->response) {
			PDEBUG_CHAN(DCNETZ, DEBUG_NOTICE, "Received challenge response (0x%016" PRIx64 ") does not match the expected one (0x%016" PRIx64 "), releasing!\n", telegramm->authorisierungsparameter, cnetz->response);
			if (trans->callref) {
				call_up_release(trans->callref, CAUSE_TEMPFAIL); /* jolly guesses that */
				trans->callref = 0;
			}
			cnetz_release(trans, CNETZ_CAUSE_GASSENBESETZT); /* when authentication is not valid */
			break;
		}
		PDEBUG_CHAN(DCNETZ, DEBUG_NOTICE, "Completed authentication with subscriber's card, challenge response: 0x%016" PRIx64 "\n", telegramm->authorisierungsparameter);
		timer_stop(&trans->timer);
		trans_new_state(trans, TRANS_VHQ_K);
		timer_start(&trans->timer, 0.0375 * F_VHQK); /* F_VHQK frames */
		break;
	case OPCODE_VH_K:
		if (!match_fuz(telegramm)) {
			break;
		}
		if (!match_futln(telegramm, trans->futln_nat, trans->futln_fuvst, trans->futln_rest)) {
			break;
		}
		PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Received connection hold 'Verbindung halten' message.\n");
		valid_frame = 1;
		if (trans->state != TRANS_VHQ_K)
			break;
		timer_stop(&trans->timer);
		break;
	case OPCODE_RTAQ_K:
		if (!match_fuz(telegramm)) {
			break;
		}
		if (!match_futln(telegramm, trans->futln_nat, trans->futln_fuvst, trans->futln_rest)) {
			break;
		}
		valid_frame = 1;
		PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Received ringback 'Rufton anschalten Quittung' message.\n");
		if (trans->state != TRANS_RTA)
			break;
		timer_start(&trans->timer, 0.0375 * F_RTA); /* F_RTA frames */
		break;
	case OPCODE_AH_K:
		if (!match_fuz(telegramm)) {
			break;
		}
		if (!match_futln(telegramm, trans->futln_nat, trans->futln_fuvst, trans->futln_rest)) {
			break;
		}
		PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Received answer frame 'Abheben' message.\n");
		valid_frame = 1;
		/* if already received this frame, or if we are already on VHQ or if we are releasing */
		if (trans->state == TRANS_AHQ || trans->state == TRANS_VHQ_K || trans->state == TRANS_VHQ_V || trans->state == TRANS_AF)
			break;
		cnetz->scrambler = telegramm->betriebs_art;
		cnetz->scrambler_switch = 0;
		trans_new_state(trans, TRANS_AHQ);
		trans->repeat = 0;
		timer_stop(&trans->timer);
		call_up_answer(trans->callref, transaction2rufnummer(trans));
		break;
	case OPCODE_AT_K:
		if (!match_fuz(telegramm)) {
			break;
		}
		if (!match_futln(telegramm, trans->futln_nat, trans->futln_fuvst, trans->futln_rest)) {
			break;
		}
		PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Received release frame 'Ausloesen durch FuTln' message.\n");
		valid_frame = 1;
		/* if already received this frame, if we are releasing */
		if (trans->state == TRANS_AT || trans->state == TRANS_AF)
			break;
		trans_new_state(trans, TRANS_AT);
		trans->repeat = 0;
		timer_stop(&trans->timer);
		if (trans->callref) {
			call_up_release(trans->callref, CAUSE_NORMAL);
			trans->callref = 0;
		}
		break;
	default:
		PDEBUG_CHAN(DCNETZ, DEBUG_NOTICE, "Received unexpected Telegramm (opcode %d = %s)\n", opcode, telegramm_name(opcode));
	}

	if (valid_frame)
		cnetz_sync_frame(cnetz, telegramm->sync_time, -1);
}

/* transmit distributed messages */
const telegramm_t *cnetz_transmit_telegramm_spk_v(cnetz_t *cnetz)
{
	static telegramm_t telegramm;
	transaction_t *trans = cnetz->trans_list;
	int meter = 0;

	if (!trans)
		return NULL;

	memset(&telegramm, 0, sizeof(telegramm));

	if (cnetz->metering) {
		double now = get_time();
		if (!trans->call_start)
			trans->call_start = now;
		meter = (now - trans->call_start) / (double)cnetz->metering + 1;
	}

	telegramm.max_sendeleistung = cnetz_power2bits(cnetz->ms_power);
	telegramm.sendeleistungsanpassung = (cnetz->ms_power < 8) ? 1 : 0;
	telegramm.ankuendigung_gespraechsende = 0;
	telegramm.gebuehren_stand = meter;
	telegramm.fuz_nationalitaet = si.fuz_nat;
	telegramm.fuz_fuvst_nr = si.fuz_fuvst;
	telegramm.fuz_rest_nr = si.fuz_rest;
	telegramm.futln_nationalitaet = trans->futln_nat;
	telegramm.futln_heimat_fuvst_nr = trans->futln_fuvst;
	telegramm.futln_rest_nr = trans->futln_rest;
	telegramm.frequenz_nr = cnetz->kanal;
	telegramm.entfernung = si.entfernung;
	telegramm.bedingte_genauigkeit_der_fufst = si.genauigkeit;
	telegramm.gueltigkeit_des_gebuehrenstandes = 0;
	telegramm.ausloesegrund = trans->release_cause;

	switch (trans->state) {
	case TRANS_VHQ_V:
		if ((cnetz->sched_ts & 8) == 0) { /* sub frame 1 and 3 */
			PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Sending 'Quittung Verbindung halten 1' on traffic channel\n");
			telegramm.opcode = OPCODE_VHQ1_V;
		} else { /* sub frame 2 and 4 */
			PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Sending 'Quittung Verbindung halten 2' on traffic channel\n");
			telegramm.opcode = OPCODE_VHQ2_V;
		}
		break;
	case TRANS_AF:
		PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Sending 'Ausloesen durch FuFSt' on traffic channel\n");
		telegramm.opcode = OPCODE_AF_V;
		if (++trans->repeat == N_AFV) {
			destroy_transaction(trans);
			cnetz_go_idle(cnetz);
		}
		break;
	case TRANS_AT:
		PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Sending acknowledge to 'Ausloesen durch FuTln' on traffic channel\n");
		telegramm.opcode = OPCODE_AF_V;
		if (++trans->repeat == 1) {
			destroy_transaction(trans);
			cnetz_go_idle(cnetz);
		}
		break;
	}

	return &telegramm;
}

/* receive distributed messages */
void cnetz_receive_telegramm_spk_v(cnetz_t *cnetz, telegramm_t *telegramm)
{
	uint8_t opcode = telegramm->opcode;
	int valid_frame = 0;
	transaction_t *trans = cnetz->trans_list;

	if (!trans)
		return;

	switch (opcode) {
	case OPCODE_VH_V:
	case OPCODE_USAI_V:
	case OPCODE_USAE_V:
		if (!match_fuz(telegramm)) {
			break;
		}
		if (!match_futln(telegramm, trans->futln_nat, trans->futln_fuvst, trans->futln_rest)) {
			break;
		}
		if (trans->state != TRANS_VHQ_V)
			break;
		timer_start(&trans->timer, 0.6 * F_VHQ); /* F_VHQ frames */
		switch (opcode) {
		case OPCODE_VH_V:
			PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Received supervisory frame 'Verbindung halten' message%s.\n", (telegramm->test_telefonteilnehmer_geraet) ? ", phone is a test-phone" : "");
			break;
		case OPCODE_USAI_V:
			PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Received internal handover request frame 'Umschaltantrag intern' message%s.\n", (telegramm->test_telefonteilnehmer_geraet) ? ", phone is a test-phone" : "");
			break;
		case OPCODE_USAE_V:
			PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Received external handover request frame 'Umschaltantrag extern' message%s.\n", (telegramm->test_telefonteilnehmer_geraet) ? ", phone is a test-phone" : "");
			break;
		}
		valid_frame = 1;
		if (cnetz->scrambler != telegramm->betriebs_art) {
			/* if the scrambler mode changes, we wait 3 frames */
			/* i guess that this was implemented to prevent switching by one corrupt frame. */
			if (++cnetz->scrambler_switch >= 3) {
				cnetz->scrambler = telegramm->betriebs_art;
				cnetz->scrambler_switch = 0;
			}
		} else
			cnetz->scrambler_switch = 0;
		break;
	case OPCODE_AT_V:
		if (!match_fuz(telegramm)) {
			break;
		}
		if (!match_futln(telegramm, trans->futln_nat, trans->futln_fuvst, trans->futln_rest)) {
			break;
		}
		PDEBUG_CHAN(DCNETZ, DEBUG_INFO, "Received release frame 'Ausloesen durch FuTln' message.\n");
		valid_frame = 1;
		/* if already received this frame, if we are releasing */
		if (trans->state == TRANS_AT || trans->state == TRANS_AF)
			break;
		trans_new_state(trans, TRANS_AT);
		trans->repeat = 0;
		timer_stop(&trans->timer);
		if (trans->callref) {
			call_up_release(trans->callref, CAUSE_NORMAL);
			trans->callref = 0;
		}
		break;
	default:
		PDEBUG_CHAN(DCNETZ, DEBUG_NOTICE, "Received unexpected Telegramm (opcode %d = %s)\n", opcode, telegramm_name(opcode));
	}

	if (valid_frame)
		cnetz_sync_frame(cnetz, telegramm->sync_time, -1);
}

void dump_info(void)
{
	dump_db();
}

