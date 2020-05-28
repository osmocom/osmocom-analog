/* SIM card emulator
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
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#ifndef ARDUINO
#include "../libdebug/debug.h"
#endif
#include "sim.h"
#include "eeprom.h"

#ifdef ARDUINO
#define PDEBUG(cat, level, fmt, arg...) while(0)
#define EINVAL 22
static uint32_t my_strtoul(const char *nptr, char **endptr, int base)
{
	uint32_t number = 0;

	while (*nptr >= '0' && *nptr <= '9')
		number = number * 10 + (*nptr++ - '0');

	return number;
}
#else
#define my_strtoul strtoul
#endif

static void my_ultostr(char *nptr, uint32_t value, int zeros)
{
	int digits = 0;
	uint32_t temp;

	/* count digits */
	temp = value;
	while (temp) {
		temp /= 10;
		digits++;
	}

	/* minium digits to fill up with '0' */
	if (digits < zeros)
		digits = zeros;

	/* go to end and terminate */
	nptr += digits;
	*nptr-- = '\0';

	/* apply digits backwards */
	while (digits--) {
		*nptr-- = (value % 10) + '0';
		value /= 10;
	}
}

static void tx_sdu(sim_sim_t *sim, uint8_t ccrc, uint8_t *data, int length);
static void tx_pdu(sim_sim_t *sim, uint8_t *data, int length);
static void tx_block(sim_sim_t *sim, enum l2_cmd cmd, uint8_t *data, int length);

/* read flags from eeprom */
static void read_flags(sim_sim_t *sim)
{
	uint8_t flags;

	flags = eeprom_read(EEPROM_FLAGS);
	sim->pin_len = (flags >> EEPROM_FLAG_PIN_LEN) & 0xf;
	sim->pin_try = (flags >> EEPROM_FLAG_PIN_TRY) & 0x3;
	if ((flags >> EEPROM_FLAG_GEBZ) & 0x1)
		sim->gebz_locked = 1;
	if ((flags >> EEPROM_FLAG_APP) & 0x1)
		sim->app_locked = 1;
}

/* write flags to eeprom */
static void write_flags(sim_sim_t *sim)
{
	uint8_t flags = 0;

	flags |= sim->pin_len << EEPROM_FLAG_PIN_LEN;
	flags |= sim->pin_try << EEPROM_FLAG_PIN_TRY;
	if (sim->gebz_locked)
		flags |= (1 << EEPROM_FLAG_GEBZ);
	if (sim->app_locked)
		flags |= (1 << EEPROM_FLAG_APP);
	eeprom_write(EEPROM_FLAGS, flags);
}

/* encode EBDT from strings */
int encode_ebdt(uint8_t *data, const char *futln, const char *sicherung, const char *karten, const char *sonder, const char *wartung)
{
	uint32_t temp;
	int i;

	if (futln) {
		temp = strlen(futln);
		if (temp < 7 || temp > 8) {
			PDEBUG(DSIM7, DEBUG_NOTICE, "Given FUTLN '%s' invalid length. (Must be 7 or 8 Digits)\n", futln);
			return -EINVAL;
		}
		if (futln[0] < '0' || futln[0] > '7') {
			PDEBUG(DSIM7, DEBUG_NOTICE, "Given FUTLN '%s' has invalid first digit. (Must be '0' .. '7')\n", futln);
			return -EINVAL;
		}
		data[0] = (futln[0] - '0') << 5;
		futln++;
		if (temp == 8) {
			/* 8 digits */
			temp = (futln[0] - '0') * 10 + (futln[1] - '0');
			if (futln[0] < '0' || futln[0] > '9' || futln[1] < '0' || futln[1] > '9' || temp > 31) {
				PDEBUG(DSIM7, DEBUG_NOTICE, "Given FUTLN '%s' has invalid second and third digit. (Must be '00' .. '31')\n", futln);
				return -EINVAL;
			}
			data[0] |= temp;
			futln += 2;
		} else {
			/* 7 digits */
			if (futln[0] < '0' || futln[0] > '9') {
				PDEBUG(DSIM7, DEBUG_NOTICE, "Given FUTLN '%s' has invalid second digit. (Must be '0' .. '9')\n", futln);
				return -EINVAL;
			}
			data[0] |= (futln[0] - '0');
			futln++;
		}
		for (i = 0; i < 5; i++) {
			if (futln[i] < '0' || futln[i] > '9')
				break;
		}
		temp = my_strtoul(futln, NULL, 0);
		if (i < 5 || temp > 65535) {
			PDEBUG(DSIM7, DEBUG_NOTICE, "Given FUTLN '%s' has invalid last digits. (Must be '00000' .. '65535')\n", futln);
			return -EINVAL;
		}
		data[1] = temp >> 8;
		data[2] = temp;
	}

	if (sicherung) {
		temp = my_strtoul(sicherung, NULL, 0);
		if (temp > 65535) {
			PDEBUG(DSIM7, DEBUG_NOTICE, "Given security code '%s' has invalid digits. (Must be '0' .. '65535')\n", sicherung);
			return -EINVAL;
		}
		data[3] = temp >> 8;
		data[4] = temp;
	}

	if (karten) {
		temp = my_strtoul(karten, NULL, 0);
		if (temp > 7) {
			PDEBUG(DSIM7, DEBUG_NOTICE, "Given card number '%s' has invalid digit. (Must be '0' .. '7')\n", karten);
			return -EINVAL;
		}
		data[5] = (data[5] & 0x1f) | ((karten[0] - '0') << 5);
	}

	if (sonder) {
		temp = my_strtoul(sonder, NULL, 0);
		if (temp > 8191) {
			PDEBUG(DSIM7, DEBUG_NOTICE, "Given spacial code '%s' has invalid digits. (Must be '0' .. '8191')\n", sonder);
			return -EINVAL;
		}
		data[5] = (data[5] & 0xe0) | (temp >> 8);
		data[6] = temp;
	}

	if (wartung) {
		temp = my_strtoul(wartung, NULL, 0);
		if (temp > 65535) {
			PDEBUG(DSIM7, DEBUG_NOTICE, "Given maintenance code '%s' has invalid digits. (Must be '0' .. '65535')\n", wartung);
			return -EINVAL;
		}
		data[7] = temp >> 8;
		data[8] = temp;
	}

	return 0;
}

