/* MTS/IMTS protocol handling
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
 *
 * 
 * How it works:
 *
 * (There should be a more detailed description of the call process!)
 *
 * There are call states defined by imts->state.
 * imts_receive_tone() is called whenever a tone/silence/noise is detected.
 * imts_lost_tone() is calls as soon as (only) a tone is gone.
 * imts_timeout() is called when the timer has timed out.
 * All these callbacks are used to process the call setup and disconnect.
 * The imts_timeout() function will not only handle failures due to timeouts,
 * but also E.g. end of pulsed digit or seize detection.
 *
 */

#define CHAN imts->sender.kanal

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../libsample/sample.h"
#include "../liblogging/logging.h"
#include <osmocom/core/timer.h>
#include "../libmobile/call.h"
#include "../libmobile/cause.h"
#include "../libmobile/get_time.h"
#include <osmocom/cc/message.h>
#include "imts.h"
#include "dsp.h"

#define CUT_OFF_EMPHASIS_IMTS	796.0 /* FIXME: really 200 uS time constant? */

/* band info */
#define VHF_LOW 0
#define VHF_HIGH 1
#define UHF 2
static const char *band_name[] = {
	"VHF-Low Band",
	"VHF-High Band",
	"UHF Band",
};

/* I measured VHF_HIGH deviation with 5000, others told me that all bands are 5000. */
#define DEV_VHF_LOW	5000.0
#define DEV_VHF_HIGH	5000.0
#define DEV_UHF		5000.0

static struct channel_info {
	int		band;		/* which band it belongs to */
	const char	*name;		/* name of channel */
	const char	*channel;	/* number of channel */
	double		downlink_mhz;	/* base station frequency */
	double		uplink_mhz;	/* mobile station frequency */
	int		canada_only;	/* channel used in canada only */
	int		channels_24;	/* only available for 24 channel mobiles */
} channel_info[] = {
	{ VHF_LOW,	"ZO",	NULL,	35.26,		43.26,		0,	0 },
	{ VHF_LOW,	"ZF",	NULL,	35.30,		43.30,		0,	0 },
	{ VHF_LOW,	"ZM",	NULL,	35.38,		43.38,		0,	0 },
	{ VHF_LOW,	"ZH",	NULL,	35.34,		43.34,		0,	0 },
	{ VHF_LOW,	"ZA",	NULL,	35.42,		43.32,		0,	0 },
	{ VHF_LOW,	"ZY",	NULL,	35.46,		43.46,		0,	0 },
	{ VHF_LOW,	"ZR",	NULL,	35.50,		43.50,		0,	0 },
	{ VHF_LOW,	"ZB",	NULL,	35.54,		43.54,		0,	0 },
	{ VHF_LOW,	"ZW",	NULL,	35.62,		43.62,		0,	0 },
	{ VHF_LOW,	"ZL",	NULL,	35.66,		43.66,		0,	0 },
	{ VHF_HIGH,	"JJ",	"1",	152.48,		157.74,		1,	0 },
	{ VHF_HIGH,	"JL",	"3",	152.51,		157.77,		0,	0 },
	{ VHF_HIGH,	"YL",	"5",	152.54,		157.80,		0,	0 },
	{ VHF_HIGH,	"JP",	"7",	152.57,		157.83,		0,	0 },
	{ VHF_HIGH,	"YP",	"9",	152.60,		157.86,		0,	0 },
	{ VHF_HIGH,	"YJ",	"11",	152.63,		157.89,		0,	0 },
	{ VHF_HIGH,	"YK",	"13",	152.66,		157.92,		0,	0 },
	{ VHF_HIGH,	"JS",	"15",	152.69,		157.95,		0,	0 },
	{ VHF_HIGH,	"YS",	"17",	152.72,		157.98,		0,	0 },
	{ VHF_HIGH,	"YR",	"19",	152.75,		158.01,		0,	0 },
	{ VHF_HIGH,	"JK",	"21",	152.78,		158.04,		0,	0 },
	{ VHF_HIGH,	"JR",	"23",	152.81,		158.07,		0,	0 },
	{ VHF_HIGH,	"JW",	"25",	152.84,		158.10,		1,	0 },
	{ VHF_HIGH,	"XJ",	"2",	152.495,	157.755,	0,	1 },
	{ VHF_HIGH,	"XK",	"4",	152.525,	157.785,	0,	1 },
	{ VHF_HIGH,	"XL",	"6",	152.555,	157.815,	0,	1 },
	{ VHF_HIGH,	"XP",	"8",	152.585,	157.845,	0,	1 },
	{ VHF_HIGH,	"XR",	"10",	152.615,	157.875,	0,	1 },
	{ VHF_HIGH,	"XS",	"12",	152.645,	157.905,	0,	1 },
	{ VHF_HIGH,	"XT",	"14",	152.675,	157.935,	0,	1 },
	{ VHF_HIGH,	"XU",	"16",	152.705,	157.965,	0,	1 },
	{ VHF_HIGH,	"XV",	"18",	152.735,	157.995,	0,	1 },
	{ VHF_HIGH,	"XW",	"20",	152.765,	158.025,	0,	1 },
	{ VHF_HIGH,	"XX",	"22",	152.795,	158.055,	0,	1 },
	{ VHF_HIGH,	"XY",	"24",	152.825,	158.085,	0,	1 },
	{ UHF,		"QC",	"31",	454.375,	459.375,	0,	0 },
	{ UHF,		"QJ",	"32",	454.40,		459.40,		0,	0 },
	{ UHF,		"QD",	"33",	454.425,	459.425,	0,	0 },
	{ UHF,		"QA",	"34",	454.45,		459.45,		0,	0 },
	{ UHF,		"QE",	"35",	454.475,	459.475,	0,	0 },
	{ UHF,		"QP",	"36",	454.50,		459.50,		0,	0 },
	{ UHF,		"QK",	"37",	454.525,	459.525,	0,	0 },
	{ UHF,		"QB",	"38",	454.55,		459.55,		0,	0 },
	{ UHF,		"QO",	"39",	454.575,	459.575,	0,	0 },
	{ UHF,		"QR",	"40",	454.60,		459.60,		0,	0 },
	{ UHF,		"QY",	"41",	454.625,	459.625,	0,	0 },
	{ UHF,		"QF",	"42",	454.65,		459.65,		0,	0 },
	{ 0, NULL, NULL, 0.0, 0.0, 0, 0}
};

void imts_list_channels(void)
{
	int last_band = -1;
	int i;

	for (i = 0; channel_info[i].name; i++) {
		if (last_band != channel_info[i].band) {
			last_band = channel_info[i].band;
			printf("\n%s:\n\n", band_name[channel_info[i].band]);
			printf("Channel\t\tDownlink\tUplink\t\tComment\n");
			printf("----------------------------------------------------------------\n");
		}
		printf("%s", channel_info[i].name);
		if (channel_info[i].channel)
			printf(" (%s)", channel_info[i].channel);
		printf("\t\t%.3f MHz\t%.3f MHz\t%s%s\n", channel_info[i].downlink_mhz, channel_info[i].uplink_mhz, (channel_info[i].canada_only) ? "Canada only" : "", (channel_info[i].channels_24) ? "24-Cannel only" : "");
	}
	printf("\n");
}

