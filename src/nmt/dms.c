/* NMT DMS (data service) processing
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
#include "../libsample/sample.h"
#include "../liblogging/logging.h"
#include "../libmobile/get_time.h"
#include "nmt.h"

#define MUTE_DURATION		0.300	/* 200ms, and about 95ms for the frame itself */

#define DMS_DOTTING		"101010101010101"
#define DMS_SYNC		"00101000111"

int dms_allow_loopback = 0;

/*
 * support
 */

/* calculate CRC from the bits of label and data and 16 zeroes.
 * the result is the remainder of the polynomial division and
 * conforms to DMS standard.
 */
static uint16_t crc16(uint8_t *bits, int len)
{
	uint16_t generator = 0x1021;
	uint16_t crc = 0; /* init crc register with 0 */
	int i;

	for (i = 0; i < len; i++) {
		/* check if MSB is set */
		if ((crc & 0x8000)) {   /* MSB set, shift it out of the register */
			/* shift in next bit of input stream */
			crc = (crc << 1) | bits[i];
			/* Perform the 'division' by XORing the crc register with the generator polynomial */
			crc = crc ^ generator;
		} else {   /* MSB not set, shift it out and shift in next bit of input stream. Same as above, just no division */
			crc = (crc << 1) | bits[i];
		}
	}

	return crc;
}

/*
 * frame handling
 */

