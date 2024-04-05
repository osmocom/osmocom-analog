/* AM7910 / AM7911 modem chip emulation and signal processing
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

/*
 * Not implemented:
 *  - auto answer
 *  - Bell 202 5 bps back channel
 *  - equalizer
 */

/*
 * Implementation is done according to the AM7910/AM7911 datasheet. This is
 * only a (short and bad) summary, so read the datasheet!!!
 *
 * DTR state:
 * When DTR is off, state is clamped to INIT state.
 * When DTR becomes on, RX and TX state machines begin to run.
 * When DTR becomes off, state machines change to INIT state immediately.
 *
 * (B)RTS state:
 * When (B)RTS becomes on, data transmission is enabled.
 * Then (B)CTS becomes on, when the timer (B)RCON is complete.
 * This means that data transmission is now allowed by upper layer.
 * When (B)RTS becomes off, data transmission is disabled.
 * Then (B)CTS becomes off, when the timer (B)RCOFF is complete.
 *
 * (B)CD state:
 * When carrier is detected, timer (B)CDON is started.
 * When carrier is stable and timer is complete, (B)CD becomes on and data
 * reception is enabled.
 * When carrier is lost, timer (B)CDOFF is started.
 * When carrier timer is complete, (B)CD becomes off and data reception is
 * disabled.
 * While transmitting half duplex mode, (B)CD will be blocked to prevent
 * carrier detection from loopback of audio signal.
 *
 * (B)TD data:
 * When transmitting, data is requested from upper layer and forwarded into
 * modulator.
 * When not transmitting, this data is blocked, meaning that '1' (MARK) is
 * transmitted into the modulator, regardless of the upper layer data.
 * STO (soft turn off) and/or silence is sent after transmission is over.
 *
 * (B)RD data:
 * When data reception is not blocked, data is received from the demodulator
 * and forwarded towards upper layer.
 * While receiving in half duplex mode, (B)RD is blocked, meaning that '1'
 * (MARK) is forwarded toward upper layer, regardless of the data from the
 * demodulator.
 * Squelch (mute receive audio) is used to prevent noise when turning off
 * half duplex transmission.
 *
 * Audio level is based on milliwatts (at 600 Ohms), which is a value of 1.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "../liblogging/logging.h"
#include <osmocom/core/timer.h>
#include "../libsample/sample.h"
#include "../libfsk/fsk.h"
#include "am791x.h"

#define FLOAT_TO_TIMEOUT(f) floor(f), ((f) - floor(f)) * 1000000

#define db2level(db)	pow(10, (double)(db) / 20.0)
#define level2db(level)	(20 * log10(level))

/* levels used (related to dBm0 (1 mW)) */

#define TX_LEVEL	-3	/* according to datasheet (at 600 Ohms) */
#define RX_CD_ON_7910	-40.5	/* according to datasheet (at 600 Ohms) */
#define RX_CD_ON_7911	-42.0	/* according to datasheet (at 600 Ohms) */
#define RX_CD_OFF_7910	-45.0	/* according to datasheet (at 600 Ohms) */
#define RX_CD_OFF_7911	-47.5	/* according to datasheet (at 600 Ohms) */
#define RX_QUALITY	0.1	/* FIXME: minimum quality */
#define BIT_ADJUST	0.5 /* must be 0.5 to completely sync each bit */

/* frequencies used */

/*	standard	f0, f1 (tx)	f0, f1 (rx)	f0, f1 (back tx+rx) */
#define	BELL_103_ORIG	1070,	1270,	2025,	2225,	0,	0,
#define	BELL_103_ANS	2025,	2225,	1070,	1270,	0,	0,
#define	BELL_103_ORIG_L	1070,	1270,	1070,	1270,	0,	0,
#define	BELL_103_ANS_L	2025,	2225,	2025,	2225,	0,	0,
#define	CCITT_V21_ORIG	1180,	980,	1850,	1650,	0,	0,
#define	CCITT_V21_ANS	1850,	1650,	1180,	980,	0,	0,
#define	CCITT_V21_ORG_L	1180,	980,	1180,	980,	0,	0,
#define	CCITT_V21_ANS_L	1850,	1650,	1850,	1650,	0,	0,
#define	CCITT_V23_M1	1700,	1300,	1700,	1300,	0,	0,
#define	CCITT_V23_M1B	1700,	1300,	1700,	1300,	450,	390,
#define	CCITT_V23_M2	2100,	1300,	2100,	1300,	0,	0,
#define	CCITT_V23_M2B	2100,	1300,	2100,	1300,	450,	390,
#define	BELL_202	2200,	1200,	2200,	1200,	0,	0,
#define	BELL_202B	2200,	1200,	2200,	1200,	487,	387,
#define	CCITT_V23_BACK	0,	0,	0,	0,	450,	390,
#define	BELL_202_BACK	0,	0,	0,	0,	487,	387,
#define	RESERVED	0,	0,	0,	0,	0,	0,

/* timings used */

/*	timer/std	7910	7911 */
#define T_RCON_B103	0.2083,	0.025,
#define T_RCOFF_B103	0.0004,	0.00052,
#define T_BRCON_B103	NAN,	NAN,
#define	T_BRCOFF_B103	NAN,	NAN,
#define T_CDON_B103	0.0291,	0.010,
#define	T_CDOFF_B103	0.021,	0.007,
#define T_BCDON_B103	NAN,	NAN,
#define T_BCDOFF_B103	NAN,	NAN,
#define T_AT_B103	1.9,	1.9,
#define	T_SIL1_B103	1.3,	2.0,
#define T_SIL2_B103	NAN,	NAN,
#define	T_SQ_B103	NAN,	NAN,
#define T_STO_B103	NAN,	NAN,

#define T_B103 \
	T_RCON_B103	T_RCOFF_B103	T_BRCON_B103	T_BRCOFF_B103	\
	T_CDON_B103	T_CDOFF_B103	T_BCDON_B103	T_BCDOFF_B103	\
	T_AT_B103	T_SIL1_B103	T_SIL2_B103	T_SQ_B103	T_STO_B103

/*	timer/std	7910	7911 */
#define T_RCON_V21	0.400,	0.025,
#define T_RCOFF_V21	0.0004,	0.00052,
#define T_BRCON_V21	NAN,	NAN,
#define	T_BRCOFF_V21	NAN,	NAN,
#define T_CDON_V21	0.301,	0.010,
#define	T_CDOFF_V21	0.021,	0.007,
#define T_BCDON_V21	NAN,	NAN,
#define T_BCDOFF_V21	NAN,	NAN,
#define T_AT_V21	3.0,	3.0,
#define	T_SIL1_V21	1.9,	2.0,
#define T_SIL2_V21	NAN,	0.075,
#define	T_SQ_V21	NAN,	NAN,
#define T_STO_V21	NAN,	NAN,

#define T_V21 \
	T_RCON_V21	T_RCOFF_V21	T_BRCON_V21	T_BRCOFF_V21	\
	T_CDON_V21	T_CDOFF_V21	T_BCDON_V21	T_BCDOFF_V21	\
	T_AT_V21	T_SIL1_V21	T_SIL2_V21	T_SQ_V21	T_STO_V21