/* convert EBDT to string */
void decode_ebdt(uint8_t *data, char *futln, char *sicherung, char *karten, char *sonder, char *wartung)
{
	if (futln) {
		/* second value becomes two digits automatically, if > 9 */
		my_ultostr(futln++, data[0] >> 5, 1);
		my_ultostr(futln++, data[0] & 0x1f, 1);
		if (*futln)
			futln++;
		my_ultostr(futln, ((uint16_t)data[1] << 8) | (uint16_t)data[2], 5);
	}

	if (sicherung)
		my_ultostr(sicherung, ((uint16_t)data[3] << 8) | (uint16_t)data[4], 1);

	if (karten)
		my_ultostr(karten, data[5] >> 5, 1);

	if (sonder)
		my_ultostr(sonder, ((uint16_t)(data[5] & 0x1f) << 8) | (uint16_t)data[6], 1);

	if (wartung)
		my_ultostr(wartung, ((uint16_t)data[7] << 8) | (uint16_t)data[8], 1);
}

/* get size of phone directory (including allocation map) */
int directory_size(void)
{
	/* get size from space in eeprom */
	int size = (eeprom_length() - EEPROM_RUFN) / 24;

	/* may have 184 entries (23*8) plus allocation map (entry 0) */
	if (size > 184 + 1)
		size = 184 + 1;

	return size;
}

/* store one phone number in the directory; also set allocation mask) */
int save_directory(int location, uint8_t *data)
{
	int size, i, pos;
	uint8_t mask;

	size = directory_size();
	if (location < 1 || location >= size) {
		PDEBUG(DSIM7, DEBUG_NOTICE, "Given location for phone number '%d' is out of range. (Must be '01' .. '%02d')\n", location, size - 1);
		return -EINVAL;
	}

	/* store entry */
	for (i = 0; i < 24; i++)
		eeprom_write(EEPROM_RUFN + 24 * location + i, data[i]);
	/* set bit mask */
	pos = EEPROM_RUFN + 1 + ((location - 1) >> 3);
	mask = eeprom_read(pos);
	if ((data[7] & 0xf) == 0xf)
		mask |= (0x80 >> ((location - 1) & 7));
	else
		mask &= ~(0x80 >> ((location - 1) & 7));
	eeprom_write(pos, mask);

	return 0;
}

/* load one phone number from the directory; location 0 is the allocation mask) */
void load_directory(int location, uint8_t *data)
{
	int i;

	for (i = 0; i < 24; i++)
		data[i] = eeprom_read(EEPROM_RUFN + 24 * location + i);
	/* set directory size, on allocation map */
	if (location == 0)
		data[0] = directory_size() - 1;
}

/* encode number an name into directory data */
int encode_directory(uint8_t *data, const char *number, const char *name)
{
	int len, pos, i;

	len = strlen(number);
	if (len > 16) {
		PDEBUG(DSIM7, DEBUG_NOTICE, "Given phone number '%s' has too many digits. (Must be <= 16)\n", number);
		return -EINVAL;
	}

	memset(data, 0xff, 8);
	memset(data + 8, ' ', 16);
	for (i = 0; i < len; i++) {
		if (number[i] < '0' || number[i] > '9') {
			PDEBUG(DSIM7, DEBUG_NOTICE, "Given phone number '%s' has illegal digits. (Must be '0' .. '9')\n", number);
			return -EINVAL;
		}
		pos = 16 - len + i;
		if ((pos & 1) == 0)
			data[pos >> 1] += ((number[i] - '0') << 4) - 0xf0;
		else
			data[pos >> 1] += number[i] - '0' - 0xf;
	}
	len = strlen(name);
	if (len > 16)
		len = 16;
	for (i = 0; i < len; i++) {
		pos = 8 + i;
		data[pos] = name[i];
	}

	return 0;
}

void decode_directory(uint8_t *data, char *number, char *name)
{
	int i, j;
	char digit;

	if (number) {
		j = 0;
		for (i = 0; i < 16; i++) {
			if ((i & 1) == 0)
				digit = (data[i >> 1] >> 4) + '0';
			else
				digit = (data[i >> 1] & 0xf) + '0';
			if (digit <= '9')
				number[j++] = digit;
		}
		number[j] = '\0';
	}

	if (name) {
		memcpy(name, data + 8, 16);
		name[16] = '\0';
		/* remove spaces in the end of the string */
		for (i = 16 - 1; i >= 0; i--) {
			if (name[i] != ' ')
				break;
			name[i] = '\0';
		}
	}
}

/* get APRC of NETZ-C application */
static uint8_t get_aprc(sim_sim_t *sim)
{
	uint8_t aprc = 0x00;

	if (sim->pin_required)
		aprc |= APRC_PIN_REQ;
	if (sim->app_locked)
		aprc |= APRC_APP_LOCKED;
	if (sim->gebz_locked)
		aprc |= APRC_GEBZ_LOCK;
	if (sim->gebz_full)
		aprc |= APRC_GEBZ_FULL;

	return aprc;
}

