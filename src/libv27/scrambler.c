/* V.27(bis) Scrambler / Descrambler
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

/* Based on original Scrambler code from SIT-Rom:

 r6 already has input bit at position 0.
 r6 (low) and r7 (high) are the shift register.
 The register is shifed during process, so that compare at 00BE and
 00C5 refers to the already shifted register and not to the original
 position.

00A4         L00A4:
00A4 : FE            mov     a,r6
00A5 : ED AF         djnz    r5,L00AF
00A7 : D3 01         xrl     a,#001H
00A9 : BD 22         mov     r5,#022H
00AB         L00AB:
00AB : D2 B3         jb6     L00B3
00AD : 04 B5         jmp     L00B5
             ;
00AF         L00AF:
00AF : 00            nop
00B0 : 00            nop
00B1 : 04 AB         jmp     L00AB
             ;
00B3         L00B3:
00B3 : D3 01         xrl     a,#001H
00B5         L00B5:
00B5 : F2 B9         jb7     L00B9
00B7 : 04 BB         jmp     L00BB
             ;
00B9         L00B9:
00B9 : D3 01         xrl     a,#001H
00BB         L00BB:
00BB : 97            clr     c
00BC : F7            rlc     a
00BD : AE            mov     r6,a
00BE : 32 CB         jb1     L00CB
00C0 : FF            mov     a,r7
00C1 : F7            rlc     a
00C2 : AF            mov     r7,a
00C3 : 37            cpl     a
00C4 : 00            nop
00C5         L00C5:
00C5 : 53 26         anl     a,#026H
00C7 : C6 D0         jz      L00D0
00C9 : 04 D2         jmp     L00D2
             ;
00CB         L00CB:
00CB : FF            mov     a,r7
00CC : F7            rlc     a
00CD : AF            mov     r7,a
00CE : 04 C5         jmp     L00C5
             ; 
00D0          00D0:
00D0 : BD 22         mov     r5,#022H
00D2          00D2:
00D2 : 83            ret

*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "scrambler.h"

#define GUARD_COUNT	34

/* init scrambler */
void v27_scrambler_init(v27scrambler_t *scram, int bis, int descramble)
{
	memset(scram, 0, sizeof(*scram));

	scram->descramble = descramble;

	/* set bits 9 and 12 (and 8 for V.27bis) */
	if (bis)
		scram->resetmask = 0x1300;
	else
		scram->resetmask = 0x1200;

	/* guard counter */
	scram->counter = GUARD_COUNT;
}

/* scramble/descramble one bit */
uint8_t v27_scrambler_bit(v27scrambler_t *scram, uint8_t in)
{
	uint8_t bit0 = in & 1;
	uint16_t shift = scram->shift;


	/* the descrambler stores the input bit */
	if (scram->descramble) {
		/* put bit 0 into shift register and shift */
		shift |= bit0;
		scram->shift = shift << 1;
	}

	/* process guaard counter */
	if (--scram->counter == 0) {
		/* restart counter */
		scram->counter = GUARD_COUNT;
		/* invert this time */
		bit0 ^= 1;
	}

	/* xor bit 0 with bits 6 and 7: polynome 1 + x^-6 + x^-7 */
	bit0 ^= ((shift >> 6) & 1);
	bit0 ^= ((shift >> 7) & 1);

	/* the scrambler stores the output bit */
	if (!scram->descramble) {
		/* put bit 0 into shift register and shift */
		shift |= bit0;
		scram->shift = shift << 1;
	}

	/* check if bits (8),9,12 are repitions of bit 0 in shift register (prior shift) */
	if (!(shift & 1))
		shift ^= ~0;
	if (!(shift & scram->resetmask)) {
		/* any repetition is not true, reset counter */
		scram->counter = GUARD_COUNT;
	}

	return bit0;
}

/* scramble/descramble block of bytes (LSB first) */
void v27_scrambler_block(v27scrambler_t *scram, uint8_t *data, int len)
{
	int i, j;
	uint8_t in, out = 0;

	for (i = 0; i < len; i++) {
		in = data[i];
		for (j = 0; j < 8; j++) {
			out >>= 1;
			// Note: 'in' will be masked to bit 0 only
			out |= v27_scrambler_bit(scram, in) << 7;
			in >>= 1;
		}
		data[i] = out;
	}
}

