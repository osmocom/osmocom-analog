/* Endpoint and call process handling
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
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "../libtimer/timer.h"
#include "../libselect/select.h"
#include "../libdebug/debug.h"
#include "endpoint.h"
#include "message.h"

#define SCREEN_QUESTIONMARK	1
#define SCREEN_STAR		2
#define SCREEN_AT		3

void osmo_cc_help_screen(void)
{
	printf("Screening options:\n\n");

	printf("screen-calling-in [attrs] <current caller ID> [attrs] <new caller ID>\n");
	printf("screen-called-in [attrs] <current dialed number> [attrs] <new dialed number>\n");
	printf("screen-calling-out [attrs] <current caller ID> [attrs] <new caller ID>\n");
	printf("screen-called-out [attrs] <current dialed number> [attrs] <new dialed number>\n\n");

	printf("These options allow to screen an incoming or outgoing caller ID or dialed\n");
	printf("number. If 'the current caller ID' or 'current dialed number' matches, it will\n");
	printf("be replaced by 'new caller ID' or 'new dialed number'.  'incoming' means from\n");
	printf(" the interface and 'outgoing' means towards the interface.\n\n");

	printf("Attributes prior 'current caller ID' or 'new dialed number' may be used to\n");
	printf("perform screening only if the attribute match. Attributes prior\n");
	printf("'new caller ID' or 'new dialed number' may be used to alter them. Attribute to\n");
	printf("define the type of number can be: 'unknown', 'international', 'national',\n");
	printf("'network', 'subscriber', 'abbreviated' Attribute to define the restriction of a\n");
	printf("caller ID: 'allowed', 'restricted'\n\n");

	printf("The current caller ID or dialed number may contain one or more '?', to allow\n");
	printf("any digit to match. The current caller ID or dialed number may contain a '*',\n");
	printf("to allow any suffix to match from now on. The new caller ID or dialed number\n");
	printf("may contain a '*', to append the suffix from the current caller ID or dialed\n");
	printf("number.\n\n");

	printf("When screening an incoming caller ID or dialed number, the '@' can be appended\n");
	printf("to the 'new caller ID', followed by a 'host:port', to route call to a special\n");
	printf("Osmo-CC endpoint. This way it is possible to do simple routing.\n\n");
}

char *osmo_cc_strtok_quotes(const char **text_p)
{
	static char token[1024];
	const char *text = *text_p;
	int i, quote;

	/* skip spaces */
	while (*text) {
		if (*text > 32)
			break;
		text++;
	}

	/* if eol, return NULL */
	if (!(*text))
		return NULL;

	i = 0;
	quote = 0;
	while (*text) {
		/* escape allows all following characters */
		if (*text == '\\') {
			text++;
			if (*text)
				token[i++] = *text++;
			continue;
		}
		/* no quote, check for them or break on white space */
		if (quote == 0) {
			if (*text == '\'') {
				quote = 1;
				text++;
				continue;
			}
			if (*text == '\"') {
				quote = 2;
				text++;
				continue;
			}
			if (*text <= ' ')
				break;
		}
		/* single quote, check for unquote */
		if (quote == 1 && *text == '\'') {
			quote = 0;
			text++;
			continue;
		}
	       	/* double quote, check for unquote */
		if (quote == 2 && *text == '\"') {
			quote = 0;
			text++;
			continue;
		}
		/* copy character */
		token[i++] = *text++;
	}
	token[i] = '\0';

	*text_p = text;
	return token;
}

