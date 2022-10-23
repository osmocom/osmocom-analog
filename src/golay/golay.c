/* Golay/GSC transcoding (encoding only - maybe)
 *
 * (C) 2022 by Andreas Eversberg <jolly@eversberg.eu>
 * All Rights Reserved
 *
 * Inspired by GSC code written by Brandon Creighton <cstone@pobox.com>.
 *
 * Inspired by GOLAY code written by Robert Morelos-Zaragoza
 * <robert@spectra.eng.hawaii.edu>.
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

/* Golay code was is use since 1973, the GSC extension was used after 1982.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include "../libsample/sample.h"
#include "../libdebug/debug.h"
#include "../libmobile/call.h"
#include "../libmobile/main_mobile.h"
#include "../libmobile/cause.h"
#include "golay.h"
#include "dsp.h"

/* Create transceiver instance and link to a list. */
int golay_create(const char *kanal, double frequency, const char *device, int use_sdr, int samplerate, double rx_gain, double tx_gain, double deviation, double polarity, const char *message, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, const char *read_tx_wave, int loopback)
{
	gsc_t *gsc;
	int rc;

	gsc = calloc(1, sizeof(*gsc));
	if (!gsc) {
		PDEBUG(DGOLAY, DEBUG_ERROR, "No memory!\n");
		return -ENOMEM;
	}

	PDEBUG(DGOLAY, DEBUG_DEBUG, "Creating 'GOLAY' instance for frequency = %s (sample rate %d).\n", kanal, samplerate);

	/* init general part of transceiver */
	rc = sender_create(&gsc->sender, kanal, frequency, frequency, device, use_sdr, samplerate, rx_gain, tx_gain, 0, 0, write_rx_wave, write_tx_wave, read_rx_wave, read_tx_wave, loopback, PAGING_SIGNAL_NONE);
	if (rc < 0) {
		PDEBUG(DGOLAY, DEBUG_ERROR, "Failed to init transceiver process!\n");
		goto error;
	}

	/* init audio processing */
	rc = dsp_init_sender(gsc, samplerate, deviation, polarity);
	if (rc < 0) {
		PDEBUG(DGOLAY, DEBUG_ERROR, "Failed to init audio processing!\n");
		goto error;
	}

	gsc->tx = 1;
	gsc->default_message = message;

	PDEBUG(DGOLAY, DEBUG_NOTICE, "Created transmitter for frequency %s\n", kanal);

	return 0;

error:
	golay_destroy(&gsc->sender);

	return rc;
}

static void golay_msg_destroy(gsc_t *gsc, gsc_msg_t *msg);

/* Destroy transceiver instance and unlink from list. */
void golay_destroy(sender_t *sender)
{
	gsc_t *gsc = (gsc_t *) sender;

	PDEBUG(DGOLAY, DEBUG_DEBUG, "Destroying 'GOLAY' instance for frequency = %s.\n", sender->kanal);

	while (gsc->msg_list)
		golay_msg_destroy(gsc, gsc->msg_list);
	dsp_cleanup_sender(gsc);
	sender_destroy(&gsc->sender);
	free(gsc);
}

/* Create message and add to queue */
static gsc_msg_t *golay_msg_create(gsc_t *gsc, const char *address, const char *text, int force_type)
{
	gsc_msg_t *msg, **msgp;

	PDEBUG(DGOLAY, DEBUG_INFO, "Creating msg instance to page address '%s'.\n", address);

	/* create */
	msg = calloc(1, sizeof(*msg));
	if (!msg) {
		PDEBUG(DGOLAY, DEBUG_ERROR, "No mem!\n");
		abort();
	}
	if (strlen(address) != sizeof(msg->address) - 1) {
		PDEBUG(DGOLAY, DEBUG_NOTICE, "Address has incorrect length, cannot page!\n");
		return NULL;
	}
	if (strlen(text) > sizeof(msg->data) - 1) {
		PDEBUG(DGOLAY, DEBUG_NOTICE, "Given test is too long, cannot page!\n");
		return NULL;
	}

	/* init */
	strcpy(msg->address, address);
	msg->force_type = force_type;
	strcpy(msg->data, text);

	/* link */
	msgp = &gsc->msg_list;
	while ((*msgp))
		msgp = &(*msgp)->next;
	(*msgp) = msg;

	return msg;
}