/* print CT/DT frame in 8-bit or 7-bit mode. */
static const char *print_ct_dt(uint8_t s, uint8_t n, uint8_t *data, int eight_bits)
{
	static char text[128], *ct;

	if (s)
		ct = "    ";
	else switch (data[0]) {
	case 0:
		ct = "IDLE";
		break;
	case 73:
		ct = "ID  ";
		break;
	case 82:
		ct = "RAND";
		break;
	case 84:
		ct = "CT84";
		break;
	default:
		ct = "????";
	}

	if (!eight_bits || s == 0)
		sprintf(text, "%cT(%d) = %s %3d %3d %3d %3d %3d %3d %3d %3d", 'C' + s, n, ct, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
	else
		sprintf(text, "%cT(%d) = %s 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x", 'C' + s, n, ct,
			((data[0] << 1) & 0x80) | data[1],
			((data[0] << 2) & 0x80) | data[2],
			((data[0] << 3) & 0x80) | data[3],
			((data[0] << 4) & 0x80) | data[4],
			((data[0] << 5) & 0x80) | data[5],
			((data[0] << 6) & 0x80) | data[6],
			((data[0] << 7) & 0x80) | data[7]);

	return text;
}

/* link DMS frame to list of TX frames */
void link_dms_frame(nmt_t *nmt, struct dms_frame *frame)
{
	dms_t *dms = &nmt->dms;
	struct dms_frame **framep;

	LOGP(DDMS, LOGL_DEBUG, "link DMS frame\n");

	/* attach to end of list */
	framep = &dms->state.frame_list;
	while (*framep)
		framep = &((*framep)->next);
	*framep = frame;
}

/* unlink DMS frame from list of TX frames */
void unlink_dms_frame(nmt_t *nmt, struct dms_frame *frame)
{
	dms_t *dms = &nmt->dms;
	struct dms_frame **framep;

	LOGP(DDMS, LOGL_DEBUG, "unlink DMS frame\n");

	/* unlink */
	framep = &dms->state.frame_list;
	while (*framep && *framep != frame)
		framep = &((*framep)->next);
	if (!(*framep)) {
		LOGP(DTRANS, LOGL_ERROR, "Frame not in list, please fix!!\n");
		abort();
	}
	*framep = frame->next;
}

/* add DMS frame to list of TX frames */
static void dms_frame_add(nmt_t *nmt, int s, const uint8_t *data)
{
	dms_t *dms = &nmt->dms;
	struct dms_frame *dms_frame;

	dms_frame = calloc(1, sizeof(*dms_frame));
	if (!dms_frame) {
		LOGP(DDMS, LOGL_ERROR, "No memory!\n");
			return;
	}

	dms_frame->s = s;
	dms_frame->n = dms->state.n_count;
	dms->state.n_count = (dms->state.n_count + 1) & 7;
	memcpy(dms_frame->data, data, 8);

	LOGP(DDMS, LOGL_DEBUG, "add DMS %cT(%d) frame to queue\n", dms_frame->s + 'C', dms_frame->n);

	link_dms_frame(nmt, dms_frame);
}

/* delete DMS frame from list of TX frames */
static void dms_frame_delete(nmt_t *nmt, struct dms_frame *dms_frame)
{
	LOGP(DDMS, LOGL_DEBUG, "delete DMS frame %cT(%d) from queue\n", dms_frame->s + 'C', dms_frame->n);

	unlink_dms_frame(nmt, dms_frame);

	free(dms_frame);
}

/* add DT frame */
static void dms_frame_add_dt(nmt_t *nmt, const uint8_t *data)
{
	dms_frame_add(nmt, 1, data);
}

/* add CT frame */
static void dms_frame_add_ct(nmt_t *nmt, const uint8_t *data)
{
	dms_frame_add(nmt, 0, data);
}

/* add ID frame */
static void dms_frame_add_id(nmt_t *nmt)
{
	uint8_t frame[8];

	frame[0] = 73; /* ID */
	frame[1] = 3; // FIXME: add real id
	frame[2] = 0;
	frame[3] = 0;
	frame[4] = 0;
	frame[5] = 0;
	frame[6] = 0;
	frame[7] = 0;
	dms_frame_add_ct(nmt, frame);
}

/* add RAND frame */
static void dms_frame_add_rand(nmt_t *nmt, int eight_bits)
{
	uint32_t rand = random();
	uint8_t frame[8];

	frame[0] = 82; /* RAND */
	frame[1] = (rand >> 17) & 0x40;
	frame[2] = (rand >> 16) & 0x7f;
	frame[3] = (rand >> 9) & 0x40;
	frame[4] = (rand >> 8) & 0x7f;
	frame[5] = (rand >> 1) & 0x40;
	frame[6] = rand & 0x7f;
	frame[7] = eight_bits ? '8' : '7';
	dms_frame_add_ct(nmt, frame);
}

/*
 * init and exit
 */

/* init instance */
int dms_init_sender(nmt_t __attribute__((unused)) *nmt)
{
	/* we need some simple random */
	srandom((unsigned int)(get_time() * 1000));

	return 0;
}

/* Cleanup transceiver instance. */
void dms_cleanup_sender(nmt_t *nmt)
{
	dms_reset(nmt);
}

/*
 * transmission of frames
 */

/* encode DT frame and schedule for next transmission */
static void dms_encode_dt(nmt_t *nmt, uint8_t d, uint8_t s, uint8_t n, uint8_t *_data)
{
	dms_t *dms = &nmt->dms;
	char frame[127];
	uint8_t data[12];
	uint8_t bits[63 + 16];
	uint16_t crc;
	int i, j;

	LOGP(DDMS, LOGL_INFO, "Sending DMS frame: %s\n", print_ct_dt(s, n, _data, dms->state.eight_bits));

	/* generate label */
	data[0] = (d << 6) | (s << 5) | (3 << 3) | n;
	memcpy(data + 1, _data, 8);
	for (i = 0; i < 9; i++) {
		for (j = 0; j < 7; j++)
			bits[i * 7 + j] = (data[i] >> (6 - j)) & 1;
	}
	for (i = 0; i < 16; i++)
		bits[63 + i] = 0;
	crc = crc16(bits, 63 + 16);
	data[9] = crc >> 9;
	data[10] = crc >> 2;
	data[11] = crc & 0x3;

	/* create RR frame */
	// FIXME: no dotting on consecutive frames
	memcpy(frame, DMS_DOTTING, 15);
	memcpy(frame + 15, DMS_SYNC, 11);
	for (i = 0; i < 11; i++) {
		for (j = 0; j < 7; j++)
			frame[26 + j + i*9] = ((data[i] >> (6 - j)) & 1) | '0';
		frame[26 + 7 + i*9] = '1';
		frame[26 + 8 + i*9] = '1';
	}
	frame[125] = ((data[11] >> 1) & 1) | '0';
	frame[126] = (data[11] & 1) | '0';
#if 0
	for (i = 0; i < 127; i++) {
		if (i == 15 || i == 26 || (i - 26) % 9 == 6 || (i - 26) % 9 == 8)
			printf(" ");
		printf("%c", frame[i]);
	}
	printf("\n");
#endif

	/* store frame */
	memcpy(dms->tx_frame, frame, 127);
	dms->tx_frame_length = 127;
	dms->tx_frame_pos = 0;
	dms->tx_frame_valid = 1;
}

/* encode RR frame and schedule for next transmission */
static void dms_encode_rr(nmt_t *nmt, uint8_t d, uint8_t s, uint8_t n)
{
	dms_t *dms = &nmt->dms;
	uint8_t data;
	char frame[77], label[9];
	int parity, i;

	/* generate label */
	data = (d << 6) | (s << 5) | (1 << 3) | n;
	parity = '0';
	for (i = 0; i < 7; i++) {
		label[i] = ((data >> (6 - i)) & 1) | '0';
		if (label[i] == '1')
			parity ^= 1;
	}
	label[7] = '1';
	label[8] = '1';

	/* create RR frame */
	memcpy(frame, DMS_DOTTING, 15);
	memcpy(frame + 15, DMS_SYNC, 11);
	memcpy(frame + 26, label, 9);
	memcpy(frame + 35, label, 9);
	frame[44] = parity; frame[45] = parity;
	memcpy(frame + 46, frame + 15, 31);
#if 0
	for (i = 0; i < 77; i++) {
		if (i == 15 || i == 26 || i == 33 || i == 35
		 || i == 42 || i == 44 || i == 46 || i == 57
		 || i == 64 || i == 66 || i == 73 || i == 75)
			printf(" ");
		printf("%c", frame[i]);
	}
	printf("\n");
#endif

	/* store frame */
	memcpy(dms->tx_frame, frame, 77);
	dms->tx_frame_length = 77;
	dms->tx_frame_pos = 0;
	dms->tx_frame_valid = 1;
}

/* check if we have to transmit a frame and render it
 * also do nothing until a currently transmitted frame is completely
 * transmitted.
 *
 * this function is public, so it can be used by test routine.
 */
void trigger_frame_transmission(nmt_t *nmt)
{
	dms_t *dms = &nmt->dms;
	struct dms_frame *dms_frame;
	int i;

	/* ongoing transmission, so we wait */
	if (dms->tx_frame_valid)
		return;

	/* check for RR first, because high priority */
	if (dms->state.send_rr) {
		LOGP(DDMS, LOGL_DEBUG, "Found pending RR(%d) frame, sending.\n", dms->state.n_r);
		dms->state.send_rr = 0;
		dms_encode_rr(nmt, dms->state.dir ^ 1, 1, dms->state.n_r);
		return;
	}

	/* get next frame to send */
	/* loop 4 times, because only 4 unacked frames may be transmitted */
	dms_frame = dms->state.frame_list;
	for (i = 0; i < 4 && dms_frame; i++) {
		/* stop before DT frame, if RAND was not acked */
		if (dms_frame->next && dms_frame->next->s == 1 && !dms->state.established)
			break;
		if (dms_frame->n == dms->state.n_s)
			break;
		dms_frame = dms_frame->next;
	}

	/* check if outstanding frame */
	if (!dms_frame) {
		LOGP(DDMS, LOGL_DEBUG, "No pending RR/CT/DT frame found.\n");
		if (dms->state.tx_pending) {
			dms->state.tx_pending = 0;
			dms_all_sent(nmt);
		}
		return;
	}

	LOGP(DDMS, LOGL_DEBUG, "Found pending %cT(%d) frame, sending.\n", dms_frame->s + 'C', dms_frame->n);

	/* sent next send state to next frame in buffer.
	 * if there is no next frame, set it to the first frame (cycle).
	 * also if RAND was not acked, but next frame is DT, send first frame.
	 */
	if (!dms_frame->next) {
		dms->state.n_s = dms->state.frame_list->n;
		LOGP(DDMS, LOGL_DEBUG, " -> Next sequence number is %d, because this was the last frame in queue.\n", dms->state.n_s);
	} else if (!dms->state.established && dms_frame->next->s == 1) {
		dms->state.n_s = dms->state.frame_list->n;
		LOGP(DDMS, LOGL_DEBUG, " -> Next sequence number is %d, because this was the last frame before DT queue, and RAND has not been acked yet.\n", dms->state.n_s);
	} else if (i == 3) {
		dms->state.n_s = dms->state.frame_list->n;
		LOGP(DDMS, LOGL_DEBUG, " -> Next sequence number is %d, because we reached max number of unacknowledged frames.\n", dms->state.n_s);
	} else if (!dms->state.established && dms_frame->next->s == 0) {
		dms->state.n_s = dms_frame->next->n;
		LOGP(DDMS, LOGL_DEBUG, " -> Next sequence number is %d, because this is the next CT frame in queue.\n", dms->state.n_s);
	} else {
		dms->state.n_s = dms_frame->next->n;
		LOGP(DDMS, LOGL_DEBUG, " -> Next sequence number is %d, because this is the next frame in queue.\n", dms->state.n_s);
	}

	dms_encode_dt(nmt, dms->state.dir ^ 1, dms_frame->s, dms_frame->n, dms_frame->data);
}

/* send data using FSK */
int dms_send_bit(nmt_t *nmt)
{
	dms_t *dms = &nmt->dms;

	if (!dms->tx_frame_valid)
		return -1;

	if (!dms->tx_frame_length || dms->tx_frame_pos == dms->tx_frame_length) {
		dms->tx_frame_valid = 0;
		trigger_frame_transmission(nmt);
		if (!dms->tx_frame_valid)
			return -1;
	}

	return dms->tx_frame[dms->tx_frame_pos++];
}

/*
 * reception of frames
 */

/* decode DT frame from mobile */
static void dms_rx_dt(nmt_t *nmt, uint8_t d, uint8_t s, uint8_t n, uint8_t *data)
{
	dms_t *dms = &nmt->dms;
	int length;

	/* start transfer */
	if (!dms->state.started) {
		LOGP(DDMS, LOGL_INFO, "Starting DMS transfer (mobile originated)\n");
		dms->state.started = 1;
		dms->state.established = 0;
		dms->state.dir = d;
		dms->state.n_r = 0;
		dms->state.n_s = 0;
		dms->state.n_a = 0;
		dms->state.n_count = 0;
		dms->state.rand_sent = 0;
	}

	if (dms->state.dir != d && !dms_allow_loopback) {
		/* drop frames with wrong direction indicator */
		LOGP(DDMS, LOGL_INFO, "DMS frame ignored, direction indicator mismatch!\n");
		return;
	}

	if (dms->state.n_r != n) {
		/* ignore out of sequence frames */
		LOGP(DDMS, LOGL_DEBUG, "DMS frame number mismatch (due to resending)\n");
	} else {
		LOGP(DDMS, LOGL_INFO, "Received valid DMS frame: %s\n", print_ct_dt(s, n, data, dms->state.eight_bits));

		/* cycle sequence */
		dms->state.n_r = (n + 1) % 8;

		/* CT frames */
		if (s == 0) {
			switch (data[0]) {
			case 73: /* ID */
				break;
			case 82: /* RAND */
				LOGP(DDMS, LOGL_DEBUG, "RAND frame has been received, so we can send/receive DT frame\n");
				/* when we sent RAND, we do not resend it again, this would be wrong */
				if (!dms->state.rand_sent) {
					dms_frame_add_rand(nmt, data[7]);
					dms->state.rand_sent = 1;
				}
				dms->state.established = 1;
				dms->state.eight_bits = (data[7] == '8') ? 1 : 0;
				break;
			default:
				;
			}
		} else {
			if (!dms->state.established)
				LOGP(DDMS, LOGL_NOTICE, "Received DT frame, but RAND frame has not been received yet\n");
			else {
				if (!dms->state.eight_bits)
					length = 8;
				else {
					int i;

					for (i = 1; i < 8; i++)
						data[i] |= ((data[0] << i) & 0x80);
					length = 7;
					data++;
				}
				/* according to NMT Doc 450-3 10.8 remove trailing zeroes */
				while (length > 1) {
					if (data[length - 1] == 0)
						length--;
					else
						break;
				}
				dms_receive(nmt, data, length, dms->state.eight_bits);
			}
		}
	}

	/* schedule sending of RR frame */
	dms->state.send_rr = 1;
	/* now trigger frame transmission */
	trigger_frame_transmission(nmt);
}

/* decode RR frame from mobile */
static void dms_rx_rr(nmt_t *nmt, uint8_t d, uint8_t s, uint8_t n)
{
	dms_t *dms = &nmt->dms;
	struct dms_frame *dms_frame, *dms_frame_next;
	int i, j;

	if (!dms->state.started)
		return;

	if (dms->state.dir != d && !dms_allow_loopback) {
		/* drop frames with wrong direction indicator */
		LOGP(DDMS, LOGL_INFO, "DMS frame ignored, direction indicator mismatch!\n");
		return;
	}

	/* check to which entry in the list of frames this ack belongs to */
	/* loop 4 times, because only 4 unacked frames may have been transmitted */
	dms_frame = dms->state.frame_list;
	for (i = 0; i < 4 && dms_frame; i++) {
		if (dms_frame->n == ((n - 1) & 7))
			break;
		dms_frame = dms_frame->next;
	}

	/* if we don't find a frame, it must have been already acked, so we ignore RR */
	if (!dms_frame || i == 4) {
		LOGP(DDMS, LOGL_DEBUG, "Received already acked DMS frame: RR(%d) (s = %d), ignoring\n", n, s);
		return;
	}

	LOGP(DDMS, LOGL_INFO, "Received valid DMS frame: RR(%d) (s = %d)\n", n, s);

	/* flush all acked frames. */
	dms_frame = dms->state.frame_list;
	for (j = 0; j <= i; j++) {
		if (dms_frame->data[0] == 82) { /* RAND */
			LOGP(DDMS, LOGL_DEBUG, "RAND frame has been acknowledged, so we can continue to send DT frame\n");
			dms->state.established = 1;
		}
		/* increment ack counter */
		dms->state.n_a = (dms_frame->n + 1) & 7;
		/* raise send counter if required */
		if (dms->state.n_s == dms_frame->n) {
			dms->state.n_s = dms->state.n_a;
			LOGP(DDMS, LOGL_DEBUG, "Raising next frame to send to #%d\n", dms->state.n_s);
		}
		LOGP(DDMS, LOGL_DEBUG, "Removing acked frame #%d\n", dms_frame->n);
		dms_frame_next = dms_frame->next;
		dms_frame_delete(nmt, dms_frame);
		dms_frame = dms_frame_next;
	}

	/* now trigger frame transmission */
	trigger_frame_transmission(nmt);
}

/* decode NR frame from mobile */
static void dms_rx_nr(nmt_t *nmt, uint8_t d, uint8_t s, uint8_t n)
{
	dms_t *dms = &nmt->dms;

	if (!dms->state.started)
		return;

	if (dms->state.dir != d && !dms_allow_loopback) {
		/* drop frames with wrong direction indicator */
		LOGP(DDMS, LOGL_INFO, "DMS frame ignored, direction indicator mismatch!\n");
		return;
	}

	LOGP(DDMS, LOGL_INFO, "Received valid DMS frame: NR(%d) (s = %d)\n", n, s);

	// FIXME: support NR

	/* now trigger frame transmission */
	trigger_frame_transmission(nmt);
}

/* Check for DMS SYNC bits, then collect data bits */
void fsk_receive_bit_dms(nmt_t *nmt, int bit, double quality, double level)
{
	dms_t *dms = &nmt->dms;
//	double frames_elapsed;
	int i;

//	printf("bit=%d quality=%.4f\n", bit, quality);
	/* we always search for sync, because the sync cannot show up inside the message itself */
	dms->rx_sync = (dms->rx_sync << 1) | bit;

	/* sync level and quality */
	dms->rx_sync_level[dms->rx_sync_count & 0xff] = level;
	dms->rx_sync_quality[dms->rx_sync_count & 0xff] = quality;
	dms->rx_sync_count++;

	/* check if pattern 00101000111 matches */
	if ((dms->rx_sync & 0x07ff) == 0x0147) {
		/* average level and quality */
		level = quality = 0;
		for (i = 0; i < 16; i++) {
			level += dms->rx_sync_level[(dms->rx_sync_count - 1 - i) & 0xff];
			quality += dms->rx_sync_quality[(dms->rx_sync_count - 1 - i) & 0xff];
		}
		level /= 16.0; quality /= 16.0;
//		printf("DMS sync (level = %.2f, quality = %.2f\n", level, quality);

		/* do not accept garbage */
		if (quality < 0.65)
			return;

		LOGP(DDSP, LOGL_DEBUG, "DMS sync  RX Level: %.0f%% Quality=%.0f\n", level * 100.0 + 0.5, quality * 100.0 + 0.5);

		/* rest sync register */
		dms->rx_sync = 0;
		dms->rx_in_sync = 1;
		dms->rx_frame_count = 0;
		dms->rx_bit_count = 0;
		memset(dms->rx_frame, 0, sizeof(dms->rx_frame));
		memset(dms->rx_frame_level, 0, sizeof(dms->rx_frame_level));
		memset(dms->rx_frame_quality, 0, sizeof(dms->rx_frame_quality));

		/* set muting of receive path */
		nmt->rx_mute = (int)((double)nmt->sender.samplerate * MUTE_DURATION);
		return;
	}

	if (!dms->rx_in_sync)
		return;

	/* read bits */
	if (++dms->rx_bit_count <= 7)
		dms->rx_frame[dms->rx_frame_count] = (dms->rx_frame[dms->rx_frame_count] << 1) | bit;
	dms->rx_frame_level[dms->rx_frame_count] += level;
	dms->rx_frame_quality[dms->rx_frame_count] += quality;

	/* check label */
	if (dms->rx_frame_count == 0) {
		if (dms->rx_bit_count == 9) {
			dms->rx_bit_count = 0;
			dms->rx_label.d = (dms->rx_frame[0] >> 6) & 0x1;
			dms->rx_label.s = (dms->rx_frame[0] >> 5) & 0x1;
			dms->rx_label.p = (dms->rx_frame[0] >> 3) & 0x3;
			dms->rx_label.n = dms->rx_frame[0] & 0x7;
			LOGP(DDMS, LOGL_DEBUG, "Got DMS label (d = %d, s = %d, p = %d, n = %d)\n", dms->rx_label.d,dms->rx_label.s,dms->rx_label.p,dms->rx_label.n);
			dms->rx_frame_count++;
			if (dms->rx_label.p == 0) {
				LOGP(DDMS, LOGL_DEBUG, "Spare prefix '00' ignoring!\n");
				dms->rx_in_sync = 0;
			}
		}
		return;
	}

	if (dms->rx_label.p == 3) {
		/* read DT frame */
		if (dms->rx_frame_count <= 8) {
			if (dms->rx_bit_count == 9) {
				dms->rx_bit_count = 0;
				uint8_t c = dms->rx_frame[dms->rx_frame_count];
				LOGP(DDMS, LOGL_DEBUG, "Got DMS word 0x%02x (%c)\n", c, (c >= 32 && c <= 126) ? c : '.');
				dms->rx_frame_count++;
			}
			return;
		}
		if (dms->rx_frame_count <= 10) {
			if (dms->rx_bit_count == 9) {
				dms->rx_bit_count = 0;
				LOGP(DDMS, LOGL_DEBUG, "Got DMS CRC 0x%02x\n", dms->rx_frame[dms->rx_frame_count]);
				dms->rx_frame_count++;
			}
			return;
		}
		if (dms->rx_bit_count == 2) {
			uint16_t crc_got, crc_calc;
			uint8_t bits[63 + 16];
			int i, j;
			dms->rx_bit_count = 0;
			LOGP(DDMS, LOGL_DEBUG, "Got DMS CRC 0x%x\n", dms->rx_frame[dms->rx_frame_count]);
			crc_got = (dms->rx_frame[9] << 9) | (dms->rx_frame[10] << 2) | dms->rx_frame[11];
			for (i = 0; i < 9; i++) {
				for (j = 0; j < 7; j++)
					bits[i * 7 + j] = (dms->rx_frame[i] >> (6 - j)) & 1;
			}
			for (i = 0; i < 16; i++)
				bits[63 + i] = 0;
			crc_calc = crc16(bits, 63 + 16);
			LOGP(DDMS, LOGL_DEBUG, "DMS CRC = 0x%04x %s\n", crc_got, (crc_calc == crc_got) ? "(OK)" : "(CRC error)");
			if (crc_calc == crc_got)
				dms_rx_dt(nmt, dms->rx_label.d, dms->rx_label.s, dms->rx_label.n, dms->rx_frame + 1);
			dms->rx_in_sync = 0;
			return;
		}
		return;
	} else {
		/* read RR/NR frame */
		if (dms->rx_frame_count <= 1) {
			if (dms->rx_bit_count == 9) {
				dms->rx_bit_count = 0;
				if (dms->rx_frame[0] != dms->rx_frame[1]) {
					LOGP(DDMS, LOGL_DEBUG, "Repeated DMS label mismatches!\n");
					dms->rx_in_sync = 0;
					return;
				}
				dms->rx_frame_count++;
				LOGP(DDMS, LOGL_DEBUG, "Repeated label matches\n");
			}
			return;
		}
		if (dms->rx_bit_count == 2) {
			uint8_t parity_got, parity_calc = 0, bit;
			int i;
			dms->rx_bit_count = 0;
			parity_got = dms->rx_frame[2];
			LOGP(DDMS, LOGL_DEBUG, "Got DMS parity 0x%x\n", dms->rx_frame[dms->rx_frame_count]);
			for (i = 0; i < 7; i++) {
				bit = (dms->rx_frame[0] >> i) & 1;
				if (bit)
					parity_calc ^= 0x3;
			}
			LOGP(DDMS, LOGL_DEBUG, "DMS parity %s\n", (parity_calc == parity_got) ? "(OK)" : "(parity error)");
			if (parity_calc == parity_got) {
				if (dms->rx_label.p == 1)
					dms_rx_rr(nmt, dms->rx_label.d, dms->rx_label.s, dms->rx_label.n);
				else
					dms_rx_nr(nmt, dms->rx_label.d, dms->rx_label.s, dms->rx_label.n);
			}
			dms->rx_in_sync = 0;
			return;
		}
	}
}

/*
 * calls from upper layer
 */

/* receive data from upper layer to be sent as DT frames
 * the DT frames are generated */
void dms_send(nmt_t *nmt, const uint8_t *data, int length, int eight_bits)
{
	dms_t *dms = &nmt->dms;
	uint8_t frame[8];
	int i, copied;

	LOGP(DDMS, LOGL_DEBUG, "Received message with %d digits of %d bits\n", length, (eight_bits) ? 8 : 7);

	/* active connection */
	if (dms->state.started) {
		if (dms->state.eight_bits != eight_bits) {
			LOGP(DDMS, LOGL_ERROR, "DMS session active, but upper layer sends wrong bit format!\n");
			return;
		}
	}

	if (!dms->state.started) {
		LOGP(DDMS, LOGL_DEBUG, "Transfer not started, so we send ID + RAND first\n");
		dms->state.started = 1;
		dms->state.established = 0;
		dms->state.eight_bits = eight_bits;
		dms->state.dir = 1; /* we send 0, we expect 1 */
		dms->state.n_r = 0;
		dms->state.n_s = 0;
		dms->state.n_a = 0;
		dms->state.n_count = 0;
		dms_frame_add_id(nmt);
		dms_frame_add_rand(nmt, eight_bits);
		dms->state.rand_sent = 1;
	}

	LOGP(DDMS, LOGL_DEBUG, "Queueing message data as DT frames...\n");
	while (length) {
		if (eight_bits) {
			/* copy what we have */
			for (i = 1; i < 8 && length; i++) {
				frame[i] = *data++;
				length--;
			}
			copied = i - 1;
			/* padd with 0, if required */
			for (; i < 8; i++)
				frame[i] = 0;
			/* move the 8th bits to first character */
			frame[0] = 0;
			for (i = 1; i < 8; i++) {
				frame[0] |= (frame[i] & 0x80) >> i;
				frame[i] &= 0x7f;
			}
		} else {
			/* copy what we have */
			for (i = 0; i < 8 && length; i++) {
				frame[i] = (*data++) & 0x7f;
				length--;
			}
			copied = i;
			/* padd with 0, if required */
			for (; i < 8; i++)
				frame[i] = 0;
		}
		/* according to NMT Doc 450-3 10.8 trailing zeros are ignored.
		 * we put back the trailing zeros to the data buffer.
		 * except for a single zero, we keep it, because all digits
		 * 0 means a single zero is transmitted.
		 */
		for (i = 0; i < copied - 1; i++) {
			if (data[-1] == 0) {
				/* put back last take byte */
				data--;
				length++;
			}
		}
		dms_frame_add_dt(nmt, frame);
		/* indicate that we have pending data */
		dms->state.tx_pending = 1;
	}

	/* now trigger frame transmission */
	trigger_frame_transmission(nmt);
}

/* reset DMS instance */
void dms_reset(nmt_t *nmt)
{
	dms_t *dms = &nmt->dms;
	LOGP(DDMS, LOGL_DEBUG, "Resetting DMS states\n");

	dms->rx_in_sync = 0;
	memset(&dms->state, 0, sizeof(dms->state));

	dms->tx_frame_valid = 0;

	while (dms->state.frame_list)
		dms_frame_delete(nmt, dms->state.frame_list);
}