/*	timer/std	7910	7911 */
#define T_RCON_V23	0.2083,	0.008,
#define T_RCOFF_V23	0.0004,	0.00052,
#define T_BRCON_V23	0.0823,	0.0823,
#define	T_BRCOFF_V23	0.0004,	0.0005,
#define T_CDON_V23	0.011,	0.003,
#define	T_CDOFF_V23	0.0035,	0.002,
#define T_BCDON_V23	0.017,	0.018,
#define T_BCDOFF_V23	0.021,	0.022,
#define T_AT_V23	3.0,	3.0,
#define	T_SIL1_V23	1.9,	2.0,
#define T_SIL2_V23	NAN,	0.075,
#define	T_SQ_V23	0.1563,	0.009,
#define T_STO_V23	NAN,	0.008,

#define T_V23 \
	T_RCON_V23	T_RCOFF_V23	T_BRCON_V23	T_BRCOFF_V23	\
	T_CDON_V23	T_CDOFF_V23	T_BCDON_V23	T_BCDOFF_V23	\
	T_AT_V23	T_SIL1_V23	T_SIL2_V23	T_SQ_V23	T_STO_V23

/*	timer/std	7910	7911 */
#define T_RCON_B202	0.1833,	0.008,
#define T_RCOFF_B202	0.0004,	0.00052,
#define T_BRCON_B202	0.0823,	0.0823,
#define	T_BRCOFF_B202	0.0004,	0.0005,
#define T_CDON_B202	0.018,	0.003,
#define	T_CDOFF_B202	0.012,	0.002,
#define T_BCDON_B202	0.017,	0.018,
#define T_BCDOFF_B202	0.021,	0.022,
#define T_AT_B202	1.9,	1.9,
#define	T_SIL1_B202	1.3,	2.0,
#define T_SIL2_B202	NAN,	NAN,
#define	T_SQ_B202	0.1563,	0.009,
#define T_STO_B202	0.024,	0.008,

#define T_B202 \
	T_RCON_B202	T_RCOFF_B202	T_BRCON_B202	T_BRCOFF_B202	\
	T_CDON_B202	T_CDOFF_B202	T_BCDON_B202	T_BCDOFF_B202	\
	T_AT_B202	T_SIL1_B202	T_SIL2_B202	T_SQ_B202	T_STO_B202

#define T_RES \
	NAN, NAN,	NAN, NAN,	NAN, NAN,	NAN, NAN,	\
	NAN, NAN,	NAN, NAN,	NAN, NAN,	NAN, NAN,	\
	NAN, NAN,	NAN, NAN,	NAN, NAN,	NAN, NAN,	NAN, NAN,

/* mode definition */

static struct am791x_mode {
	int sup_7910, sup_7911;			/* supported */
	int f0_tx, f1_tx;			/* frequencies */
	int f0_rx, f1_rx;
	int f0_back, f1_back;
	double t_rcon_7910, t_rcon_7911;	/* timers */
	double t_rcoff_7910, t_rcoff_7911;
	double t_brcon_7910, t_brcon_7911;
	double t_brcoff_7910, t_brcoff_7911;
	double t_cdon_7910, t_cdon_7911;
	double t_cdoff_7910, t_cdoff_7911;
	double t_bcdon_7910, t_bcdon_7911;
	double t_bcdoff_7910, t_bcdoff_7911;
	double t_at_7910, t_at_7911;
	double t_sil1_7910, t_sil1_7911;
	double t_sil2_7910, t_sil2_7911;
	double t_sq_7910, t_sq_7911;
	double t_sto_7910, t_sto_7911;
	int fullduplex;				/* duplex */
	int loopback_main, loopback_back;	/* loopback */
	int equalizer, sto;			/* equalizer, soft turn off */
	int bell_202;				/* is BELL 202 mode */
	double max_baud;			/* maximum baud rate */
	const char *description;		/* description */
} am791x_modes[32] = {
	/*sup	frequencies	timers	duplex	loop	EQ,STO	BELL202	maxBAUD	description */
	/* normal modes */
	{ 1, 1,	BELL_103_ORIG	T_B103	1,	0, 0,	0, 0,	0,	300,	"Bell 103 originate (300 bps full-duplex)" },
	{ 1, 1,	BELL_103_ANS	T_B103	1,	0, 0,	0, 0,	1,	300,	"Bell 103 answer (300 bps full-duplex)" },
	{ 1, 1,	BELL_202	T_B202	0,	0, 0,	0, 0,	1,	1200,	"Bell 202 (1200 bps half-duplex)" },
	{ 1, 1,	BELL_202	T_B202	0,	0, 0,	1, 0,	1,	1200,	"Bell 202 with equalizer (1200 bps half-duplex)" },
	{ 1, 1,	CCITT_V21_ORIG	T_V21	1,	0, 0,	0, 0,	0,	300,	"CCITT V.21 originate (300 bps full-duplex)" },
	{ 1, 1,	CCITT_V21_ANS	T_V21	1,	0, 0,	0, 0,	0,	300,	"CCITT V.21 answer (300 bps full-duplex)" },
	{ 1, 1,	CCITT_V23_M2	T_V23	0,	0, 0,	0, 0,	0,	1200,	"CCITT V.23 mode 2 (1200 bps half-duplex)" },
	{ 1, 1,	CCITT_V23_M2	T_V23	0,	0, 0,	1, 0,	0,	1200,	"CCITT V.23 mode 2 with equalizer (1200 bps half-duplex)" },
	{ 1, 1,	CCITT_V23_M1B	T_V23	0,	0, 0,	0, 0,	0,	1200,	"CCITT V.23 mode 1 (600/75 bps half-duplex)" },
	{ 0, 0,	RESERVED	T_RES	0,	0, 0,	0, 0,	0,	0,	"Reserved" },
	{ 0, 1,	BELL_202B	T_B202	0,	0, 0,	0, 0,	1,	1200,	"Bell 202 (1200/150 bps half-duplex)" },
	{ 0, 1,	BELL_202B	T_B202	0,	0, 0,	1, 0,	1,	1200,	"Bell 202 with equalizer (1200/150 bps half-duplex)" },
	{ 0, 1,	CCITT_V23_M1B	T_V23	0,	0, 0,	0, 1,	0,	1200,	"CCITT V.23 mode 1 with soft turn-off (600/75 bps half-duplex)" },
	{ 0, 0,	RESERVED	T_RES	0,	0, 0,	0, 0,	0,	0,	"Reserved" },
	{ 0, 1,	CCITT_V23_M2B	T_V23	0,	0, 0,	0, 1,	0,	1200,	"CCITT V.23 mode 2 with soft turn-off (1200/75 bps half-duplex)" },
	{ 0, 1,	CCITT_V23_M2B	T_V23	0,	0, 0,	1, 1,	0,	1200,	"CCITT V.23 mode 2 with soft turn-off and equalizer (1200/75 bps half-duplex)" },
	/* loopback modes */
	{ 1, 1,	BELL_103_ORIG_L	T_B103	0,	1, 0,	0, 0,	0,	300,	"Bell 103 orig loopback (300 bps)" },
	{ 1, 1,	BELL_103_ANS_L	T_B103	0,	1, 0,	0, 0,	0,	300,	"Bell 103 answer loopback (300 bps)" },
	{ 1, 1,	BELL_202	T_B202	0,	1, 0,	0, 0,	1,	1200,	"Bell 202 main loopback (1200 bps)" },
	{ 1, 1,	BELL_202	T_B202	0,	1, 0,	1, 0,	1,	1200,	"Bell 202 main loopback with equalizer (1200 bps)" },
	{ 1, 1,	CCITT_V21_ORG_L T_V21	0,	1, 0,	0, 0,	0,	300,	"CCITT V.21 originate loopback (300 bps)" },
	{ 1, 1,	CCITT_V21_ANS_L T_V21	0,	1, 0,	0, 0,	0,	300,	"CCITT V.21 answer loopback (300 bps)" },
	{ 1, 1,	CCITT_V23_M2	T_V23	0,	1, 0,	0, 0,	0,	1200,	"CCITT V.23 mode 2 main loopback (1200 bps)" },
	{ 1, 1,	CCITT_V23_M2	T_V23	0,	1, 0,	1, 0,	0,	1200,	"CCITT V.23 mode 2 main loopback with equalizer (1200 bps)" },
	{ 1, 1,	CCITT_V23_M1	T_V23	0,	1, 0,	0, 0,	0,	1200,	"CCITT V.23 mode 1 main loopback (600 bps)" },
	{ 1, 1,	CCITT_V23_BACK	T_V23	0,	0, 1,	0, 0,	0,	150,	"CCITT V.23 back loopback (75/150 bps)" },
	{ 0, 1,	BELL_202_BACK	T_B202	0,	0, 1,	0, 0,	1,	150,	"Bell 202 back loopback (150 bps)" },
	{ 0, 0,	RESERVED	T_RES	0,	0, 0,	0, 0,	0,	0,	"Reserved" },
	{ 0, 0,	RESERVED	T_RES	0,	0, 0,	0, 0,	0,	0,	"Reserved" },
	{ 0, 0,	RESERVED	T_RES	0,	0, 0,	0, 0,	0,	0,	"Reserved" },
	{ 0, 0,	RESERVED	T_RES	0,	0, 0,	0, 0,	0,	0,	"Reserved" },
	{ 0, 0,	RESERVED	T_RES	0,	0, 0,	0, 0,	0,	0,	"Reserved" },
};

