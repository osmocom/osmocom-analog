/* Osmo-CC: Media Session handling
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
#include "../liboptions/options.h"
#include "endpoint.h"

#define NTP_OFFSET      2208988800U

void osmo_cc_set_local_peer(osmo_cc_session_config_t *conf, enum osmo_cc_session_nettype nettype, enum osmo_cc_session_addrtype addrtype, const char *address)
{
	conf->default_nettype = nettype;
	conf->default_addrtype = addrtype;
	conf->default_unicast_address = options_strdup(address);
}

osmo_cc_session_t *osmo_cc_new_session(osmo_cc_session_config_t *conf, void *priv, const char *username, const char *sess_id, const char *sess_version, enum osmo_cc_session_nettype nettype, enum osmo_cc_session_addrtype addrtype, const char *unicast_address, const char *session_name, int debug)
{
	osmo_cc_session_t *session;

	if (debug) PDEBUG(DCC, DEBUG_DEBUG, "Creating session structure.\n");

	session = calloc(1, sizeof(*session));
	if (!session) {
		PDEBUG(DCC, DEBUG_ERROR, "No mem!\n");
		abort();
	}
	session->config = conf;
	session->priv = priv;
	if (username) {
		int i;
		for (i = 0; username[i]; i++) {
			if ((uint8_t)username[i] < 33) {
				PDEBUG(DCC, DEBUG_ERROR, "Fatal error: SDP's originator (username) uses invalid characters, please fix!\n");
				abort();
			}
		}
		session->origin_local.username = strdup(username);
	}
	if (!username)
		session->origin_local.username = strdup("-");
	if (debug) PDEBUG(DCC, DEBUG_DEBUG, " -> user name = %s\n", session->origin_local.username);
	if (sess_id)
		session->origin_local.sess_id = strdup(sess_id);
	if (sess_version)
		session->origin_local.sess_version = strdup(sess_version);
	if (!sess_id || !sess_version) {
		struct timeval tv;
		char ntp_timestamp[32];
		/* get time NTP format time stamp (time since 1900) */
		gettimeofday(&tv, NULL);
		sprintf(ntp_timestamp, "%" PRIu64, (uint64_t)tv.tv_sec + NTP_OFFSET);
		if (!sess_id)
			session->origin_local.sess_id = strdup(ntp_timestamp);
		if (!sess_version)
			session->origin_local.sess_version = strdup(ntp_timestamp);
	}
	if (debug) PDEBUG(DCC, DEBUG_DEBUG, " -> session ID = %s\n", session->origin_local.sess_id);
	if (debug) PDEBUG(DCC, DEBUG_DEBUG, " -> session version = %s\n", session->origin_local.sess_version);
	if (nettype)
		session->origin_local.nettype = strdup(osmo_cc_session_nettype2string(nettype));
	else
		session->origin_local.nettype = strdup(osmo_cc_session_nettype2string(conf->default_nettype));
	if (debug) PDEBUG(DCC, DEBUG_DEBUG, " -> network type = %s\n", session->origin_local.nettype);
	if (addrtype)
		session->origin_local.addrtype = strdup(osmo_cc_session_addrtype2string(addrtype));
	else
		session->origin_local.addrtype = strdup(osmo_cc_session_addrtype2string(conf->default_addrtype));
	if (debug) PDEBUG(DCC, DEBUG_DEBUG, " -> address type = %s\n", session->origin_local.addrtype);
	if (unicast_address)
		session->origin_local.unicast_address = strdup(unicast_address);
	else
		session->origin_local.unicast_address = strdup(conf->default_unicast_address);
	if (debug) PDEBUG(DCC, DEBUG_DEBUG, " -> unicast address = %s\n", session->origin_local.unicast_address);
	if (session_name)
		session->name = strdup(session_name);
	if (!session_name)
		session->name = strdup("-");
	if (debug) PDEBUG(DCC, DEBUG_DEBUG, " -> session name = %s\n", session->name);

	return session;
}

