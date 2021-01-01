/* Session Description Protocol parsing and generator
 * This shall be simple and is incomplete.
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
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "../libdebug/debug.h"
#include "../libtimer/timer.h"
#include "endpoint.h"
#include "sdp.h"

#define strncat_printf(sdp, fmt, arg...) \
	{ \
		snprintf(sdp + strlen(sdp), sizeof(sdp) - strlen(sdp), fmt, ## arg); \
		sdp[sizeof(sdp) - 1] = '\0'; \
	}

/* generate SDP from session structure */
char *osmo_cc_session_gensdp(osmo_cc_session_t *session)
{
	/* calc max size of SDP: quick an dirty (close to max UDP payload size) */
	static char sdp[65000];
	const char *username, *sess_id, *sess_version, *nettype, *addrtype, *unicast_address;
	const char *session_name;
	int individual_connection_data = 1; /* in case there is no media, there is no connection data */
	int individual_send_receive = 1; /* in case there is no media, there is no send/receive attribute */
	struct osmo_cc_session_media *media;
	struct osmo_cc_session_codec *codec;

	sdp[0] = 0;

	/* Version */
	strncat_printf(sdp, "v=0\r\n");

	/* Origin */
	username = session->origin_local.username;
	sess_id = session->origin_local.sess_id;
	sess_version = session->origin_local.sess_version;
	nettype = session->origin_local.nettype;
	addrtype = session->origin_local.addrtype;
	unicast_address = session->origin_local.unicast_address;
	strncat_printf(sdp, "o=%s %s %s %s %s %s\r\n", username, sess_id, sess_version, nettype, addrtype, unicast_address);

	/* Session */
	session_name = session->name;
	strncat_printf(sdp, "s=%s\r\n", session_name);

	/* Connection Data (if all media have the same data) */
	if (session->media_list) {
		osmo_cc_session_for_each_media(session->media_list->next, media) {
			if (session->media_list->connection_data_local.nettype != media->connection_data_local.nettype)
				break;
			if (session->media_list->connection_data_local.addrtype != media->connection_data_local.addrtype)
				break;
			if (!!strcmp(session->media_list->connection_data_local.address, media->connection_data_local.address))
				break;
		}
		if (!media)
			individual_connection_data = 0;
	}
	if (!individual_connection_data)
		strncat_printf(sdp, "c=%s %s %s\r\n", osmo_cc_session_nettype2string(session->media_list->connection_data_local.nettype), osmo_cc_session_addrtype2string(session->media_list->connection_data_local.addrtype), session->media_list->connection_data_local.address);

	/* timestamp */
	strncat_printf(sdp, "t=0 0\r\n");

	/* sendonly /recvonly (if all media have the same data) */
	if (session->media_list) {
		osmo_cc_session_for_each_media(session->media_list->next, media) {
			if (session->media_list->send != media->send)
				break;
			if (session->media_list->receive != media->receive)
				break;
		}
		if (!media)
			individual_send_receive = 0;
	}
	if (!individual_send_receive) {
		if (session->media_list->send && !session->media_list->receive)
			strncat_printf(sdp, "a=sendonly\r\n");
		if (!session->media_list->send && session->media_list->receive)
			strncat_printf(sdp, "a=recvonly\r\n");
		if (!session->media_list->send && !session->media_list->receive)
			strncat_printf(sdp, "a=inactive\r\n");
	}

	/* media */
	osmo_cc_session_for_each_media(session->media_list, media) {
		strncat_printf(sdp, "m=%s %u %s",
			osmo_cc_session_media_type2string(media->description.type) ? : media->description.type_name,
			media->description.port_local,
			osmo_cc_session_media_proto2string(media->description.proto) ? : media->description.proto_name);
		osmo_cc_session_for_each_codec(media->codec_list, codec)
			strncat_printf(sdp, " %u", codec->payload_type_local);
		strncat_printf(sdp, "\r\n");
		/* don't list rtpmap when session was canceled by setting port to 0 */
		if (media->description.port_local == 0)
			continue;
		if (individual_connection_data)
			strncat_printf(sdp, "c=%s %s %s\r\n", osmo_cc_session_nettype2string(media->connection_data_local.nettype), osmo_cc_session_addrtype2string(media->connection_data_local.addrtype), media->connection_data_local.address);
		osmo_cc_session_for_each_codec(media->codec_list, codec) {
			strncat_printf(sdp, "a=rtpmap:%u %s/%d", codec->payload_type_local, codec->payload_name, codec->payload_rate);
			if (codec->payload_channels >= 2)
				strncat_printf(sdp, "/%d", codec->payload_channels);
			strncat_printf(sdp, "\r\n");
		}
		if (individual_send_receive) {
			if (media->send && !media->receive)
				strncat_printf(sdp, "a=sendonly\r\n");
			if (!media->send && media->receive)
				strncat_printf(sdp, "a=recvonly\r\n");
			if (!media->send && !media->receive)
				strncat_printf(sdp, "a=inactive\r\n");
		}
	}

	/* check for overflow and return */
	if (strlen(sdp) == sizeof(sdp) - 1) {
		PDEBUG(DCC, DEBUG_ERROR, "Fatal error: Allocated SDP buffer with %d bytes is too small, please fix!\n", (int)sizeof(sdp));
		return NULL;
	}
	return sdp;
}