/* Remove and destroy msg from queue */
static void golay_msg_destroy(gsc_t *gsc, gsc_msg_t *msg)
{
	gsc_msg_t **msgp;

	/* unlink */
	msgp = &gsc->msg_list;
	while ((*msgp) != msg)
		msgp = &(*msgp)->next;
	(*msgp) = msg->next;

	/* destroy */
	free(msg);
}

/* uncomment this for showing encoder tables ("<parity> <information>", LSB is the right most bit) */
//#define DEBUG_TABLE

static uint32_t golay_table[4096];

#define X22	0x00400000
#define X11	0x00000800
#define MASK12	0xfffff800
#define GEN_GOL	0x00000c75

/* generate golay encoding table. the redundancy is shifted 12 bits */
void init_golay(void)
{
	uint32_t syndrome, aux;
	int data;

	for (data = 0; data < 4096; data++) {
		syndrome = data << 11;
		/* calculate syndrome */
		aux = X22;
		if (syndrome >= X11) {
			while (syndrome & MASK12) {
				while (!(aux & syndrome))
					aux = aux >> 1;
				syndrome ^= (aux / X11) * GEN_GOL;
			}
		}
		golay_table[data] = data | (syndrome << 12);
#ifdef DEBUG_TABLE
		printf("Golay %4d: ", data);
		for (int i = 22; i >= 0; i--) {
			if (i == 11)
				printf(" ");
			printf("%d", (golay_table[data] >> i) & 1);
		}
		printf("\n");
#endif
	}
}

static uint16_t bch_table[128];

#define X14	0x4000
#define X8	0x0100
#define MASK7	0xff00
#define GEN_BCH	0x00000117

/* generate bch encoding table. the redundancy is shifted 7 bits */
void init_bch(void)
{
	uint16_t syndrome, aux;
	int data;

	for (data = 0; data < 128; data++) {
		syndrome = data << 8;
		/* calculate syndrome */
		aux = X14;
		if (syndrome >= X8) {
			while (syndrome & MASK7) {
				while (!(aux & syndrome))
					aux = aux >> 1;
				syndrome ^= (aux / X8) * GEN_BCH;
			}
		}
		bch_table[data] = data | (syndrome << 7);
#ifdef DEBUG_TABLE
		printf("BCH %3d: ", data);
		for (int i = 14; i >= 0; i--) {
			if (i == 6)
				printf(" ");
			printf("%d", (bch_table[data] >> i) & 1);
		}
		printf("\n");
#endif
	}
}

static inline uint32_t calc_golay(uint16_t data)
{
	return golay_table[data & 0xfff];
}

static inline uint16_t calc_bch(uint16_t data)
{
	return bch_table[data & 0x7f];
}

static const uint16_t preamble_values[] = {
	2030, 1628, 3198,  647,  191, 3315, 1949, 2540, 1560, 2335,
};

static const uint32_t start_code = 713;

/* Rep. 900-2 Table VI */
static const uint16_t word1s[50] = {
	 721, 2731, 2952, 1387, 1578, 1708, 2650, 1747, 2580, 1376,
	2692,  696, 1667, 3800, 3552, 3424, 1384, 3595,  876, 3124,
	2285, 2608,  899, 3684, 3129, 2124, 1287, 2616, 1647, 3216,
	 375, 1232, 2824, 1840,  408, 3127, 3387,  882, 3468, 3267,
	1575, 3463, 3152, 2572, 1252, 2592, 1552,  835, 1440,  160,
};