const char *am791x_state_names[] = {
	"INIT",
	"RCON",
	"CDON",
	"DATA",
	"RCOFF",
	"CDOFF",
	"STO_OFF",
	"SQ_OFF",
	"BRCON",
	"BCDON",
	"BDATA",
	"BRCOFF",
	"BCDOFF",
};

/* list all modes */
void am791x_list_mc(enum am791x_type type)
{
	int i;
	const char *description;

	for (i = 0; i < 32; i++) {
		if ((!type && am791x_modes[i].sup_7910) || (type && am791x_modes[i].sup_7911))
			description = am791x_modes[i].description;
		else
			description = am791x_modes[31].description;
		printf("mc %d: %s\n", i, description);
	}
}

/* init STO signal */
static void init_sto(am791x_t *am791x)
{
	am791x->sto_phaseshift65536 = 900 / (double)am791x->samplerate * 65536.0;
}

/* transmit STO signal, use phase from FSK modulator, to avoid phase jumps */
static int send_sto(am791x_t *am791x, sample_t *sample, int length)
{
	fsk_mod_t *fsk = &am791x->fsk_tx;
	int count = 0;
	double phase, phaseshift;

	phase = fsk->tx_phase65536;

	/* modulate STO */
	phaseshift = am791x->sto_phaseshift65536;
	while (count < length) {
		sample[count++] = fsk->sin_tab[(uint16_t)phase];
		phase += phaseshift;
		if (phase >= 65536.0)
			phase -= 65536.0;
	}

	fsk->tx_phase65536 = phase;

	return count;
}

/* send audio from FSK modulator */
void am791x_send(am791x_t *am791x, sample_t *samples, int length)
{
	if (am791x->tx_sto)
		send_sto(am791x, samples, length);
	else {
		if (am791x->f0_tx) {
			/* if filter set (even if tx_silence, we want to request bits from upper layer) */
			fsk_mod_send(&am791x->fsk_tx, samples, length, 0);
		}
		if (!am791x->f0_tx || am791x->tx_silence) {
			/* if filter not set, or silence  */
			memset(samples, 0, length * sizeof(*samples));
		}
	}
}

/* receive audio and feed into FSK demodulator */
void am791x_receive(am791x_t *am791x, sample_t *samples, int length)
{
	if (!am791x->f0_rx) {
		/* no mode set */
		memset(samples, 0, length * sizeof(*samples));
		return;
	}
	if (am791x->squelch) {
		/* handle squelch, but then demod... */
		memset(samples, 0, length * sizeof(*samples));
	}
	if (am791x->f0_rx) {
		/* handle RX audio */
		fsk_demod_receive(&am791x->fsk_rx, samples, length);
	}
}

/* provide bit to FSK modulator */
static int fsk_send_bit(void *inst)
{
	am791x_t *am791x = (am791x_t *)inst;
	int bit, bbit;

	bit = am791x->td_cb(am791x->inst);
	bbit = am791x->btd_cb(am791x->inst);

	/* main channel returns TD */
	if (!am791x->block_td) {
#ifdef HEAVY_DEBUG
		LOGP(DDSP, LOGL_DEBUG, "Modulating bit '%d' for MAIN channel\n", bit);
#endif
		return bit;
	}
	/* back channel returns BTD */
	if (!am791x->block_btd) {
#ifdef HEAVY_DEBUG
		LOGP(DDSP, LOGL_DEBUG, "Modulating bit '%d' for BACK channel\n", bbit);
#endif
		return bbit;
	}
#ifdef HEAVY_DEBUG
	LOGP(DDSP, LOGL_DEBUG, "Modulating bit '1', because TD & BTD is ignored\n");
#endif
	return 1;
}

static void handle_rx_state(am791x_t *am791x);

