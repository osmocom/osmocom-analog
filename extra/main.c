/* SIM Card for C-Netz "Berechtigungskarte als Speicherkarte"
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
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <../src/liblogging/logging.h>
#include <../src/liboptions/options.h>

int num_kanal = 1;
static uint8_t magic1 = 0x1e;
static uint8_t magic2 = 0x7;
static uint8_t magic3 = 0x2;
static const char *futln;
static uint8_t futln_nat;
static uint8_t futln_fuvst;
static uint16_t futln_rest;
static uint16_t sicherungscode = 12345;
static uint16_t sonderheitenschluessel = 0;
static uint16_t wartungsschluessel = 0xffff;

/* return 1, if 1-bits are odd, so parity becomes even */
static int gen_parity(uint8_t *bits)
{
	int i;
	uint8_t parity = 0;

	for (i = 0; i < 8; i++)
		parity ^= (bits[i] & 1);

	return parity;
}

static uint8_t *gen_memory(uint8_t magic1, uint8_t magic2, uint8_t magic3, uint8_t futln_nat, uint8_t futln_fuvst, uint16_t futln_rest, uint16_t sicherungscode, uint16_t sonderheitenschluessel, uint16_t wartungsschluessel)
{
	static uint8_t memory[416];
	int i;

	/* meaningless data */
	for (i = 0; i <= 10; i++)
		memory[i] = 1;

	/* magic data */
	for (i = 11; i <= 15; i++)
		memory[i] = (magic1 >> (i - 11)) & 1;

	/* meaningless data */
	for (i = 16; i <= 17; i++)
		memory[i] = 0;

	/* magic data */
	for (i = 18; i <= 20; i++)
		memory[i] = (magic2 >> (i - 18)) & 1;

	/* magic data */
	for (i = 21; i <= 23; i++)
		memory[i] = (magic3 >> (i - 21)) & 1;

	/* meaningless data */
	for (i = 24; i <= 113; i++)
		memory[i] = 1;

	/* number */
	for (i = 114; i <= 116; i++)
		memory[i] = (futln_nat >> (i - 114)) & 1;
	for (i = 117; i <= 121; i++)
		memory[i] = (futln_fuvst >> (i - 117)) & 1;
	memory[122] = gen_parity(memory + 114);
	for (i = 123; i <= 130; i++)
		memory[i] = (futln_rest >> (i - 123)) & 1;
	memory[131] = gen_parity(memory + 123);
	for (i = 132; i <= 139; i++)
		memory[i] = (futln_rest >> (i - 132 + 8)) & 1;
	memory[140] = gen_parity(memory + 132);

	/* sicherungscode */
	for (i = 141; i <= 148; i++)
		memory[i] = (sicherungscode >> (i - 141)) & 1;
	memory[149] = gen_parity(memory + 141);
	for (i = 150; i <= 157; i++)
		memory[i] = (sicherungscode >> (i - 150 + 8)) & 1;
	memory[158] = gen_parity(memory + 150);

	/* sonderheitenschluessel */
	for (i = 159; i <= 166; i++)
		memory[i] = (sonderheitenschluessel >> (i - 159)) & 1;
	memory[167] = gen_parity(memory + 159);
	for (i = 168; i <= 175; i++)
		memory[i] = (sonderheitenschluessel >> (i - 168 + 8)) & 1;
	memory[176] = gen_parity(memory + 168);

	/* wartungschluessel */
	for (i = 177; i <= 184; i++)
		memory[i] = (wartungsschluessel >> (i - 177)) & 1;
	memory[185] = gen_parity(memory + 177);
	for (i = 186; i <= 193; i++)
		memory[i] = (wartungsschluessel >> (i - 186 + 8)) & 1;
	memory[194] = gen_parity(memory + 186);

	/* meaningless data */
	for (i = 195; i <= 351; i++)
		memory[i] = 1;

	/* all zero */
	for (i = 352; i <= 415; i++)
		memory[i] = 0;

	return memory;
}