/* Timers */
#define PAGING_TO	4,0		/* Time to wait for the phone to respond */
#define RINGING_TO	45,0		/* Time to wait for the mobile user to answer */
#define SEIZE_TO	1,0		/* Time to wait for the phone to seize (Connect tone) */
#define ANI_TO		1,0		/* Time to wait for first / next digit */
#define DIALTONE_TO	10,0		/* Time to wait until dialing must be performed */
#define DIALING_TO	3,0		/* Time to wait until number is recognized as complete */
#define RELEASE_TO	0,350000	/* Time to turn off transmitter before going idle ".. for about 300 ms .." */
#define ANI_PULSE_TO	0,100000	/* Time to detect end of digit */
#define DIAL_PULSE_TO	0,200000	/* Time to detect end of digit */
#define DISC_PULSE_TO	0.1		/* Time until aborting disconnect detection */
#define PAGE_PULSE_TO	0.2		/* Time to detect end of digit */

/* Counters */
#define DISC_COUNT	2		/* Number of pulses to detect disconnect (100 ms) */
#define RING_PULSES	40		/* 2 seconds ringer on */

/* Durations */
#define IDLE_DETECT	0,500000	/* Time to detect Idle signal (loopback) */
#define PAGE_SEIZE	0.400		/* Time to seize channel until start paging pulses FIXME */
#define PAGE_PAUSE	0.225		/* Time to pause after each digit */
#define PAGE_MARK	0.050		/* Mark duration of page pulse */
#define PAGE_SPACE	0.050		/* Space duration of page pulse */
#define PAGE_PULSE	0.100		/* Duration of a complete pulse (MTS) */
#define RING_MARK	0.025		/* Mark duration of ring pulse */
#define RING_SPACE	0.025		/* Space duration of ring pulse */
#define RING_OFF	4.0		/* 4 seconds ringer off */
#define GUARD_TIME	0.200		/* Time until detecting Guard tone from mobile */
#define SEIZE_TIME	0,300000	/* Time until sending Seize tone >= 250 */
#define SEIZE_LENGTH	0.250		/* Length of Seize */
#define RECEIVE_TIME	0.200		/* Time until detecting receive signal (Guard tone) from mobile */
#define ANSWER_TIME	0.200		/* Time until detecting answer signal (Connect tone) from mobile */

const char *imts_state_name(enum imts_state state)
{
	static char invalid[16];

	switch (state) {
	case IMTS_NULL:
		return "(NULL)";
	case IMTS_OFF:
		return "IDLE (off)";
	case IMTS_IDLE:
		return "IDLE (tone)";
	case IMTS_SEIZE:
		return "SEIZE (mobile call)";
	case IMTS_ANI:
		return "ANI (mobile call)";
	case IMTS_DIALING:
		return "DIALING (mobile call)";
	case IMTS_PAGING:
		return "PAGING (station call)";
	case IMTS_RINGING:
		return "RINGING (station call)";
	case IMTS_CONVERSATION:
		return "CONVERSATION";
	case IMTS_RELEASE:
		return "RELEASE";
	case IMTS_PAGING_TEST:
		return "PAGING TEST";
	case IMTS_DETECTOR_TEST:
		return "DETECTOR TEST";
	}

	sprintf(invalid, "invalid(%d)", state);
	return invalid;
}

static void imts_display_status(void)
{
	sender_t *sender;
	imts_t *imts;

	display_status_start();
	for (sender = sender_head; sender; sender = sender->next) {
		imts = (imts_t *) sender;
		display_status_channel(imts->sender.kanal, NULL, imts_state_name(imts->state));
		if (imts->station_id[0])
			display_status_subscriber(imts->station_id, NULL);
	}
	display_status_end();
}

static void imts_new_state(imts_t *imts, enum imts_state new_state)
{
	if (imts->state == new_state)
		return;
	LOGP_CHAN(DIMTS, LOGL_DEBUG, "State change: %s -> %s\n", imts_state_name(imts->state), imts_state_name(new_state));
	imts->state = new_state;
	imts_display_status();
}

/* Convert channel name to frequency number of base station.
   Set 'uplink' to 1 to get frequency of mobile station. */
double imts_channel2freq(const char *kanal, int uplink, int *band, int *canada_only, int *channels_24)
{
	int i;

	for (i = 0; channel_info[i].name; i++) {
		if (!strcasecmp(channel_info[i].name, kanal)
		 || (channel_info[i].channel && !strcmp(channel_info[i].channel, kanal))) {
			if (band)
				*band = channel_info[i].band;
			if (canada_only)
				*canada_only = channel_info[i].canada_only;
			if (channels_24)
				*channels_24 = channel_info[i].channels_24;
			if (uplink == 2)
				return (channel_info[i].downlink_mhz - channel_info[i].uplink_mhz) * 1e6;
			else if (uplink)
				return channel_info[i].uplink_mhz * 1e6;
			else
				return channel_info[i].downlink_mhz * 1e6;
		}
	}

	return 0.0;
}

/* check if number is a valid station ID */
const char *mts_number_valid(const char *number)
{
	int i;

	/* assume that the number has valid length(s) and digits */

	for (i = 0; number[i]; i++) {
		if (number[i] == '1')
			return "Digits value '1' is not allowed within MTS number.";
	}

	return NULL;
}

/* global init */
int imts_init(void)
{
	return 0;
}

static void imts_timeout(void *data);
static void imts_go_idle(imts_t *imts);
static void imts_paging(imts_t *imts, const char *dial_string, int loopback);
static void imts_detector_test(imts_t *imts, double length_1, double length_2, double length_3);