/* validate PIN and change states */
static int validate_pin(sim_sim_t *sim, uint8_t *data, int length)
{
	uint8_t valid = 0, program_mode = 0;
	int i;

	if (!sim->pin_required)
		return 0;

	/* no PIN mode */
	if (length == 4 && data[0] == '0' && data[1] == '0' && data[2] == '0' && data[3] >= '0' && data[3] <= '0' + MAX_CARDS) {
		valid = 1;
		if (data[3] > '0')
			sim->card = data[3] - '1';
		PDEBUG(DSIM1, DEBUG_INFO, "System PIN '000%c' entered. Selecting card #%d.\n", data[3], sim->card + 1);
	}

	/* programming mode */
	if (length == 4 && data[0] == '9' && data[1] == '9' && data[2] == '9' && data[3] >= '0' && data[3] <= '0' + MAX_CARDS) {
		program_mode = 1;
		valid = 1;
		if (data[3] > '0')
			sim->card = data[3] - '1';
		PDEBUG(DSIM1, DEBUG_INFO, "Configuration PIN '999%c' entered. Selecting card #%d in configuration mode.\n", data[3], sim->card + 1);
	}

	/* if not 'program mode' and PIN matches EEPROM */
	if (!valid && length == sim->pin_len) {
		for (i = 0; i < length; i++) {
			if (data[i] != eeprom_read(EEPROM_PIN_DATA + i))
				break;
		}
		if (i == length) {
			valid = 1;
			PDEBUG(DSIM1, DEBUG_INFO, "Correct PIN was entered. Selecting card #%d.\n", sim->card + 1);
		}
	}

	if (valid) {
		/* prevent permanent write when not needed */
		if (sim->pin_try != MAX_PIN_TRY) {
			sim->pin_try = MAX_PIN_TRY;
			write_flags(sim);
		}
		sim->pin_required = 0;
		if (program_mode)
			sim->program_mode = 1;
		return 0;
	} else {
		PDEBUG(DSIM1, DEBUG_INFO, "Wrong PIN was entered.\n");
#ifndef ARDUINO
		/* decrement error counter */
		if (sim->pin_try) {
			sim->pin_try--;
			write_flags(sim);
		}
#endif
		return -EINVAL;
	}
}

/* message buffer handling */

/* get space for return message */
uint8_t *alloc_msg(sim_sim_t *sim, int size)
{
	/* we add 4, because we push 4 bytes (ICL and L2 header later) */
	if (size + 4 > (int)sizeof(sim->block_tx_data))
		PDEBUG(DSIM1, DEBUG_NOTICE, "TX buffer overflow: size+4=%d > buffer size (%d)\n", size + 4, (int)sizeof(sim->block_tx_data));
	return sim->block_tx_data;
}

/* push space in front of a message */
uint8_t *push_msg(uint8_t *data, int length, int offset)
{
	int i;

	for (i = length - 1; i >= 0; --i)
		data[i + offset] = data[i];

	return data;
}

/* Layer 7 */

static void return_error(sim_sim_t *sim)
{
	uint8_t *data;

	data = alloc_msg(sim, 0);
	tx_sdu(sim, CCRC_ERROR, data, 0);
}

static void return_pin_not_ok(sim_sim_t *sim)
{
	uint8_t *data;

	data = alloc_msg(sim, 0);
	tx_sdu(sim, CCRC_PIN_NOK, data, 0);
}

/* command: open application */
static void sl_appl(sim_sim_t *sim, uint8_t *data, int length)
{
	uint8_t app;

	if (length < 11) {
		PDEBUG(DSIM7, DEBUG_NOTICE, "SL-APPL too short\n");
		return_error(sim);
		return;
	}

	/* application number */
	app = (data[6] - '0') * 100;
	app += (data[7] - '0') * 10;
	app += data[8] - '0';

	PDEBUG(DSIM7, DEBUG_INFO, " SL-APPL app %d\n", app);

	/* check and set application */
	if (app != APP_NETZ_C && app != APP_RUFN_GEBZ) {
		PDEBUG(DSIM7, DEBUG_NOTICE, "SL-APPL invalid app %d\n", sim->app);
		return_error(sim);
		return;
	}
	sim->app = app;

	/* if PIN is required, we request it, but we've already selected the app */
	if (sim->pin_required) {
		return_pin_not_ok(sim);
		return;
	}

	/* respond */
	data = alloc_msg(sim, 0);
	tx_sdu(sim, 0, data, 0);
}

/* command: close application */
static void cl_appl(sim_sim_t *sim)
{
	uint8_t *data;

	PDEBUG(DSIM7, DEBUG_INFO, " CL-APPL\n");

	/* remove app */
	sim->app = 0;

	/* respond */
	data = alloc_msg(sim, 0);
	tx_sdu(sim, 0, data, 0);
}

/* command: show application */
static void sh_appl(sim_sim_t *sim)
{
	uint8_t *data;

	PDEBUG(DSIM7, DEBUG_INFO, " SH-APPL\n");

	/* respond */
	data = alloc_msg(sim, 33);
	switch (sim->sh_appl_count) {
	case 0: // first application is shown
		/* L */
		data[0] = 11;
		/* APP-IDN */
		data[1] = '8'; data[2] = '9';
		data[3] = '4'; data[4] = '9';
		data[5] = '0'; data[6] = '1';
		data[7] = '0'; data[8] = '0'; data[9] = '3';
		data[10] = '0'; data[11] = '1';
		/* APP-TXT */
		memcpy(data + 12, "Netz C              ", 20);
		/* APP-STS */
		data[32] = get_aprc(sim);
		tx_sdu(sim, 0, data, 33);
		sim->sh_appl_count++;
		break;
	default: // no more application
		tx_sdu(sim, 0, data, 0);
		sim->sh_appl_count = 0;
	}
}

/* command: show state of chip card */
static void chk_kon(sim_sim_t *sim)
{
	uint8_t *data;

	PDEBUG(DSIM7, DEBUG_INFO, " CHK-KON\n");

	/* respond */
	data = alloc_msg(sim, 0);
	tx_sdu(sim, 0, data, 0);
}

/* command: read subscriber data */
static void rd_ebdt(sim_sim_t *sim)
{
	uint8_t *data;

	PDEBUG(DSIM7, DEBUG_INFO, " RD-EBDT\n");

	/* respond */
	data = alloc_msg(sim, 9);
	if (sim->program_mode) {
		/* SERVICE MODE */
		data[0] = 0;
		data[1] = 0;
		data[2] = sim->card + 1;
		data[3] = 12345 >> 8;
		data[4] = 12345 & 0xff;
		data[5] = 3 << 5;
		data[6] = 0;
		data[7] = 0x0ff;
		data[8] = 0x0ff;
	} else {
		data[0] = eeprom_read(EEPROM_FUTLN_H + sim->card);
		data[1] = eeprom_read(EEPROM_FUTLN_M + sim->card);
		data[2] = eeprom_read(EEPROM_FUTLN_L + sim->card);
		data[3] = eeprom_read(EEPROM_SICH_H + sim->card);
		data[4] = eeprom_read(EEPROM_SICH_L + sim->card);
		data[5] = eeprom_read(EEPROM_SONDER_H + sim->card);
		data[6] = eeprom_read(EEPROM_SONDER_L + sim->card);
		data[7] = eeprom_read(EEPROM_WARTUNG_H + sim->card);
		data[8] = eeprom_read(EEPROM_WARTUNG_L + sim->card);
	}
	tx_sdu(sim, 0, data, 9);
}