/* get bit from FSK demodulator */
static void fsk_receive_bit(void *inst, int bit, double quality, double level)
{
	am791x_t *am791x = (am791x_t *)inst;
	int *block, *cd;

#ifdef HEAVY_DEBUG
	LOGP(DDSP, LOGL_DEBUG, "Demodulated bit '%d' (level = %.0f dBm, quality = %%%.0f)\n", bit, level2db(level), quality * 100.0);
#endif

	if (!am791x->rx_back_channel) {
		block = &am791x->block_cd;
		cd = &am791x->cd;
	} else {
		block = &am791x->block_bcd;
		cd = &am791x->bcd;
	}

	/* detection/loss of carrier */
	if (*block && *cd) {
		*cd = 0;
		handle_rx_state(am791x);
	} else
	if (!(*block) && !(*cd) && level > am791x->cd_on && quality >= RX_QUALITY) {
		LOGP(DDSP, LOGL_DEBUG, "Good quality (level = %.0f dBm, quality = %%%.0f)\n", level2db(level), quality * 100.0);
		*cd = 1;
		handle_rx_state(am791x);
	} else
	if (*cd && (level < am791x->cd_off || quality < RX_QUALITY)) {
		LOGP(DDSP, LOGL_DEBUG, "Bad quality (level = %.0f dBm, quality = %%%.0f)\n", level2db(level), quality * 100.0);
		*cd = 0;
		handle_rx_state(am791x);
	}

	/* assume 1 if no carrier */
	if (!(*cd)) {
		bit = 1;
		quality = 0;
		level = 0;
	}


	/* main channel forwards bit to RD */
	if (!am791x->block_rd) {
#ifdef HEAVY_DEBUG
		LOGP(DDSP, LOGL_DEBUG, " -> Forwarding bit '%d' to MAIN channel\n", bit);
#endif
		am791x->rd_cb(am791x->inst, bit, quality * 100.0, level2db(level));
	} else {
#ifdef HEAVY_DEBUG
		LOGP(DDSP, LOGL_DEBUG, " -> Forwarding bit '1' to MAIN channel, because RD is set to MARK\n");
#endif
		am791x->rd_cb(am791x->inst, 1, NAN, NAN);
	}
	/* main channel forwards bit to RD */
	if (!am791x->block_brd) {
#ifdef HEAVY_DEBUG
		LOGP(DDSP, LOGL_DEBUG, " -> Forwarding bit '%d' to BACK channel\n", bit);
#endif
		am791x->brd_cb(am791x->inst, bit, quality * 100.0, level2db(level));
	} else {
#ifdef HEAVY_DEBUG
		LOGP(DDSP, LOGL_DEBUG, " -> Forwarding bit '1' to BACK channel, because BRD is set to MARK\n");
#endif
		am791x->brd_cb(am791x->inst, 1, NAN, NAN);
	}
}

/* setup FSK */
static void set_filters(am791x_t *am791x)
{
	const char *name_tx = NULL, *name_rx = NULL;
	int f0_tx = -1, f1_tx = -1;
	int f0_rx = -1, f1_rx = -1;
	uint8_t mc = am791x->mc;

	/* not supported */
	if (!((am791x->type) ? am791x_modes[mc].sup_7911 : am791x_modes[mc].sup_7910)) {
		f0_tx = 0;
		f1_tx = 0;
		f0_rx = 0;
		f1_rx = 0;
	} else
	switch (am791x->tx_state) {
	case AM791X_STATE_INIT:
		/* when RTS and BRTS are not asserted */
		f0_tx = 0; // TX !!
		f1_tx = 0;
		name_tx = "MAIN";
		if (am791x->loopback_back) {
			/* loopback (BACK): always listens to back channel */
			f0_rx = am791x_modes[mc].f0_back; // RX !!
			f1_rx = am791x_modes[mc].f1_back;
			name_rx = "BACK";
			am791x->rx_back_channel = 1;
		} else {
			/* listen to main channel's RX frequencies */
			f0_rx = am791x_modes[mc].f0_rx; // RX !!
			f1_rx = am791x_modes[mc].f1_rx;
			name_rx = "MAIN";
			am791x->rx_back_channel = 0;
		}
		break;
	case AM791X_STATE_RCON:
		/* when asserting RTS */
		f0_tx = am791x_modes[mc].f0_tx;
		f1_tx = am791x_modes[mc].f1_tx;
		name_tx = "MAIN";
		if (!am791x->loopback_main && am791x_modes[mc].f0_back && am791x_modes[mc].f1_back) {
			/* switch receiver to back channel, (if not in loopback mode) */
			f0_rx = am791x_modes[mc].f0_back;
			f1_rx = am791x_modes[mc].f1_back;
			name_rx = "BACK";
			am791x->rx_back_channel = 1;
		}
		break;
	case AM791X_STATE_BRCON:
		/* when asserting BRTS */
		f0_tx = am791x_modes[mc].f0_back;
		f1_tx = am791x_modes[mc].f1_back;
		name_tx = "BACK";
		break;
	default:
		/* keep current frequencies in other state */
		return;
	}

	/* transmitter not used anymore or has changed */
	if (f0_tx >= 0 && am791x->f0_tx && (f0_tx != am791x->f0_tx || f1_tx != am791x->f1_tx)) {
		/* disable transmitter */
		fsk_mod_cleanup(&am791x->fsk_tx);
		am791x->f0_tx = 0;
		am791x->f1_tx = 0;
	}

	/* transmitter used */
	if (f0_tx > 0 && am791x->f0_tx == 0) {
		LOGP(DDSP, LOGL_DEBUG, "Setting modulator to %s channel's frequencies (F0 = %d, F1 = %d), baudrate %.0f\n", name_tx, f0_tx, f1_tx, am791x->tx_baud);
		if (fsk_mod_init(&am791x->fsk_tx, am791x, fsk_send_bit, am791x->samplerate, am791x->tx_baud, (double)f0_tx, (double)f1_tx, am791x->tx_level, 0, 1) < 0)
			LOGP(DDSP, LOGL_ERROR, "FSK RX init failed!\n");
		else {
			am791x->f0_tx = f0_tx;
			am791x->f1_tx = f1_tx;
		}
	}

	/* receiver has changed */
	if (f0_rx >= 0 && am791x->f0_rx && (f0_rx != am791x->f0_rx || f1_rx != am791x->f1_rx)) {
		/* disable receiver */
		fsk_demod_cleanup(&am791x->fsk_rx);
		am791x->f0_rx = 0;
		am791x->f1_rx = 0;
	}

	/* receiver used */
	if (f0_rx > 0 && am791x->f0_rx == 0) {
		LOGP(DDSP, LOGL_DEBUG, "Setting demodulator to %s channel's frequencies (F0 = %d, F1 = %d), baudrate %.0f\n", name_rx, f0_rx, f1_rx, am791x->rx_baud);
		if (fsk_demod_init(&am791x->fsk_rx, am791x, fsk_receive_bit, am791x->samplerate, am791x->rx_baud, (double)f0_rx, (double)f1_rx, BIT_ADJUST) < 0)
			LOGP(DDSP, LOGL_ERROR, "FSK RX init failed!\n");
		else {
			am791x->f0_rx = f0_rx;
			am791x->f1_rx = f1_rx;
		}
	}
}

/* new state */
static void new_tx_state(am791x_t *am791x, enum am791x_st state)
{
	if (am791x->tx_state != state)
		LOGP(DAM791X, LOGL_DEBUG, "Change TX state %s -> %s\n", am791x_state_names[am791x->tx_state], am791x_state_names[state]);
	am791x->tx_state = state;
}

static void new_rx_state(am791x_t *am791x, enum am791x_st state)
{
	if (am791x->rx_state != state)
		LOGP(DAM791X, LOGL_DEBUG, "Change RX state %s -> %s\n", am791x_state_names[am791x->rx_state], am791x_state_names[state]);
	am791x->rx_state = state;
}

