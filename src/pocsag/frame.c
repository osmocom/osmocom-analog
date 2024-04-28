/* POCSAG framing
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../libsample/sample.h"
#include "../liblogging/logging.h"
#include "pocsag.h"
#include "frame.h"

#define CHAN pocsag->sender.kanal

#define PREAMBLE_COUNT		18
#define CODEWORD_PREAMBLE	0xaaaaaaaa
#define CODEWORD_SYNC		0x7cd215d8
#define CODEWORD_IDLE		0x7a89c197
#define IDLE_BATCHES		2

static const char numeric[16] = "0123456789RU -][";
static const char hex[16] = "0123456789abcdef";

static const char *ctrl_char[32] = {
	"<NUL>",
	"<SOH>",
	"<STX>",
	"<ETX>",
	"<EOT>",
	"<ENQ>",
	"<ACK>",
	"<BEL>",
	"<BS>",
	"<HT>",
	"<LF>",
	"<VT>",
	"<FF>",
	"<CR>",
	"<SO>",
	"<SI>",
	"<DLE>",
	"<DC1>",
	"<DC2",
	"<DC3>",
	"<DC4>",
	"<NAK>",
	"<SYN>",
	"<ETB>",
	"<CAN>",
	"<EM>",
	"<SUB>",
	"<ESC>",
	"<FS>",
	"<GS>",
	"<RS>",
	"<US>",
};

static const char *del_char = "<DEL>";

const char *print_message(const char *message, int message_length)
{
	static char message_print[1024];
	const char *c;
	int i, ii, clen;

	/* i is input counter, ii is output counter */
	for (i = 0, ii = 0; i < message_length; i++) {
		if (message[i] >= 0 && message[i] <= 31)
			c = ctrl_char[(int)message[i]];
		else if (message[i] == 127)
			c = del_char;
		else {
			message_print[ii++] = message[i];
			continue;
		}
		clen = strlen(c);
		if (ii + clen == sizeof(message_print))
			break;
		memcpy(message_print + ii, c, clen);
		ii += clen;
	}
	message_print[ii++] = '\0';

	return message_print;
}

int scan_message(const char *message_input, int message_input_length, char *message_output, int message_output_length)
{
	int i, ii, j, clen;

	/* i is input counter, ii is output counter */
	for (i = 0, ii = 0; i < message_input_length; ii++) {
		if (ii == message_output_length)
			break;
		if (message_input[i] == '<') {
			/* maybe a control character ? */
			for (j = 0; j < 32; j++) {
				clen = strlen(ctrl_char[j]);
				/* skip, if control sequence would not fit into the input buffer */
				if (clen <= message_input_length - i && !memcmp(message_input + i, ctrl_char[j], clen)) {
					/* found control sequence, so break the loop */
					break;
				}
			}
			if (j < 32) {
				/* if loop was not completed, use the found character */
				message_output[ii] = j;
				i += clen;
			} else {
				clen = strlen(del_char);
				/* skip, if control sequence would not fit into the input buffer */
				if (clen <= i - message_input_length && !memcmp(message_input + i, del_char, clen)) {
					/* found control sequence, copy DEL character */
					message_output[ii] = 127;
					i += clen;
				} else {
					/* found no control sequence, copy '<' character */
					message_output[ii] = '<';
					i++;
				}
			}
		} else {
			/* no control character */
			message_output[ii] = message_input[i];
			i++;
		}
	}

	return ii;
}

static uint32_t pocsag_crc(uint32_t word)
{
	uint32_t denominator = 0x76900000;
	int i;

	word <<= 10;

	for (i = 0; i < 21; i++) {
		if ((word >> (30 - i)) & 1)
			word ^= denominator;
		denominator >>= 1;
	}

	return word & 0x3ff;
}

static uint32_t pocsag_parity(uint32_t word)
{
	word ^= word >> 16;
	word ^= word >> 8;
	word ^= word >> 4;
	word ^= word >> 2;
	word ^= word >> 1;

	return word & 1;
}