/* Create transceiver instance and link to a list. */
int imts_create(const char *kanal, const char *device, int use_sdr, int samplerate, double rx_gain, double tx_gain, int pre_emphasis, int de_emphasis, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, const char *read_tx_wave, int loopback, double squelch_db, int ptt, double fast_seize, enum mode mode, const char *operator, double length_1, double length_2, double length_3)
{
	imts_t *imts;
	int band, canada_only, channels_24;
	int rc;

	if (imts_channel2freq(kanal, 0, &band, &canada_only, &channels_24) == 0.0) {
		LOGP(DIMTS, LOGL_ERROR, "Channel number %s invalid.\n", kanal);
		return -EINVAL;
	}
	if (canada_only) {
		LOGP(DIMTS, LOGL_NOTICE, "*******************************************************************************\n");
		LOGP(DIMTS, LOGL_NOTICE, "Given channel '%s' was only available in Canada with Canadian phones.\n", kanal);
		LOGP(DIMTS, LOGL_NOTICE, "*******************************************************************************\n");
	}
	if (channels_24) {
		LOGP(DIMTS, LOGL_NOTICE, "*******************************************************************************\n");
		LOGP(DIMTS, LOGL_NOTICE, "Given channel '%s' only works with phones that support 24 channels.\n", kanal);
		LOGP(DIMTS, LOGL_NOTICE, "*******************************************************************************\n");
	}
	if (mode == MODE_IMTS && band == VHF_LOW) {
		LOGP(DIMTS, LOGL_NOTICE, "*******************************************************************************\n");
		LOGP(DIMTS, LOGL_NOTICE, "Given channel '%s' was only available at MTS network.\n", kanal);
		LOGP(DIMTS, LOGL_NOTICE, "*******************************************************************************\n");
		return -EINVAL;
	}
	if (mode == MODE_MTS && band == UHF) {
		LOGP(DIMTS, LOGL_NOTICE, "*******************************************************************************\n");
		LOGP(DIMTS, LOGL_NOTICE, "Given channel '%s' was only available at IMTS network.\n", kanal);
		LOGP(DIMTS, LOGL_NOTICE, "*******************************************************************************\n");
		return -EINVAL;
	}

	imts = calloc(1, sizeof(imts_t));
	if (!imts) {
		LOGP(DIMTS, LOGL_ERROR, "No memory!\n");
		return -EIO;
	}

	LOGP(DIMTS, LOGL_DEBUG, "Creating 'IMTS' instance for channel = %s (sample rate %d).\n", kanal, samplerate);

	imts->fast_seize = fast_seize;
	imts->mode = mode;
	imts->operator = operator;
	imts->ptt = ptt;

	/* init general part of transceiver */
	/* do not enable emphasis, since it is done by imts code, not by common sender code */
	rc = sender_create(&imts->sender, kanal, imts_channel2freq(kanal, 0, NULL, NULL, NULL), imts_channel2freq(kanal, 1, NULL, NULL, NULL), device, use_sdr, samplerate, rx_gain, tx_gain, 0, 0, write_rx_wave, write_tx_wave, read_rx_wave, read_tx_wave, loopback, PAGING_SIGNAL_NONE);
	if (rc < 0) {
		LOGP(DIMTS, LOGL_ERROR, "Failed to init 'Sender' processing!\n");
		goto error;
	}

	/* init audio processing */
	rc = dsp_init_transceiver(imts, squelch_db, ptt);
	if (rc < 0) {
		LOGP(DIMTS, LOGL_ERROR, "Failed to init signal processing!\n");
		goto error;
	}

	osmo_timer_setup(&imts->timer, imts_timeout, imts);

	imts->pre_emphasis = pre_emphasis;
	imts->de_emphasis = de_emphasis;
	rc = init_emphasis(&imts->estate, samplerate, CUT_OFF_EMPHASIS_IMTS, CUT_OFF_HIGHPASS_DEFAULT, CUT_OFF_LOWPASS_DEFAULT);
	if (rc < 0)
		goto error;

	if (length_1 > 0.0 || length_2 > 0.0 || length_3 > 0.0) {
		imts_detector_test(imts, length_1, length_2, length_3);
		/* go into detector test state */
	} else if (loopback) {
		/* go into loopback test state */
		imts_paging(imts, "1234567890", loopback);
	} else {
		/* go into idle state */
		imts_go_idle(imts);
	}

	LOGP(DIMTS, LOGL_NOTICE, "Created channel #%s\n", kanal);

	return 0;

error:
	imts_destroy(&imts->sender);

	return rc;
}

/* Destroy transceiver instance and unlink from list. */
void imts_destroy(sender_t *sender)
{
	imts_t *imts = (imts_t *) sender;

	LOGP(DIMTS, LOGL_DEBUG, "Destroying 'IMTS' instance for channel = %s.\n", sender->kanal);

	osmo_timer_del(&imts->timer);
	dsp_cleanup_transceiver(imts);
	sender_destroy(&imts->sender);
	free(sender);
}

/* Return to IDLE */
static void imts_go_idle(imts_t *imts)
{
	sender_t *sender;
	imts_t *idle;

	osmo_timer_del(&imts->timer);
	imts->station_id[0] = '\0'; /* remove station ID before state change, so status is shown correctly */

	for (sender = sender_head; sender; sender = sender->next) {
		idle = (imts_t *) sender;
		if (idle == imts)
			continue;
		if (idle->state == IMTS_IDLE)
			break;
	}
	if (sender) {
		LOGP(DIMTS, LOGL_INFO, "Entering IDLE state on channel %s, turning transmitter off.\n", imts->sender.kanal);
		imts_new_state(imts, IMTS_OFF);
		imts_set_dsp_mode(imts, DSP_MODE_OFF, 0, 0.0, 0);
	} else {
		if (imts->mode == MODE_IMTS) {
			LOGP(DIMTS, LOGL_INFO, "Entering IDLE state on channel %s, sending 2000 Hz tone.\n", imts->sender.kanal);
			imts_new_state(imts, IMTS_IDLE);
			/* also reset detector, so if there is a new call it is answered */
			imts_set_dsp_mode(imts, DSP_MODE_TONE, TONE_IDLE, 0.0, 1);
		} else {
			LOGP(DIMTS, LOGL_INFO, "Entering IDLE state on channel %s, sending 600 Hz tone.\n", imts->sender.kanal);
			imts_new_state(imts, IMTS_IDLE);
			/* also reset detector, so if there is a new call it is answered */
			imts_set_dsp_mode(imts, DSP_MODE_TONE, TONE_600, 0.0, 1);
		}
	}
}

/* If a channel is occupied, we need to hunt for an IDLE channel to be turned on. */
static void imts_activate_idle(void)
{
	sender_t *sender;
	imts_t *idle;

	for (sender = sender_head; sender; sender = sender->next) {
		idle = (imts_t *) sender;
		if (idle->state == IMTS_OFF)
			break;
	}
	if (sender)
		imts_go_idle(idle);
	else
		LOGP(DIMTS, LOGL_INFO, "All channels are busy now, cannot activate any other channel.\n");
}

/* Release connection towards mobile station by sending pause for a while. */
static void imts_release(imts_t *imts)
{
	osmo_timer_del(&imts->timer);
	/* remove station ID before state change, so status is shown correctly */
	imts->station_id[0] = '\0';

	if (imts->mode == MODE_MTS && imts->state == IMTS_RINGING) {
		/*
		 * In MTS mode we abort ringing by sending pulse.
		 * Afterwards we call imts_release again and turn transmitter off.
		 */
		int tone;
		if (imts->tone == TONE_1500)
			tone = TONE_600;
		else
			tone = TONE_1500;
		LOGP_CHAN(DIMTS, LOGL_INFO, "Sending pulse to stop ringing of the phone.\n");
		imts_new_state(imts, IMTS_RELEASE);
		imts_set_dsp_mode(imts, DSP_MODE_TONE, tone, PAGE_PAUSE, 0);
	} else {
		LOGP_CHAN(DIMTS, LOGL_INFO, "Turing transmitter off.\n");
		if (imts->state != IMTS_RELEASE)
			imts_new_state(imts, IMTS_RELEASE);
		imts_set_dsp_mode(imts, DSP_MODE_OFF, 0, 0.0, 0);
		osmo_timer_schedule(&imts->timer, RELEASE_TO);
	}
}