/* command: read phone directory */
static void rd_rufn(sim_sim_t *sim, uint8_t *data, int length)
{
	uint8_t rufn = data[0];
	int size;

	if (length < 1) {
		PDEBUG(DSIM7, DEBUG_NOTICE, "RD_RUFN too short\n");
		return_error(sim);
		return;
	}

	PDEBUG(DSIM7, DEBUG_INFO, " RD-RUFN (loc=%d)\n", rufn);

	/* SERVICE MODE */
	if (sim->program_mode) {
		char number[16];

		/* respond */
		data = alloc_msg(sim, 24);
		switch (rufn) {
		case 0: /* send bitmap for service mode */ 
			memset(data, 0xff, 24);
			data[0] = 5; /* 5 entries */
			data[1] = 0x07; /* upper 5 bits = 0 */
			break;
		case 1: /* FUTLN */
			data[0] = eeprom_read(EEPROM_FUTLN_H + sim->card);
			data[1] = eeprom_read(EEPROM_FUTLN_M + sim->card);
			data[2] = eeprom_read(EEPROM_FUTLN_L + sim->card);
			decode_ebdt(data, number, NULL, NULL, NULL, NULL);
			encode_directory(data, number, "FUTLN");
			PDEBUG(DSIM7, DEBUG_INFO, "service mode: FUTLN = %s\n", number);
			break;
		case 2: /* security code */
			data[3] = eeprom_read(EEPROM_SICH_H + sim->card);
			data[4] = eeprom_read(EEPROM_SICH_L + sim->card);
			decode_ebdt(data, NULL, number, NULL, NULL, NULL);
			encode_directory(data, number, "Sicherungscode");
			PDEBUG(DSIM7, DEBUG_INFO, "service mode: security = %s\n", number);
			break;
		case 3: /* card ID */
			data[5] = eeprom_read(EEPROM_SONDER_H + sim->card);
			decode_ebdt(data, NULL, NULL, number, NULL, NULL);
			encode_directory(data, number, "Kartenkennung");
			PDEBUG(DSIM7, DEBUG_INFO, "service mode: card = %s\n", number);
			break;
		case 4:	/* special key */
			data[5] = eeprom_read(EEPROM_SONDER_H + sim->card);
			data[6] = eeprom_read(EEPROM_SONDER_L + sim->card);
			decode_ebdt(data, NULL, NULL, NULL, number, NULL);
			encode_directory(data, number, "Sonderheitsschl.");
			PDEBUG(DSIM7, DEBUG_INFO, "service mode: special = %s\n", number);
			break;
		case 5: /* maintenance key */
			data[7] = eeprom_read(EEPROM_WARTUNG_H + sim->card);
			data[8] = eeprom_read(EEPROM_WARTUNG_L + sim->card);
			decode_ebdt(data, NULL, NULL, NULL, NULL, number);
			encode_directory(data, number, "Wartungsschl.");
			PDEBUG(DSIM7, DEBUG_INFO, "service mode: maintenance = %s\n", number);
			break;
		}
		tx_sdu(sim, 0, data, 24);
		return;
	}

	size = directory_size();
	/* first entry (0) is used as allocation map */
	PDEBUG(DSIM7, DEBUG_INFO, " %d numbers can be stored in EEPROM\n", size - 1);
	if (rufn >= size) {
		PDEBUG(DSIM7, DEBUG_NOTICE, "RD_RUFN entry #%d out of range\n", rufn);
		return_error(sim);
		return;
	}

	/* respond */
	data = alloc_msg(sim, 24);
	load_directory(rufn, data);
	tx_sdu(sim, 0, data, 24);
}

/* command: write phone directory */
static void wt_rufn(sim_sim_t *sim, uint8_t *data, int length)
{
	uint8_t rufn = data[0];
	
	if (length < 25) {
		PDEBUG(DSIM7, DEBUG_NOTICE, "WT_RUFN too short\n");
		return_error(sim);
		return;
	}

	PDEBUG(DSIM7, DEBUG_INFO, " WT-RUFN (loc=%d)\n", rufn);

	/* SERVICE MODE */
	if (sim->program_mode) {
		int rc;
		char number[17];

		decode_directory(data + 1, number, NULL);
		/* if number is cleared, we ignore that */
		if (number[0] == '\0')
			goto respond;
		switch (rufn) {
		case 1: /* FUTLN */
			PDEBUG(DSIM7, DEBUG_INFO, "service mode: FUTLN = %s\n", number);
			rc = encode_ebdt(data, number, NULL, NULL, NULL, NULL);
			if (rc < 0)
				break;
			eeprom_write(EEPROM_FUTLN_H + sim->card, data[0]);
			eeprom_write(EEPROM_FUTLN_M + sim->card, data[1]);
			eeprom_write(EEPROM_FUTLN_L + sim->card, data[2]);
			break;
		case 2: /* security code */
			PDEBUG(DSIM7, DEBUG_INFO, "service mode: security = %s\n", number);
			rc = encode_ebdt(data, NULL, number, NULL, NULL, NULL);
			if (rc < 0)
				break;
			eeprom_write(EEPROM_SICH_H + sim->card, data[3]);
			eeprom_write(EEPROM_SICH_L + sim->card, data[4]);
			break;
		case 3: /* card ID */
			PDEBUG(DSIM7, DEBUG_INFO, "service mode: card = %s\n", number);
			data[5] = eeprom_read(EEPROM_SONDER_H + sim->card);
			rc = encode_ebdt(data, NULL, NULL, number, NULL, NULL);
			if (rc < 0)
				break;
			eeprom_write(EEPROM_SONDER_H + sim->card, data[5]);
			break;
		case 4:	/* special key */
			PDEBUG(DSIM7, DEBUG_INFO, "service mode: special = %s\n", number);
			data[5] = eeprom_read(EEPROM_SONDER_H + sim->card);
			rc = encode_ebdt(data, NULL, NULL, NULL, number, NULL);
			if (rc < 0)
				break;
			eeprom_write(EEPROM_SONDER_H + sim->card, data[5]);
			eeprom_write(EEPROM_SONDER_L + sim->card, data[6]);
			break;
		case 5: /* maintenance key */
			PDEBUG(DSIM7, DEBUG_INFO, "service mode: maintenance = %s\n", number);
			rc = encode_ebdt(data, NULL, NULL, NULL, NULL, number);
			if (rc < 0)
				break;
			eeprom_write(EEPROM_WARTUNG_H + sim->card, data[7]);
			eeprom_write(EEPROM_WARTUNG_L + sim->card, data[8]);
			break;
		}
		/* respond */
		goto respond;
	}

	if (rufn >= directory_size() || rufn < 1) {
		PDEBUG(DSIM7, DEBUG_NOTICE, "WT_RUFN entry #%d out of range\n", rufn);
		return_error(sim);
		return;
	}

	save_directory(data[0], data + 1);

	/* respond */
respond:
	data = alloc_msg(sim, 0);
	tx_sdu(sim, 0, data, 0);
}