/* separate a word from string that is delimited with one or more space characters */
static char *wordsep(char **text_p)
{
	char *text = *text_p;
	static char word[256];
	int i;

	/* no text */
	if (text == NULL || *text == '\0')
		return NULL;
	/* skip spaces before text */
	while (*text && *text <= ' ')
		text++;
	/* copy content */
	i = 0;
	while (*text > ' ' && i < (int)sizeof(word))
		word[i++] = *text++;
	word[i] = '\0';
	/* set next */
	*text_p = text;
	return word;
}

/*
 * codecs and their default values
 *
 * if format is -1, payload type is dynamic
 * if rate is 0, rate may be any rate
 */
struct codec_defaults {
	int fmt;
	char *name;
	uint32_t rate;
	int channels;
} codec_defaults[] = {
	{ 0,	"PCMU",	 	8000,	1 },
	{ 3,	"GSM",	 	8000,	1 },
	{ 4,	"G723",	 	8000,	1 },
	{ 5,	"DVI4",	 	8000,	1 },
	{ 6,	"DVI4",	 	16000,	1 },
	{ 7,	"LPC",	 	8000,	1 },
	{ 8,	"PCMA",	 	8000,	1 },
	{ 9,	"G722",	 	8000,	1 },
	{ 10,	"L16",	 	44100,	2 },
	{ 11,	"L16",	 	44100,	1 },
	{ 12,	"QCELP", 	8000,	1 },
	{ 13,	"CN",	 	8000,	1 },
	{ 14,	"MPA",	 	90000,	1 },
	{ 15,	"G728",	 	8000,	1 },
	{ 16,	"DVI4",	 	11025,	1 },
	{ 17,	"DVI4",	 	22050,	1 },
	{ 18,	"G729",	 	8000,	1 },
	{ 25,	"CELB",	 	90000,	0 },
	{ 26,	"JPEG",	 	90000,	0 },
	{ 28,	"nv",	 	90000,	0 },
	{ 31,	"H261",	 	90000,	0 },
	{ 32,	"MPV",	 	90000,	0 },
	{ 33,	"MP2T",	 	90000,	0 },
	{ 34,	"H263",	 	90000,	0 },
	{ -1, NULL, 0, 0 },
};

static void complete_codec_by_fmt(uint8_t fmt, const char **name, uint32_t *rate, int *channels)
{
	int i;

	for (i = 0; codec_defaults[i].name; i++) {
		if (codec_defaults[i].fmt == fmt)
			break;
	}
	if (!codec_defaults[i].name)
		return;

	free((char *)*name);
	*name = strdup(codec_defaults[i].name);
	*rate = codec_defaults[i].rate;
	*channels = codec_defaults[i].channels;
}