void print_help(const char *arg0)
{
	printf("Usage: %s [options] <subscriber number>\n", arg0);
	/*      -                                                                             - */
	printf("General options:\n");
	printf(" -h --help\n");
	printf("        This help\n");
	printf(" -1 --magic1 <value>\n");
	printf("        Magic value for bits 011-015 (default = 0x%02x)\n", magic1);
	printf(" -2 --magic2 <value>\n");
	printf("        Magic value for bits 018-021 (default = 0x%x)\n", magic2);
	printf(" -3 --magic3 <value>\n");
	printf("        Magic value for bits 022-024 (default = 0x%x)\n", magic3);
	printf(" -C --security-code \n");
	printf("        Security code (\"Sicherungscode\") (default = %d)\n", sicherungscode);
	printf(" -S --special-key \n");
	printf("        Special key (\"Sonderheitenschluessel\") (default = %d)\n", sonderheitenschluessel);
	printf(" -W --maintenance-key \n");
	printf("        Maintenance key (\"Wartungsschluessel\") (default = %d)\n", wartungsschluessel);
	printf("\nSubscriber number:\n");
	printf("        Give 7 (8) digits of C-Netz subscriber number (FUTLN) without prefix.\n");
}

void add_options(void)
{
	option_add('h', "help", 0);
	option_add('1', "magic1", 1);
	option_add('2', "magic2", 2);
	option_add('3', "magic3", 3);
	option_add('C', "security-code", 1);
	option_add('S', "special-key", 1);
	option_add('W', "maintenance-key", 1);
};

int handle_options(int short_option, int argi, char **argv)
{
	switch (short_option) {
	case 'h':
		print_help(argv[0]);
		return 0;
	case '1':
		magic1 = strtoul(argv[argi], NULL, 0);
		break;
	case '2':
		magic2 = strtoul(argv[argi], NULL, 0);
		break;
	case '3':
		magic3 = strtoul(argv[argi], NULL, 0);
		break;
	case 'C':
		sicherungscode = strtoul(argv[argi], NULL, 0);
		break;
	case 'S':
		sonderheitenschluessel = strtoul(argv[argi], NULL, 0);
		break;
	case 'W':
		wartungsschluessel = strtoul(argv[argi], NULL, 0);
		break;
	default:
		return -EINVAL;
	}

	return 1;
}

int main(int argc, char *argv[])
{
	int argi;
	int i;

	loglevel = LOGL_INFO;
	logging_init();

	add_options();

	/* parse command line */
	argi = options_command_line(argc, argv, handle_options);
	if (argi <= 0)
		return argi;

	/* get subscriber number */
	if (argi >= argc) {
		fprintf(stderr, "Expecting subscriber number, use '-h' for help!\n");
		return 0;
	}
	futln = argv[argi];
	if (strlen(futln) == 7) {
		futln_nat = *futln++ - '0';
		futln_fuvst = *futln++ - '0';
	} else if (strlen(futln) == 8) {
		futln_nat = *futln++ - '0';
		futln_fuvst = (*futln++ - '0') * 10;
		futln_fuvst += *futln++ - '0';
	} else {
inval_sub:
		fprintf(stderr, "Invalid subscriber number, use '-h' for help!\n");
		return 0;
	}
	futln_rest = (*futln++ - '0') * 10000;
	futln_rest += (*futln++ - '0') * 1000;
	futln_rest += (*futln++ - '0') * 100;
	futln_rest += (*futln++ - '0') * 10;
	futln_rest += *futln++ - '0';
	if (futln_nat > 7 || futln_fuvst > 31)
		goto inval_sub;

	printf("\n");
	printf("Magic Data 011..015 = %d (0x%02x)\n", magic1, magic1);
	printf("Magic Data 018..020 = %d\n", magic2);
	printf("Magic Data 021..023 = %d\n", magic3);
	printf("FUTLN NAT = %d\n", futln_nat);
	printf("FUTLN FUVST = %d\n", futln_fuvst);
	printf("FUTLN REST = %d\n", futln_rest);
	printf("Sicherungscode=%d\n", sicherungscode);
	printf("Sonderheitenschluessel=%d (0x%04x)\n", sonderheitenschluessel, sonderheitenschluessel);
	printf("Wartungsschluessel=%d (0x%04x)\n", wartungsschluessel, wartungsschluessel);
	printf("\nBinary data: (LSB first)\n");

	uint8_t *bits = gen_memory(magic1, magic2, magic3, futln_nat, futln_fuvst, futln_rest, sicherungscode, sonderheitenschluessel, wartungsschluessel);
	for (i = 0; i < 52; i++) {
		printf("0x%02x, ", bits[0] + (bits[1] << 1) + (bits[2] << 2) + (bits[3] << 3) + (bits[4] << 4) + (bits[5] << 5) + (bits[6] << 6) + (bits[7] << 7));
		bits += 8;
		if ((i & 7) == 7)
			printf("\n");
	}
	printf("\n");

	return 0;
}

void osmo_cc_set_log_cat(void) {}