/* Enter detector test state */
static void imts_detector_test(imts_t *imts, double length_1, double length_2, double length_3)
{
	LOGP_CHAN(DIMTS, LOGL_INFO, "Entering detector test state, sending test sequence.\n");
	imts->detector_test_length_1 = length_1;
	imts->detector_test_length_2 = length_2;
	imts->detector_test_length_3 = length_3;
	imts_new_state(imts, IMTS_DETECTOR_TEST);
	imts_set_dsp_mode(imts, DSP_MODE_TONE, TONE_SILENCE, 1.0, 0);
	
}

/* Enter paging state */
static void imts_paging(imts_t *imts, const char *dial_string, int loopback)
{
	/* stop timer, since it may be running while measuring Guard tone at IDLE state */
	osmo_timer_del(&imts->timer);

	if (loopback)
		LOGP_CHAN(DIMTS, LOGL_INFO, "Entering paging test state, sending digits %s.\n", dial_string);
	else
		LOGP_CHAN(DIMTS, LOGL_INFO, "Entering paging state, sending phone's ID '%s'.\n", dial_string);
	/* set station ID before state change, so status is shown correctly */
	strncpy(imts->station_id, dial_string, sizeof(imts->station_id) - 1);
	imts->tx_page_index = 0;
	imts->tx_page_pulse = 0;
	imts_new_state(imts, (loopback) ? IMTS_PAGING_TEST : IMTS_PAGING);
	imts_activate_idle(); /* must activate another channel right after station is not idle anymore */
	if (imts->mode == MODE_IMTS) {
		/* seize channel before dialing */
		imts_set_dsp_mode(imts, DSP_MODE_TONE, TONE_SEIZE, PAGE_SEIZE, 0);
	} else {
		/* send single pulse + pause, which causes the call selector to reset */
		imts_set_dsp_mode(imts, DSP_MODE_TONE, TONE_1500, PAGE_PAUSE, 0);
		/* for loopback test, we need time stamp */
		imts->tx_page_timestamp = get_time();
	}
}

/* Enter ringing state */
static void imts_ringing(imts_t *imts)
{
	LOGP_CHAN(DIMTS, LOGL_INFO, "Received response from mobile phone, ringing.\n");
	imts->tx_ring_pulse = 0;
	imts_new_state(imts, IMTS_RINGING);
	imts_set_dsp_mode(imts, DSP_MODE_TONE, TONE_IDLE, RING_MARK, 0);
	osmo_timer_schedule(&imts->timer, RINGING_TO);
}

/* Enter conversation state */
static void imts_answer(imts_t *imts)
{
	LOGP_CHAN(DIMTS, LOGL_INFO, "Received answer from mobile phone, conversation started.\n");
	osmo_timer_del(&imts->timer);
	imts_new_state(imts, IMTS_CONVERSATION);
	imts_set_dsp_mode(imts, DSP_MODE_AUDIO, 0, 0.0, 0);
	imts->rx_disc_pulse = 0;
}

/* Loss of signal was detected, release active call. */
void imts_loss_indication(imts_t *imts, double loss_time)
{
	/* stop timer */
	if (imts->mode == MODE_MTS && (imts->state == IMTS_IDLE || imts->state == IMTS_RINGING))
		osmo_timer_del(&imts->timer);

	if (!imts->ptt && imts->state == IMTS_CONVERSATION) {
		LOGP_CHAN(DIMTS, LOGL_NOTICE, "Detected loss of signal after %.1f seconds, releasing.\n", loss_time);
		imts_release(imts);
		call_up_release(imts->callref, CAUSE_TEMPFAIL);
		imts->callref = 0;
	}
}

/* if signal is detected, phone picked up */
void imts_signal_indication(imts_t *imts)
{
	/* setup a call from mobile to base station */
	if (imts->mode == MODE_MTS && imts->state == IMTS_IDLE) {
		LOGP_CHAN(DIMTS, LOGL_INFO, "Detects RF signal in IDLE mode, calling the opterator at '%s'.\n", imts->operator);
		imts->callref = call_up_setup(NULL, imts->operator, OSMO_CC_NETWORK_MTS_NONE, "");
		imts_new_state(imts, IMTS_CONVERSATION);
		imts_set_dsp_mode(imts, DSP_MODE_AUDIO, 0, 0.0, 0);
	}

	/* answer a call from base station to mobile */
	if (imts->mode == MODE_MTS && imts->state == IMTS_RINGING) {
		LOGP_CHAN(DIMTS, LOGL_INFO, "Detected RF signal, mobile is now transmitting.\n");
		call_up_answer(imts->callref, imts->station_id);
		imts_answer(imts);
	}
}

/* decode seize from mobile */
static void imts_receive_seize(imts_t *imts, int tone)
{
	/* other tone stops IDLE / GUARD timer */
	osmo_timer_del(&imts->timer);

	switch (tone) {
	case TONE_IDLE:
	case TONE_600:
		osmo_timer_schedule(&imts->timer, IDLE_DETECT);
		break;
	case TONE_GUARD:
		imts->rx_guard_timestamp = get_time();
		if (imts->fast_seize)
			osmo_timer_schedule(&imts->timer, 0,imts->fast_seize * 1000000);
		break;
	case TONE_CONNECT:
		if (imts->last_tone == TONE_GUARD && imts->rx_guard_duration >= GUARD_TIME) {
			LOGP_CHAN(DIMTS, LOGL_INFO, "Received seize (Guard + Connect tone) from mobile phone.\n");
			LOGP_CHAN(DIMTS, LOGL_INFO, " -> Guard tone duration: %.0f ms (level %.0f%%)\n", (get_time() - imts->rx_guard_timestamp) * 1000.0, imts->last_sigtone_amplitude * 100.0);
			imts_set_dsp_mode(imts, DSP_MODE_TONE, TONE_SILENCE, 0.0, 0);
			imts_new_state(imts, IMTS_SEIZE);
			imts_activate_idle(); /* must activate another channel right after station is not idle anymore */
			osmo_timer_schedule(&imts->timer, SEIZE_TIME);
		}
		break;
	default:
		;
	}
}