int osmo_cc_payload_type_by_attrs(uint8_t *fmt, const char *name, uint32_t *rate, int *channels)
{
	int i;

	for (i = 0; codec_defaults[i].name; i++) {
		if (!strcmp(codec_defaults[i].name, name)
		 && (*rate == 0 || codec_defaults[i].rate == *rate)
		 && (*channels == 0 || codec_defaults[i].channels == *channels))
			break;
	}
	if (!codec_defaults[i].name)
		return -EINVAL;

	*fmt = codec_defaults[i].fmt;
	*rate = codec_defaults[i].rate;
	*channels = codec_defaults[i].channels;

	return 0;
}

/* parses data and codec list from SDP
 *
 * sdp = given SDP text
 * return: SDP session description structure */
struct osmo_cc_session *osmo_cc_session_parsesdp(void *priv, const char *_sdp)
{
	char buffer[strlen(_sdp) + 1], *sdp = buffer;
	char *line, *p, *word, *next_word;
	int line_no = 0;
	struct osmo_cc_session_connection_data ccd, *cd;
	int csend = 1, creceive = 1; /* common default */
	struct osmo_cc_session *session = NULL;
	struct osmo_cc_session_media *media = NULL;
	struct osmo_cc_session_codec *codec = NULL;

	/* prepare data */
	strcpy(sdp, _sdp);
	memset(&ccd, 0, sizeof(ccd));

	/* create SDP session description */
	session = osmo_cc_new_session(priv, NULL, NULL, NULL, osmo_cc_session_nettype_inet, osmo_cc_session_addrtype_ipv4, "127.0.0.1", NULL, 0); // values will be replaced by local definitions during negotiation