void osmo_cc_free_session(osmo_cc_session_t *session)
{
	PDEBUG(DCC, DEBUG_DEBUG, "Free session structure.\n");

	free((char *)session->origin_local.username);
	free((char *)session->origin_local.sess_id);
	free((char *)session->origin_local.sess_version);
	free((char *)session->origin_local.nettype);
	free((char *)session->origin_local.addrtype);
	free((char *)session->origin_local.unicast_address);
	free((char *)session->origin_remote.username);
	free((char *)session->origin_remote.sess_id);
	free((char *)session->origin_remote.sess_version);
	free((char *)session->origin_remote.nettype);
	free((char *)session->origin_remote.addrtype);
	free((char *)session->origin_remote.unicast_address);
	free((char *)session->name);
	while (session->media_list)
		osmo_cc_free_media(session->media_list);
	free(session);
}

osmo_cc_session_media_t *osmo_cc_add_media(osmo_cc_session_t *session, enum osmo_cc_session_nettype nettype, enum osmo_cc_session_addrtype addrtype, const char *address, enum osmo_cc_session_media_type type, uint16_t port, enum osmo_cc_session_media_proto proto, int send, int receive, void (*receiver)(struct osmo_cc_session_codec *codec, uint8_t marker, uint16_t sequence_number, uint32_t timestamp, uint32_t ssrc, uint8_t *data, int len), int debug)
{
	osmo_cc_session_config_t *conf = session->config;
	osmo_cc_session_media_t *media, **mediap;

	media = calloc(1, sizeof(*media));
	if (!media) {
		PDEBUG(DCC, DEBUG_ERROR, "No mem!\n");
		abort();
	}
	media->session = session;
	if (nettype)
		media->connection_data_local.nettype = nettype;
	else
		media->connection_data_local.nettype = conf->default_nettype;
	if (addrtype)
		media->connection_data_local.addrtype = addrtype;
	else
		media->connection_data_local.addrtype = conf->default_addrtype;
	if (address)
		media->connection_data_local.address = strdup(address);
	else
		media->connection_data_local.address = strdup(conf->default_unicast_address);
	media->description.type = type;
	media->description.port_local = port;
	media->description.proto = proto;
	media->send = send;
	media->receive = receive;
	media->receiver = receiver;
	media->tx_sequence = random();
	media->tx_timestamp = random();
	mediap = &media->session->media_list;
	while (*mediap)
		mediap = &((*mediap)->next);
	*mediap = media;

	if (debug) PDEBUG(DCC, DEBUG_DEBUG, "Adding session media.\n");
	if (debug) PDEBUG(DCC, DEBUG_DEBUG, " -> network type = %s\n", osmo_cc_session_nettype2string(media->connection_data_local.nettype));
	if (debug) PDEBUG(DCC, DEBUG_DEBUG, " -> address type = %s\n", osmo_cc_session_addrtype2string(media->connection_data_local.addrtype));
	if (debug) PDEBUG(DCC, DEBUG_DEBUG, " -> address = %s\n", media->connection_data_local.address);
	if (debug) PDEBUG(DCC, DEBUG_DEBUG, " -> media type = %s\n", osmo_cc_session_media_type2string(media->description.type));
	if (debug) PDEBUG(DCC, DEBUG_DEBUG, " -> media port = %d\n", media->description.port_local);
	if (debug) PDEBUG(DCC, DEBUG_DEBUG, " -> media proto = %s\n", osmo_cc_session_media_proto2string(media->description.proto));
	if (debug) PDEBUG(DCC, DEBUG_DEBUG, "Opening and binding media port %d\n", media->description.port_local);

	return media;
}

void osmo_cc_free_media(osmo_cc_session_media_t *media)
{
	osmo_cc_session_media_t **mediap;

	PDEBUG(DCC, DEBUG_DEBUG, "Free session media.\n");

	osmo_cc_rtp_close(media);
	free((char *)media->connection_data_local.nettype_name);
	free((char *)media->connection_data_local.addrtype_name);
	free((char *)media->connection_data_local.address);
	free((char *)media->connection_data_remote.nettype_name);
	free((char *)media->connection_data_remote.addrtype_name);
	free((char *)media->connection_data_remote.address);
	while (media->codec_list)
		osmo_cc_free_codec(media->codec_list);
	mediap = &media->session->media_list;
	while (*mediap != media)
		mediap = &((*mediap)->next);
	*mediap = media->next;
	free(media);
}