/* decode ANI digits */
static void imts_receive_ani(imts_t *imts, int tone)
{
	/* wait for connect tone the first time */
	if (!imts->rx_ani_totpulses && tone != TONE_CONNECT)
		return;

	switch (tone) {
	case TONE_CONNECT:
		/* pulse detected */
		imts->rx_ani_pulse++;
		imts->rx_ani_totpulses++;
		if (imts->rx_ani_pulse > 10) {
			LOGP_CHAN(DIMTS, LOGL_NOTICE, "Received too many pulses, releasing!\n");
			imts_release(imts);
			break;
		}
		LOGP_CHAN(DIMTS, LOGL_DEBUG, "Detected ANI pulse #%d.\n", imts->rx_ani_pulse);
		osmo_timer_schedule(&imts->timer, ANI_PULSE_TO);
		break;
	case TONE_GUARD:
		/* even pulse completed */
		if ((imts->rx_ani_totpulses & 1)) {
			LOGP_CHAN(DIMTS, LOGL_NOTICE, "Parity error: Received Guard tone after %d (odd) pulses, releasing!\n", imts->rx_ani_totpulses);
			imts_release(imts);
			break;
		}
		osmo_timer_schedule(&imts->timer, ANI_PULSE_TO);
		break;
	case TONE_SILENCE:
		/* odd pulse completed */
		if (!(imts->rx_ani_totpulses & 1)) {
			LOGP_CHAN(DIMTS, LOGL_NOTICE, "Parity error: Received silence after %d (even) pulses, releasing!\n", imts->rx_ani_totpulses);
			imts_release(imts);
			break;
		}
		osmo_timer_schedule(&imts->timer, ANI_PULSE_TO);
		break;
	default:
		/* received noise */
		LOGP_CHAN(DIMTS, LOGL_NOTICE, "Received noise while dialing, releasing!\n");
		imts_release(imts);
	}
}

/* decode dialing digits */
static void imts_receive_dialing(imts_t *imts, int tone)
{
	switch (tone) {
	case TONE_CONNECT:
		/* turn off dialtone */
		if (!imts->dial_number[0] && !imts->rx_dial_pulse)
			imts_set_dsp_mode(imts, DSP_MODE_TONE, TONE_SILENCE, 0.0, 0);
		/* pulse detected */
		imts->rx_dial_pulse++;
		if (imts->rx_dial_pulse > 10) {
			LOGP_CHAN(DIMTS, LOGL_NOTICE, "Received too many pulses, releasing!\n");
			imts_release(imts);
			break;
		}
		LOGP_CHAN(DIMTS, LOGL_DEBUG, "Detected dialing pulse #%d.\n", imts->rx_dial_pulse);
		osmo_timer_schedule(&imts->timer, DIAL_PULSE_TO);
		break;
	case TONE_GUARD:
		/* pulse completed */
		if (imts->rx_dial_pulse)
			osmo_timer_schedule(&imts->timer, DIAL_PULSE_TO);
		break;
	default:
		;
#if 0
		/* received noise */
		LOGP_CHAN(DIMTS, LOGL_NOTICE, "Received noise while dialing, releasing!\n");
		imts_release(imts);
#endif
	}
}

/* check for disconnect */
static void imts_receive_disconnect(imts_t *imts, int tone, double elapsed, double amplitude)
{
	/* reset disc counter on timeout */
	if (elapsed > DISC_PULSE_TO) {
		if (imts->rx_disc_pulse) {
			LOGP_CHAN(DIMTS, LOGL_DEBUG, "Timeout Disconnect sequence\n");
			imts->rx_disc_pulse = 0;
		}
		return;
	}

	switch (tone) {
	case TONE_DISCONNECT:
		imts->rx_disc_pulse++;
		LOGP_CHAN(DIMTS, LOGL_DEBUG, "Detected Disconnect pulse #%d.\n", imts->rx_disc_pulse);
		break;
	case TONE_GUARD:
		if (imts->rx_disc_pulse == DISC_COUNT) {
			LOGP_CHAN(DIMTS, LOGL_INFO, "Received disconnect sequence from mobile phone (level %.0f%%).\n", amplitude * 100.0);
			if (imts->state == IMTS_SEIZE
			 || imts->state == IMTS_ANI
			 || imts->state == IMTS_DIALING
			 || imts->state == IMTS_PAGING
			 || imts->state == IMTS_RINGING
			 || imts->state == IMTS_CONVERSATION) {
				if (imts->callref)
					call_up_release(imts->callref, CAUSE_NORMAL);
				imts_release(imts);
			}
			break;
		}
		break;
	default:
		if (imts->rx_disc_pulse) {
			LOGP_CHAN(DIMTS, LOGL_DEBUG, "Disconnect sequence not detected anymore\n");
			imts->rx_disc_pulse = 0;
		}
	}
}

/* decode page test digits */
static void receive_page_imts(imts_t *imts, int tone)
{
	switch (tone) {
	case TONE_IDLE:
		/* pulse detected */
		imts->rx_page_pulse++;
		LOGP_CHAN(DIMTS, LOGL_DEBUG, "Detected page test pulse #%d.\n", imts->rx_page_pulse);
		if (imts->rx_page_pulse > 10) {
			LOGP_CHAN(DIMTS, LOGL_NOTICE, "Received too many pulses!\n");
		}
		osmo_timer_schedule(&imts->timer, 0,PAGE_PULSE_TO * 1000000); // use for page test timeout
		break;
	case TONE_SEIZE:
		/* pulse completed */
		osmo_timer_schedule(&imts->timer, 0,PAGE_PULSE_TO * 1000000); // use for page test timeout
		break;
	default:
		;
	}
}

/* decode page test digits */
static void receive_page_mts(imts_t *imts, int tone)
{
	if (tone == TONE_600 || tone == TONE_1500) {
		/* pulse detected */
		imts->rx_page_pulse++;
		LOGP_CHAN(DIMTS, LOGL_DEBUG, "Detected page test pulse #%d.\n", imts->rx_page_pulse);
		if (imts->rx_page_pulse > 10) {
			LOGP_CHAN(DIMTS, LOGL_NOTICE, "Received too many pulses!\n");
		}
		osmo_timer_schedule(&imts->timer, 0,PAGE_PULSE_TO * 1000000); // use for page test timeout
	}
}

/* A tone was detected or is gone. */
void imts_receive_tone(imts_t *imts, int tone, double elapsed, double amplitude)
{
	/* used for several states */
	imts_receive_disconnect(imts, tone, elapsed, amplitude);

	switch (imts->state) {
	case IMTS_IDLE:
		imts_receive_seize(imts, tone);
		break;
	case IMTS_ANI:
		imts_receive_ani(imts, tone);
		break;
	case IMTS_DIALING:
		imts_receive_dialing(imts, tone);
		break;
	case IMTS_CONVERSATION:
		break;
	case IMTS_PAGING_TEST:
		if (imts->mode == MODE_IMTS)
			receive_page_imts(imts, tone);
		else
			receive_page_mts(imts, tone);
		break;
	default:
		;
	}

	/* remember last tone, also store amplitude of last signaling tone */
	imts->last_tone = tone;
	if (imts->last_tone < NUM_SIG_TONES)
		imts->last_sigtone_amplitude = amplitude;
}

