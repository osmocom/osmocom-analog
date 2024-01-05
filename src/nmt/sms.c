/* NMT SMS (short message service) processing
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
#include <time.h>
#include <errno.h>
#include "../libsample/sample.h"
#include "../liblogging/logging.h"
#include "nmt.h"

#define SMS_RECEIVE_TO		5,0
#define SMS_RELEASE_TO		2,0

/* TP-Message-Type-Indicator (TP-MTI) */
#define	MTI_SMS_DELIVER		0x00 /* SC -> MS */
#define	MTI_SMS_DELIVER_REPORT	0x00 /* MS -> SC */
#define	MTI_SMS_STATUS_REPORT	0x02 /* SC -> MS */
#define	MTI_SMS_COMMAND		0x02 /* MS -> SC */
#define	MTI_SMS_SUBMIT		0x01 /* MS -> SC */
#define	MTI_SMS_SUBMIT_REPORT	0x01 /* SC -> MS */
#define MTI_MASK		0x03 /* Bits 0 and 1 */

/* TP-More-Messages-to-Send (TP-MMS) */
#define MMS_NORE		0x00
#define MMS_NO_MORE		0x04
#define MMS_MASK		0x04 /* Bit 2 */	

/* TP-Validity-Period-Format (TP-VPF) */
#define VPF_NOT_PRESENT		0x00
#define VPF_PRESENT_INTEGER	0x10
#define VPF_PRESENT_SEMI_OCTET	0x18
#define VPF_MASK		0x18 /* Bits 3 and 4 */

/* TP-Status-Report-Indication (TP-SRI) */
#define SRI_NO_REPORT		0x00
#define SRI_REPORT		0x20
#define SRI_MASK		0x20

/* TP-Status-Report-Request (TP-SRR) */
#define SSR_NO_REPORT		0x00
#define SSR_REPORT		0x20
#define SSR_MASK		0x20

/* TP-Failure-Cause (TP-FCS) */
#define FCS_BUSY		0xc0
#define FCS_NO_SC_SUBSCRIPTION	0xc1
#define FSC_SC_SYSTEM_FAILURE	0xC2
#define FSC_DEST_SME_BARRED	0xC4
#define FSC_ERROR_IN_MS		0xD2
#define FSC_MEMORY_EXCEEDED	0xD3
#define FSC_UNSPECIFIED_ERROR	0xFF

/* RP-Message-Type-Indicator (RP-MTI) */
#define RP_MO_DATA		0x00	/* MS -> SC */
#define RP_MT_DATA		0x01	/* SC -> MS */
#define RP_MT_ACK		0x02	/* MS -> SC */
#define RP_MO_ACK		0x03	/* SC -> MS */
#define RP_MT_ERROR		0x04	/* MS -> SC */
#define RP_MO_ERROR		0x05	/* SC -> MS */
#define RP_SM_MEMORY_AVAILABLE	0x06	/* MS -> SC */
#define RP_SM_READY_TO_RECEIVE	0x07	/* MS -> SC */
#define RP_SM_NO_MESSAGE	0x07	/* SC -> MS */
#define RP_MTI_MASK		0x07

/* RP IEs */
#define RP_IE_USER_DATA		0x41 /* wrong in NMT Doc.450-3 1998-04-03 */
#define RP_IE_CAUSE		0x42

/* SC -> MS header */
static const char sms_header[] = {
	0x01, 0x18, 0x53, 0x4d, 0x53, 0x48, 0x18, 'A', 'B', 'C', 0x02
};

/*
 * init and exit
 */

static void sms_timeout(void *data);

/* init instance */
int sms_init_sender(nmt_t *nmt)
{
	osmo_timer_setup(&nmt->sms_timer, sms_timeout, nmt);

	return 0;
}

/* Cleanup transceiver instance. */
void sms_cleanup_sender(nmt_t *nmt)
{
	sms_reset(nmt);
	osmo_timer_del(&nmt->sms_timer);
}