/* new flags */
static void set_flag(int *flag_p, int value, const char *name)
{
	if (*flag_p != value) {
		LOGP(DAM791X, LOGL_DEBUG, " -> %s\n", name);
		*flag_p = value;
	}
}

/*
 * state machine according to datasheet
 */
static void go_main_channel_tx(am791x_t *am791x)
{
	LOGP(DAM791X, LOGL_DEBUG, "Enable transmitter on main channel\n");

	/* only block RD, if not full duplex and not 4-wire (loopback mode) */
	if (!am791x->fullduplex && !am791x->loopback_main) {
		set_flag(&am791x->block_rd, 1, "RD = MARK");
		set_flag(&am791x->block_cd, 1, "SET CD HIGH");
	}

	/* activate TD now and set CTS timer (RCON) */
	set_flag(&am791x->block_td, 0, "TD RELEASED");
	set_flag(&am791x->tx_silence, 0, "RESET SILENCE");
	/* Flag timer, because it must be added in main thread. */
	am791x->tx_timer_f = am791x->t_rcon;
	new_tx_state(am791x, AM791X_STATE_RCON);
	set_filters(am791x);
	/* check CD to be blocked */
	if (!am791x->fullduplex && !am791x->loopback_main) {
		if (am791x->line_cd) {
			/* send CD off */
			am791x->cd_cb(am791x->inst, 0);
		}
	}
}

static void rcon_release_rts(am791x_t *am791x)
{
	LOGP(DAM791X, LOGL_DEBUG, "RTS was released\n");

	set_flag(&am791x->block_td, 1, "TD IGNORED");
	set_flag(&am791x->tx_silence, 1, "SET SILENCE");
	set_flag(&am791x->block_cd, 0, "RELEASE CD");
	new_tx_state(am791x, AM791X_STATE_INIT);
	set_filters(am791x);
	/* check CD to be released */
	if (am791x->line_cd) {
		/* send CD on */
		am791x->cd_cb(am791x->inst, 1);
	}
}

static void rcon_done(am791x_t *am791x)
{
	LOGP(DAM791X, LOGL_DEBUG, "Transmission started\n");

	new_tx_state(am791x, AM791X_STATE_DATA);
	/* CTS on */
	am791x->cts_cb(am791x->inst, 1);
}

static void tx_data_done(am791x_t *am791x)
{
	LOGP(DAM791X, LOGL_DEBUG, "RTS was released\n");

	new_tx_state(am791x, AM791X_STATE_RCOFF);
	set_flag(&am791x->block_td, 1, "TD IGNORED");
	if (am791x->sto) {
		set_flag(&am791x->tx_sto, 1, "start STO");
	} else {
		set_flag(&am791x->tx_silence, 1, "SET SILENCE (if not STO)");
	}
	if (!am791x->fullduplex) {
		set_flag(&am791x->squelch, 1, "SET SQUELCH (ON)");
	}
	/* Flag timer, because it must be added in main thread. */
	am791x->tx_timer_f = am791x->t_rcoff;
}

static void rcoff_done(am791x_t *am791x)
{
	LOGP(DAM791X, LOGL_DEBUG, "Transmission over\n");

	/* CTS off */
	am791x->cts_cb(am791x->inst, 0);
	if (am791x->fullduplex) {
		new_tx_state(am791x, AM791X_STATE_INIT);
		set_filters(am791x);
		return;
	}
	if (!am791x->sto) {
		/* Flag timer, because it must be added in main thread. */
		am791x->tx_timer_f = am791x->t_sq - am791x->t_rcoff;
		new_tx_state(am791x, AM791X_STATE_SQ_OFF);
		return;
	}
	/* Flag timer, because it must be added in main thread. */
	am791x->tx_timer_f = am791x->t_sto - am791x->t_rcoff;
	new_tx_state(am791x, AM791X_STATE_STO_OFF);
}

static void sq_done(am791x_t *am791x)
{
	LOGP(DAM791X, LOGL_DEBUG, "Squelch over\n");

	set_flag(&am791x->block_cd, 0, "CD RELEASED");
	new_tx_state(am791x, AM791X_STATE_INIT);
	set_filters(am791x);
	/* SET SQUELCH OFF */
	set_flag(&am791x->squelch, 0, "SET SQUELCH OFF");
	if (am791x->line_cd) {
		/* send CD on */
		am791x->cd_cb(am791x->inst, 1);
	}
}

static void sto_done(am791x_t *am791x)
{
	LOGP(DAM791X, LOGL_DEBUG, "STO over\n");

	set_flag(&am791x->tx_sto, 0, "stop STO");
	/* Flag timer, because it must be added in main thread. */
	am791x->tx_timer_f = am791x->t_sq - am791x->t_sto;
	new_tx_state(am791x, AM791X_STATE_SQ_OFF);
}

static void go_back_channel_tx(am791x_t *am791x)
{
	LOGP(DAM791X, LOGL_DEBUG, "Enable transmitter on back channel\n");

	if (!am791x->loopback_back) {
		set_flag(&am791x->block_brd, 1, "BRD = MARK");
		set_flag(&am791x->block_bcd, 1, "SET BCD HIGH");
	}

	/* activate BTD now and set BCTS timer (BRCON) */
	set_flag(&am791x->block_btd, 0, "BTD RELEASED");
	set_flag(&am791x->tx_silence, 0, "RESET SILENCE");
	/* Flag timer, because it must be added in main thread. */
	am791x->tx_timer_f = am791x->t_brcon;
	new_tx_state(am791x, AM791X_STATE_BRCON);
	set_filters(am791x);
	/* check BCD to be blocked */
	if (!am791x->loopback_back) {
		if (am791x->line_bcd) {
			/* send BCD off */
			am791x->bcd_cb(am791x->inst, 0);
		}
	}
}

static void brcon_release_brts(am791x_t *am791x)
{
	LOGP(DAM791X, LOGL_DEBUG, "BRTS was released\n");

	set_flag(&am791x->tx_silence, 1, "SET SILENCE");
	new_tx_state(am791x, AM791X_STATE_INIT);
	set_filters(am791x);
}

static void brcon_done(am791x_t *am791x)
{
	LOGP(DAM791X, LOGL_DEBUG, "Transmission started\n");

	new_tx_state(am791x, AM791X_STATE_BDATA);
	/* BCTS on */
	am791x->bcts_cb(am791x->inst, 1);
}

static void tx_bdata_done(am791x_t *am791x)
{
	LOGP(DAM791X, LOGL_DEBUG, "BRTS was released\n");

	set_flag(&am791x->block_btd, 1, "BTD IGNORED");
	set_flag(&am791x->tx_silence, 1, "SET SILENCE");
	/* Flag timer, because it must be added in main thread. */
	am791x->tx_timer_f = am791x->t_brcoff;
}

static void brcoff_done(am791x_t *am791x)
{
	LOGP(DAM791X, LOGL_DEBUG, "Transmission over\n");

	/* BCTS off */
	am791x->bcts_cb(am791x->inst, 0);
	set_flag(&am791x->block_bcd, 0, "BCD RELEASED");
	new_tx_state(am791x, AM791X_STATE_INIT);
	set_filters(am791x);
	/* check BCD to be released */
	if (am791x->line_bcd) {
		/* send BCD on */
		am791x->bcd_cb(am791x->inst, 1);
	}
}

