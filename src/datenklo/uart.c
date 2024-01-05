/* Software UART
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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "../liblogging/logging.h"
#include "uart.h"

static uint32_t calc_parity(uint32_t data, uint8_t data_bits, enum uart_parity parity)
{
	int i;

	for (i = 0; i < data_bits; i++)
		parity |= (data >> i);
	parity &= 1;

	switch (parity) {
	case UART_PARITY_NONE:
	case UART_PARITY_SPACE:
		return 0;
	case UART_PARITY_MARK:
		return 1;
	case UART_PARITY_EVEN:
		return parity;
	case UART_PARITY_ODD:
		return parity ^ 1;
	}

	return 0; /* never reached */
}

int uart_init(uart_t *uart, void *inst, uint8_t data_bits, enum uart_parity parity, uint8_t stop_bits, int (*tx_cb)(void *inst), void (*rx_cb)(void *inst, int data, uint32_t flags))
{
	memset(uart, 0, sizeof(*uart));

	uart->inst = inst;
	uart->tx_cb = tx_cb;
	uart->rx_cb = rx_cb;
	uart->data_bits = data_bits;
	if (uart->data_bits > 9) {
		LOGP(DUART, LOGL_ERROR, "Illegal number of data bits, please fix!\n");
		abort();
	}
	uart->parity = parity;
	uart->stop_bits = stop_bits;
	if (uart->stop_bits < 1 || uart->stop_bits > 2) {
		LOGP(DUART, LOGL_ERROR, "Illegal number of stop bits, please fix!\n");
		abort();
	}
	uart->tx_pos = -1;
	uart->rx_pos = -1;
	uart->length = uart->stop_bits + !!uart->parity + uart->data_bits;

	return 0;
}

/* called by modulator to get next bit from uart */
int uart_tx_bit(uart_t *uart)
{
	uint32_t bit, parity;

	if (uart->tx_pos < 0) {
		/* no transmission, get data */
		uart->tx_data = uart->tx_cb(uart->inst);
		/* return 1, if no data has not be sent */
		if (uart->tx_data > 0x7fffffff)
			return 1;
		/* all bits after data are stop bits */
		uart->tx_data |= 0xffffffff << uart->data_bits;
		/* calculate parity */
		if (uart->parity)
			parity = calc_parity(uart->tx_data, uart->data_bits, uart->parity);
		/* add parity bit */
		if (uart->parity) {
			/* erase bit for parity */
			uart->tx_data ^= 1 << uart->data_bits;
			/* put parity bit */
			uart->tx_data |= parity << uart->data_bits;
		}
		/* start with the first bit */
		uart->tx_pos = 0;
		/* return start bit */
		return 0;
	}
	/* get bit to be send */
	bit = (uart->tx_data >> uart->tx_pos) & 1;
	/* go to next bit and set tx_pos to -1, if there is no more bit */
	if (++uart->tx_pos == uart->length)
		uart->tx_pos = -1;
	/* return bit */
	return bit;
}

int uart_is_tx(uart_t *uart)
{
	if (uart->tx_pos >= 0)
		return 1;
	return 0;
}

/* called by demodulator to indicate bit for uart */
void uart_rx_bit(uart_t *uart, int bit)
{
	uint32_t flags = 0;
	uint32_t parity;

	bit &= 1;

	/* if no data is receivd, check for start bit */
	if (uart->rx_pos < 0) {
		/* if no start bit */
		if (bit != 0 || uart->last_bit != 1)
			goto out;
		/* start bit */
		uart->rx_data = 0;
		uart->rx_pos = 0;
		return;
	}
	/* shift bit */
	uart->rx_data |= bit << (uart->rx_pos);
	/* end of transmission */
	if (++uart->rx_pos == uart->length) {
		/* turn off reception */
		uart->rx_pos = -1;
		/* check if parity is invalid */
		if (uart->parity) {
			parity = calc_parity(uart->rx_data, uart->data_bits, uart->parity);
			if (((uart->rx_data >> uart->data_bits) & 1) != parity)
				flags |= UART_PARITY_ERROR;
		}
		/* check if last stop bit is invalid */
		if (((uart->rx_data >> (uart->length - 1)) & 1) == 0) {
			flags |= UART_CODE_VIOLATION;
		}
		/* check if all bits are 0 */
		if (!uart->rx_data) {
			flags |= UART_BREAK;
		}
		/* clear all bits after data */
		uart->rx_data &= ~(0xffffffff << uart->data_bits);
		uart->rx_cb(uart->inst, uart->rx_data, flags);
	}

out:
	/* remember last bit for start bit detection (1 -> 0 transition) */
	uart->last_bit = bit;
}