int osmo_cc_add_screen(osmo_cc_endpoint_t *ep, const char *text)
{
	osmo_cc_screen_list_t **list_p = NULL, *list;
	const char *token;
	int no_present = 0, calling_in = 0, star_used, at_used;
	int i, j;

	star_used = 0;
	if (!strncasecmp(text, "screen-calling-in", 17)) {
		text += 17;
		list_p = &ep->screen_calling_in;
		no_present = 1;
		calling_in = 1;
	} else if (!strncasecmp(text, "screen-called-in", 16)) {
		text += 16;
		list_p = &ep->screen_called_in;
		calling_in = 1;
	} else if (!strncasecmp(text, "screen-calling-out", 18)) {
		text += 18;
		list_p = &ep->screen_calling_out;
		no_present = 1;
	} else if (!strncasecmp(text, "screen-called-out", 17)) {
		text += 17;
		list_p = &ep->screen_called_out;
	} else {
		PDEBUG(DCC, DEBUG_ERROR, "Invalid screening definition \"%s\". It must start with 'screen-calling-in' or 'screen-called-in' or 'screen-calling-out' or 'screen-called-out'\n", text);
		return -EINVAL;
	}

	/* skip space behind screen list string */
	while (*text) {
		if (*text > 32)
			break;
		text++;
	}

	list = calloc(1, sizeof(*list));
	if (!list)
		return -ENOMEM;

next_from:
	token = osmo_cc_strtok_quotes(&text);
	if (!token) {
		free(list);
		PDEBUG(DCC, DEBUG_ERROR, "Missing 'from' string in screening definition \"%s\". If the string shall be empty, use double quotes. (\'\' or \"\")\n", text);
		return -EINVAL;
	}
	if (!strcasecmp(token, "unknown")) {
		list->has_from_type = 1;
		list->from_type = OSMO_CC_TYPE_UNKNOWN;
		goto next_from;
	} else
	if (!strcasecmp(token, "international")) {
		list->has_from_type = 1;
		list->from_type = OSMO_CC_TYPE_INTERNATIONAL;
		goto next_from;
	} else
	if (!strcasecmp(token, "national")) {
		list->has_from_type = 1;
		list->from_type = OSMO_CC_TYPE_NATIONAL;
		goto next_from;
	} else
	if (!strcasecmp(token, "network")) {
		list->has_from_type = 1;
		list->from_type = OSMO_CC_TYPE_NETWORK;
		goto next_from;
	} else
	if (!strcasecmp(token, "subscriber")) {
		list->has_from_type = 1;
		list->from_type = OSMO_CC_TYPE_SUBSCRIBER;
		goto next_from;
	} else
	if (!strcasecmp(token, "abbreviated")) {
		list->has_from_type = 1;
		list->from_type = OSMO_CC_TYPE_ABBREVIATED;
		goto next_from;
	} else
	if (!strcasecmp(token, "allowed")) {
		if (no_present) {
no_present_error:
			free(list);
			PDEBUG(DCC, DEBUG_ERROR, "Error in screening definition '%s'.\n", text);
			PDEBUG(DCC, DEBUG_ERROR, "Keyword '%s' not allowed in screen entry for called number\n", token);
			return -EINVAL;
		}
		list->has_from_present = 1;
		list->from_present = OSMO_CC_PRESENT_ALLOWED;
		goto next_from;
	} else
	if (!strcasecmp(token, "restricted")) {
		if (no_present)
			goto no_present_error;
		list->has_from_present = 1;
		list->from_present = OSMO_CC_PRESENT_RESTRICTED;
		goto next_from;
	} else {
		star_used = 0;
		for (i = j = 0; token[i] && j < (int)sizeof(list->from) - 1; i++, j++) {
			if (token[i] == '?')
				list->from[j] = SCREEN_QUESTIONMARK;
			else
			if (token[i] == '*') {
				if (star_used) {
					free(list);
					PDEBUG(DCC, DEBUG_ERROR, "Error in screening definition '%s'.\n", text);
					PDEBUG(DCC, DEBUG_ERROR, "The '*' may be used only once.\n");
					return -EINVAL;
				}
				list->from[j] = SCREEN_STAR;
				star_used = 1;
			} else
			if (token[i] == '\\' && token[i + 1] != '\0')
				list->from[j] = token[++i];
			else
				list->from[j] = token[i];
		}
		list->from[j] = '\0';
	}

next_to:
	token = osmo_cc_strtok_quotes(&text);
	if (!token) {
		free(list);
		PDEBUG(DCC, DEBUG_ERROR, "Error in screening definition '%s'.\n", text);
		PDEBUG(DCC, DEBUG_ERROR, "Missing screening result. If the string shall be empty, use double quotes. (\'\' or \"\")\n");
		return -EINVAL;
	}
	if (!strcasecmp(token, "unknown")) {
		list->has_to_type = 1;
		list->to_type = OSMO_CC_TYPE_UNKNOWN;
		goto next_to;
	} else
	if (!strcasecmp(token, "international")) {
		list->has_to_type = 1;
		list->to_type = OSMO_CC_TYPE_INTERNATIONAL;
		goto next_to;
	} else
	if (!strcasecmp(token, "national")) {
		list->has_to_type = 1;
		list->to_type = OSMO_CC_TYPE_NATIONAL;
		goto next_to;
	} else
	if (!strcasecmp(token, "network")) {
		list->has_to_type = 1;
		list->to_type = OSMO_CC_TYPE_NETWORK;
		goto next_to;
	} else
	if (!strcasecmp(token, "subscriber")) {
		list->has_to_type = 1;
		list->to_type = OSMO_CC_TYPE_SUBSCRIBER;
		goto next_to;
	} else
	if (!strcasecmp(token, "abbreviated")) {
		list->has_to_type = 1;
		list->to_type = OSMO_CC_TYPE_ABBREVIATED;
		goto next_to;
	} else
	if (!strcasecmp(token, "allowed")) {
		if (no_present)
			goto no_present_error;
		list->has_to_present = 1;
		list->to_present = OSMO_CC_PRESENT_ALLOWED;
		goto next_to;
	} else
	if (!strcasecmp(token, "restricted")) {
		if (no_present)
			goto no_present_error;
		list->has_to_present = 1;
		list->to_present = OSMO_CC_PRESENT_RESTRICTED;
		goto next_to;
	} else {
		at_used = star_used = 0;
		for (i = j = 0; token[i] && j < (int)sizeof(list->to) - 1; i++, j++) {
			if (token[i] == '*') {
				if (star_used) {
					free(list);
					PDEBUG(DCC, DEBUG_ERROR, "Error in screening definition '%s'.\n", text);
					PDEBUG(DCC, DEBUG_ERROR, "The '*' may be used only once.\n");
					return -EINVAL;
				}
				list->to[j] = SCREEN_STAR;
				star_used = 1;
			} else
			if (token[i] == '@') {
				if (!calling_in) {
					free(list);
					PDEBUG(DCC, DEBUG_ERROR, "Error in screening definition '%s'.\n", text);
					PDEBUG(DCC, DEBUG_ERROR, "The '@' may be used only for incoming calls from interface.\n");
					return -EINVAL;
				}
				if (at_used) {
					free(list);
					PDEBUG(DCC, DEBUG_ERROR, "Error in screening definition '%s'.\n", text);
					PDEBUG(DCC, DEBUG_ERROR, "The '@' may be used only once.\n");
					return -EINVAL;
				}
				list->to[j] = SCREEN_AT;
				at_used = 1;
			} else
			if (token[i] == '\\' && token[i + 1] != '\0')
				list->to[j] = token[++i];
			else
				list->to[j] = token[i];
		}
		list->to[j] = '\0';
	}

	token = osmo_cc_strtok_quotes(&text);
	if (token) {
		free(list);
		PDEBUG(DCC, DEBUG_ERROR, "Error in screening definition '%s'.\n", text);
		PDEBUG(DCC, DEBUG_ERROR, "Got garbage behind screening result.\n");
		return -EINVAL;
	}

	/* attach screen entry to list */
	while (*list_p)
		list_p = &((*list_p)->next);
	*list_p = list;

	return 0;
}