/* command: check PIN (enter PIN and unlock) */
static void chk_pin(sim_sim_t *sim, uint8_t *data, int length)
{
	int rc;

	PDEBUG(DSIM7, DEBUG_INFO, " CHK-PIN\n");

	if (length < 4 || length > 8) {
		PDEBUG(DSIM7, DEBUG_NOTICE, "SET-PIN wrong length: %d\n", length);
		return_error(sim);
		return;
	}

	/* validate PIN */
	rc = validate_pin(sim, data, length);
	if (rc) {
		return_pin_not_ok(sim);
		return;
	}

	/* respond */
	data = alloc_msg(sim, 0);
	tx_sdu(sim, 0, data, 0);
}

/* command: set PIN */
static void set_pin(sim_sim_t *sim, uint8_t *data, int length)
{
	uint8_t len_old, len_new;
	uint8_t *pin_old, *pin_new;
	int i;
	int rc;

	PDEBUG(DSIM7, DEBUG_INFO, " SET-PIN\n");

	if (length < 1) {
		PDEBUG(DSIM7, DEBUG_NOTICE, "SET-PIN too short\n");
		return_error(sim);
		return;
	}

	len_old = data[0];
	pin_old = data + 1;
	len_new = length - len_old - 1;
	pin_new = data + 1 + len_old;
	if (len_new < 4 || len_new > 8) {
		PDEBUG(DSIM7, DEBUG_NOTICE, "New PIN wrong length %d!\n", len_new);
		return_error(sim);
		return;
	}

	/* validate PIN */
	rc = validate_pin(sim, pin_old, length);
	if (rc) {
		return_pin_not_ok(sim);
		return;
	}

	/* write PIN */
	sim->pin_len = len_new;
	write_flags(sim);
	for (i = 0; i < len_new; i++)
		eeprom_write(EEPROM_PIN_DATA + i, pin_new[i]);

	/* respond */
	data = alloc_msg(sim, 0);
	tx_sdu(sim, 0, data, 0);
}

/* command: increment metering counter */
static void eh_gebz(sim_sim_t *sim, uint8_t *data, int length)
{
	uint32_t gebz;

	PDEBUG(DSIM7, DEBUG_INFO, " EH-GEBZ\n");

	if (length < 1) {
		PDEBUG(DSIM7, DEBUG_NOTICE, "EH-GEBZ wrong length: %d\n", length);
		return_error(sim);
		return;
	}

	/* increment counter */
	gebz = eeprom_read(EEPROM_GEBZ_H) << 16;
	gebz |= eeprom_read(EEPROM_GEBZ_M) << 8;
	gebz |= eeprom_read(EEPROM_GEBZ_L);
	gebz += data[0];
	eeprom_write(EEPROM_GEBZ_H, gebz >> 16);
	eeprom_write(EEPROM_GEBZ_M, gebz >> 8);
	eeprom_write(EEPROM_GEBZ_L, gebz);

	/* respond */
	data = alloc_msg(sim, 0);
	tx_sdu(sim, 0, data, 0);
}

/* command: clear metering counter */
static void cl_gebz(sim_sim_t *sim)
{
	uint8_t *data;

	PDEBUG(DSIM7, DEBUG_INFO, " CL-GEBZ\n");

	/* clear counter */
	eeprom_write(EEPROM_GEBZ_H, 0);
	eeprom_write(EEPROM_GEBZ_M, 0);
	eeprom_write(EEPROM_GEBZ_L, 0);

	/* respond */
	data = alloc_msg(sim, 0);
	tx_sdu(sim, 0, data, 0);
}

/* command: read metering counter */
static void rd_gebz(sim_sim_t *sim)
{
	uint8_t *data;

	PDEBUG(DSIM7, DEBUG_INFO, " RD-GEBZ\n");

	/* respond */
	data = alloc_msg(sim, 3);
	data[0] = eeprom_read(EEPROM_GEBZ_H);
	data[1] = eeprom_read(EEPROM_GEBZ_M);
	data[2] = eeprom_read(EEPROM_GEBZ_L);
	tx_sdu(sim, 0, data, 3);
}

/* command: lock metering counter and directory */
static void sp_gzrv(sim_sim_t *sim)
{
	uint8_t *data;

	PDEBUG(DSIM7, DEBUG_INFO, " SP-GZRV\n");

	sim->gebz_locked = 1;
	write_flags(sim);

	/* respond */
	data = alloc_msg(sim, 0);
	tx_sdu(sim, 0, data, 0);
}

/* command: unlock metering counter and directory */
static void fr_gzrv(sim_sim_t *sim)
{
	uint8_t *data;

	PDEBUG(DSIM7, DEBUG_INFO, " FR-GZRV\n");

	sim->gebz_locked = 0;
	write_flags(sim);

	/* respond */
	data = alloc_msg(sim, 0);
	tx_sdu(sim, 0, data, 0);
}