static void handle_tx_state(am791x_t *am791x)
{
	switch (am791x->tx_state) {
	/* depending on the states we change to "main TX " or to "back TX" state */
	case AM791X_STATE_INIT:
		/* select TX on main or back channel, according to states */
		if (am791x->line_brts) {
			if (am791x->loopback_back) {
				go_back_channel_tx(am791x);
				break;
			}
			if (!am791x->line_rts && !am791x->loopback_main && !am791x->fullduplex) {
				go_back_channel_tx(am791x);
				break;
			}
		}
		if (!am791x->loopback_back && am791x->line_rts) {
			go_main_channel_tx(am791x);
			break;
		}
		break;
	/* all main channel states ... */
	case AM791X_STATE_RCON:
		if (!am791x->line_rts) {
			rcon_release_rts(am791x);
			break;
		}
		/* If timer is about to be switched off, of if it already has been switched off. */
		if (am791x->tx_timer_f < 0.0 || (!osmo_timer_pending(&am791x->tx_timer) && am791x->tx_timer_f == 0.0)) {
			rcon_done(am791x);
			break;
		}
		break;
	case AM791X_STATE_DATA:
		if (!am791x->line_rts) {
			tx_data_done(am791x);
			break;
		}
		break;
	case AM791X_STATE_RCOFF:
		/* If timer is about to be switched off, of if it already has been switched off. */
		if (am791x->tx_timer_f < 0.0 || (!osmo_timer_pending(&am791x->tx_timer) && am791x->tx_timer_f == 0.0)) {
			rcoff_done(am791x);
			break;
		}
		break;
	case AM791X_STATE_STO_OFF:
		/* If timer is about to be switched off, of if it already has been switched off. */
		if (am791x->tx_timer_f < 0.0 || (!osmo_timer_pending(&am791x->tx_timer) && am791x->tx_timer_f == 0.0)) {
			sto_done(am791x);
			break;
		}
		break;
	case AM791X_STATE_SQ_OFF:
		/* If timer is about to be switched off, of if it already has been switched off. */
		if (am791x->tx_timer_f < 0.0 || (!osmo_timer_pending(&am791x->tx_timer) && am791x->tx_timer_f == 0.0)) {
			sq_done(am791x);
			break;
		}
		break;
	/* all back channel states */
	case AM791X_STATE_BRCON:
		if (!am791x->line_brts) {
			brcon_release_brts(am791x);
			break;
		}
		/* If timer is about to be switched off, of if it already has been switched off. */
		if (am791x->tx_timer_f < 0.0 || (!osmo_timer_pending(&am791x->tx_timer) && am791x->tx_timer_f == 0.0)) {
			brcon_done(am791x);
			break;
		}
		break;
	case AM791X_STATE_BDATA:
		if (!am791x->line_brts) {
			tx_bdata_done(am791x);
			break;
		}
		break;
	case AM791X_STATE_BRCOFF:
		/* If timer is about to be switched off, of if it already has been switched off. */
		if (am791x->tx_timer_f < 0.0 || (!osmo_timer_pending(&am791x->tx_timer) && am791x->tx_timer_f == 0.0)) {
			brcoff_done(am791x);
			break;
		}
		break;
	default:
		LOGP(DAM791X, LOGL_ERROR, "State %s not handled!\n", am791x_state_names[am791x->rx_state]);
	}
}

static void go_main_channel_rx(am791x_t *am791x)
{
	LOGP(DAM791X, LOGL_DEBUG, "Enable receiver on main channel\n");

	/* Flag timer, because it must be added in main thread. */
	am791x->rx_timer_f = am791x->t_cdon;
	new_rx_state(am791x, AM791X_STATE_CDON);
}

static void cdon_done(am791x_t *am791x)
{
	LOGP(DAM791X, LOGL_DEBUG, "Reception started\n");

	set_flag(&am791x->block_rd, 0, "RD RELEASED");
	new_rx_state(am791x, AM791X_STATE_DATA);
	set_flag(&am791x->line_cd, 1, "set CD");
	/* check CD not blocked */
	if (!am791x->block_cd) {
		/* send CD on */
		am791x->cd_cb(am791x->inst, 1);
	}
}

static void cdon_no_cd(am791x_t *am791x)
{
	LOGP(DAM791X, LOGL_DEBUG, "Carrier is gone\n");

	/* Flag timer, because it must be deleted in main thread. */
	am791x->rx_timer_f = -1;
	new_rx_state(am791x, AM791X_STATE_INIT);
}

static void rx_data_done(am791x_t *am791x)
{
	LOGP(DAM791X, LOGL_DEBUG, "Carrier lost\n");

	/* Flag timer, because it must be added in main thread. */
	am791x->rx_timer_f = am791x->t_cdoff;
	new_rx_state(am791x, AM791X_STATE_CDOFF);
}

static void cdoff_done(am791x_t *am791x)
{
	LOGP(DAM791X, LOGL_DEBUG, "Reception finished\n");

	set_flag(&am791x->block_rd, 1, "RD = MARK");
	new_rx_state(am791x, AM791X_STATE_INIT);
	set_flag(&am791x->line_cd, 0, "release CD");
	/* check CD not blocked */
	if (!am791x->block_cd) {
		/* send CD off */
		am791x->cd_cb(am791x->inst, 0);
	}
}

static void cdoff_cd(am791x_t *am791x)
{
	LOGP(DAM791X, LOGL_DEBUG, "Carrier recovered\n");

	/* Flag timer, because it must be deleted in main thread. */
	am791x->rx_timer_f = -1;
	new_rx_state(am791x, AM791X_STATE_DATA);
}

static void go_back_channel_rx(am791x_t *am791x)
{
	LOGP(DAM791X, LOGL_DEBUG, "Enable receiver on back channel\n");

	/* Flag timer, because it must be added in main thread. */
	am791x->rx_timer_f = am791x->t_bcdon;
	new_rx_state(am791x, AM791X_STATE_BCDON);
}

static void bcdon_done(am791x_t *am791x)
{
	LOGP(DAM791X, LOGL_DEBUG, "Carrier was detected\n");

	set_flag(&am791x->block_brd, 0, "BRD RELEASED");
	new_rx_state(am791x, AM791X_STATE_BDATA);
	set_flag(&am791x->line_bcd, 1, "set BCD");
	/* check BCD not blocked */
	if (!am791x->block_bcd) {
		/* send BCD on */
		am791x->bcd_cb(am791x->inst, 1);
	}
}

static void bcdon_no_cd(am791x_t *am791x)
{
	LOGP(DAM791X, LOGL_DEBUG, "Carrier is gone\n");

	/* Flag timer, because it must be deleted in main thread. */
	am791x->rx_timer_f = -1;
	new_rx_state(am791x, AM791X_STATE_INIT);
}