/* Rep. 900-2 Table VII (left column) */
static char encode_alpha(char c)
{
	if (c >= 'a' && c <= 'z')
		c = c - 'a' + 'A';
	switch (c) {
	case 0x0a:
	case 0x0d:
		PDEBUG(DGOLAY, DEBUG_DEBUG, " -> CR/LF character.\n");
		c = 0x3c;
		break;
	case '{':
		PDEBUG(DGOLAY, DEBUG_DEBUG, " -> '%c' character.\n", c);
		c = 0x3b;
		break;
	case '}':
		PDEBUG(DGOLAY, DEBUG_DEBUG, " -> '%c' character.\n", c);
		c = 0x3d;
		break;
	case '\\':
		PDEBUG(DGOLAY, DEBUG_DEBUG, " -> '%c' character.\n", c);
		c = 0x20;
		break;
	default:
		if (c < 0x20 || c > 0x5d) {
			PDEBUG(DGOLAY, DEBUG_DEBUG, " -> ' ' character.\n");
			c = 0x20;
		} else {
			PDEBUG(DGOLAY, DEBUG_DEBUG, " -> '%c' character.\n", c);
			c = c - 0x20;
		}
	}
	return c;
}

/* Rep. 900-2 Table VII (right columns) */
static char encode_numeric(char c)
{
	switch (c) {
	case 'u':
	case 'U':
		PDEBUG(DGOLAY, DEBUG_DEBUG, " -> 'U' character.\n");
		c = 0xb;
		break;
	case ' ':
		PDEBUG(DGOLAY, DEBUG_DEBUG, " -> '%c' character.\n", c);
		c = 0xc;
		break;
	case '-':
		PDEBUG(DGOLAY, DEBUG_DEBUG, " -> '%c' character.\n", c);
		c = 0xd;
		break;
	case '=':
	case '*':
		PDEBUG(DGOLAY, DEBUG_DEBUG, " -> '*' character.\n");
		c = 0xe;
		break;
	case 'a':
	case 'A':
		PDEBUG(DGOLAY, DEBUG_DEBUG, " -> 'A' character.\n");
		c = 0xf0;
		break;
	case 'b':
	case 'B':
		PDEBUG(DGOLAY, DEBUG_DEBUG, " -> 'B' character.\n");
		c = 0xf1;
		break;
	case 'c':
	case 'C':
		PDEBUG(DGOLAY, DEBUG_DEBUG, " -> 'C' character.\n");
		c = 0xf2;
		break;
	case 'd':
	case 'D':
		PDEBUG(DGOLAY, DEBUG_DEBUG, " -> 'D' character.\n");
		c = 0xf3;
		break;
	case 'e':
	case 'E':
		PDEBUG(DGOLAY, DEBUG_DEBUG, " -> 'E' character.\n");
		c = 0xf4;
		break;
	case 'f':
	case 'F':
		PDEBUG(DGOLAY, DEBUG_DEBUG, " -> 'F' character.\n");
		c = 0xf6;
		break;
	case 'g':
	case 'G':
		PDEBUG(DGOLAY, DEBUG_DEBUG, " -> 'G' character.\n");
		c = 0xf7;
		break;
	case 'h':
	case 'H':
		PDEBUG(DGOLAY, DEBUG_DEBUG, " -> 'H' character.\n");
		c = 0xf8;
		break;
	case 'j':
	case 'J':
		PDEBUG(DGOLAY, DEBUG_DEBUG, " -> 'J' character.\n");
		c = 0xf9;
		break;
	case 'l':
	case 'L':
		PDEBUG(DGOLAY, DEBUG_DEBUG, " -> 'L' character.\n");
		c = 0xfb;
		break;
	case 'n':
	case 'N':
		PDEBUG(DGOLAY, DEBUG_DEBUG, " -> 'N' character.\n");
		c = 0xfc;
		break;
	case 'p':
	case 'P':
		PDEBUG(DGOLAY, DEBUG_DEBUG, " -> 'P' character.\n");
		c = 0xfd;
		break;
	case 'r':
	case 'R':
		PDEBUG(DGOLAY, DEBUG_DEBUG, " -> 'r' character.\n");
		c = 0xfe;
		break;
	default:
		if (c >= '0' && c <= '9') {
			PDEBUG(DGOLAY, DEBUG_DEBUG, " -> '%c' character.\n", c);
			c = c - '0';
		} else {
			PDEBUG(DGOLAY, DEBUG_DEBUG, " -> ' ' character.\n");
			c = 0xc;
		}
	}
	return c;
}