/*
 * send to lower layer
 */

/* encode header */
static int encode_header(uint8_t *data)
{
	memcpy(data, sms_header, sizeof(sms_header));

	return sizeof(sms_header);
}

/* encode address fields */
static int encode_address(uint8_t *data, const char *address, uint8_t type, uint8_t plan)
{
	int length = 1;
	uint8_t digit;
	int i, j;

	LOGP(DSMS, LOGL_DEBUG, "Encode SC->MS header\n");

	data[length++] = 0x80 | (type << 4) | plan;
	j = 0;
	for (i = 0; address[i]; i++) {
		if (address[i] >= '1' && address[i] <= '9')
			digit = address[i] - '0';
		else if (address[i] == '0')
			digit = 10;
		else if (address[i] == '*')
			digit = 11;
		else if (address[i] == '#')
			digit = 12;
		else if (address[i] == '+')
			digit = 13;
		else
			continue;
		if ((j & 1) == 0)
			data[length] = digit;
		else
			data[length++] |= digit << 4;
		j++;
	}
	if ((j & 1))
		data[length++] |= 0xf0;

	/* length field: number of semi-octets */
	data[0] = j;

	return length;
}

/* encode time stamp */
static int encode_time(uint8_t *data, time_t timestamp, int local)
{
	struct tm *tm = (local) ? localtime(&timestamp) : gmtime(&timestamp);
	int length = 0;
	uint8_t digit1, digit2;
	int quarters, sign;

	LOGP(DSMS, LOGL_DEBUG, "Encode time stamp '%02d.%02d.%02d %02d:%02d:%02d'\n", tm->tm_mday, tm->tm_mon + 1, tm->tm_year % 100, tm->tm_hour, tm->tm_min, tm->tm_sec);

	/* year */
	digit1 = (tm->tm_year % 100) / 10;
	if (digit1 == 0)
		digit1 = 10;
	digit2 = tm->tm_year % 10;
	if (digit2 == 0)
		digit2 = 10;
	data[length++] = (digit2 << 4) | digit1;

	/* month */
	digit1 = (tm->tm_mon + 1) / 10;
	if (digit1 == 0)
		digit1 = 10;
	digit2 = (tm->tm_mon + 1) % 10;
	if (digit2 == 0)
		digit2 = 10;
	data[length++] = (digit2 << 4) | digit1;

	/* day */
	digit1 = tm->tm_mday / 10;
	if (digit1 == 0)
		digit1 = 10;
	digit2 = tm->tm_mday % 10;
	if (digit2 == 0)
		digit2 = 10;
	data[length++] = (digit2 << 4) | digit1;

	/* hour */
	digit1 = tm->tm_hour / 10;
	if (digit1 == 0)
		digit1 = 10;
	digit2 = tm->tm_hour % 10;
	if (digit2 == 0)
		digit2 = 10;
	data[length++] = (digit2 << 4) | digit1;

	/* min */
	digit1 = tm->tm_min / 10;
	if (digit1 == 0)
		digit1 = 10;
	digit2 = tm->tm_min % 10;
	if (digit2 == 0)
		digit2 = 10;
	data[length++] = (digit2 << 4) | digit1;

	/* sec */
	digit1 = tm->tm_sec / 10;
	if (digit1 == 0)
		digit1 = 10;
	digit2 = tm->tm_sec % 10;
	if (digit2 == 0)
		digit2 = 10;
	data[length++] = (digit2 << 4) | digit1;

	/* zone */
	quarters = (local) ? (timezone / 900) : 0;
	if (quarters < 0) {
		quarters = -quarters;
		sign = 1;
	} else {
		quarters = -quarters;
		sign = 0;
	}
	data[length++] = (quarters << 4) | (sign << 3) | (quarters >> 4);

	return length;
}