osmo_cc_session_codec_t *osmo_cc_add_codec(osmo_cc_session_media_t *media, const char *payload_name, uint32_t payload_rate, int payload_channels, void (*encoder)(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len), void (*decoder)(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len), int debug)
{
	osmo_cc_session_codec_t *codec, **codecp;
	int rc;

	codec = calloc(1, sizeof(*codec));
	if (!codec) {
		PDEBUG(DCC, DEBUG_ERROR, "No mem!\n");
		abort();
	}
	codec->media = media;
	if (payload_name) {
		codec->payload_name = strdup(payload_name);
		codec->payload_rate = payload_rate;
		codec->payload_channels = payload_channels;
		rc = osmo_cc_payload_type_by_attrs(&codec->payload_type_local, payload_name, &payload_rate, &payload_channels);
		if (rc < 0) {
			/* hunt for next free dynamic payload type */
			uint8_t fmt = 96;
			osmo_cc_session_codec_t *c;
			osmo_cc_session_for_each_codec(media->codec_list, c) {
				if (c->payload_type_local >= fmt)
					fmt = c->payload_type_local + 1;
			}
			codec->payload_type_local = fmt;
		}
	}
	codec->encoder = encoder;
	codec->decoder = decoder;
	codecp = &codec->media->codec_list;
	while (*codecp)
		codecp = &((*codecp)->next);
	*codecp = codec;

	if (debug) PDEBUG(DCC, DEBUG_DEBUG, "Adding session codec.\n");
	if (debug) PDEBUG(DCC, DEBUG_DEBUG, " -> payload type = %d\n", codec->payload_type_local);
	if (debug) PDEBUG(DCC, DEBUG_DEBUG, " -> payload name = %s\n", codec->payload_name);
	if (debug) PDEBUG(DCC, DEBUG_DEBUG, " -> payload rate = %d\n", codec->payload_rate);
	if (debug) PDEBUG(DCC, DEBUG_DEBUG, " -> payload channels = %d\n", codec->payload_channels);

	return codec;
}

void osmo_cc_free_codec(osmo_cc_session_codec_t *codec)
{
	osmo_cc_session_codec_t **codecp;

	PDEBUG(DCC, DEBUG_DEBUG, "Free session codec.\n");

	free((char *)codec->payload_name);
	codecp = &codec->media->codec_list;
	while (*codecp != codec)
		codecp = &((*codecp)->next);
	*codecp = codec->next;
	free(codec);
}

int osmo_cc_session_check(osmo_cc_session_t *session, int remote)
{
	struct osmo_cc_session_origin *orig;
	struct osmo_cc_session_media *media;
	struct osmo_cc_session_connection_data *cd;
	struct osmo_cc_session_media_description *md;
	struct osmo_cc_session_codec *codec;
	int i, j;

	if (remote)
		orig = &session->origin_remote;
	else
		orig = &session->origin_local;
	if (!orig->username
	 || !orig->sess_id
	 || !orig->sess_version
	 || !orig->nettype
	 || !orig->addrtype
	 || !orig->unicast_address) {
		PDEBUG(DCC, DEBUG_NOTICE, "Missing data in session origin\n");
		return -EINVAL;
	}
	if (!session->name) {
		PDEBUG(DCC, DEBUG_NOTICE, "Missing data in session origin\n");
		return -EINVAL;
	}
	if (!session->media_list) {
		PDEBUG(DCC, DEBUG_NOTICE, "Missing media session\n");
		return -EINVAL;
	}
	i = 0;
	osmo_cc_session_for_each_media(session->media_list, media) {
		i++;
		if (remote)
			cd = &media->connection_data_remote;
		else
			cd = &media->connection_data_local;
		if (!cd->nettype && !cd->nettype_name) {
			PDEBUG(DCC, DEBUG_NOTICE, "Session with media #%d is missing connection network type\n", i);
			return -EINVAL;
		}
		if (!cd->addrtype && !cd->addrtype_name) {
			PDEBUG(DCC, DEBUG_NOTICE, "Session with media #%d is missing connection address type\n", i);
			return -EINVAL;
		}
		if (!cd->address) {
			PDEBUG(DCC, DEBUG_NOTICE, "Session with media #%d is missing connection address\n", i);
			return -EINVAL;
		}
		md = &media->description;
		if (!md->type && !md->type_name) {
			PDEBUG(DCC, DEBUG_NOTICE, "Session with media #%d is missing media type\n", i);
			return -EINVAL;
		}
		if (!md->proto && !md->proto_name) {
			PDEBUG(DCC, DEBUG_NOTICE, "Session with media #%d is missing protocol\n", i);
			return -EINVAL;
		}
		j = 0;
		osmo_cc_session_for_each_codec(media->codec_list, codec) {
			j++;
			if (!codec->payload_name) {
				PDEBUG(DCC, DEBUG_NOTICE, "Session with media #%d, codec #%d is missing name\n", i, j);
				return -EINVAL;
			}
			if (!codec->payload_rate) {
				PDEBUG(DCC, DEBUG_NOTICE, "Session with media #%d, codec #%d is missing rate\n", i, j);
				return -EINVAL;
			}
			if (!codec->payload_channels) {
				PDEBUG(DCC, DEBUG_NOTICE, "Session with media #%d, codec #%d is missing channel count\n", i, j);
				return -EINVAL;
			}
		}
	}

	return 0;
}