static int encode_address(const char *code, int *preamble, uint16_t *word1, uint16_t *word2)
{
	static const uint16_t illegal_low[16] = {   0,  25,  51, 103, 206, 340, 363, 412, 445, 530, 642, 726, 782, 810, 825, 877 };
	static const uint16_t illegal_high[7] = {   0, 292, 425, 584, 631, 841, 851 };
	int idx, g0, g1, a0, a1, a2, ap0, ap, ap1, ap2, ap3, b1b0, b3b2, g1g0, a2a1a0;
	int i;

	for (i = 0; i < 7; i++) {
		if (code[i] < '0' || code[i] > '9')
			break;
	}
	if (code[i]) {
		PDEBUG(DGOLAY, DEBUG_NOTICE, "Invalid functional address character. Only 0..9 are allowed.\n");
		return -EINVAL;
	}

	idx = code[0] - '0';
	g1 = code[1] - '0';
	g0 = code[2] - '0';
	a2 = code[3] - '0';
	a1 = code[4] - '0';
	a0 = code[5] - '0';

	*preamble = (idx + g0) % 10;

	ap = a2 * 200 + a1 * 20 + a0 * 2;
	ap3 = ap / 1000;
	ap2 = (ap / 100) % 10;
	ap1 = (ap / 10) % 10;
	ap0 = ap % 10;

	b1b0 = (ap1 * 10 + ap0) / 2;
	b3b2 = (ap3 * 10 + ap2);

	g1g0 = (g1 * 10 + g0);
	if (g1g0 >= 50) {
		*word1 = word1s[g1g0 - 50];
		*word2 = b3b2 * 100 + b1b0 + 50;
	} else {
		*word1 = word1s[g1g0];
		*word2 = b3b2 * 100 + b1b0;
	}

	a2a1a0 = a2 * 100 + a1 * 10 + a0;
	if (g1g0 < 50) {
		for (i = 0; i < 16; i++) {
			if (a2a1a0 == illegal_low[i])
				break;
		}
		if (i < 16) {
			PDEBUG(DGOLAY, DEBUG_NOTICE, "Functional address has invlid value '%03d' for last three characters.\n", a2a1a0);
			return -EINVAL;
		}
	} else {
		for (i = 0; i < 7; i++) {
			if (a2a1a0 == illegal_high[i])
				break;
		}
		if (i < 7) {
			PDEBUG(DGOLAY, DEBUG_NOTICE, "Functional address has invlid value '%03d' for last three characters.\n", a2a1a0);
			return -EINVAL;
		}
	}

	return 0;
}

static inline void queue_reset(gsc_t *gsc)
{
	gsc->bit_index = 0;
	gsc->bit_num = 0;
	gsc->bit_overflow = 0;
}

static inline void queue_bit(gsc_t *gsc, int bit)
{
	if (gsc->bit_num == sizeof(gsc->bit))
		gsc->bit_overflow = 1;
	if (gsc->bit_overflow) {
		gsc->bit_num++;
		return;
	}
	gsc->bit[gsc->bit_num++] = bit;
}

static inline void queue_dup(gsc_t *gsc, uint32_t data, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		queue_bit(gsc, (data >> i) & 1);
		queue_bit(gsc, (data >> i) & 1);
	}
}

static inline void queue_comma(gsc_t *gsc, int bits, uint8_t polarity)
{
	int i;

	for (i = 0; i < bits; i++) {
		queue_bit(gsc, polarity);
		polarity = !polarity;
	}
}