/* encode user data */
static int encode_userdata(uint8_t *data, const char *message)
{
	int length = 1;
	uint8_t character;
	int i, j, pos;

	LOGP(DSMS, LOGL_DEBUG, "Encode user data '%s'\n", message);

	j = 0;
	pos = 0;
	for (i = 0; message[i]; i++) {
		if ((int8_t)message[i] >= 0)
			character = message[i]; /* 0..127 */
		else
			character = '?'; /* 128..255 */
		j++;
		if (pos == 0) {
			/* character fits and is aligned to the right, new octet */
			data[length] = character;
			pos = 7;
		} else {
			/* character is shifted by pos */
			data[length] |= character << pos;
			if (pos > 1) {
				/* not all bits fit in octet, so fill the rest to next octet */
				length++;
				data[length] = character >> (8 - pos);
				pos--;
			} else {
				/* all bits fit in octet, so go to next octet */
				pos = 0;
				length++;
			}
		}
	}
	if (pos)
		length++;

	/* length field: number of characters */
	data[0] = j;

	return length;
}

/* deliver SMS (SC->MS) */
int sms_deliver(nmt_t *nmt, uint8_t ref, const char *orig_address, uint8_t orig_type, uint8_t orig_plan, time_t timestamp, int local, const char *message)
{
	uint8_t data[256], *tpdu_length;
	int length = 0;
	int orig_len;
	int msg_len;

	LOGP(DSMS, LOGL_INFO, "Delivering SMS from upper layer\n");

	orig_len = strlen(orig_address);
	msg_len = strlen(message);

	if (orig_len > 24) {
		LOGP(DSMS, LOGL_NOTICE, "Originator Address too long (%d characters)\n", orig_len);
		return -EINVAL;
	}
	if (msg_len > 140) {
		LOGP(DSMS, LOGL_NOTICE, "Message too long (%d characters)\n", msg_len);
		return -EINVAL;
	}

	/* HEADER */
	length = encode_header(data);

	/* RP */
	data[length++] = RP_MT_DATA;
	data[length++] = ref;
	data[length++] = RP_IE_USER_DATA;
	tpdu_length = data + length++;

	/* TP */
	data[length++] = MTI_SMS_DELIVER | MMS_NO_MORE | VPF_NOT_PRESENT | SRI_NO_REPORT;
	length += encode_address(data + length, orig_address, orig_type, orig_plan); /* TP-OA */
	data[length++] = 0; /* TP-PID */
	data[length++] = 0; /* TP-DCS */
	length += encode_time(data + length, timestamp, local);
	length += encode_userdata(data + length, message);

	/* RP length */
	*tpdu_length = length - (uint8_t)(tpdu_length - data) - 1;
	LOGP(DSMS, LOGL_DEBUG, " -> TPDU length = %d\n", *tpdu_length);

	nmt->sms.mt = 1;
	dms_send(nmt, data, length, 1);

	/* start timer */
	osmo_timer_schedule(&nmt->sms_timer, SMS_RECEIVE_TO);

	return 0;
}

/* report SMS (SC->MS) */
static void sms_submit_report(nmt_t *nmt, uint8_t ref, int error)
{
	uint8_t data[64];
	int length = 0;

	LOGP(DSMS, LOGL_INFO, "Sending Submit Report (%s)\n", (error) ? "error" : "ok");

	/* HEADER */
	length = encode_header(data);

	/* RP */
	data[length++] = (error) ? RP_MO_ERROR : RP_MO_ACK;
	data[length++] = ref;

	dms_send(nmt, data, length, 1);
}

/*
 * receive from lower layer
 */

/* decode 7-bit character message from 8 bit data */
static void decode_message_7(const uint8_t *data, int length, char *message)
{
	int fill;
	int i;
	uint16_t result;

	fill = 0;
	result = 0;
	for (i = 0; i < length; i++) {
		result |= data[i] << fill;
		fill += 8;
		while (fill >= 7) {
			*message++ = result & 0x7f;
			result >>= 7;
			fill -= 7;
		}
	}
	*message++ = '\0';
}