void imts_lost_tone(imts_t *imts, int tone, double elapsed)
{
	switch (imts->state) {
	case IMTS_IDLE:
		osmo_timer_del(&imts->timer);
		imts->rx_guard_duration = elapsed;
		break;
	case IMTS_PAGING:
		if (elapsed >= 0.300 && tone == TONE_GUARD && osmo_timer_pending(&imts->timer)) {
			LOGP_CHAN(DIMTS, LOGL_INFO, "Received acknowledge (Guard tone) from mobile phone (level %.0f%%).\n", imts->last_sigtone_amplitude * 100.0);
			call_up_alerting(imts->callref);
			imts_ringing(imts);
			break;
		}
		break;
	case IMTS_RINGING:
		if (elapsed >= 0.190 && tone == TONE_CONNECT) {
			LOGP_CHAN(DIMTS, LOGL_INFO, "Received answer (Connect tone) from mobile phone (level %.0f%%).\n", imts->last_sigtone_amplitude * 100.0);
			call_up_answer(imts->callref, imts->station_id);
			imts_answer(imts);
			break;
		}
		break;
	default:
		;
	}
}

static void ani_after_digit(imts_t *imts)
{
	/* timeout after pulses: digit complete
	 * timeout after digit: ANI in complete
	 */
	if (imts->rx_ani_pulse) {
		if (imts->rx_ani_pulse == 10)
			imts->rx_ani_pulse = 0;
		imts->station_id[imts->rx_ani_index] = imts->rx_ani_pulse + '0';
		imts->rx_ani_pulse = 0;
		LOGP_CHAN(DIMTS, LOGL_INFO, "Received ANI digit '%c' from mobile phone (level %.0f%%).\n", imts->station_id[imts->rx_ani_index], imts->last_sigtone_amplitude * 100.0);
		imts->station_id[++imts->rx_ani_index] = '\0';
		/* update status while receiving station ID */
		imts_display_status();
		/* if all digits have been received */
		if (imts->rx_ani_index == 7) {
			LOGP_CHAN(DIMTS, LOGL_INFO, "ANI '%s' complete, sending dial tone.\n", imts->station_id);
dt:
			imts_set_dsp_mode(imts, DSP_MODE_TONE, TONE_DIALTONE, 0.0, 0);
			osmo_timer_schedule(&imts->timer, DIALTONE_TO);
			imts->dial_number[0] = '\0';
			imts->rx_dial_index = 0;
			imts->rx_dial_pulse = 0;
			imts_new_state(imts, IMTS_DIALING);
			return;
		}
		osmo_timer_schedule(&imts->timer, ANI_TO);
	} else {
		/* if only 5 digits have been received */
		if (imts->rx_ani_index == 5) {
			LOGP_CHAN(DIMTS, LOGL_INFO, "ANI '%s' (5 digits) complete, sending dial tone.\n", imts->station_id);
			goto dt;
		}
		LOGP_CHAN(DIMTS, LOGL_NOTICE, "Timeout receiving ANI from mobile phone, releasing!\n");
		imts_release(imts);
	}
}

static void dial_after_digit(imts_t *imts)
{
	/* special case where nothing happens after dial tone */
	if (!imts->rx_dial_pulse && !imts->rx_dial_index) {
		LOGP_CHAN(DANETZ, LOGL_NOTICE, "Mobile phone does not start dialing, releasing!\n");
		imts_release(imts);
		return;
	}

	/* timeout after pulses: digit complete
	 * timeout after digit: number complete
	 */
	if (imts->rx_dial_pulse) {
		if (imts->rx_dial_index == sizeof(imts->dial_number) - 1) {
			LOGP_CHAN(DANETZ, LOGL_NOTICE, "Mobile phone dials too many digits, releasing!\n");
			imts_release(imts);
			return;
		}
		if (imts->rx_dial_pulse == 10)
			imts->rx_dial_pulse = 0;
		imts->dial_number[imts->rx_dial_index] = imts->rx_dial_pulse + '0';
		imts->rx_dial_pulse = 0;
		LOGP_CHAN(DIMTS, LOGL_INFO, "Received dial digit '%c' from mobile phone. (level %.0f%%)\n", imts->dial_number[imts->rx_dial_index], imts->last_sigtone_amplitude * 100.0);
		imts->dial_number[++imts->rx_dial_index] = '\0';
		osmo_timer_schedule(&imts->timer, DIALING_TO);
	} else {
		LOGP_CHAN(DIMTS, LOGL_INFO, "Timeout receiving dialing from mobile phone, number complete.\n");
		imts->callref = call_up_setup(imts->station_id, imts->dial_number, OSMO_CC_NETWORK_IMTS_NONE, "");
		imts_new_state(imts, IMTS_CONVERSATION);
		imts_set_dsp_mode(imts, DSP_MODE_AUDIO, 0, 0.0, 0);
		imts->rx_disc_pulse = 0;
	}
}

static void page_after_digit(imts_t *imts)
{
	char digit;
	double delay;

	/* timeout after pulses: digit complete */
	if (imts->rx_page_pulse) {
		if (imts->rx_page_pulse < 11) {
			if (imts->rx_page_pulse == 10)
				imts->rx_page_pulse = 0;
			digit = imts->rx_page_pulse + '0';
			delay = get_time() - imts->tx_page_timestamp - PAGE_PULSE_TO;
			LOGP_CHAN(DIMTS, LOGL_INFO, "Received paging test digit '%c' (level %.0f%%  delay %.0f ms).\n", digit, imts->last_sigtone_amplitude * 100.0, delay * 1000.0);
		}
		imts->rx_page_pulse = 0;
	}
}