/* command: authenticate */
static void aut_1(sim_sim_t *sim)
{
	uint8_t *data;
	int i;

	PDEBUG(DSIM7, DEBUG_INFO, " AUTH-1\n");

	/* respond */
	data = alloc_msg(sim, 1);
	for (i = 0; i < 8; i++)
		data[i] = eeprom_read(EEPROM_AUTH_DATA + i);
	tx_sdu(sim, 0, data, 8);
}

/* command: UNKNOWN */
static void rd_f4(sim_sim_t *sim)
{
	uint8_t *data;

	PDEBUG(DSIM7, DEBUG_INFO, " RD-F4\n");

	/* respond */
	data = alloc_msg(sim, 2);
	data[0] = 0x00;
	data[1] = 0x13;
	tx_sdu(sim, 0, data, 2);
}

/* command: UNKNOWN */
static void rd_f5(sim_sim_t *sim)
{
	uint8_t *data;

	PDEBUG(DSIM7, DEBUG_INFO, " RD-F5\n");

	/* respond */
	data = alloc_msg(sim, 0);
	tx_sdu(sim, 0, data, 0);
}

/* command: UNKNOWN */
static void rd_04(sim_sim_t *sim)
{
	uint8_t *data;

	PDEBUG(DSIM7, DEBUG_INFO, " RD-04\n");

	/* respond */
	data = alloc_msg(sim, 25);
	data[0] = 0x63;
	memset(data + 1, 0x00, 24);
	tx_sdu(sim, 0, data, 25);
}

/* parse layer 7 header */
static void rx_sdu(sim_sim_t *sim, uint8_t *data, int length)
{
	uint8_t cla, ins, dlng;

	if (length < 3) {
		PDEBUG(DSIM7, DEBUG_NOTICE, "SDU too short\n");
		return;
	}

	/* skip all responses, because we don't send commands */
	if (*data & CCRC_IDENT) {
		PDEBUG(DSIM7, DEBUG_NOTICE, "Skipping SDU with response\n");
		return;
	}

	/* read application layer header */
	cla = *data++ & 0x7f;
	ins = *data++;
	dlng = *data++;
	length -= 3;

	/* check length */
	if (dlng != length) {
		PDEBUG(DSIM7, DEBUG_NOTICE, "Skipping SDU with invalid length\n");
		return;
	}

	/* select command */
	switch (cla) {
	case CLA_CNTR:
		switch (ins) {
		case SL_APPL: sl_appl(sim, data, length); return;
		case CL_APPL: cl_appl(sim); return;
		case SH_APPL: sh_appl(sim); return;
		}
		break;
	case CLA_STAT:
		switch (ins) {
		case CHK_KON: chk_kon(sim); return;
		}
		break;
	case CLA_WRTE:
		switch (ins) {
		case WT_RUFN: wt_rufn(sim, data, length); return;
		}
		break;
	case CLA_READ:
		switch (ins) {
		case RD_EBDT: rd_ebdt(sim); return;
		case RD_RUFN: rd_rufn(sim, data, length); return;
		case RD_GEBZ: rd_gebz(sim); return;
		case 0xf4: rd_f4(sim); return;
		case 0xf5: rd_f5(sim); return;
		case 0x04: rd_04(sim); return;
		}
		break;
	case CLA_EXEC:
		switch (ins) {
		case CHK_PIN: chk_pin(sim, data, length); return;
		case SET_PIN: set_pin(sim, data, length); return;
		}
		if (sim->app == APP_NETZ_C) switch (ins) {
		case EH_GEBZ: eh_gebz(sim, data, length); return;
		case CL_GEBZ: cl_gebz(sim); return;
		}
		if (sim->app == APP_RUFN_GEBZ) switch (ins) {
		case SP_GZRV: sp_gzrv(sim); return;
		case FR_GZRV: fr_gzrv(sim); return;
		}
		break;
	case CLA_AUTO:
		switch (ins) {
		case AUT_1: aut_1(sim); return;
		}
		break;
	}

	/* unsupported message */
	PDEBUG(DSIM7, DEBUG_NOTICE, "CLA 0x%02x INS 0x%02x uknown\n", cla, ins);
	data = alloc_msg(sim, 0);
	tx_sdu(sim, CCRC_ERROR, data, 0);
}

/* create layer 7 message for ICL layer */
static void tx_sdu(sim_sim_t *sim, uint8_t ccrc, uint8_t *data, int length)
{
	/* header */
	data = push_msg(data, length, 3);
	data[0] = CCRC_IDENT | ccrc | CCRC_APRC_VALID;
	data[1] = 0;
	if (sim->pin_try == 0)
		data[0] |= CCRC_AFBZ_NULL;
	data[1] = get_aprc(sim);
	data[2] = length;
	length += 3;

	/* forward to ICL layer */
	tx_pdu(sim, data, length);
}

/* ICL layer */

/* parse ICL header */
static void rx_pdu(sim_sim_t *sim, uint8_t *data, int length)
{
	uint8_t ext = 1;

	if (length < 1) {
too_short:
		PDEBUG(DSIMI, DEBUG_NOTICE, "PDU too short\n");
		return;
	}

	/* read ICB1 */
	sim->icl_online = (*data & ICB1_ONLINE) != 0;
	sim->icl_master = (*data & ICB1_MASTER) != 0;
	sim->icl_error = (*data & ICB1_ERROR) != 0;
	sim->icl_chaining = (*data & ICB1_CHAINING) != 0;

	/* skip all ICBx (should only one exist) */
	while (ext) {
		if (length < 1)
			goto too_short;
		ext = (*data++ & ICB_EXT) != 0;
		length--;
	}

	rx_sdu(sim, data, length);
}

/* create ICL layer message for layer 2 */
static void tx_pdu(sim_sim_t *sim, uint8_t *data, int length)
{
	/* header */
	data = push_msg(data, length, 1);
	data[0] = 0;
	if (sim->icl_online)
		data[0] |= ICB1_ONLINE;
	if (!sim->icl_master)
		data[0] |= ICB1_MASTER;
	if (sim->icl_error)
		data[0] |= ICB1_ERROR | ICB1_CONFIRM;
	if (sim->icl_chaining)
		data[0] |= ICB1_CHAINING | ICB1_CONFIRM;
	length++;

	tx_block(sim, L2_I, data, length);
}