static int debug_word(uint32_t word, int slot)
{
	if (pocsag_crc(word >> 11) != ((word >> 1) & 0x3ff)) {
		LOGP(DPOCSAG, LOGL_NOTICE, "CRC error in codeword 0x%08x.\n", word);
		return -EINVAL;
	}

	if (pocsag_parity(word)) {
		LOGP(DPOCSAG, LOGL_NOTICE, "Parity error in codeword 0x%08x.\n", word);
		return -EINVAL;
	}

	if (word == CODEWORD_SYNC) {
		LOGP(DPOCSAG, LOGL_DEBUG, "-> valid sync word\n");
		return 0;
	}

	if (word == CODEWORD_IDLE) {
		LOGP(DPOCSAG, LOGL_DEBUG, "-> valid idle word\n");
		return 0;
	}

	if (!(word & 0x80000000)) {
		LOGP(DPOCSAG, LOGL_DEBUG, "-> valid address word: RIC = '%d', function = '%d' (%s)\n", ((word >> 10) & 0x1ffff8) + slot, (word >> 11) & 0x3, pocsag_function_name[(word >> 11) & 0x3]);
	} else {
		LOGP(DPOCSAG, LOGL_DEBUG, "-> valid message word: message = '0x%05x'\n", (word >> 11) & 0xfffff);
	}

	return 0;
}

static uint32_t encode_address(pocsag_msg_t *msg)
{
	uint32_t word;

	/* compose message */
	word = 0x0;

	/* RIC */
	word = (word << 18) | (msg->ric >> 3);
	word = (word << 2) | msg->function;

	word = (word << 10) | pocsag_crc(word);
	word = (word << 1) | pocsag_parity(word);

	return word;
}

static void decode_address(uint32_t word, uint8_t slot, uint32_t *ric, enum pocsag_function *function)
{
	*ric = ((word >> 10) & 0x1ffff8) + slot;
	*function = (word >> 11) & 0x3;
}

static uint32_t encode_numeric(pocsag_msg_t *msg)
{
	uint8_t digit[5] = { 0xc, 0xc, 0xc, 0xc, 0xc };
	int index, i;
	uint32_t word;

	/* get characters from string */
	index = 0;
	while (msg->data_index < msg->data_length) {
		for (i = 0; i < 16; i++) {
			if (numeric[i] == msg->data[msg->data_index])
				break;
		}
		msg->data_index++;
		if (i < 16)
			digit[index++] = i;
		if (index == 5)
			break;
	}

	/* compose message */
	word = 0x1;
	for (i = 0; i < 5; i++) {
		word = (word << 1) | (digit[i] & 0x1);
		word = (word << 1) | ((digit[i] >> 1) & 0x1);
		word = (word << 1) | ((digit[i] >> 2) & 0x1);
		word = (word << 1) | ((digit[i] >> 3) & 0x1);
	}

	word = (word << 10) | pocsag_crc(word);
	word = (word << 1) | pocsag_parity(word);

	return word;
}

static void decode_numeric(pocsag_t *pocsag, uint32_t word)
{
	uint8_t digit;
	int i;

	for (i = 0; i < 5; i++) {
		if (pocsag->rx_msg_data_length == sizeof(pocsag->rx_msg_data))
			return;
		digit = (word >> (27 - i * 4)) & 0x1;
		digit = (digit << 1) | ((word >> (28 - i * 4)) & 0x1);
		digit = (digit << 1) | ((word >> (29 - i * 4)) & 0x1);
		digit = (digit << 1) | ((word >> (30 - i * 4)) & 0x1);
		pocsag->rx_msg_data[pocsag->rx_msg_data_length++] = numeric[digit];
	}
}

static uint32_t encode_alpha(pocsag_msg_t *msg)
{
	int bits;
	uint32_t word;

	/* compose message */
	word = 0x1;
	bits = 0;

	/* get character from string */
	while (msg->data_index < msg->data_length) {
		if ((msg->data[msg->data_index] & 0x80)) {
			msg->data_index++;
			continue;
		}
		while (42) {
			word = (word << 1) | ((msg->data[msg->data_index] >> msg->bit_index) & 1);
			bits++;
			if (++msg->bit_index == 7) {
				msg->bit_index = 0;
				msg->data_index++;
				break;
			}
			if (bits == 20)
				break;
		}
		if (bits == 20)
			break;
	}

	/* fill remaining digit space with 0x04 (EOT) */
	while (bits <= 13) {
		word = (word << 7) | msg->padding;
		bits += 7;
	}

	/* fill remaining bits with '0's */
	if (bits < 20)
		word <<= 20 - bits;

	word = (word << 10) | pocsag_crc(word);
	word = (word << 1) | pocsag_parity(word);

	return word;
}

static void decode_alpha(pocsag_t *pocsag, uint32_t word)
{
	int i;

	for (i = 0; i < 20; i++) {
		if (pocsag->rx_msg_data_length == sizeof(pocsag->rx_msg_data))
			return;
		if (!pocsag->rx_msg_bit_index)
			pocsag->rx_msg_data[pocsag->rx_msg_data_length] = 0x00;
		pocsag->rx_msg_data[pocsag->rx_msg_data_length] >>= 1;
		pocsag->rx_msg_data[pocsag->rx_msg_data_length] |= ((word >> (30 - i)) & 0x1) << 6;
		if (++pocsag->rx_msg_bit_index == 7) {
			pocsag->rx_msg_bit_index = 0;
			pocsag->rx_msg_data_length++;
		}
	}
}