static int queue_batch(gsc_t *gsc, const char *address, int force_type, const char *message)
{
	int type;
	int preamble;
	uint16_t word1, word2;
	uint8_t function;
	uint32_t golay;
	uint16_t bch[8];
	uint8_t msg[12], digit, shifted, contbit, checksum;
	int i, j, k;
	int rc;

	queue_reset(gsc);

	/* check address length */
	if (!address || strlen(address) != 7) {
		PDEBUG(DGOLAY, DEBUG_NOTICE, "Invalid functional address '%s' size. Only 7 digits are allowed.\n", address);
		return -EINVAL;
	}

	/* calculate address */
	rc = encode_address(address, &preamble, &word1, &word2);
	if (rc < 0)
		return rc;

	/* calculate function */
	switch (address[6]) {
		case '1': type = TYPE_VOICE; function = 0; break;
		case '2': type = TYPE_VOICE; function = 1; break;
		case '3': type = TYPE_VOICE; function = 2; break;
		case '4': type = TYPE_VOICE; function = 3; break;
		case '5': type = TYPE_ALPHA; function = 0; break;
		case '6': type = TYPE_ALPHA; function = 1; break;
		case '7': type = TYPE_ALPHA; function = 2; break;
		case '8': type = TYPE_ALPHA; function = 3; break;
		case '9': type = TYPE_TONE;  function = 0; break;
		case '0': type = TYPE_TONE;  function = 1; break;
		default:
			PDEBUG(DGOLAY, DEBUG_NOTICE, "Illegal function suffix '%c' in last address digit.\n", address[6]);
			return -EINVAL;
	}

	/* override type */
	if (force_type >= 0) {
		type = force_type;
		PDEBUG(DGOLAY, DEBUG_INFO, "Overriding message type as defined by sender.\n");
	}

	if (type == TYPE_ALPHA || type == TYPE_NUMERIC)
		PDEBUG(DGOLAY, DEBUG_INFO, "Coding text message for functional address '%s' and message '%s'.\n", address, message);
	else
		PDEBUG(DGOLAY, DEBUG_INFO, "Coding tone only message for functional address %s.\n", address);

	/* encode preamble and store */
	PDEBUG(DGOLAY, DEBUG_DEBUG, "Encoding preamble '%d'.\n", preamble);
	golay = calc_golay(preamble_values[preamble]);
	queue_comma(gsc, 28, golay & 1);
	for (i = 0; i < 18; i++) {
		queue_dup(gsc, golay, 23);
	}

	/* encode start code and store */
	PDEBUG(DGOLAY, DEBUG_DEBUG, "Encoding start code.\n");
	golay = calc_golay(start_code);
	queue_comma(gsc, 28, golay & 1);
	queue_dup(gsc, golay, 23);
	golay ^= 0x7fffff;
	queue_bit(gsc, (golay & 1) ^ 1);
	queue_dup(gsc, golay, 23);

	/* encode address and store */
	PDEBUG(DGOLAY, DEBUG_DEBUG, "Encoding address words '%d' and '%d'.\n", word1, word2);
	golay = calc_golay(word1);
	if (function & 0x2)
		golay ^= 0x7fffff;
	queue_comma(gsc, 28, golay & 1);
	queue_dup(gsc, golay, 23);
	golay = calc_golay(word2);
	if (function & 0x1)
		golay ^= 0x7fffff;
	queue_bit(gsc, (golay & 1) ^ 1);
	queue_dup(gsc, golay, 23);

	/* encode message */
	if (type == TYPE_ALPHA) {
		PDEBUG(DGOLAY, DEBUG_DEBUG, "Encoding %d alphanumeric digits.\n", (int)strlen(message));
		for (i = 0; *message; i++) {
			if (i == MAX_ADB) {
				PDEBUG(DGOLAY, DEBUG_NOTICE, "Message overflows %d characters, cropping message.\n", MAX_ADB * 8);
			}
			for (j = 0; *message && j < 8; j++) {
				msg[j] = encode_alpha(*message++);
			}
			/* fill empty characters with NULL */
			while (j < 8)
				msg[j++] = 0x3e;
			/* 8 characters + continue-bit */
			bch[0] = calc_bch((msg[0] | (msg[1] << 6)) & 0x7f);
			bch[1] = calc_bch(((msg[1] >> 1) | (msg[2] << 5)) & 0x7f);
			bch[2] = calc_bch(((msg[2] >> 2) | (msg[3] << 4)) & 0x7f);
			bch[3] = calc_bch(((msg[3] >> 3) | (msg[4] << 3)) & 0x7f);
			bch[4] = calc_bch(((msg[4] >> 4) | (msg[5] << 2)) & 0x7f);
			bch[5] = calc_bch(((msg[5] >> 5) | (msg[6] << 1)) & 0x7f);
			if (*message && i < MAX_ADB)
				contbit = 1;
			else
				contbit = 0;
			bch[6] = calc_bch((contbit << 6) | msg[7]);
			/* checksum */
			checksum = bch[0] + bch[1] + bch[2] + bch[3] + bch[4] + bch[5] + bch[6];
			bch[7] = calc_bch(checksum & 0x7f);
			/* store comma bit */
			queue_bit(gsc, (bch[0] & 1) ^ 1); // inverted first bit
			/* store interleaved bits */
			for (j = 0; j < 15; j++) {
				for (k = 0; k < 8; k++)
					queue_bit(gsc, (bch[k] >> j) & 1);
			}
		}
	}
	if (type == TYPE_NUMERIC) {
		PDEBUG(DGOLAY, DEBUG_DEBUG, "Encoding %d numeric digits.\n", (int)strlen(message));
		shifted = 0;
		for (i = 0; *message; i++) {
			if (i == MAX_NDB) {
				PDEBUG(DGOLAY, DEBUG_NOTICE, "Message overflows %d characters, cropping message.\n", MAX_NDB * 12);
			}
			for (j = 0; *message && j < 12; j++) {
				/* get next digit or shifted digit */
				if (shifted) {
					digit = shifted & 0xf;
					shifted = 0;
				} else
					digit = encode_numeric(*message);
				/* if digit is extended, use the shifted code and later the digit itself */
				if (digit > 0xf) {
					shifted = digit;
					msg[j] = digit >> 4;
				} else {
					msg[j] = digit;
					message++;
				}
			}
			/* fill empty digits with NULL */
			while (j < 12)
				msg[j++] = 0xa;
			/* 8 digits + continue-bit */
			bch[0] = calc_bch((msg[0] | (msg[1] << 4)) & 0x7f);
			bch[1] = calc_bch(((msg[1] >> 3) | (msg[2] << 1) | (msg[3] << 5)) & 0x7f);
			bch[2] = calc_bch(((msg[3] >> 2) | (msg[4] << 2) | (msg[5] << 6)) & 0x7f);
			bch[3] = calc_bch(((msg[5] >> 1) | (msg[6] << 3)) & 0x7f);
			bch[4] = calc_bch((msg[7] | (msg[8] << 4)) & 0x7f);
			bch[5] = calc_bch(((msg[8] >> 3) | (msg[9] << 1) | (msg[10] << 5)) & 0x7f);
			if (*message && i < MAX_NDB)
				contbit = 1;
			else
				contbit = 0;
			bch[6] = calc_bch((contbit << 6) | (msg[10] >> 2) | (msg[11] << 2));
			/* checksum */
			checksum = bch[0] + bch[1] + bch[2] + bch[3] + bch[4] + bch[5] + bch[6];
			bch[7] = calc_bch(checksum & 0x7f);
			/* store comma bit */
			queue_bit(gsc, (bch[0] & 1) ^ 1); // inverted first bit
			/* store interleaved bits */
			for (j = 0; j < 15; j++) {
				for (k = 0; k < 8; k++)
					queue_bit(gsc, (bch[k] >> j) & 1);
			}
		}
	}

	/* encode comma after message and store */
	PDEBUG(DGOLAY, DEBUG_DEBUG, "Encoding 'comma' sequence after message.\n");
	queue_comma(gsc, 121 * 8, 1);

	/* check overflow */
	if (gsc->bit_overflow) {
		PDEBUG(DGOLAY, DEBUG_ERROR, "Bit stream (%d bits) overflows bit buffer size (%d bits), please fix!\n", gsc->bit_num, (int)sizeof(gsc->bit));
		return -EOVERFLOW;
	}

	return 0;
}