/* check session description and generate SDP */
const char *osmo_cc_session_send_offer(osmo_cc_session_t *session)
{
	const char *sdp;
	int rc;

	PDEBUG(DCC, DEBUG_DEBUG, "Generating session offer and opening RTP stream.\n");

	rc = osmo_cc_session_check(session, 0);
	if (rc < 0) {
		PDEBUG(DCC, DEBUG_ERROR, "Please fix!\n");
		abort();
	}

	sdp = osmo_cc_session_gensdp(session);
	osmo_cc_debug_sdp(sdp);

	return sdp;
}

osmo_cc_session_t *osmo_cc_session_receive_offer(osmo_cc_session_config_t *conf, void *priv, const char *sdp)
{
	osmo_cc_session_t *session;
	int rc;

	PDEBUG(DCC, DEBUG_DEBUG, "Parsing session offer.\n");

	osmo_cc_debug_sdp(sdp);
	session = osmo_cc_session_parsesdp(conf, priv, sdp);
	if (!session)
		return NULL;

	rc = osmo_cc_session_check(session, 0);
	if (rc < 0) {
		osmo_cc_free_session(session);
		return NULL;
	}

	return session;
}

void osmo_cc_session_accept_media(osmo_cc_session_media_t *media, enum osmo_cc_session_nettype nettype, enum osmo_cc_session_addrtype addrtype, const char *address, int send, int receive, void (*receiver)(struct osmo_cc_session_codec *codec, uint8_t marker, uint16_t sequence_number, uint32_t timestamp, uint32_t ssrc, uint8_t *data, int len))
{
	osmo_cc_session_config_t *conf = media->session->config;

	media->accepted = 1;
	if (nettype)
		media->connection_data_local.nettype = nettype;
	else
		media->connection_data_local.nettype = conf->default_nettype;
	if (addrtype)
		media->connection_data_local.addrtype = addrtype;
	else
		media->connection_data_local.addrtype = conf->default_addrtype;
	free((char *)media->connection_data_local.address);
	if (address)
		media->connection_data_local.address = strdup(address);
	else
		media->connection_data_local.address = strdup(conf->default_unicast_address);
	media->send = send;
	media->receive = receive;
	media->receiver = receiver;

	PDEBUG(DCC, DEBUG_DEBUG, "Accepting session media.\n");
	PDEBUG(DCC, DEBUG_DEBUG, " -> network type = %s\n", osmo_cc_session_nettype2string(media->connection_data_local.nettype));
	PDEBUG(DCC, DEBUG_DEBUG, " -> address type = %s\n", osmo_cc_session_addrtype2string(media->connection_data_local.addrtype));
	PDEBUG(DCC, DEBUG_DEBUG, " -> address = %s\n", media->connection_data_local.address);
}