static void decode_hex(pocsag_t *pocsag, uint32_t word)
{
	uint8_t digit;
	int i;

	for (i = 0; i < 5; i++) {
		if (pocsag->rx_msg_data_length == sizeof(pocsag->rx_msg_data))
			return;
		digit = (word >> (27 - i * 4)) & 0x1;
		digit = (digit << 1) | ((word >> (28 - i * 4)) & 0x1);
		digit = (digit << 1) | ((word >> (29 - i * 4)) & 0x1);
		digit = (digit << 1) | ((word >> (30 - i * 4)) & 0x1);
		pocsag->rx_msg_data[pocsag->rx_msg_data_length++] = hex[digit];
	}
}

/* get codeword from scheduler */
int64_t get_codeword(pocsag_t *pocsag)
{
	pocsag_msg_t *msg;
	uint32_t word = 0; // make GCC happy
	uint8_t slot = (pocsag->word_count - 1) >> 1;
	uint8_t subslot = (pocsag->word_count - 1) & 1;

	/* no codeword, if not transmitting */
	if (!pocsag->tx)
		return -1;

	/* transmitter state */
	switch (pocsag->state) {
	case POCSAG_IDLE:
		return -1;
	case POCSAG_PREAMBLE:
		if (!pocsag->word_count)
			LOGP_CHAN(DPOCSAG, LOGL_INFO, "Sending preamble.\n");
		/* transmit preamble */
		LOGP_CHAN(DPOCSAG, LOGL_DEBUG, "Sending 32 bits of preamble pattern 0x%08x.\n", CODEWORD_PREAMBLE);
		if (++pocsag->word_count == PREAMBLE_COUNT) {
			pocsag_new_state(pocsag, POCSAG_MESSAGE);
			pocsag->word_count = 0; 
			pocsag->idle_count = 0;
		}
		word =  CODEWORD_PREAMBLE;
		break;
	case POCSAG_MESSAGE:
		if (!pocsag->word_count)
			LOGP_CHAN(DPOCSAG, LOGL_INFO, "Sending batch.\n");
		/* send sync */
		if (pocsag->word_count == 0) {
			LOGP_CHAN(DPOCSAG, LOGL_DEBUG, "Sending 32 bits of sync pattern 0x%08x.\n", CODEWORD_SYNC);
			/* count codewords */
			++pocsag->word_count;
			word = CODEWORD_SYNC;
			break;
		}
		/* send message data, if there is an ongoing message */
		if ((msg = pocsag->current_msg)) {
			/* reset idle counter */
			pocsag->idle_count = 0;
			/* encode data */
			switch (msg->function) {
			case POCSAG_FUNCTION_NUMERIC:
				word = encode_numeric(msg);
				break;
			case POCSAG_FUNCTION_ALPHA:
				word = encode_alpha(msg);
				break;
			default:
				word = CODEWORD_IDLE; /* should never happen */
			}
			/* if message is complete, reset index and remove message */
			if (msg->data_index == msg->data_length) {
				pocsag->current_msg = NULL;
				msg->data_index = 0;
				pocsag_msg_destroy(msg);
				pocsag_msg_done(pocsag);
			}
			/* prevent 'use-after-free' from this point on */
			msg = NULL;
			LOGP_CHAN(DPOCSAG, LOGL_DEBUG, "Sending 32 bits of message codeword 0x%08x (frame %d.%d).\n", word, slot, subslot);
			/* count codewords */
			if (++pocsag->word_count == 17)
				pocsag->word_count = 0;
			break;
		}
		/* if we are about to send an address codeword, we search for a pending message */
		for (msg = pocsag->msg_list; msg; msg = msg->next) {
			/* if a message matches the right time slot */
			if ((msg->ric & 7) == slot)
				break;
		}
		if (msg) {
			LOGP_CHAN(DPOCSAG, LOGL_INFO, "Sending message to RIC '%d' / function '%d' (%s)\n", msg->ric, msg->function, pocsag_function_name[msg->function]);
			/* reset idle counter */
			pocsag->idle_count = 0;
			/* encode address */
			word = encode_address(msg);
			/* link message, if there is data to be sent */
			if (msg->function == POCSAG_FUNCTION_NUMERIC || msg->function == POCSAG_FUNCTION_ALPHA) {
				LOGP_CHAN(DPOCSAG, LOGL_INFO, " -> Message text is \"%s\".\n", print_message(msg->data, msg->data_length));
				pocsag->current_msg = msg;
				msg->data_index = 0;
				msg->bit_index = 0;
			} else {
				/* remove message */
				pocsag_msg_destroy(msg);
				pocsag_msg_done(pocsag);
				/* prevent 'use-after-free' from this point on */
				msg = NULL;
			}
			LOGP_CHAN(DPOCSAG, LOGL_DEBUG, "Sending 32 bits of address codeword 0x%08x (frame %d.%d).\n", word, slot, subslot);
			/* count codewords */
			if (++pocsag->word_count == 17)
				pocsag->word_count = 0;
			break;
		}
		/* no message, so we send idle pattern */
		LOGP_CHAN(DPOCSAG, LOGL_DEBUG, "Sending 32 bits of idle pattern 0x%08x (frame %d.%d).\n", CODEWORD_IDLE, slot, subslot);
		/* count codewords */
		if (++pocsag->word_count == 17) {
			pocsag->word_count = 0;
			/* if no message has been scheduled during transmission and idle counter is reached, stop transmitter */
			if (!pocsag->msg_list && pocsag->idle_count++ == IDLE_BATCHES) {
				LOGP_CHAN(DPOCSAG, LOGL_INFO, "Transmission done.\n");
				LOGP_CHAN(DPOCSAG, LOGL_DEBUG, "Reached %d of idle batches, turning transmitter off.\n", IDLE_BATCHES);
				pocsag_new_state(pocsag, POCSAG_IDLE);
			}
		}
		word = CODEWORD_IDLE;
		break;
	}

	if (word != CODEWORD_PREAMBLE)
		debug_word(word, slot);

	return word;
}

