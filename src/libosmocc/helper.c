/* Osmo-CC: helpers to simplify Osmo-CC usage
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
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <inttypes.h>
#include "../libtimer/timer.h"
#include "../libdebug/debug.h"
#include "endpoint.h"
#include "helper.h"

osmo_cc_session_t *osmo_cc_helper_audio_offer(void *priv, struct osmo_cc_helper_audio_codecs *codecs, void (*receiver)(struct osmo_cc_session_codec *codec, uint16_t sequence_number, uint32_t timestamp, uint8_t *data, int len), osmo_cc_msg_t *msg, int debug)
{
	osmo_cc_session_t *session;
	osmo_cc_session_media_t *media;
	const char *sdp;
	int i;

	session = osmo_cc_new_session(priv, NULL, NULL, NULL, 0, 0, NULL, NULL, debug);
	if (!session)
		return NULL;

	media = osmo_cc_add_media(session, 0, 0, NULL, osmo_cc_session_media_type_audio, 0, osmo_cc_session_media_proto_rtp, 1, 1, receiver, debug);
	osmo_cc_rtp_open(media);

	for (i = 0; codecs[i].payload_name; i++)
		osmo_cc_add_codec(media, codecs[i].payload_name, codecs[i].payload_rate, codecs[i].payload_channels, codecs[i].encoder, codecs[i].decoder, debug);

	sdp = osmo_cc_session_send_offer(session);
	osmo_cc_add_ie_sdp(msg, sdp);

	return session;
}

const char *osmo_cc_helper_audio_accept(void *priv, struct osmo_cc_helper_audio_codecs *codecs, void (*receiver)(struct osmo_cc_session_codec *codec, uint16_t sequence_number, uint32_t timestamp, uint8_t *data, int len), osmo_cc_msg_t *msg, osmo_cc_session_t **session_p, osmo_cc_session_codec_t **codec_p, int force_our_codec)
{
	char offer_sdp[65536];
	const char *accept_sdp;
	osmo_cc_session_media_t *media, *selected_media = NULL;
	osmo_cc_session_codec_t *codec, *selected_codec = NULL;
	int rc;
	int i, selected_i;

	if (*session_p) {
		PDEBUG(DCC, DEBUG_ERROR, "Session already set, please fix!\n");
		abort();
	}
	if (*codec_p) {
		PDEBUG(DCC, DEBUG_ERROR, "Codec already set, please fix!\n");
		abort();
	}

	/* SDP IE */
	rc = osmo_cc_get_ie_sdp(msg, 0, offer_sdp, sizeof(offer_sdp));
	if (rc < 0) {
		PDEBUG(DCC, DEBUG_ERROR, "There is no SDP included in setup request.\n");
		return NULL;
	}

	*session_p = osmo_cc_session_receive_offer(priv, offer_sdp);
	if (!*session_p) {
		PDEBUG(DCC, DEBUG_ERROR, "Failed to parse SDP.\n");
		return NULL;
	}

	selected_i = -1;
	osmo_cc_session_for_each_media((*session_p)->media_list, media) {
		/* only audio */
		if (media->description.type != osmo_cc_session_media_type_audio)
			continue;
		osmo_cc_session_for_each_codec(media->codec_list, codec) {
			for (i = 0; codecs[i].payload_name; i++) {
				if (osmo_cc_session_if_codec(codec, codecs[i].payload_name, codecs[i].payload_rate, codecs[i].payload_channels)) {
					/* select the first matchting codec or the one we prefer */
					if (selected_i < 0 || i < selected_i) {
						selected_codec = codec;
						selected_media = media;
						selected_i = i;
					}
					/* if we don't force our preferred codec, use the preferred one from the remote */
					if (!force_our_codec)
						break;
				}
			}
		}
	}
	if (!selected_codec) {
		PDEBUG(DCC, DEBUG_ERROR, "No codec found in setup message that we support.\n");
		osmo_cc_free_session(*session_p);
		return NULL;
	}
	osmo_cc_session_accept_codec(selected_codec, codecs[selected_i].encoder, codecs[selected_i].decoder);
	osmo_cc_session_accept_media(selected_media, 0, 0, NULL, 1, 1, receiver);
	osmo_cc_rtp_open(selected_media);
	osmo_cc_rtp_connect(selected_media);
	*codec_p = selected_codec;

	accept_sdp = osmo_cc_session_send_answer(*session_p);
	if (!accept_sdp) {
		osmo_cc_free_session(*session_p);
		return NULL;
	}

	return accept_sdp;
}

int osmo_cc_helper_audio_negotiate(osmo_cc_msg_t *msg, osmo_cc_session_t **session_p, osmo_cc_session_codec_t **codec_p)
{
	char sdp[65536];
	osmo_cc_session_media_t *media;
	int rc;

	if (!(*session_p)) {
		PDEBUG(DCC, DEBUG_ERROR, "Session not set, please fix!\n");
		abort();
	}

	/* once done, just ignore further messages that reply to setup */
	if (*codec_p)
		return 0;

	/* SDP IE */
	rc = osmo_cc_get_ie_sdp(msg, 0, sdp, sizeof(sdp));
	if (rc < 0)
		return 0; // no reply in this message

	rc = osmo_cc_session_receive_answer(*session_p, sdp);
	if (rc < 0)
		return rc;

	osmo_cc_session_for_each_media((*session_p)->media_list, media) {
		/* only audio */
		if (media->description.type != osmo_cc_session_media_type_audio)
			continue;
		/* select first codec, if one was accpeted */
		if (media->codec_list)
			*codec_p = media->codec_list;
		if (*codec_p) {
			osmo_cc_rtp_connect(media);
			/* no more media streams */
			break;
		}
	}
	if (!(*codec_p)) {
		PDEBUG(DCC, DEBUG_ERROR, "No codec found in setup reply message that we support.\n");
		return -EIO;
	}

	return 0;
}