static const char digits2ascii[16] = {
	'?', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', '0', '*', '#', '+', '?', '?' };

static void decode_address(const uint8_t *data, int digits, char *address)
{
	int i;

	for (i = 0; i < digits; i++) {
		if (!(i & 1))
			*address++ = digits2ascii[(*data) & 0xf];
		else
			*address++ = digits2ascii[(*data++) >> 4];
	}
	*address++ = '\0';
}

/* decode sms submit message
 * return 1 if done, -1 if failed, 0, if more data is required */
static int decode_sms_submit(nmt_t *nmt, const uint8_t *data, int length)
{
	uint8_t ref, msg_ref;
	const uint8_t *orig_data, *tpdu_data, *dest_data, *msg_data;
	int orig_len, tpdu_len, dest_len, msg_len;
	int orig_digits, dest_digits, msg_chars;
	uint8_t orig_type, orig_plan, dest_type, dest_plan;
	int tp_vpf_present = 0;
	int coding = 0;
	int rc;

	/* decode ref */
	ref = data[1];
	data += 2;
	length -= 2;

	/* do we have originator address length ? */
	if (length < 2) {
		LOGP(DSMS, LOGL_DEBUG, "SMS still incomplete, waiting for originator address\n");
		return 0;
	}
	orig_data = 2 + data;
	orig_digits = data[0];
	orig_type = (data[1] >> 4) & 0x7;
	orig_plan = data[1] & 0x0f;
	orig_len = (orig_digits + 1) >> 1;
	if (length < 2 + orig_len) {
		LOGP(DSMS, LOGL_DEBUG, "SMS still incomplete, waiting for originator address digits (got %d of %d)\n", length - 1, orig_len);
		return 0;
	}
	data += 2 + orig_len;
	length -= 2 + orig_len;
	
	/* do we have user data IE ? */
	if (length < 2) {
		LOGP(DSMS, LOGL_DEBUG, "SMS still incomplete, waiting for user data IE\n");
		return 0;
	}
	if (data[0] != RP_IE_USER_DATA) {
		LOGP(DSMS, LOGL_NOTICE, "missing user data IE\n");
		return -FSC_ERROR_IN_MS;
	}
	tpdu_len = data[1];
	tpdu_data = 2 + data;
	if (length < 2 + tpdu_len) {
		LOGP(DSMS, LOGL_DEBUG, "SMS still incomplete, waiting for TPDU to be complete\n");
		return 0;
	}
	data += 2 + tpdu_len;
	length -= 2 + tpdu_len;

	/* decode orig address */
	char orig_address[orig_digits + 1];
	decode_address(orig_data, orig_digits, orig_address);
	LOGP(DSMS, LOGL_DEBUG, "Decoded originating address: '%s'\n", orig_address);

	/* go into TP */
	data = tpdu_data;
	length = tpdu_len;

	/* check msg_type */
	if (length < 1) {
		LOGP(DSMS, LOGL_NOTICE, "short read user data IE\n");
		return -FSC_ERROR_IN_MS;
	}
	if ((data[0] & MTI_MASK) != MTI_SMS_SUBMIT) {
		LOGP(DSMS, LOGL_NOTICE, "especting SUBMIT MTI, but got 0x%02x\n", data[0]);
		return -FSC_ERROR_IN_MS;
	}
	if ((data[0] & VPF_MASK))
		tp_vpf_present = 1;
	data++;
	length--;

	/* decode msg ref */
	if (length < 1) {
		LOGP(DSMS, LOGL_NOTICE, "short read user data IE\n");
		return -FSC_ERROR_IN_MS;
	}
	msg_ref = data[0];
	data++;
	length--;

	/* decode dest address */
	if (length < 2) {
		LOGP(DSMS, LOGL_NOTICE, "short read user data IE\n");
		return -FSC_ERROR_IN_MS;
	}
	dest_data = 2 + data;
	dest_digits = data[0];
	dest_type = (data[1] >> 4) & 0x7;
	dest_plan = data[1] & 0x0f;
	dest_len = (dest_digits + 1) >> 1;
	if (length < 2 + dest_len) {
		LOGP(DSMS, LOGL_NOTICE, "short read user data IE\n");
		return -FSC_ERROR_IN_MS;
	}
	data += 2 + dest_len;
	length -= 2 + dest_len;
	char dest_address[dest_digits + 1];
	decode_address(dest_data, dest_digits, dest_address);
	LOGP(DSMS, LOGL_DEBUG, "Decoded destination address: '%s'\n", dest_address);

	/* skip above protocol identifier */
	if (length < 1) {
		LOGP(DSMS, LOGL_NOTICE, "short read above protocol identifier IE\n");
		return -FSC_ERROR_IN_MS;
	}
	data++;
	length--;

	/* decode data coding scheme */
	if (length < 1) {
		LOGP(DSMS, LOGL_NOTICE, "short data coding scheme IE\n");
		return -FSC_ERROR_IN_MS;
	}
	if (data[0] == 0x00) {
		LOGP(DSMS, LOGL_DEBUG, "SMS coding is 7 bits (got 0x%02x)\n", data[0]);
		coding = 7;
	} else if ((data[0] & 0xf0) == 0x30) {
		LOGP(DSMS, LOGL_DEBUG, "SMS coding is 8 bits (got 0x%02x)\n", data[0]);
		coding = 8;
	} else {
		LOGP(DSMS, LOGL_NOTICE, "SMS coding unsupported (got 0x%02x)\n", data[0]);
		return -FSC_ERROR_IN_MS;
	}
	data++;
	length--;

	/* skip validity period */
	if (tp_vpf_present) {
		if (length < 1) {
			LOGP(DSMS, LOGL_NOTICE, "short read validity period IE\n");
			return -FSC_ERROR_IN_MS;
		}
		data++;
		length--;
	}

	/* decode data message text */
	if (length < 1) {
		LOGP(DSMS, LOGL_NOTICE, "short read user data IE\n");
		return -FSC_ERROR_IN_MS;
	}
	msg_data = data + 1;
	msg_chars = data[0];
	if (coding == 7)
		msg_len = (msg_chars * 7 + 7) / 8;
	else
		msg_len = msg_chars;
	if (length < 1 + msg_len) {
		LOGP(DSMS, LOGL_NOTICE, "short read user data IE\n");
		return -FSC_ERROR_IN_MS;
	}
	char message[msg_chars + 1];
	if (coding == 7) {
		decode_message_7(msg_data, msg_len, message);
		LOGP(DSMS, LOGL_DEBUG, "Decoded message: '%s'\n", message);
	} else {
		memcpy(message, msg_data, msg_len);
		message[msg_len] = '\0';
		LOGP(DSMS, LOGL_DEBUG, "Included message: '%s'\n", message);
	}

	LOGP(DSMS, LOGL_INFO, "Submitting SMS to upper layer\n");

	rc = sms_submit(nmt, ref, orig_address, orig_type, orig_plan, msg_ref, dest_address, dest_type, dest_plan, message);
	if (rc < 0)
		return -FSC_SC_SYSTEM_FAILURE;

	return 1;
}

/* decode deliver report
 * return 1 if done, -1 if failed, 0, if more data is required */
static int decode_deliver_report(nmt_t *nmt, const uint8_t *data, int length)
{
	uint8_t ref, cause = 0;
	int error = 0;

	ref = data[1];

	if ((data[0] & RP_MTI_MASK) == RP_MT_ERROR) {
		error = 1;
		if (length < 4) {
			LOGP(DSMS, LOGL_DEBUG, "deliver report still incomplete, waiting for cause IE\n");
			return 0;
		}
		if (length < 4 + data[3]) {
			LOGP(DSMS, LOGL_DEBUG, "deliver report still incomplete, waiting for cause IE content\n");
			return 0;
		}
		if (data[2] == RP_IE_CAUSE && data[3] > 0)
			cause = data[4];
		LOGP(DSMS, LOGL_INFO, "Received Delivery report: ERROR, cause=%d\n", cause);
	} else
		LOGP(DSMS, LOGL_INFO, "Received Delivery report: OK\n");

	sms_deliver_report(nmt, ref, error, cause);

	return 1;
}

/* receive from DMS layer */
void dms_receive(nmt_t *nmt, const uint8_t *data, int length, int __attribute__((unused)) eight_bits)
{
	sms_t *sms = &nmt->sms;
	int space;
	int rc = 0;
	char debug_text[length * 5 + 1];
	int i;

	for (i = 0; i < length; i++)
		sprintf(debug_text + i * 5, " 0x%02x", data[i]);
	debug_text[length * 5] = '\0';

	/* restart timer */
	osmo_timer_schedule(&nmt->sms_timer, SMS_RECEIVE_TO);

	LOGP(DSMS, LOGL_DEBUG, "Received %d bytes from DMS layer:%s\n", length, debug_text);

	if (sms->mt && !sms->data_sent) {
		LOGP(DSMS, LOGL_NOTICE, "Ignoring data while we transmit data\n");
		return;
	}

	/* append received data */
	space = sizeof(sms->rx_buffer) - sms->rx_count;
	if (space < length) {
		LOGP(DSMS, LOGL_NOTICE, "Received message exceeds RX buffer, terminating call!\n");
release:
		osmo_timer_schedule(&nmt->sms_timer, SMS_RELEASE_TO);
		return;
	}
	memcpy(sms->rx_buffer + sms->rx_count, data, length);
	sms->rx_count += length;

	/* go into buffer */
	data = sms->rx_buffer;
	length = sms->rx_count;

	/* check if complete */
	if (length < 2)
		return;
	switch (data[0] & RP_MTI_MASK) {
	case RP_MT_ACK:
		rc = decode_deliver_report(nmt, data, length);
		break;
	case RP_MT_ERROR:
		rc = decode_deliver_report(nmt, data, length);
		break;
	case RP_MO_DATA:
		rc = decode_sms_submit(nmt, data, length);
		if (rc < 0)
			sms_submit_report(nmt, data[1], -rc);
		else if (rc > 0) {
			sms_submit_report(nmt, data[1], 0);
		}
		/* no release, we release afeter the report */
		rc = 0;
		break;
	case RP_SM_READY_TO_RECEIVE:
		LOGP(DSMS, LOGL_NOTICE, "Received READY-TO-RECEVIE message.\n");
		data += length;
		length -= length;
		break;
	default:
		LOGP(DSMS, LOGL_NOTICE, "Received unknown RP message type %d.\n", data[0]);
		rc = -1;
	}
	if (rc)
		goto release;
	
	return;
}

static void sms_timeout(void *data)
{
	nmt_t *nmt = data;

	sms_release(nmt);
}

/* all data has been sent to mobile */
void dms_all_sent(nmt_t *nmt)
{
	sms_t *sms = &nmt->sms;

	if (!sms->data_sent) {
		if (!sms->mt) {
			LOGP(DSMS, LOGL_DEBUG, "Done sending submit report, releasing.\n");
			osmo_timer_schedule(&nmt->sms_timer, SMS_RELEASE_TO);
		}
		sms->data_sent = 1;
		LOGP(DSMS, LOGL_DEBUG, "DMS layer indicates acknowledge of sent data\n");
	}
}

void sms_reset(nmt_t *nmt)
{
	sms_t *sms = &nmt->sms;

	LOGP(DSMS, LOGL_DEBUG, "Resetting SMS states\n");
	osmo_timer_del(&nmt->sms_timer);

	memset(sms, 0, sizeof(*sms));
}