void osmo_cc_flush_screen(osmo_cc_screen_list_t *list)
{
	osmo_cc_screen_list_t *temp;

	while (list) {
		temp = list;
		list = list->next;
		free(temp);
	}
}

const char *print_rule_string(const char *input)
{
	static char output[256];
	int i;

	for (i = 0; *input && i < (int)sizeof(output) - 1; i++, input++) {
		switch (*input) {
		case SCREEN_QUESTIONMARK:
			output[i] = '?';
			break;
		case SCREEN_STAR:
			output[i] = '*';
			break;
		case SCREEN_AT:
			output[i] = '@';
			break;
		default:
			output[i] = *input;
		}
	}

	output[i] = '\0';
	return output;
}

static int osmo_cc_screen(const char *what, osmo_cc_screen_list_t *list, uint8_t *type, uint8_t *present, char *id_to, int id_to_size, const char *id_from, const char **routing_p)
{
	const char *suffix;
	int i, j, rule;

	PDEBUG(DCC, DEBUG_INFO, "Screening %s '%s':\n", what, id_from);
	switch (*type) {
	case OSMO_CC_TYPE_UNKNOWN:
		PDEBUG(DCC, DEBUG_INFO, " -> type = unknown\n");
		break;
	case OSMO_CC_TYPE_INTERNATIONAL:
		PDEBUG(DCC, DEBUG_INFO, " -> type = international\n");
		break;
	case OSMO_CC_TYPE_NATIONAL:
		PDEBUG(DCC, DEBUG_INFO, " -> type = national\n");
		break;
	case OSMO_CC_TYPE_NETWORK:
		PDEBUG(DCC, DEBUG_INFO, " -> type = network\n");
		break;
	case OSMO_CC_TYPE_SUBSCRIBER:
		PDEBUG(DCC, DEBUG_INFO, " -> type = subscriber\n");
		break;
	case OSMO_CC_TYPE_ABBREVIATED:
		PDEBUG(DCC, DEBUG_INFO, " -> type = abbreviated\n");
		break;
	}
	if (present) switch (*present) {
	case OSMO_CC_PRESENT_ALLOWED:
		PDEBUG(DCC, DEBUG_INFO, " -> present = allowed\n");
		break;
	case OSMO_CC_PRESENT_RESTRICTED:
		PDEBUG(DCC, DEBUG_INFO, " -> present = restricted\n");
		break;
	}

	rule = 0;
	while (list) {
		rule++;
		PDEBUG(DCC, DEBUG_INFO, "Comparing with rule #%d: '%s':\n", rule, print_rule_string(list->from));
		if (list->has_from_type) switch (list->from_type) {
		case OSMO_CC_TYPE_UNKNOWN:
			PDEBUG(DCC, DEBUG_INFO, " -> type = unknown\n");
			break;
		case OSMO_CC_TYPE_INTERNATIONAL:
			PDEBUG(DCC, DEBUG_INFO, " -> type = international\n");
			break;
		case OSMO_CC_TYPE_NATIONAL:
			PDEBUG(DCC, DEBUG_INFO, " -> type = national\n");
			break;
		case OSMO_CC_TYPE_NETWORK:
			PDEBUG(DCC, DEBUG_INFO, " -> type = network\n");
			break;
		case OSMO_CC_TYPE_SUBSCRIBER:
			PDEBUG(DCC, DEBUG_INFO, " -> type = subscriber\n");
			break;
		case OSMO_CC_TYPE_ABBREVIATED:
			PDEBUG(DCC, DEBUG_INFO, " -> type = abbreviated\n");
			break;
		}
		if (list->has_from_present) switch (list->from_present) {
		case OSMO_CC_PRESENT_ALLOWED:
			PDEBUG(DCC, DEBUG_INFO, " -> present = allowed\n");
			break;
		case OSMO_CC_PRESENT_RESTRICTED:
			PDEBUG(DCC, DEBUG_INFO, " -> present = restricted\n");
			break;
		}
		suffix = NULL;
		/* attributes do not match */
		if (list->has_from_type && list->from_type != *type) {
			PDEBUG(DCC, DEBUG_INFO, "Rule does not match, because 'type' is different.\n");
			continue;
		}
		if (present && list->has_from_present && list->from_present != *present) {
			PDEBUG(DCC, DEBUG_INFO, "Rule does not match, because 'present' is different.\n");
			continue;
		}
		for (i = 0; list->from[i] && id_from[i]; i++) {
			/* '?' means: any digit, so it machtes */
			if (list->from[i] == SCREEN_QUESTIONMARK) {
				continue;
			}
			/* '*' means: anything may follow, so it machtes */
			if (list->from[i] == SCREEN_STAR) {
				suffix = id_from + i;
				break;
			}
			/* check if digit doesn't matches */
			if (list->from[i] != id_from[i])
				break;
		}
		/* if last checked digit is '*', we have a match */
		/* also if we hit EOL at id_from and next check digit is '*' */
		if (list->from[i] == SCREEN_STAR)
			break;
		/* if all digits have matched */
		if (list->from[i] == '\0' && id_from[i] == '\0')
			break;
		PDEBUG(DCC, DEBUG_INFO, "Rule does not match, because %s is different.\n", what);
		list = list->next;
	}

	/* if no list entry matches */
	if (!list)
		return -1;

	/* replace ID */
	if (list->has_to_type) {
		*type = list->to_type;
	}
	if (present && list->has_to_present) {
		*present = list->to_present;
	}
	for (i = j = 0; list->to[i]; i++) {
		if (j == id_to_size - 1)
			break;
		/* '*' means to use suffix of input string */
		if (list->to[i] == SCREEN_STAR && suffix) {
			while (*suffix) {
				id_to[j++] = *suffix++;
				if (j == id_to_size - 1)
					break;
			}
			continue;
		/* '@' means to stop and return routing also */
		} else if (list->to[i] == SCREEN_AT) {
			if (routing_p)
				*routing_p = &list->to[i + 1];
			break;
		}
		/* copy output digit */
		id_to[j++] = list->to[i];
	}
	id_to[j] = '\0';

	PDEBUG(DCC, DEBUG_INFO, "Rule matches, changing %s to '%s'.\n", what, print_rule_string(id_to));
	if (list->has_to_type) switch (list->to_type) {
	case OSMO_CC_TYPE_UNKNOWN:
		PDEBUG(DCC, DEBUG_INFO, " -> type = unknown\n");
		break;
	case OSMO_CC_TYPE_INTERNATIONAL:
		PDEBUG(DCC, DEBUG_INFO, " -> type = international\n");
		break;
	case OSMO_CC_TYPE_NATIONAL:
		PDEBUG(DCC, DEBUG_INFO, " -> type = national\n");
		break;
	case OSMO_CC_TYPE_NETWORK:
		PDEBUG(DCC, DEBUG_INFO, " -> type = network\n");
		break;
	case OSMO_CC_TYPE_SUBSCRIBER:
		PDEBUG(DCC, DEBUG_INFO, " -> type = subscriber\n");
		break;
	case OSMO_CC_TYPE_ABBREVIATED:
		PDEBUG(DCC, DEBUG_INFO, " -> type = abbreviated\n");
		break;
	}
	if (list->has_to_present) switch (list->to_present) {
	case OSMO_CC_PRESENT_ALLOWED:
		PDEBUG(DCC, DEBUG_INFO, " -> present = allowed\n");
		break;
	case OSMO_CC_PRESENT_RESTRICTED:
		PDEBUG(DCC, DEBUG_INFO, " -> present = restricted\n");
		break;
	}
	if (routing_p && *routing_p)
		PDEBUG(DCC, DEBUG_INFO, " -> remote = %s\n", *routing_p);

	return 0;
}

