/* V27 Modem
 *
 * (C) 2020 by Andreas Eversberg <jolly@eversberg.eu>
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
#include "../libdebug/debug.h"
#include "../libsample/sample.h"
#include "modem.h"

static int psk_send_bit(void *inst)
{
	v27modem_t *modem = (v27modem_t *)inst;
	uint8_t bit;

	bit = modem->send_bit(modem->inst);
	bit = v27_scrambler_bit(&modem->scrambler, bit);

	return bit;
}

static void psk_receive_bit(void *inst, int bit)
{
	v27modem_t *modem = (v27modem_t *)inst;

	bit = v27_scrambler_bit(&modem->descrambler, bit);
	modem->receive_bit(modem->inst, bit);
}

/* init psk */
int v27_modem_init(v27modem_t *modem, void *inst, int (*send_bit)(void *inst), void (*receive_bit)(void *inst, int bit), int samplerate, int bis)
{
	int rc = 0;

	memset(modem, 0, sizeof(*modem));

	modem->send_bit = send_bit;
	modem->receive_bit = receive_bit;
	modem->inst = inst;

	/* V.27bis/ter with 4800 bps */
	rc = psk_mod_init(&modem->psk_mod, modem, psk_send_bit, samplerate, 1600.0);
	if (rc)
		goto error;
	rc = psk_demod_init(&modem->psk_demod, modem, psk_receive_bit, samplerate, 1600.0);
	if (rc)
		goto error;
	v27_scrambler_init(&modem->scrambler, bis, 0);
	v27_scrambler_init(&modem->descrambler, bis, 1);

	return 0;

error:
	v27_modem_exit(modem);
	return rc;
}

void v27_modem_exit(v27modem_t *modem)
{
	psk_mod_exit(&modem->psk_mod);
	psk_demod_exit(&modem->psk_demod);
}

void v27_modem_send(v27modem_t *modem, sample_t *sample, int length)
{
	psk_mod(&modem->psk_mod, sample, length);
}

void v27_modem_receive(v27modem_t *modem, sample_t *sample, int length)
{
	psk_demod(&modem->psk_demod, sample, length);
}

