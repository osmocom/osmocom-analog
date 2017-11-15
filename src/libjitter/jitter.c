/* Jitter buffering functions
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "../common/sample.h"
#include "../common/debug.h"
#include "jitter.h"

/* create jitter buffer */
int jitter_create(jitter_t *jitter, int length)
{
	memset(jitter, 0, sizeof(*jitter));
	jitter->spl = calloc(length * sizeof(sample_t), 1);
	if (!jitter->spl) {
		PDEBUG(DDSP, DEBUG_ERROR, "No memory for jitter buffer.\n");
		return -ENOMEM;
	}
	jitter->len = length;

	return 0;
}

void jitter_destroy(jitter_t *jitter)
{
	if (jitter->spl) {
		free(jitter->spl);
		jitter->spl = NULL;
	}
}

/* store audio in jitterbuffer
 *
 * stop if buffer is completely filled
 */
void jitter_save(jitter_t *jb, sample_t *samples, int length)
{
	sample_t *spl;
	int inptr, outptr, len, space;
	int i;

	spl = jb->spl;
	inptr = jb->inptr;
	outptr = jb->outptr;
	len = jb->len;
	space = (outptr - inptr + len - 1) % len;

	if (space < length)
		length = space;
	for (i = 0; i < length; i++) {
		spl[inptr++] = *samples++;
		if (inptr == len)
			inptr = 0;
	}

	jb->inptr = inptr;
}

/* get audio from jitterbuffer
 */
void jitter_load(jitter_t *jb, sample_t *samples, int length)
{
	sample_t *spl;
	int inptr, outptr, len, fill;
	int i, ii;

	spl = jb->spl;
	inptr = jb->inptr;
	outptr = jb->outptr;
	len = jb->len;
	fill = (inptr - outptr + len) % len;

	if (fill < length)
		ii = fill;
	else
		ii = length;

	/* fill what we got */
	for (i = 0; i < ii; i++) {
		*samples++ = spl[outptr++];
		if (outptr == len)
			outptr = 0;
	}
	/* on underrun, fill with silence */
	for (; i < length; i++) {
		*samples++ = 0;
	}

	jb->outptr = outptr;
}

void jitter_clear(jitter_t *jb)
{
	jb->inptr = jb->outptr = 0;
}