/* Layer 2 */

/* process received L2 message */
static void rx_block(sim_sim_t *sim)
{
	uint8_t ns, nr;
	uint8_t *data;

	/* NOTE: This procedure is simplified, it does not comply with the specs. */

	PDEBUG(DSIM2, DEBUG_INFO, "RX message\n");
	sim->addr_src = sim->block_address >> 4;
	sim->addr_dst = sim->block_address & 0xf;
	if (sim->block_checksum != 0) {
		PDEBUG(DSIM2, DEBUG_NOTICE, "Checksum error!\n");
		goto reject;
	}
	if ((sim->block_control & 0x11) == 0x00) {
		ns = (sim->block_control >> 1) & 7;
		nr = sim->block_control >> 5;
		PDEBUG(DSIM2, DEBUG_INFO, " control I: N(S)=%d N(R)=%d\n", ns, nr);
		if (ns == sim->vr && nr == sim->vs) {
			/* receive data */
			sim->vr = (sim->vr + 1) & 0x7;
			rx_pdu(sim, sim->block_rx_data, sim->block_rx_length);
			return;
		} else {
			PDEBUG(DSIM2, DEBUG_NOTICE, "Seqeuence error!\n");
reject:
			/* reject (or send resync after 3 times) */
			data = alloc_msg(sim, 0);
			if (1) { // if (sim->reject_count < 3) {
				tx_block(sim, L2_REJ, data, 0);
				sim->reject_count++;
			} else {
				tx_block(sim, L2_RES, data, 0);
			}
			return;
		}
		return;
	}
	if ((sim->block_control & 0x1f) == 0x09) {
		nr = sim->block_control >> 5;
		PDEBUG(DSIM2, DEBUG_INFO, " control REJ: N(R)=%d\n", nr);
		/* repeat last message */
		if (sim->block_tx_length) {
			tx_block(sim, L2_I, sim->block_tx_data, sim->block_tx_length);
			return;
		}
		/* no block sent yet, sending resync */
		data = alloc_msg(sim, 0);
		tx_block(sim, L2_RES, data, 0);
		return;
	}
	if (sim->block_control == 0xef) {
		PDEBUG(DSIM2, DEBUG_INFO, " control RES\n");
		sim->vr = sim->vs = 0;
		sim->reject_count = 0;
		if (sim->resync_sent == 0) {
			/* resync */
			data = alloc_msg(sim, 0);
			tx_block(sim, L2_RES, data, 0);
			return;
		}
		return;
	}
}

/* receive data from layer 1 and create layer 2 message */
static int rx_char(sim_sim_t *sim, uint8_t c)
{
	sim->block_checksum ^= c;

	switch (sim->block_state) {
	case BLOCK_STATE_ADDRESS:
		sim->block_address = c;
		sim->block_state = BLOCK_STATE_CONTROL;
		sim->block_checksum = c;
		return 0;
	case BLOCK_STATE_CONTROL:
		sim->block_control = c;
		sim->block_state = BLOCK_STATE_LENGTH;
		return 0;
	case BLOCK_STATE_LENGTH:
		if (c > sizeof(sim->block_rx_data)) {
			c = sizeof(sim->block_rx_data);
			PDEBUG(DSIM1, DEBUG_NOTICE, "RX buffer overflow: length=%d > buffer size (%d)\n", c, (int)sizeof(sim->block_rx_data));
		}
		sim->block_rx_length = c;
		sim->block_count = 0;
		sim->block_state = BLOCK_STATE_DATA;
		return 0;
	case BLOCK_STATE_DATA:
		if (sim->block_count < sim->block_rx_length) {
			sim->block_rx_data[sim->block_count++] = c;
			return 0;
		}
		sim->l1_state = L1_STATE_IDLE;
		rx_block(sim);
	}

	return -1;
}

/* create layer 2 message for layer 1 */
static void tx_block(sim_sim_t *sim, enum l2_cmd cmd, uint8_t __attribute__((unused)) *data, int length)
{
	PDEBUG(DSIM2, DEBUG_INFO, "TX resonse\n");

	/* header */
	sim->block_address = (sim->addr_dst << 4) | sim->addr_src;
	switch (cmd) {
	case L2_I:
		PDEBUG(DSIM2, DEBUG_INFO, " control I: N(S)=%d N(R)=%d\n", sim->vs, sim->vr);
		sim->block_control = (sim->vr << 5) | (sim->vs << 1);
		sim->vs = (sim->vs + 1) & 0x7;
		sim->resync_sent = 0;
		break;
	case L2_REJ:
		PDEBUG(DSIM2, DEBUG_INFO, " control REJ: N(R)=%d\n", sim->vr);
		sim->block_control = (sim->vr << 5) | 0x09;
		sim->resync_sent = 0;
		break;
	case L2_RES:
		PDEBUG(DSIM2, DEBUG_INFO, " control RES\n");
		sim->block_control = 0xef;
		sim->resync_sent = 1;
		break;
	}
	sim->block_tx_length = length;

	sim->l1_state = L1_STATE_SEND;
	sim->block_state = BLOCK_STATE_ADDRESS;
}

/* transmit character of current message to layer 1 */
static uint8_t tx_char(sim_sim_t *sim)
{
	uint8_t c = -1;

	switch (sim->block_state) {
	case BLOCK_STATE_ADDRESS:
		c = sim->block_address;
		sim->block_state = BLOCK_STATE_CONTROL;
		sim->block_checksum = 0;
		break;
	case BLOCK_STATE_CONTROL:
		c = sim->block_control;
		sim->block_state = BLOCK_STATE_LENGTH;
		break;
	case BLOCK_STATE_LENGTH:
		c = sim->block_tx_length;
		sim->block_count = 0;
		sim->block_state = BLOCK_STATE_DATA;
		break;
	case BLOCK_STATE_DATA:
		if (sim->block_count < sim->block_tx_length) {
			c = sim->block_tx_data[sim->block_count++];
			break;
		}
		c = sim->block_checksum;
		sim->l1_state = L1_STATE_IDLE;
		break;
	}

	sim->block_checksum ^= c;

	return c;
}