/* Timeout handling */
static void imts_timeout(void *data)
{
	imts_t *imts = data;

	switch (imts->state) {
	case IMTS_IDLE:
		switch (imts->last_tone) {
		case TONE_IDLE:
			LOGP_CHAN(DIMTS, LOGL_NOTICE, "Received idle tone (level of %.0f%%), loopback?\n", imts->last_sigtone_amplitude * 100.0);
			/* trigger reset of decoder to force detection again and again */
			imts_set_dsp_mode(imts, DSP_MODE_TONE, TONE_IDLE, 0.0, 1);
			break;
		case TONE_600:
			LOGP_CHAN(DIMTS, LOGL_NOTICE, "Received 600 Hz tone with level of %.0f%%, loopback?\n", imts->last_sigtone_amplitude * 100.0);
			/* trigger reset of decoder to force detection again and again */
			imts_set_dsp_mode(imts, DSP_MODE_TONE, TONE_600, 0.0, 1);
			break;
		case TONE_GUARD:
			LOGP_CHAN(DIMTS, LOGL_NOTICE, "Received Guard tone, turning off IDLE tone\n");
			imts_set_dsp_mode(imts, DSP_MODE_TONE, TONE_SILENCE, 0.5, 0);
			break;
		}
		break;
	case IMTS_PAGING:
		LOGP_CHAN(DIMTS, LOGL_NOTICE, "No response from mobile phone.\n");
		imts_go_idle(imts);
		call_up_release(imts->callref, CAUSE_OUTOFORDER);
		imts->callref = 0;
		break;
	case IMTS_RINGING:
		LOGP_CHAN(DIMTS, LOGL_NOTICE, "No answer from mobile phone's user, releasing.\n");
		imts_release(imts);
		call_up_release(imts->callref, CAUSE_NOANSWER);
		imts->callref = 0;
		break;
	case IMTS_RELEASE:
		imts_go_idle(imts);
		break;
	case IMTS_SEIZE:
		LOGP_CHAN(DIMTS, LOGL_NOTICE, "Sending Seize to mobile phone.\n");
		imts_set_dsp_mode(imts, DSP_MODE_TONE, TONE_SEIZE, SEIZE_LENGTH, 0);
		osmo_timer_schedule(&imts->timer, SEIZE_LENGTH + ANI_TO);
		imts->station_id[0] = '\0';
		imts->rx_ani_index = 0;
		imts->rx_ani_pulse = 0;
		imts->rx_ani_totpulses = 0;
		imts_new_state(imts, IMTS_ANI);
		break;
	case IMTS_ANI:
		ani_after_digit(imts);
		break;
	case IMTS_DIALING:
		dial_after_digit(imts);
		break;
	case IMTS_PAGING_TEST:
		page_after_digit(imts);
		break;
	default:
		;
	}
}

/* generate pulse sequence to page phone */
static void paging_pulses_imts(imts_t *imts, int tone)
{
	double duration;
	int pulses;

	pulses = imts->station_id[imts->tx_page_index] - '0';
	if (pulses == 0)
		pulses = 10;

	if (tone == TONE_SEIZE) {
		if (imts->tx_page_pulse == 0)
			LOGP_CHAN(DIMTS, LOGL_INFO, "Sending paging digit '%c' as pulses.\n", imts->station_id[imts->tx_page_index]);
		/* send mark (pulse start) */
		imts_set_dsp_mode(imts, DSP_MODE_TONE, TONE_IDLE, PAGE_MARK, 0);
		imts->tx_page_pulse++;
	} else {
		if (imts->tx_page_pulse <= pulses) {
			/* send space (pulse end), use long space after last pulse */
			if (imts->tx_page_pulse < pulses)
				duration = PAGE_SPACE;
			else {
				imts->tx_page_pulse = 0;
				imts->tx_page_index++;
				imts->tx_page_timestamp = get_time();
				/* restart test pattern */
				if (!imts->station_id[imts->tx_page_index] && imts->state == IMTS_PAGING_TEST)
					imts->tx_page_index = 0;
				if (imts->station_id[imts->tx_page_index])
					duration = PAGE_PAUSE;
				else {
					duration = 0;
					osmo_timer_schedule(&imts->timer, PAGING_TO);
				}
			}
			imts_set_dsp_mode(imts, DSP_MODE_TONE, TONE_SEIZE, duration, 0);
		}
	}
}
static void paging_pulses_mts(imts_t *imts, int tone)
{
	double duration;
	int pulses;

	pulses = imts->station_id[imts->tx_page_index] - '0';
	if (pulses == 0)
		pulses = 10;

	if (tone == TONE_1500)
		tone = TONE_600;
	else
		tone = TONE_1500;

	if (imts->tx_page_pulse == 0) {
		LOGP_CHAN(DIMTS, LOGL_INFO, "Sending paging digit '%c' as pulses.\n", imts->station_id[imts->tx_page_index]);
	}
	imts->tx_page_pulse++;
	if (imts->tx_page_pulse < pulses)
		duration = PAGE_PULSE;
	else {
		imts->tx_page_pulse = 0;
		imts->tx_page_index++;
		imts->tx_page_timestamp = get_time();
		/* restart test pattern */
		if (!imts->station_id[imts->tx_page_index] && imts->state == IMTS_PAGING_TEST)
			imts->tx_page_index = 0;
		if (imts->station_id[imts->tx_page_index])
			duration = PAGE_PAUSE;
		else {
			LOGP_CHAN(DIMTS, LOGL_INFO, "Digits complete, assuming the phone is ringing.\n");
			duration = 0;
			imts_new_state(imts, IMTS_RINGING);
			call_up_alerting(imts->callref);
		}
	}
	imts_set_dsp_mode(imts, DSP_MODE_TONE, tone, duration, 0);
}

/* generate pulse sequence to ring phone */
static void ringing_pulses(imts_t *imts, int tone)
{
	if (tone == TONE_IDLE) {
		if (imts->tx_ring_pulse == 0)
			LOGP_CHAN(DIMTS, LOGL_INFO, "Sending ringing signal as pulses.\n");
		/* send space (pulse end) */
		imts_set_dsp_mode(imts, DSP_MODE_TONE, TONE_SEIZE, RING_SPACE, 0);
		imts->tx_ring_pulse++;
	} else {
		if (imts->tx_ring_pulse < RING_PULSES) {
			/* send mark (pulse start) */
			imts_set_dsp_mode(imts, DSP_MODE_TONE, TONE_IDLE, RING_MARK, 0);
		} else {
			LOGP_CHAN(DIMTS, LOGL_INFO, "Sending pause after ringing.\n");
			/* send long space after last pulse */
			imts_set_dsp_mode(imts, DSP_MODE_TONE, TONE_SEIZE, RING_OFF, 0);
			imts->tx_ring_pulse = 0;
		}
	}
}

/* after sending Seize tone switch to silence and await ANI */
static void seize_sent(imts_t *imts, int tone)
{
	if (tone == TONE_SEIZE) {
		imts_set_dsp_mode(imts, DSP_MODE_TONE, TONE_SILENCE, 0.0, 0);
	}
}