	/* check every line of SDP and parse its data */
	while(*sdp) {
		if ((p = strchr(sdp, '\r'))) {
			*p++ = '\0';
			if (*p == '\n')
				p++;
			line = sdp;
			sdp = p;
		} else if ((p = strchr(sdp, '\n'))) {
			*p++ = '\0';
			line = sdp;
			sdp = p;
		} else {
			line = sdp;
			sdp = strchr(sdp, '\0');
		}
		next_word = line + 2;
		line_no++;

		if (line[0] == '\0')
			continue;

		if (line[1] != '=') {
			PDEBUG(DCC, DEBUG_NOTICE, "SDP line %d = '%s' is garbage, expecting '=' as second character.\n", line_no, line);
			continue;
		}

		switch(line[0]) {
		case 'v':
			PDEBUG(DCC, DEBUG_DEBUG, " -> Version: %s\n", next_word);
			if (atoi(next_word) != 0) {
				PDEBUG(DCC, DEBUG_NOTICE, "SDP line %d = '%s' describes unsupported version.\n", line_no, line);
				osmo_cc_free_session(session);
				return NULL;
			}
			break;
		case 'o':
			PDEBUG(DCC, DEBUG_DEBUG, " -> Originator: %s\n", next_word);
			/* Originator */
			word = wordsep(&next_word);
			if (!word)
				break;
			free((char *)session->origin_remote.username); // if already set
			session->origin_remote.username = strdup(word);
			word = wordsep(&next_word);
			if (!word)
				break;
			free((char *)session->origin_remote.sess_id); // if already set
			session->origin_remote.sess_id = strdup(word);
			word = wordsep(&next_word);
			if (!word)
				break;
			free((char *)session->origin_remote.sess_version); // if already set
			session->origin_remote.sess_version = strdup(word);
			word = wordsep(&next_word);
			if (!word)
				break;
			free((char *)session->origin_remote.nettype); // if already set
			session->origin_remote.nettype = strdup(word);
			word = wordsep(&next_word);
			if (!word)
				break;
			free((char *)session->origin_remote.addrtype); // if already set
			session->origin_remote.addrtype = strdup(word);
			word = wordsep(&next_word);
			if (!word)
				break;
			free((char *)session->origin_remote.unicast_address); // if already set
			session->origin_remote.unicast_address = strdup(word);
			break;
		case 's':
			/* Session Name */
			PDEBUG(DCC, DEBUG_DEBUG, " -> Session Name: %s\n", next_word);
			free((char *)session->name); // if already set
			session->name = strdup(next_word);
			break;
		case 'c': /* Connection Data */
			PDEBUG(DCC, DEBUG_DEBUG, " -> Connection Data: %s\n", next_word);
			if (media)
				cd = &media->connection_data_remote;
			else
				cd = &ccd;
			/* network type */
			if (!(word = wordsep(&next_word)))
				break;
			if (!strcmp(word, "IN"))
				cd->nettype = osmo_cc_session_nettype_inet;
			else {
				PDEBUG(DCC, DEBUG_NOTICE, "Unsupported network type '%s' in SDP line %d = '%s'\n", word, line_no, line);
				break;
			}
			/* address type */
			if (!(word = wordsep(&next_word)))
				break;
			if (!strcmp(word, "IP4")) {
				cd->addrtype = osmo_cc_session_addrtype_ipv4;
				PDEBUG(DCC, DEBUG_DEBUG, " -> Address Type = IPv4\n");
			} else
			if (!strcmp(word, "IP6")) {
				cd->addrtype = osmo_cc_session_addrtype_ipv6;
				PDEBUG(DCC, DEBUG_DEBUG, " -> Address Type = IPv6\n");
			} else {
				PDEBUG(DCC, DEBUG_NOTICE, "Unsupported address type '%s' in SDP line %d = '%s'\n", word, line_no, line);
				break;
			}
			/* connection address */
			if (!(word = wordsep(&next_word)))
				break;
			if ((p = strchr(word, '/')))
				*p++ = '\0';
			free((char *)cd->address); // in case of multiple lines of 'c'
			cd->address = strdup(word);
			PDEBUG(DCC, DEBUG_DEBUG, " -> Address = %s\n", word);
			break;
		case 'm': /* Media Description */
			PDEBUG(DCC, DEBUG_DEBUG, " -> Media Description: %s\n", next_word);
			/* add media description */
			media = osmo_cc_add_media(session, 0, 0, NULL, 0, 0, 0, csend, creceive, NULL, 0);
			/* copy common connection data from common connection, if exists */
			cd = &media->connection_data_remote;
			memcpy(cd, &ccd, sizeof(*cd));
			/* media type */
			if (!(word = wordsep(&next_word)))
				break;
			if (!strcmp(word, "audio"))
				media->description.type = osmo_cc_session_media_type_audio;
			else
			if (!strcmp(word, "video"))
				media->description.type = osmo_cc_session_media_type_video;
			else {
				media->description.type = osmo_cc_session_media_type_unknown;
				media->description.type_name = strdup(word);
				PDEBUG(DCC, DEBUG_DEBUG, "Unsupported media type in SDP line %d = '%s'\n", line_no, line);
			}
			/* port */
			if (!(word = wordsep(&next_word)))
				break;
			media->description.port_remote = atoi(word);
			/* proto */
			if (!(word = wordsep(&next_word)))
				break;
			if (!strcmp(word, "RTP/AVP"))
				media->description.proto = osmo_cc_session_media_proto_rtp;
			else {
				media->description.proto = osmo_cc_session_media_proto_unknown;
				media->description.proto_name = strdup(word);
				PDEBUG(DCC, DEBUG_NOTICE, "Unsupported protocol type in SDP line %d = '%s'\n", line_no, line);
				break;
			}
			/* create codec description for each codec and link */
			while ((word = wordsep(&next_word))) {
				/* create codec */
				codec = osmo_cc_add_codec(media, NULL, 0, 1, NULL, NULL, 0);
				/* fmt */
				codec->payload_type_remote = atoi(word);
				complete_codec_by_fmt(codec->payload_type_remote, &codec->payload_name, &codec->payload_rate, &codec->payload_channels);
				PDEBUG(DCC, DEBUG_DEBUG, " -> payload type = %d\n", codec->payload_type_remote);
				if (codec->payload_name)
					PDEBUG(DCC, DEBUG_DEBUG, " -> payload name = %s\n", codec->payload_name);
				if (codec->payload_rate)
					PDEBUG(DCC, DEBUG_DEBUG, " -> payload rate = %d\n", codec->payload_rate);
				if (codec->payload_channels)
					PDEBUG(DCC, DEBUG_DEBUG, " -> payload channels = %d\n", codec->payload_channels);
			}
			break;
		case 'a':
			PDEBUG(DCC, DEBUG_DEBUG, " -> Attribute: %s\n", next_word);
			word = wordsep(&next_word);
			if (!strcmp(word, "sendrecv")) {
				if (media) {
					media->receive = 1;
					media->send = 1;
				} else {
					creceive = 1;
					csend = 1;
				}
				break;
			} else
			if (!strcmp(word, "recvonly")) {
				if (media) {
					media->receive = 1;
					media->send = 0;
				} else {
					creceive = 1;
					csend = 0;
				}
				break;
			} else
			if (!strcmp(word, "sendonly")) {
				if (media) {
					media->receive = 0;
					media->send = 1;
				} else {
					creceive = 0;
					csend = 1;
				}
				break;
			} else
			if (!strcmp(word, "inactive")) {
				if (media) {
					media->receive = 0;
					media->send = 0;
				} else {
					creceive = 0;
					csend = 0;
				}
				break;
			} else
			if (!media) {
				PDEBUG(DCC, DEBUG_NOTICE, "Attribute without previously defined media in SDP line %d = '%s'\n", line_no, line);
				break;
			}
			if (!strncmp(word, "rtpmap:", 7)) {
				int fmt = atoi(word + 7);
				osmo_cc_session_for_each_codec(media->codec_list, codec) {
					if (codec->payload_type_remote == fmt)
						break;
				}
				if (!codec) {
					PDEBUG(DCC, DEBUG_NOTICE, "Attribute without previously defined codec in SDP line %d = '%s'\n", line_no, line);
					break;
				}
				PDEBUG(DCC, DEBUG_DEBUG, " -> (rtpmap) payload type = %d\n", codec->payload_type_remote);
				if (!(word = wordsep(&next_word)))
					break;
				if ((p = strchr(word, '/')))
					*p++ = '\0';
				free((char *)codec->payload_name); // in case it is already set above
				codec->payload_name = strdup(word);
				PDEBUG(DCC, DEBUG_DEBUG, " -> (rtpmap) payload name = %s\n", codec->payload_name);
				if (!(word = p))
					break;
				if ((p = strchr(word, '/')))
					*p++ = '\0';
				codec->payload_rate = atoi(word);
				PDEBUG(DCC, DEBUG_DEBUG, " -> (rtpmap) payload rate = %d\n", codec->payload_rate);
				if (!(word = p)) {
					/* if no channel is given and no default was specified, we must set 1 channel */
					if (!codec->payload_channels)
						codec->payload_channels = 1;
					break;
				}
				codec->payload_channels = atoi(word);
				PDEBUG(DCC, DEBUG_DEBUG, " -> (rtpmap) payload channels = %d\n", codec->payload_channels);
			}
			break;
		}
	}

	/* if something is incomplete, abort here */
	if (osmo_cc_session_check(session, 1)) {
		PDEBUG(DCC, DEBUG_NOTICE, "Parsing SDP failed.\n");
		osmo_cc_free_session(session);
		return NULL;
	}

	return session;
}

void osmo_cc_debug_sdp(const char *_sdp)
{
	const unsigned char *sdp = (const unsigned char *)_sdp;
	char text[256];
	int i;

	while (*sdp) {
		for (i = 0; *sdp > 0 && *sdp >= 32 && i < (int)sizeof(text) - 1; i++)
			text[i] = *sdp++;
		text[i] = '\0';
		PDEBUG(DCC, DEBUG_DEBUG, " | %s\n", text);
		while (*sdp > 0 && *sdp < 32)
			sdp++;
	}
}