static void rx_bdata_done(am791x_t *am791x)
{
	LOGP(DAM791X, LOGL_DEBUG, "Carrier lost\n");

	/* Flag timer, because it must be added in main thread. */
	am791x->rx_timer_f = am791x->t_bcdoff;
	new_rx_state(am791x, AM791X_STATE_BCDOFF);
}

static void bcdoff_done(am791x_t *am791x)
{
	LOGP(DAM791X, LOGL_DEBUG, "Reception finished\n");

	if (!am791x->bell_202)
		set_flag(&am791x->block_brd, 1, "BRD = MARK");
	new_rx_state(am791x, AM791X_STATE_INIT);
	set_flag(&am791x->line_bcd, 0, "release BCD");
	/* check BCD not blocked */
	if (!am791x->block_bcd) {
		/* send BCD off */
		am791x->bcd_cb(am791x->inst, 0);
	}
}

static void bcdoff_cd(am791x_t *am791x)
{
	LOGP(DAM791X, LOGL_DEBUG, "Carrier recovered\n");

	/* Flag timer, because it must be deleted in main thread. */
	am791x->rx_timer_f = -1;
	new_rx_state(am791x, AM791X_STATE_BDATA);
}

static void handle_rx_state(am791x_t *am791x)
{
	switch (am791x->rx_state) {
	case AM791X_STATE_INIT:
		/* select RX on main or back channel, according to states */
		if (!am791x->loopback_back) {
			if (am791x->cd) {
				go_main_channel_rx(am791x);
				break;
			}
			if (!am791x->loopback_main && !am791x->fullduplex && am791x->bcd) {
				go_back_channel_rx(am791x);
				break;
			}
		} else {
			if (am791x->bcd) {
				go_back_channel_rx(am791x);
				break;
			}
		}
		break;
	/* all main channel states ... */
	case AM791X_STATE_CDON:
		/* If timer is about to be switched off, of if it already has been switched off. */
		if (am791x->rx_timer_f < 0.0 || (!osmo_timer_pending(&am791x->rx_timer) && am791x->rx_timer_f == 0.0)) {
			cdon_done(am791x);
			break;
		}
		if (!am791x->cd) {
			cdon_no_cd(am791x);
			break;
		}
		break;
	case AM791X_STATE_DATA:
		if (!am791x->cd) {
			rx_data_done(am791x);
			break;
		}
		break;
	case AM791X_STATE_CDOFF:
		/* If timer is about to be switched off, of if it already has been switched off. */
		if (am791x->rx_timer_f < 0.0 || (!osmo_timer_pending(&am791x->rx_timer) && am791x->rx_timer_f == 0.0)) {
			cdoff_done(am791x);
			break;
		}
		if (am791x->cd) {
			cdoff_cd(am791x);
			break;
		}
		break;
	/* all back channel states ... */
	case AM791X_STATE_BCDON:
		/* If timer is about to be switched off, of if it already has been switched off. */
		if (am791x->rx_timer_f < 0.0 || (!osmo_timer_pending(&am791x->rx_timer) && am791x->rx_timer_f == 0.0)) {
			bcdon_done(am791x);
			break;
		}
		if (!am791x->bcd) {
			bcdon_no_cd(am791x);
			break;
		}
		break;
	case AM791X_STATE_BDATA:
		if (!am791x->bcd) {
			rx_bdata_done(am791x);
			break;
		}
		break;
	case AM791X_STATE_BCDOFF:
		/* If timer is about to be switched off, of if it already has been switched off. */
		if (am791x->rx_timer_f < 0.0 || (!osmo_timer_pending(&am791x->rx_timer) && am791x->rx_timer_f == 0.0)) {
			bcdoff_done(am791x);
			break;
		}
		if (am791x->bcd) {
			bcdoff_cd(am791x);
			break;
		}
		break;
	default:
		LOGP(DAM791X, LOGL_ERROR, "State %s not handled!\n", am791x_state_names[am791x->rx_state]);
	}
}

/* handle both (rx and tx) states */
static void handle_state(am791x_t *am791x)
{
	/* DTR blocks all */
	if (!am791x->line_dtr) {
		/* do reset of all states, if not in INIT state */
		if (am791x->tx_state != AM791X_STATE_INIT || am791x->rx_state != AM791X_STATE_INIT)
			am791x_reset(am791x);
		return;
	}

	/* handle states if DTR is on */
	handle_tx_state(am791x);
	handle_rx_state(am791x);
}

/* timeout events */
static void tx_timeout(void *data)
{
	am791x_t *am791x = data;

	handle_tx_state(am791x);
}

static void rx_timeout(void *data)
{
	am791x_t *am791x = data;

	handle_rx_state(am791x);
}

/* init routine, must be called before anything else */
int am791x_init(am791x_t *am791x, void *inst, enum am791x_type type, uint8_t mc, int samplerate, double tx_baud, double rx_baud, void (*cts)(void *inst, int cts), void (*bcts)(void *inst, int cts), void (*cd)(void *inst, int cd), void (*bcd)(void *inst, int cd), int (*td)(void *inst), int (*btd)(void *inst), void (*rd)(void *inst, int bit, double quality, double level), void (*brd)(void *inst, int bit, double quality, double level))
{
	memset(am791x, 0, sizeof(*am791x));

	/* init timers */
	osmo_timer_setup(&am791x->tx_timer, tx_timeout, am791x);
	osmo_timer_setup(&am791x->rx_timer, rx_timeout, am791x);

	LOGP(DAM791X, LOGL_DEBUG, "Initializing instance of AM791%d:\n", type);

	am791x->inst = inst;
	am791x->type = type;
	am791x->samplerate = samplerate;
	am791x->cts_cb = cts;
	am791x->bcts_cb = bcts;
	am791x->cd_cb = cd;
	am791x->bcd_cb = bcd;
	am791x->td_cb = td;
	am791x->btd_cb = btd;
	am791x->rd_cb = rd;
	am791x->brd_cb = brd;

	/* levels */
	am791x->tx_level = db2level(TX_LEVEL);
	am791x->cd_on = db2level((am791x->type) ? RX_CD_ON_7911 : RX_CD_ON_7910);
	am791x->cd_off = db2level((am791x->type) ? RX_CD_OFF_7911 : RX_CD_OFF_7910);

	init_sto(am791x);

	/* set initial mode and reset */
	am791x_mc(am791x, mc, samplerate, tx_baud, rx_baud);

	return 0;
}

/* exit routine, must be called when exit */
void am791x_exit(am791x_t *am791x)
{
	LOGP(DAM791X, LOGL_DEBUG, "Exit instance of AM791%d:\n", am791x->type);

	/* bring to reset state, be sure to clean FSK processes */
	am791x_reset(am791x);

	osmo_timer_del(&am791x->tx_timer);
	osmo_timer_del(&am791x->rx_timer);
}

/* get some default baud rate for each mode, before IOCTL sets it (if it sets it) */
double am791x_max_baud(uint8_t mc)
{
	if (am791x_modes[mc].max_baud)
		return am791x_modes[mc].max_baud;
	else
		return 300; /* useless modem. just report something to the caller */
}