void osmo_cc_session_accept_codec(osmo_cc_session_codec_t *codec, void (*encoder)(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len), void (*decoder)(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len))
{
	codec->accepted = 1;
	codec->encoder = encoder;
	codec->decoder = decoder;
	/* when we accept a codec, we just use the same payload type as the remote */
	codec->payload_type_local = codec->payload_type_remote;

	PDEBUG(DCC, DEBUG_DEBUG, "Accepting session codec.\n");
	PDEBUG(DCC, DEBUG_DEBUG, " -> payload type = %d\n", codec->payload_type_local);
	PDEBUG(DCC, DEBUG_DEBUG, " -> payload name = %s\n", codec->payload_name);
	PDEBUG(DCC, DEBUG_DEBUG, " -> payload rate = %d\n", codec->payload_rate);
	PDEBUG(DCC, DEBUG_DEBUG, " -> payload channels = %d\n", codec->payload_channels);
}

/* remove codecs/media that have not been accepted and generate SDP */
const char *osmo_cc_session_send_answer(osmo_cc_session_t *session)
{
	osmo_cc_session_media_t *media;
	osmo_cc_session_codec_t *codec, **codec_p;
	const char *sdp;
	int rc;

	PDEBUG(DCC, DEBUG_DEBUG, "Generating session answer.\n");

	/* loop all media */
	osmo_cc_session_for_each_media(session->media_list, media) {
		/* remove unaccepted codecs */
		codec_p = &media->codec_list;
		codec = *codec_p;
		while (codec) {
			if (!codec->accepted) {
				osmo_cc_free_codec(codec);
				codec = *codec_p;
				continue;
			}
			codec_p = &codec->next;
			codec = *codec_p;
		}
		/* mark media as unused, if no codec or not accepted */
		if (!media->accepted || !media->codec_list)
			media->description.port_local = 0;
	}

	rc = osmo_cc_session_check(session, 0);
	if (rc < 0) {
		PDEBUG(DCC, DEBUG_ERROR, "Please fix!\n");
		abort();
	}

	sdp = osmo_cc_session_gensdp(session);
	osmo_cc_debug_sdp(sdp);

	return sdp;
}

/* Apply remote session description to local session description.
 * If remote media's port is 0, remove from local session description.
 * If codecs in the remote session description are missing, remove from local session description.
 */
static int osmo_cc_session_negotiate(osmo_cc_session_t *session_local, struct osmo_cc_session *session_remote)
{
	osmo_cc_session_media_t *media_local, *media_remote, **media_local_p;
	osmo_cc_session_codec_t *codec_local, *codec_remote, **codec_local_p;
	int rc;

	PDEBUG(DCC, DEBUG_DEBUG, "Negotiating session.\n");

	/* copy remote session information */
	session_local->origin_remote.username = strdup(session_remote->origin_remote.username);
	session_local->origin_remote.sess_id = strdup(session_remote->origin_remote.sess_id);
	session_local->origin_remote.sess_version = strdup(session_remote->origin_remote.sess_version);
	session_local->origin_remote.nettype = strdup(session_remote->origin_remote.nettype);
	session_local->origin_remote.addrtype = strdup(session_remote->origin_remote.addrtype);
	session_local->origin_remote.unicast_address = strdup(session_remote->origin_remote.unicast_address);

	/* loop all media */
	for (media_local = session_local->media_list, media_remote = session_remote->media_list; media_local && media_remote; media_local = media_local->next, media_remote = media_remote->next) {
		/* copy remote media information */
		media_local->connection_data_remote.nettype = media_remote->connection_data_remote.nettype;
		if (media_remote->connection_data_remote.nettype_name)
			media_local->connection_data_remote.nettype_name = strdup(media_remote->connection_data_remote.nettype_name);
		media_local->connection_data_remote.addrtype = media_remote->connection_data_remote.addrtype;
		if (media_remote->connection_data_remote.addrtype_name)
			media_local->connection_data_remote.addrtype_name = strdup(media_remote->connection_data_remote.addrtype_name);
		if (media_remote->connection_data_remote.address)
			media_local->connection_data_remote.address = strdup(media_remote->connection_data_remote.address);
		media_local->description.port_remote = media_remote->description.port_remote;
		media_local->send = media_remote->send;
		media_local->receive = media_remote->receive;
		/* loop all codecs and remove if they are not found in local session description */
		codec_local_p = &media_local->codec_list;
		codec_local = *codec_local_p;
		while (codec_local) {
			/* search for equal codec, payload type may differe for each direction */
			osmo_cc_session_for_each_codec(media_remote->codec_list, codec_remote) {
				if (!strcmp(codec_local->payload_name, codec_remote->payload_name)
				 && codec_local->payload_rate == codec_remote->payload_rate
				 && codec_local->payload_channels == codec_remote->payload_channels)
					break;
			}
			if (!codec_remote) {
				osmo_cc_free_codec(codec_local);
				codec_local = *codec_local_p;
				continue;
			}
			/* copy remote codec information */
			codec_local->payload_type_remote = codec_remote->payload_type_remote;
			codec_local_p = &codec_local->next;
			codec_local = *codec_local_p;
		}
	}
	if (media_local) {
		PDEBUG(DCC, DEBUG_NOTICE, "Negotiation failed, because remote endpoint returns less media streams than we offered.\n");
		return -EINVAL;
	}
	if (media_remote) {
		PDEBUG(DCC, DEBUG_NOTICE, "Negotiation failed, because remote endpoint returns more media streams than we offered.\n");
		return -EINVAL;
	}

	/* remove media with port == 0 or no codec at all */
	media_local_p = &session_local->media_list;
	media_local = *media_local_p;
	while (media_local) {
		if (media_local->description.port_remote == 0 || !media_local->codec_list) {
			osmo_cc_free_media(media_local);
			media_local = *media_local_p;
			continue;
		}
		media_local_p = &media_local->next;
		media_local = *media_local_p;
	}	

	rc = osmo_cc_session_check(session_local, 1);
	if (rc < 0)
		return rc;

	return 0;
}