/* send test pattern (cycle through tones, skip if length is 0) */
static void detector_test_imts(imts_t *imts, int tone)
{
	switch (tone) {
	case TONE_SILENCE:
tone_idle:
		if (imts->detector_test_length_1 <= 0)
			goto tone_seize;
		LOGP_CHAN(DIMTS, LOGL_NOTICE, "Sending %.3fs IDLE tone.\n", imts->detector_test_length_1);
		imts_set_dsp_mode(imts, DSP_MODE_TONE, TONE_IDLE, imts->detector_test_length_1, 0);
		break;
	case TONE_IDLE:
tone_seize:
		if (imts->detector_test_length_2 <= 0)
			goto tone_silence;
		LOGP_CHAN(DIMTS, LOGL_NOTICE, "Sending %.3fs SEIZE tone.\n", imts->detector_test_length_2);
		imts_set_dsp_mode(imts, DSP_MODE_TONE, TONE_SEIZE, imts->detector_test_length_2, 0);
		break;
	case TONE_SEIZE:
tone_silence:
		if (imts->detector_test_length_3 <= 0)
			goto tone_idle;
		LOGP_CHAN(DIMTS, LOGL_NOTICE, "Sending %.3fs SILENCE.\n", imts->detector_test_length_3);
		imts_set_dsp_mode(imts, DSP_MODE_TONE, TONE_SILENCE, imts->detector_test_length_3, 0);
		break;
	}
}
static void detector_test_mts(imts_t *imts, int tone)
{
	switch (tone) {
	case TONE_SILENCE:
tone_idle:
		if (imts->detector_test_length_1 <= 0)
			goto tone_seize;
		LOGP_CHAN(DIMTS, LOGL_NOTICE, "Sending %.3fs 600 Hz tone.\n", imts->detector_test_length_1);
		imts_set_dsp_mode(imts, DSP_MODE_TONE, TONE_600, imts->detector_test_length_1, 0);
		break;
	case TONE_600:
tone_seize:
		if (imts->detector_test_length_2 <= 0)
			goto tone_silence;
		LOGP_CHAN(DIMTS, LOGL_NOTICE, "Sending %.3fs 1500 Hz tone.\n", imts->detector_test_length_2);
		imts_set_dsp_mode(imts, DSP_MODE_TONE, TONE_1500, imts->detector_test_length_2, 0);
		break;
	case TONE_1500:
tone_silence:
		if (imts->detector_test_length_3 <= 0)
			goto tone_idle;
		LOGP_CHAN(DIMTS, LOGL_NOTICE, "Sending %.3fs SILENCE.\n", imts->detector_test_length_3);
		imts_set_dsp_mode(imts, DSP_MODE_TONE, TONE_SILENCE, imts->detector_test_length_3, 0);
		break;
	}
}

/* whenever a tone has been sent (only for tones with given duration) */
void imts_tone_sent(imts_t *imts, int tone)
{
	switch (imts->state) {
	case IMTS_IDLE:
		LOGP_CHAN(DIMTS, LOGL_NOTICE, "No Seize tone after Guard tone, turning on IDLE tone\n");
		imts_set_dsp_mode(imts, DSP_MODE_TONE, TONE_IDLE, 0.0, 0);
		break;
	case IMTS_ANI:
		seize_sent(imts, tone);
		break;
	case IMTS_PAGING:
	case IMTS_PAGING_TEST:
		if (imts->mode == MODE_IMTS)
			paging_pulses_imts(imts, tone);
		else
			paging_pulses_mts(imts, tone);
		break;
	case IMTS_RINGING:
		ringing_pulses(imts, tone);
		break;
	case IMTS_RELEASE:
		imts_release(imts);
		break;
	case IMTS_DETECTOR_TEST:
		if (imts->mode == MODE_IMTS)
			detector_test_imts(imts, tone);
		else
			detector_test_mts(imts, tone);
		break;
	default:
		;
	}
}

/* Call control starts call towards mobile station. */
int call_down_setup(int callref, const char __attribute__((unused)) *caller_id, enum number_type __attribute__((unused)) caller_type, const char *dialing)
{
	char number[8];
	sender_t *sender;
	imts_t *imts;

	/* 1. check if given number is already in a call, return BUSY */
	for (sender = sender_head; sender; sender = sender->next) {
		imts = (imts_t *) sender;
		if (!strcmp(imts->station_id, dialing))
			break;
	}
	if (sender) {
		LOGP(DIMTS, LOGL_NOTICE, "Outgoing call to busy number, rejecting!\n");
		return -CAUSE_BUSY;
	}

	/* 2. check if all channels are busy, return NOCHANNEL */
	for (sender = sender_head; sender; sender = sender->next) {
		imts = (imts_t *) sender;
		if (imts->state == IMTS_IDLE)
			break;
	}
	if (!sender) {
		LOGP(DIMTS, LOGL_NOTICE, "Outgoing call, but no free channel, rejecting!\n");
		return -CAUSE_NOCHANNEL;
	}

	/* 3. convert 10 digit numbers to 7 digit station ID */
	if (strlen(dialing) == 10) {
		strncpy(number, dialing, 3);
		strcpy(number + 3, dialing + 6);
		dialing = number;
	}

	/* 4. trying to page mobile station */
	LOGP_CHAN(DIMTS, LOGL_INFO, "Call to mobile station, paging number: %s\n", dialing);
	imts->callref = callref;
	imts_paging(imts, dialing, 0);

	return 0;
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
	sender_t *sender;
	imts_t *imts;

	LOGP(DIMTS, LOGL_INFO, "Call has been disconnected by network.\n");

	for (sender = sender_head; sender; sender = sender->next) {
		imts = (imts_t *) sender;
		if (imts->callref == callref)
			break;
	}
	if (!sender) {
		LOGP(DIMTS, LOGL_NOTICE, "Outgoing disconnect, but no callref!\n");
		call_up_release(callref, CAUSE_INVALCALLREF);
		return;
	}

	/* Release when not active */
	if (imts->state == IMTS_CONVERSATION)
		return;
	switch (imts->state) {
	case IMTS_PAGING:
	case IMTS_RINGING:
		LOGP_CHAN(DIMTS, LOGL_NOTICE, "Outgoing disconnect, during paging/alerting, releasing!\n");
	 	imts_release(imts);
		break;
	default:
		break;
	}

	call_up_release(callref, cause);

	imts->callref = 0;

}

/* Call control releases call toward mobile station. */
void call_down_release(int callref, __attribute__((unused)) int cause)
{
	sender_t *sender;
	imts_t *imts;

	LOGP(DIMTS, LOGL_INFO, "Call has been released by network, releasing call.\n");

	for (sender = sender_head; sender; sender = sender->next) {
		imts = (imts_t *) sender;
		if (imts->callref == callref)
			break;
	}
	if (!sender) {
		LOGP(DIMTS, LOGL_NOTICE, "Outgoing release, but no callref!\n");
		/* don't send release, because caller already released */
		return;
	}

	imts->callref = 0;

	switch (imts->state) {
	case IMTS_CONVERSATION:
		LOGP_CHAN(DIMTS, LOGL_NOTICE, "Outgoing release, during call, releasing!\n");
	 	imts_release(imts);
		break;
	case IMTS_PAGING:
	case IMTS_RINGING:
		LOGP_CHAN(DIMTS, LOGL_NOTICE, "Outgoing release, during paging/alerting, releasing!\n");
	 	imts_release(imts);
		break;
	default:
		break;
	}
}

/* Receive audio from call instance. */
void call_down_audio(int callref, uint16_t sequence, uint32_t timestamp, uint32_t ssrc, sample_t *samples, int count)
{
	sender_t *sender;
	imts_t *imts;

	for (sender = sender_head; sender; sender = sender->next) {
		imts = (imts_t *) sender;
		if (imts->callref == callref)
			break;
	}
	if (!sender)
		return;

	if (imts->dsp_mode == DSP_MODE_AUDIO) {
		jitter_save(&imts->sender.dejitter, samples, count, 1, sequence, timestamp, ssrc);
	}
}

void call_down_clock(void) {}

void dump_info(void) {}