/* get next bit
 *
 * if there is no message, return -1, so that the transmitter is turned off.
 *
 * if there is a message, return next bit to be transmitted.
 *
 * if there is a message in the queue, encode message and return its first bit.
 */
int8_t get_bit(gsc_t *gsc)
{
	gsc_msg_t *msg;
	uint8_t bit;
	int rc;

	/* if currently transmiting message, send next bit */
	if (gsc->bit_num) {
		bit = gsc->bit[gsc->bit_index++];
		if (gsc->bit_index == gsc->bit_num) {
			queue_reset(gsc);
			PDEBUG(DGOLAY, DEBUG_INFO, "Done transmitting message.\n");
		}
		return bit;
	}

next_msg:
	msg = gsc->msg_list;

	/* no message pending, turn transmitter off */
	if (!msg)
		return -1;

	/* encode first message in queue */
	rc = queue_batch(gsc, msg->address, msg->force_type, msg->data);
	if (rc >= 0)
		PDEBUG(DGOLAY, DEBUG_INFO, "Transmitting message to address '%s'.\n", msg->address);
	golay_msg_destroy(gsc, msg);
	if (rc < 0)
		goto next_msg;

	/* return first bit */
	bit = gsc->bit[gsc->bit_index++];
	return bit;
}

void golay_msg_send(const char *text)
{
	char buffer[strlen(text) + 1], *p = buffer, *address_string, *message;
	gsc_t *gsc;
	int force_type = -1;

	strcpy(buffer, text);
	address_string = strsep(&p, ",");
	message = p;
	if (message) {
		if (message[0] == 'a' && message[1] == ',') {
			force_type = TYPE_ALPHA;
			message += 2;
		} else
		if (message[0] == 'n' && message[1] == ',') {
			force_type = TYPE_NUMERIC;
			message += 2;
		}
	} else
		message = "";

	gsc = (gsc_t *) sender_head;
	golay_msg_create(gsc, address_string, message, force_type);
}