int osmo_cc_session_receive_answer(osmo_cc_session_t *session, const char *sdp)
{
	osmo_cc_session_t *session_remote;
	int rc;

	PDEBUG(DCC, DEBUG_DEBUG, "Parsing session answer.\n");

	osmo_cc_debug_sdp(sdp);
	session_remote = osmo_cc_session_parsesdp(session->config, NULL, sdp);
	if (!session_remote)
		return -EINVAL;

	rc = osmo_cc_session_check(session_remote, 1);
	if (rc < 0) {
		osmo_cc_free_session(session_remote);
		return rc;
	}
	rc = osmo_cc_session_negotiate(session, session_remote);
	if (rc < 0) {
		osmo_cc_free_session(session_remote);
		return rc;
	}
	osmo_cc_free_session(session_remote);

	return 0;
}

const char *osmo_cc_session_nettype2string(enum osmo_cc_session_nettype nettype)
{
	switch (nettype) {
	case osmo_cc_session_nettype_inet:
		return "IN";
	default:
		return NULL;
	}
}

const char *osmo_cc_session_addrtype2string(enum osmo_cc_session_addrtype addrtype)
{
	switch (addrtype) {
	case osmo_cc_session_addrtype_ipv4:
		return "IP4";
	case osmo_cc_session_addrtype_ipv6:
		return "IP6";
	default:
		return NULL;
	}
}

const char *osmo_cc_session_media_type2string(enum osmo_cc_session_media_type media_type)
{
	switch (media_type) {
	case osmo_cc_session_media_type_audio:
		return "audio";
	case osmo_cc_session_media_type_video:
		return "video";
	default:
		return NULL;
	}
}

const char *osmo_cc_session_media_proto2string(enum osmo_cc_session_media_proto media_proto)
{
	switch (media_proto) {
	case osmo_cc_session_media_proto_rtp:
		return "RTP/AVP";
	default:
		return NULL;
	}
}

int osmo_cc_session_if_codec(osmo_cc_session_codec_t *codec, const char *name, uint32_t rate, int channels)
{
	return (!strcmp(codec->payload_name, name)
	     && codec->payload_rate == rate
	     && codec->payload_channels == channels);
}

int osmo_cc_session_handle(osmo_cc_session_t *session)
{
	osmo_cc_session_media_t *media;
	int w = 0, rc;

	osmo_cc_session_for_each_media(session->media_list, media) {
		do {
			rc = osmo_cc_rtp_receive(media);
			if (rc >= 0)
				w = 1;
		} while (rc >= 0);
	}

	return w;
}

