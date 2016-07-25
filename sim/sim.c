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

/* return 1, if 1-bits are odd, so parity becomes even */
static int gen_parity(uint8_t *bits)
{
	int i;
	uint8_t parity = 0;

	for (i = 0; i < 8; i++)
		parity ^= (bits[i] & 1);

	return parity;
}

static uint8_t *gen_memory(uint8_t futln_nat, uint8_t futln_fuvst, uint16_t futln_rest, uint16_t sicherungscode, uint16_t sonderheitsschluessel)
{
	static uint8_t memory[416];
	int i;

	/* meaningless data */
	for (i = 0; i <= 10; i++)
		memory[i] = 1;

	/* magic data */
	memory[11] = 0;
	memory[12] = 1;
	memory[13] = 1;
	memory[14] = 1;
	memory[15] = 1;

	/* meaningless data */
	for (i = 16; i <= 17; i++)
		memory[i] = 0;

	/* magic data */
	memory[18] = 1;
	memory[19] = 1;
	memory[20] = 1;

	/* magic data */
	memory[21] = 0;
	memory[22] = 1;
	memory[23] = 0;

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

	/* sonderheitsschluessel */
	for (i = 159; i <= 166; i++)
		memory[i] = (sonderheitsschluessel >> (i - 159)) & 1;
	memory[167] = gen_parity(memory + 159);
	for (i = 168; i <= 175; i++)
		memory[i] = (sonderheitsschluessel >> (i - 168 + 8)) & 1;
	memory[176] = gen_parity(memory + 168);

	/* meaningless data */
	for (i = 177; i <= 351; i++)
		memory[i] = 1;

	/* all zero */
	for (i = 352; i <= 415; i++)
		memory[i] = 0;

	return memory;
}

int main(int argc, char *argv[])
{

	if (argc <= 5) {
		printf("Usage: %s <futln_nat> <futln_fuvst> <futln_rest> <sicherungscode> <sonderheitsschluessel>\n", argv[0]);
		return 0;
	}
	int i;
	uint8_t futln_nat = atoi(argv[1]);
	uint8_t futln_fuvst = atoi(argv[2]);
	uint16_t futln_rest = atoi(argv[3]);
	uint16_t sicherungscode = atoi(argv[4]);
	uint16_t sonderheitsschluessel = atoi(argv[5]);
	printf("nat=%d\n", futln_nat);
	printf("fufvt=%d\n", futln_fuvst);
	printf("rest=%d\n", futln_rest);
	printf("sicherungscode=%d\n", sicherungscode);
	printf("sonderheitsschluessel=%d\n", sonderheitsschluessel);

	printf("Telefonnummer: %d%d%05d\n", futln_nat, futln_fuvst, futln_rest);

	uint8_t *bits = gen_memory(futln_nat, futln_fuvst, futln_rest, sicherungscode, sonderheitsschluessel);
	for (i = 0; i < 52; i++) {
//printf("%d %d %d %d %d %d %d %d\n", bits[0], bits[1], bits[2] ,bits[3] ,bits[4] ,bits[5] ,bits[6] ,bits[7]);
		printf("0x%02x, ", bits[0] + (bits[1] << 1) + (bits[2] << 2) + (bits[3] << 3) + (bits[4] << 4) + (bits[5] << 5) + (bits[6] << 6) + (bits[7] << 7));
//printf("\n");
		bits += 8;
		if ((i & 7) == 7)
			printf("\n");
	}
	printf("\n");

	return 0;
}