osmo_cc_msg_t *osmo_cc_screen_msg(osmo_cc_endpoint_t *ep, osmo_cc_msg_t *old_msg, int in, const char **routing_p)
{
	osmo_cc_msg_t *new_msg;
	char id[256], calling[256], called[256], redir[256];
	uint8_t calling_type, calling_plan, calling_present, calling_screen;
	uint8_t called_type, called_plan;
	uint8_t redir_type, redir_plan, redir_present, redir_screen, redir_reason;
	int calling_status = 0, called_status = 0, redir_status = 0;
	int rc;
	void *ie, *to_ie;
	uint8_t ie_type;
	uint16_t ie_length;
	void *ie_value;

	if (in && ep->screen_calling_in) {
		rc = osmo_cc_get_ie_calling(old_msg, 0, &calling_type, &calling_plan, &calling_present, &calling_screen, id, sizeof(id));
		if (rc >= 0) {
			rc = osmo_cc_screen("incoming caller ID", ep->screen_calling_in, &calling_type, &calling_present, calling, sizeof(calling), id, routing_p);
			if (rc >= 0)
				calling_status = 1;
		} else {
			calling_type = OSMO_CC_TYPE_UNKNOWN;
			calling_plan = OSMO_CC_PLAN_TELEPHONY;
			calling_present = OSMO_CC_PRESENT_ALLOWED;
			calling_screen = OSMO_CC_SCREEN_NETWORK;
			rc = osmo_cc_screen("incoming caller ID", ep->screen_calling_in, &calling_type, &calling_present, calling, sizeof(calling), "", routing_p);
			if (rc >= 0)
				calling_status = 1;
		}
		rc = osmo_cc_get_ie_redir(old_msg, 0, &redir_type, &redir_plan, &redir_present, &redir_screen, &redir_reason, id, sizeof(id));
		if (rc >= 0) {
			rc = osmo_cc_screen("incoming redirecting number", ep->screen_calling_in, &redir_type, &redir_present, redir, sizeof(redir), id, NULL);
			if (rc >= 0)
				redir_status = 1;
		}
	}
	if (in && ep->screen_called_in) {
		rc = osmo_cc_get_ie_called(old_msg, 0, &called_type, &called_plan, id, sizeof(id));
		if (rc >= 0) {
			rc = osmo_cc_screen("incoming dialed number", ep->screen_called_in, &called_type, NULL, called, sizeof(called), id, routing_p);
			if (rc >= 0)
				called_status = 1;
		} else {
			called_type = OSMO_CC_TYPE_UNKNOWN;
			called_plan = OSMO_CC_PLAN_TELEPHONY;
			rc = osmo_cc_screen("incoming dialed number", ep->screen_called_in, &called_type, NULL, called, sizeof(called), "", routing_p);
			if (rc >= 0)
				called_status = 1;
		}
	}
	if (!in && ep->screen_calling_out) {
		rc = osmo_cc_get_ie_calling(old_msg, 0, &calling_type, &calling_plan, &calling_present, &calling_screen, id, sizeof(id));
		if (rc >= 0) {
			rc = osmo_cc_screen("outgoing caller ID", ep->screen_calling_out, &calling_type, &calling_present, calling, sizeof(calling), id, NULL);
			if (rc >= 0)
				calling_status = 1;
		} else {
			calling_type = OSMO_CC_TYPE_UNKNOWN;
			calling_plan = OSMO_CC_PLAN_TELEPHONY;
			calling_present = OSMO_CC_PRESENT_ALLOWED;
			calling_screen = OSMO_CC_SCREEN_NETWORK;
			rc = osmo_cc_screen("outgoing caller ID", ep->screen_calling_out, &calling_type, &calling_present, calling, sizeof(calling), "", NULL);
			if (rc >= 0)
				calling_status = 1;
		}
		rc = osmo_cc_get_ie_redir(old_msg, 0, &redir_type, &redir_plan, &redir_present, &redir_screen, &redir_reason, id, sizeof(id));
		if (rc >= 0) {
			rc = osmo_cc_screen("outgoing redirecting number", ep->screen_calling_out, &redir_type, &redir_present, redir, sizeof(redir), id, NULL);
			if (rc >= 0)
				redir_status = 1;
		}
	}
	if (!in && ep->screen_called_out) {
		rc = osmo_cc_get_ie_called(old_msg, 0, &called_type, &called_plan, id, sizeof(id));
		if (rc >= 0) {
			rc = osmo_cc_screen("outgoing dialed number", ep->screen_called_out, &called_type, NULL, called, sizeof(called), id, NULL);
			if (rc >= 0)
				called_status = 1;
		} else {
			called_type = OSMO_CC_TYPE_UNKNOWN;
			called_plan = OSMO_CC_PLAN_TELEPHONY;
			rc = osmo_cc_screen("outgoing dialed number", ep->screen_called_out, &called_type, NULL, called, sizeof(called), "", NULL);
			if (rc >= 0)
				called_status = 1;
		}
	}

	/* nothing screened */
	if (!calling_status && !called_status && !redir_status)
		return old_msg;

	new_msg = osmo_cc_new_msg(old_msg->type);

	/* copy and replace */
	ie = old_msg->data;
        while ((ie_value = osmo_cc_msg_sep_ie(old_msg, &ie, &ie_type, &ie_length))) {
		switch (ie_type) {
		case OSMO_CC_IE_CALLING:
			if (calling_status) {
				osmo_cc_add_ie_calling(new_msg, calling_type, calling_plan, calling_present, calling_screen, calling);
				calling_status = 0;
				break;
			}
			goto copy;
		case OSMO_CC_IE_CALLED:
			if (called_status) {
				osmo_cc_add_ie_called(new_msg, called_type, called_plan, called);
				called_status = 0;
				break;
			}
			goto copy;
		case OSMO_CC_IE_REDIR:
			if (redir_status) {
				osmo_cc_add_ie_redir(new_msg, redir_type, redir_plan, redir_present, redir_screen, redir_reason, redir);
				redir_status = 0;
				break;
			}
			goto copy;
		default:
			copy:
			to_ie = osmo_cc_add_ie(new_msg, ie_type, ie_length);
			memcpy(to_ie, ie_value, ie_length);
		}
        }

	/* applend, if not yet in message (except redir, since it must exist) */
	if (calling_status)
		osmo_cc_add_ie_calling(new_msg, calling_type, calling_plan, calling_present, calling_screen, calling);
	if (called_status)
		osmo_cc_add_ie_called(new_msg, called_type, called_plan, called);

	free(old_msg);
	return new_msg;
}

