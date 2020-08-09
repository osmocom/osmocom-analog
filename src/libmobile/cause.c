/* Clear cause names
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
#include "cause.h"

const char *cause_name(int cause)
{
	static char cause_str[16];

	switch (cause) {
	case CAUSE_NORMAL:
		return "hangup";
	case CAUSE_BUSY:
		return "busy";
	case CAUSE_NOANSWER:
		return "no-answer";
	case CAUSE_OUTOFORDER:
		return "out-of-order";
	case CAUSE_INVALNUMBER:
		return "invalid-number";
	case CAUSE_NOCHANNEL:
		return "no-channel";
	case CAUSE_TEMPFAIL:
		return "link-failure";
	case CAUSE_RESOURCE_UNAVAIL:
		return "resource-unavail";
	case CAUSE_INVALCALLREF:
		return "invalid-callref";
	default:
		sprintf(cause_str, "cause=%d", cause);
		return cause_str;
	}

}