/* change mode on the fly, may be called any time by upper layer */
int am791x_mc(am791x_t *am791x, uint8_t mc, int samplerate, double tx_baud, double rx_baud)
{
	int rc = 0;

	/* prevent out of range value */
	if (mc >= 32)
		mc = 0;

	if (!((am791x->type) ? am791x_modes[mc].sup_7911 : am791x_modes[mc].sup_7910))
		rc = -EINVAL;

	LOGP(DAM791X, LOGL_INFO, "Setting mode %d: %s\n", mc, am791x_modes[mc].description);
	LOGP(DAM791X, LOGL_DEBUG, " -> Baud rate: %.1f/%.1f\n", rx_baud, tx_baud);

	am791x->mc = mc;
	am791x->samplerate = samplerate;
	am791x->tx_baud = tx_baud;
	am791x->rx_baud = rx_baud;

	am791x->t_rcon = (am791x->type) ? am791x_modes[mc].t_rcon_7911 : am791x_modes[mc].t_rcon_7910;
	am791x->t_rcoff = (am791x->type) ? am791x_modes[mc].t_rcoff_7911 : am791x_modes[mc].t_rcoff_7910;
	am791x->t_brcon = (am791x->type) ? am791x_modes[mc].t_brcon_7911 : am791x_modes[mc].t_brcon_7910;
	am791x->t_brcoff = (am791x->type) ? am791x_modes[mc].t_brcoff_7911 : am791x_modes[mc].t_brcoff_7910;
	am791x->t_cdon = (am791x->type) ? am791x_modes[mc].t_cdon_7911 : am791x_modes[mc].t_cdon_7910;
	am791x->t_cdoff = (am791x->type) ? am791x_modes[mc].t_cdoff_7911 : am791x_modes[mc].t_cdoff_7910;
	am791x->t_bcdon = (am791x->type) ? am791x_modes[mc].t_bcdon_7911 : am791x_modes[mc].t_bcdon_7910;
	am791x->t_bcdoff = (am791x->type) ? am791x_modes[mc].t_bcdoff_7911 : am791x_modes[mc].t_bcdoff_7910;
	am791x->t_at = (am791x->type) ? am791x_modes[mc].t_at_7911 : am791x_modes[mc].t_at_7910;
	am791x->t_sil1 = (am791x->type) ? am791x_modes[mc].t_sil1_7911 : am791x_modes[mc].t_sil1_7910;
	am791x->t_sil2 = (am791x->type) ? am791x_modes[mc].t_sil2_7911 : am791x_modes[mc].t_sil2_7910;
	am791x->t_sq = (am791x->type) ? am791x_modes[mc].t_sq_7911 : am791x_modes[mc].t_sq_7910;
	am791x->t_sto = (am791x->type) ? am791x_modes[mc].t_sto_7911 : am791x_modes[mc].t_sto_7910;
	am791x->fullduplex = am791x_modes[mc].fullduplex;
	am791x->loopback_main = am791x_modes[mc].loopback_main;
	am791x->loopback_back = am791x_modes[mc].loopback_back;
	am791x->equalizer = am791x_modes[mc].equalizer;
	am791x->sto = am791x_modes[mc].sto;
	am791x->bell_202 = am791x_modes[mc].bell_202;

	/* changing mode causes a reset */
	am791x_reset(am791x);

	/* Return error on invalid mode. The emualtion still works, but no TX/RX possible. */
	return rc;
}

/* reset at any time, may be called any time by upper layer */
void am791x_reset(am791x_t *am791x)
{
	LOGP(DAM791X, LOGL_INFO, "Reset!\n");

	/* Flag timer, because it must be deleted in main thread. */
	am791x->tx_timer_f = -1.0;
	/* Flag timer, because it must be deleted in main thread. */
	am791x->rx_timer_f = -1.0;

	if (am791x->f0_tx) {
		fsk_mod_cleanup(&am791x->fsk_tx);
		am791x->f0_tx = 0;
		am791x->f1_tx = 0;
	}
	if (am791x->f0_rx) {
		fsk_demod_cleanup(&am791x->fsk_rx);
		am791x->f0_rx = 0;
		am791x->f1_rx = 0;
	}

	/* initial states */
	am791x->tx_state = AM791X_STATE_INIT;
	am791x->rx_state = AM791X_STATE_INIT;
	am791x->tx_silence = 1; /* state machine implies that silence is sent */
	am791x->tx_sto = 0; /* state machine implies that STO is not sent */
	am791x->block_td = 1; /* state machine implies that TD is MARK (1) */
	am791x->block_rd = 1; /* state machine implies that RD is ignored */
	am791x->line_cd = 0;
	am791x->block_cd = 0; /* state machine implies that CD is released */
	am791x->block_btd = 1; /* state machine implies that BTD is MARK (1) */
	am791x->block_brd = 1; /* state machine implies that BTD is ignored */
	am791x->line_bcd = 0;
	am791x->block_bcd = 0; /* state machine implies that BCD is released */
	am791x->squelch = 0; /* state machine implies that squelch is off */

	/* set filters, if DTR is on */
	if (am791x->line_dtr)
		set_filters(am791x);

	handle_state(am791x);
}

/* change input lines */
void am791x_dtr(am791x_t *am791x, int dtr)
{
	LOGP(DAM791X, LOGL_DEBUG, "Terminal is%s ready!\n", (dtr) ? "" : " not");

	/* set filters, if DTR becomes on */
	if (!am791x->line_dtr && dtr) {
		am791x->line_dtr = dtr;
		set_filters(am791x);
	} else
		am791x->line_dtr = dtr;

	handle_state(am791x);
}

void am791x_rts(am791x_t *am791x, int rts)
{
	LOGP(DAM791X, LOGL_DEBUG, "Terminal %s RTS.\n", (rts) ? "sets" : "clears");

	am791x->line_rts = rts;
	handle_state(am791x);
}

void am791x_brts(am791x_t *am791x, int rts)
{
	LOGP(DAM791X, LOGL_DEBUG, "Terminal %s BRTS.\n", (rts) ? "sets" : "clears");

	am791x->line_brts = rts;
	handle_state(am791x);
}

void am791x_ring(am791x_t *am791x, int ring)
{
	LOGP(DAM791X, LOGL_DEBUG, "Terminal %s RING.\n", (ring) ? "sets" : "clears");

	am791x->line_ring = ring;
	handle_state(am791x);
}

void am791x_add_del_timers(am791x_t *am791x)
{
	/* Timers may only be processed in main thread, because libosmocore has timer lists for individual threads. */
	if (am791x->tx_timer_f < 0.0)
		osmo_timer_del(&am791x->tx_timer);
	if (am791x->tx_timer_f > 0.0)
		osmo_timer_schedule(&am791x->tx_timer, FLOAT_TO_TIMEOUT(am791x->tx_timer_f));
	am791x->tx_timer_f = 0.0;

	if (am791x->rx_timer_f < 0.0)
		osmo_timer_del(&am791x->rx_timer);
	if (am791x->rx_timer_f > 0.0)
		osmo_timer_schedule(&am791x->rx_timer, FLOAT_TO_TIMEOUT(am791x->rx_timer_f));
	am791x->rx_timer_f = 0.0;
}