/* ATR */

static uint8_t atr[] = {
	0x3b, 0x88, /* TS, T0 */
	0x8e,
	0xfe,
	0x53, 0x2a, 0x03, 0x1e,
	0x04,
	0x92, 0x80, 0x00, 0x41, 0x32, 0x36, 0x01, 0x11,
	0xe4, /* TCK */
};

static uint8_t tx_atr(sim_sim_t *sim)
{
	uint8_t c;

	c = atr[sim->atr_count++];
	if (sim->atr_count == sizeof(atr))
		sim->l1_state = L1_STATE_IDLE;

	return c;
}

/* Layer 1 */

int sim_init_eeprom(void)
{
	uint8_t ebdt_data[9];
	int i, rc;

	/* init EEPROM with all bits '1' */
	for (i = 0; i < (int)eeprom_length(); i++)
		eeprom_write(i, 0xff);

	/* set default values in eeprom */
	rc = encode_ebdt(ebdt_data, FUTLN_DEFAULT, SICHERUNG_DEFAULT, KARTEN_DEFAULT, SONDER_DEFAULT, WARTUNG_DEFAULT);
	if (rc < 0)
		return rc;
	for (i = 0; i < MAX_CARDS; i++) {
		eeprom_write(EEPROM_FUTLN_H + i, ebdt_data[0]);
		eeprom_write(EEPROM_FUTLN_M + i, ebdt_data[1]);
		eeprom_write(EEPROM_FUTLN_L + i, ebdt_data[2] + i);
		eeprom_write(EEPROM_SICH_H + i, ebdt_data[3]);
		eeprom_write(EEPROM_SICH_L + i, ebdt_data[4]);
		eeprom_write(EEPROM_SONDER_H + i, ebdt_data[5]);
		eeprom_write(EEPROM_SONDER_L + i, ebdt_data[6]);
		eeprom_write(EEPROM_WARTUNG_H + i, ebdt_data[7]);
		eeprom_write(EEPROM_WARTUNG_L + i, ebdt_data[8]);
	}
	eeprom_write(EEPROM_GEBZ_H, 0);
	eeprom_write(EEPROM_GEBZ_M, 0);
	eeprom_write(EEPROM_GEBZ_L, 0);
	eeprom_write(EEPROM_FLAGS, (strlen(PIN_DEFAULT) << EEPROM_FLAG_PIN_LEN) | (MAX_PIN_TRY << EEPROM_FLAG_PIN_TRY));
	for (i = 0; i < (int)strlen(PIN_DEFAULT); i++)
		eeprom_write(EEPROM_PIN_DATA + i, PIN_DEFAULT[i]);
	for (i = 0; i < 8; i++)
		eeprom_write(EEPROM_AUTH_DATA + i, AUTH_DEFAULT >> ((7 - i) * 8));

	/* now write magic characters to identify virgin or initialized EEPROM */
	eeprom_write(EEPROM_MAGIC + 0, 'C');
	eeprom_write(EEPROM_MAGIC + 1, '0' + EEPROM_VERSION);

	return 0;
}

void sim_reset(sim_sim_t *sim, int reset)
{
	int i;
	char pin[8];

	PDEBUG(DSIM1, DEBUG_INFO, "Reset singnal %s\n", (reset) ? "on (low)" : "off (high)");
	memset(sim, 0, sizeof(*sim));

	if (reset)
		return;

	/* read flags from EEPROM data */
	read_flags(sim);

	/* check PIN and set flags */
	for (i = 0; i < sim->pin_len; i++)
		pin[i] = eeprom_read(EEPROM_PIN_DATA + i);

	sim->pin_required = 1;
	/* 'system' PIN = 0000, 0001, 0002, ... */
	if (sim->pin_len == 4 && pin[0] == '0' && pin[1] == '0' && pin[2] == '0' && pin[3] >= '0' && pin[3] <= '0' + MAX_CARDS) {
		sim->pin_required = 0;
		if (pin[3] > '0')
			sim->card = pin[3] - '1';
		PDEBUG(DSIM1, DEBUG_INFO, "Card has disabled PIN (system PIN '000%c') Selecting card #%d.\n", pin[3], sim->card + 1);
	}

	PDEBUG(DSIM1, DEBUG_INFO, "Sending ATR\n");
	sim->l1_state = L1_STATE_ATR;
}

int sim_rx(sim_sim_t *sim, uint8_t c)
{
	int rc = -1;

	PDEBUG(DSIM1, DEBUG_DEBUG, "Serial RX '0x%02x'\n", c);

	switch (sim->l1_state) {
	case L1_STATE_IDLE:
		sim->l1_state = L1_STATE_RECEIVE;
		sim->block_state = BLOCK_STATE_ADDRESS;
		/* fall through */
	case L1_STATE_RECEIVE:
		rc = rx_char(sim, c);
		break;
	default:
		break;
	}

	return rc;
}

int sim_tx(sim_sim_t *sim)
{
	int c = -1;

	switch (sim->l1_state) {
	case L1_STATE_ATR:
		c = tx_atr(sim);
		break;
	case L1_STATE_SEND:
		c = tx_char(sim);
		break;
	default:
		break;
	}

	if (c >= 0)
		PDEBUG(DSIM1, DEBUG_DEBUG, "Serial TX '0x%02x'\n", c);

	return c;
}

void sim_timeout(sim_sim_t *sim)
{
	switch (sim->l1_state) {
	case L1_STATE_ATR:
		PDEBUG(DSIM1, DEBUG_NOTICE, "Timeout while transmitting ATR!\n");
		sim->l1_state = L1_STATE_RESET;
		break;
	case L1_STATE_RECEIVE:
		PDEBUG(DSIM1, DEBUG_NOTICE, "Timeout while receiving message!\n");
		sim->block_state = BLOCK_STATE_ADDRESS;
		break;
	case L1_STATE_SEND:
		PDEBUG(DSIM1, DEBUG_NOTICE, "Timeout while sending message!\n");
		sim->l1_state = L1_STATE_IDLE;
		break;
	default:
		break;
	}
}