static void done_rx_msg(pocsag_t *pocsag)
{
	const char *text;

	if (!pocsag->rx_msg_valid)
		return;

	pocsag->rx_msg_valid = 0;

	LOGP_CHAN(DPOCSAG, LOGL_INFO, "Received message from RIC '%d' / function '%d' (%s)\n", pocsag->rx_msg_ric, pocsag->rx_msg_function, pocsag_function_name[pocsag->rx_msg_function]);
	text = print_message(pocsag->rx_msg_data, pocsag->rx_msg_data_length);
	if (pocsag->rx_msg_function == POCSAG_FUNCTION_NUMERIC || pocsag->rx_msg_function == POCSAG_FUNCTION_ALPHA)
		LOGP_CHAN(DPOCSAG, LOGL_INFO, " -> Message text is \"%s\".\n", text);
	pocsag_msg_receive(pocsag->language, pocsag->sender.kanal, pocsag->rx_msg_ric, pocsag->rx_msg_function, text);
}

void put_codeword(pocsag_t *pocsag, uint32_t word, int8_t slot, int8_t subslot)
{
	int rc;

	if (slot < 0 && word == CODEWORD_SYNC) {
		LOGP_CHAN(DPOCSAG, LOGL_DEBUG, "Received 32 bits of sync pattern 0x%08x.\n", CODEWORD_SYNC);
		return;
	}

	if (word == CODEWORD_IDLE) {
		LOGP_CHAN(DPOCSAG, LOGL_DEBUG, "Received 32 bits of idle pattern 0x%08x.\n", CODEWORD_IDLE);
	} else
	if (!(word & 0x80000000))
		LOGP_CHAN(DPOCSAG, LOGL_DEBUG, "Received 32 bits of address codeword 0x%08x (frame %d.%d).\n", word, slot, subslot);
	else
		LOGP_CHAN(DPOCSAG, LOGL_DEBUG, "Received 32 bits of message codeword 0x%08x (frame %d.%d).\n", word, slot, subslot);
	rc = debug_word(word, slot);
	if (rc < 0) {
		done_rx_msg(pocsag);
		return;
	}

	if (word == CODEWORD_IDLE) {
		done_rx_msg(pocsag);
		return;
	}

	if (!(word & 0x80000000)) {
		done_rx_msg(pocsag);
		pocsag->rx_msg_valid = 1;
		decode_address(word, slot, &pocsag->rx_msg_ric, &pocsag->rx_msg_function);
		pocsag->rx_msg_data_length = 0;
		pocsag->rx_msg_bit_index = 0;
	} else {
		if (!pocsag->rx_msg_valid)
			return;
		switch (pocsag->rx_msg_function) {
		case POCSAG_FUNCTION_NUMERIC:
			decode_numeric(pocsag, word);
			break;
		case POCSAG_FUNCTION_ALPHA:
			decode_alpha(pocsag, word);
			break;
		default:
			decode_hex(pocsag, word);
			;
		}
	}
}