void call_down_clock(void)
{
}

/* Call control starts call towards paging network. */
int call_down_setup(int __attribute__((unused)) callref, const char *caller_id, enum number_type __attribute__((unused)) caller_type, const char *dialing)
{
	char channel = '\0';
	sender_t *sender;
	gsc_t *gsc;
	const char *address;
	const char *message;
	gsc_msg_t *msg;

	/* find transmitter */
	for (sender = sender_head; sender; sender = sender->next) {
		/* skip channels that are different than requested */
		if (channel && sender->kanal[0] != channel)
			continue;
		gsc = (gsc_t *) sender;
		/* check if base station cannot transmit */
		if (!gsc->tx)
			continue;
		break;
	}
	if (!sender) {
		if (channel)
			PDEBUG(DGOLAY, DEBUG_NOTICE, "Cannot page, because given station not available, rejecting!\n");
		else
			PDEBUG(DGOLAY, DEBUG_NOTICE, "Cannot page, no trasmitting station available, rejecting!\n");
		return -CAUSE_NOCHANNEL;
	}

	/* get address */
	address = dialing;

	/* get message */
	if (caller_id[0])
		message = caller_id;
	else
		message = gsc->default_message;

	/* create call process to page station */
	msg = golay_msg_create(gsc, address, message, -1);
	if (!msg)
		return -CAUSE_INVALNUMBER;
	return -CAUSE_NORMAL;
}

void call_down_answer(int __attribute__((unused)) callref)
{
}


static void _release(int __attribute__((unused)) callref, int __attribute__((unused)) cause)
{
	PDEBUG(DGOLAY, DEBUG_INFO, "Call has been disconnected by network.\n");
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
void call_down_audio(int __attribute__((unused)) callref, uint16_t __attribute__((unused)) sequence, uint32_t __attribute__((unused)) timestamp, uint32_t __attribute__((unused)) ssrc, sample_t __attribute__((unused)) *samples, int __attribute__((unused)) count)
{
}

void dump_info(void) {}

