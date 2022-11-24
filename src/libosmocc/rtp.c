/* Osmo-CC: RTP handling
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
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "../libdebug/debug.h"
#include "../libtimer/timer.h"
#include "endpoint.h"

#define RTP_VERSION 2

void osmo_cc_set_rtp_ports(osmo_cc_session_config_t *conf, uint16_t from, uint16_t to)
{
	conf->rtp_port_next = from;
	conf->rtp_port_from = from;
	conf->rtp_port_to = to;
}

struct rtp_hdr {
	uint8_t byte0;
	uint8_t byte1;
	uint16_t sequence;
	uint32_t timestamp;
	uint32_t ssrc;
} __attribute__((packed));

struct rtp_x_hdr {
	uint16_t by_profile;
	uint16_t length;
} __attribute__((packed));

static int rtp_receive(int sock, uint8_t **payload_p, int *payload_len_p, uint8_t *marker_p, uint8_t *pt_p, uint16_t *sequence_p, uint32_t *timestamp_p, uint32_t *ssrc_p)
{
	static uint8_t data[2048];
	int len;
	struct rtp_hdr *rtph = (struct rtp_hdr *)data;
	uint8_t version, padding, extension, csrc_count, marker, payload_type;
	struct rtp_x_hdr *rtpxh;
	uint8_t *payload;
	int payload_len;
	int x_len;

	len = read(sock, data, sizeof(data));
	if (len < 0) {
		if (errno == EAGAIN)
			return -EAGAIN;
		PDEBUG(DCC, DEBUG_DEBUG, "Read errno = %d (%s)\n", errno, strerror(errno));
		return -EIO;
	}
	if (len < 12) {
		PDEBUG(DCC, DEBUG_NOTICE, "Received RTP frame too short (len = %d).\n", len);
		return -EINVAL;
	}

	version = rtph->byte0 >> 6;
	padding = (rtph->byte0 >> 5) & 1;
	extension = (rtph->byte0 >> 4) & 1;
	csrc_count = rtph->byte0 & 0x0f;
	marker = rtph->byte1 >> 7;
	payload_type = rtph->byte1 & 0x7f;
	*sequence_p = ntohs(rtph->sequence);
	*timestamp_p = ntohl(rtph->timestamp);
	*ssrc_p = ntohl(rtph->ssrc);

	if (version != RTP_VERSION) {
		PDEBUG(DCC, DEBUG_NOTICE, "Received RTP version %d not supported.\n", version);
		return -EINVAL;
	}

	payload = data + sizeof(*rtph) + (csrc_count << 2);
	payload_len = len - sizeof(*rtph) - (csrc_count << 2);
	if (payload_len < 0) {
		PDEBUG(DCC, DEBUG_NOTICE, "Received RTP frame too short (len = %d, csrc count = %d).\n", len, csrc_count);
		return -EINVAL;
	}

	if (extension) {
		if (payload_len < (int)sizeof(*rtpxh)) {
			PDEBUG(DCC, DEBUG_NOTICE, "Received RTP frame too short for extension header.\n");
			return -EINVAL;
		}
		rtpxh = (struct rtp_x_hdr *)payload;
		x_len = ntohs(rtpxh->length) * 4 + sizeof(*rtpxh);
		payload += x_len;
		payload_len -= x_len;
		if (payload_len < (int)sizeof(*rtpxh)) {
			PDEBUG(DCC, DEBUG_NOTICE, "Received RTP frame too short, extension header exceeds frame length.\n");
			return -EINVAL;
		}
	}

	if (padding) {
		if (payload_len < 1) {
			PDEBUG(DCC, DEBUG_NOTICE, "Received RTP frame too short for padding length.\n");
			return -EINVAL;
		}
		payload_len -= payload[payload_len - 1];
		if (payload_len < 0) {
			PDEBUG(DCC, DEBUG_NOTICE, "Received RTP frame padding is greater than payload.\n");
			return -EINVAL;
		}
	}

	*payload_p = payload;
	*payload_len_p = payload_len;
	*marker_p = marker;
	*pt_p = payload_type;

	return 0;
}

static void rtp_send(int sock, uint8_t *payload, int payload_len, uint8_t marker, uint8_t pt, uint16_t sequence, uint32_t timestamp, uint32_t ssrc)
{
	struct rtp_hdr *rtph;
	char data[sizeof(*rtph) + payload_len];
	int len, rc;
	
	rtph = (struct rtp_hdr *)data;
	len = sizeof(*rtph);
	rtph->byte0 = RTP_VERSION << 6;
	rtph->byte1 = pt | (marker << 7);
	rtph->sequence = htons(sequence);
	rtph->timestamp = htonl(timestamp);
	rtph->ssrc = htonl(ssrc);
	len += payload_len;
	if (len > (int)sizeof(data)) {
		PDEBUG(DCC, DEBUG_NOTICE, "Buffer overflow, please fix!.\n");
		abort();
	}
	memcpy(data + sizeof(*rtph), payload, payload_len);

	rc = write(sock, data, len);
	if (rc < 0)
		PDEBUG(DCC, DEBUG_DEBUG, "Write errno = %d (%s)\n", errno, strerror(errno));
}

/* open and bind RTP
 * set local port to what we bound
 */
int osmo_cc_rtp_open(osmo_cc_session_media_t *media)
{
	osmo_cc_session_config_t *conf = media->session->config;
	int domain = 0; // make GCC happy
	uint16_t start_port;
	struct sockaddr_storage sa;
	int slen = 0; // make GCC happy
	struct sockaddr_in6 *sa6;
	struct sockaddr_in *sa4;
	uint16_t *sport;
	int flags;
	int rc;

	media->tx_ssrc = rand();

	osmo_cc_rtp_close(media);

	switch (media->connection_data_local.addrtype) {
	case osmo_cc_session_addrtype_ipv4:
		domain = AF_INET;
		memset(&sa, 0, sizeof(sa));
		sa4 = (struct sockaddr_in *)&sa;
		sa4->sin_family = domain;
		rc = inet_pton(AF_INET, media->connection_data_local.address, &sa4->sin_addr);
		if (rc < 1) {
pton_error:
			PDEBUG(DCC, DEBUG_NOTICE, "Cannot bind to address '%s'.\n", media->connection_data_local.address);
			return -EINVAL;
		}
		sport = &sa4->sin_port;
		slen = sizeof(*sa4);
		break;
	case osmo_cc_session_addrtype_ipv6:
		domain = AF_INET6;
		memset(&sa, 0, sizeof(sa));
		sa6 = (struct sockaddr_in6 *)&sa;
		sa6->sin6_family = domain;
		rc = inet_pton(AF_INET6, media->connection_data_local.address, &sa6->sin6_addr);
		if (rc < 1)
			goto pton_error;
		sport = &sa6->sin6_port;
		slen = sizeof(*sa6);
		break;
	case osmo_cc_session_addrtype_unknown:
		PDEBUG(DCC, DEBUG_NOTICE, "Unsupported address type '%s'.\n", media->connection_data_local.addrtype_name);
		return -EINVAL;
	}

	/* rtp_port_from/rtp_port_to may be changed at run time, so rtp_port_next can become out of range. */
	if (conf->rtp_port_next < conf->rtp_port_from || conf->rtp_port_next > conf->rtp_port_to)
		conf->rtp_port_next = conf->rtp_port_from;
	start_port = conf->rtp_port_next;
	while (1) {
		/* open sockets */
		rc = socket(domain, SOCK_DGRAM, IPPROTO_UDP);
		if (rc < 0) {
socket_error:
			PDEBUG(DCC, DEBUG_ERROR, "Cannot create socket (domain=%d, errno=%d(%s))\n", domain, errno, strerror(errno));
			osmo_cc_rtp_close(media);
			return -EIO;
		}
		media->rtp_socket = rc;
		rc = socket(domain, SOCK_DGRAM, IPPROTO_UDP);
		if (rc < 0)
			goto socket_error;
		media->rtcp_socket = rc;

		/* bind sockets */
		*sport = htons(conf->rtp_port_next);
		rc = bind(media->rtp_socket, (struct sockaddr *)&sa, slen);
		if (rc < 0) {
bind_error:
			osmo_cc_rtp_close(media);
			conf->rtp_port_next = (conf->rtp_port_next + 2 > conf->rtp_port_to) ? conf->rtp_port_from : conf->rtp_port_next + 2;
			if (conf->rtp_port_next == start_port) {
				PDEBUG(DCC, DEBUG_ERROR, "Cannot bind socket (errno=%d(%s))\n", errno, strerror(errno));
				return -EIO;
			}
			continue;
		}
		*sport = htons(conf->rtp_port_next + 1);
		rc = bind(media->rtcp_socket, (struct sockaddr *)&sa, slen);
		if (rc < 0)
			goto bind_error;
		media->description.port_local = conf->rtp_port_next;
		conf->rtp_port_next = (conf->rtp_port_next + 2 > conf->rtp_port_to) ? conf->rtp_port_from : conf->rtp_port_next + 2;
		/* set nonblocking io */
		flags = fcntl(media->rtp_socket, F_GETFL);
		flags |= O_NONBLOCK;
		fcntl(media->rtp_socket, F_SETFL, flags);
		flags = fcntl(media->rtcp_socket, F_GETFL);
		flags |= O_NONBLOCK;
		fcntl(media->rtcp_socket, F_SETFL, flags);
		break;
	}

	PDEBUG(DCC, DEBUG_DEBUG, "Opening media port %d\n", media->description.port_local);

	return 0;
}

/* connect RTP
 * use remote port to connect to
 */
int osmo_cc_rtp_connect(osmo_cc_session_media_t *media)
{
	struct sockaddr_storage sa;
	int slen = 0; // make GCC happy
	struct sockaddr_in6 *sa6;
	struct sockaddr_in *sa4;
	uint16_t *sport;
	int rc;

	PDEBUG(DCC, DEBUG_DEBUG, "Connecting media port %d->%d\n", media->description.port_local, media->description.port_remote);

	switch (media->connection_data_remote.addrtype) {
	case osmo_cc_session_addrtype_ipv4:
		memset(&sa, 0, sizeof(sa));
		sa4 = (struct sockaddr_in *)&sa;
		sa4->sin_family = AF_INET;
		rc = inet_pton(AF_INET, media->connection_data_remote.address, &sa4->sin_addr);
		if (rc < 1) {
pton_error:
			PDEBUG(DCC, DEBUG_NOTICE, "Cannot connect to address '%s'.\n", media->connection_data_remote.address);
			return -EINVAL;
		}
		sport = &sa4->sin_port;
		slen = sizeof(*sa4);
		break;
	case osmo_cc_session_addrtype_ipv6:
		memset(&sa, 0, sizeof(sa));
		sa6 = (struct sockaddr_in6 *)&sa;
		sa6->sin6_family = AF_INET6;
		rc = inet_pton(AF_INET6, media->connection_data_remote.address, &sa6->sin6_addr);
		if (rc < 1)
			goto pton_error;
		sport = &sa6->sin6_port;
		slen = sizeof(*sa6);
		break;
	case osmo_cc_session_addrtype_unknown:
		PDEBUG(DCC, DEBUG_NOTICE, "Unsupported address type '%s'.\n", media->connection_data_local.addrtype_name);
		return -EINVAL;
	}

	*sport = htons(media->description.port_remote);
	rc = connect(media->rtp_socket, (struct sockaddr *)&sa, slen);
	if (rc < 0) {
connect_error:
		PDEBUG(DCC, DEBUG_NOTICE, "Cannot connect to address '%s'.\n", media->connection_data_remote.address);
		osmo_cc_rtp_close(media);
		return -EIO;
	}
	*sport = htons(media->description.port_remote + 1);
	rc = connect(media->rtcp_socket, (struct sockaddr *)&sa, slen);
	if (rc < 0)
		goto connect_error;

	return 0;
}

/* send rtp data with given codec */
void osmo_cc_rtp_send(osmo_cc_session_codec_t *codec, uint8_t *data, int len, uint8_t marker, int inc_sequence, int inc_timestamp, void *priv)
{
	uint8_t *payload = NULL;
	int payload_len = 0;

	if (!codec || !codec->media->rtp_socket)
		return;

	if (codec->encoder)
		codec->encoder(data, len, &payload, &payload_len, priv);
	else {
		payload = data;
		payload_len = len;
	}

	rtp_send(codec->media->rtp_socket, payload, payload_len, marker, codec->payload_type_remote, codec->media->tx_sequence, codec->media->tx_timestamp, codec->media->tx_ssrc);
	codec->media->tx_sequence += inc_sequence;
	codec->media->tx_timestamp += inc_timestamp;

	if (codec->encoder)
		free(payload);
}

/* receive rtp data for given media, return < 0, if there is nothing this time */
int osmo_cc_rtp_receive(osmo_cc_session_media_t *media, void *priv)
{
	int rc;
	uint8_t *payload = NULL;
	int payload_len = 0;
	uint8_t marker;
	uint8_t payload_type;
	osmo_cc_session_codec_t *codec;
	uint8_t *data;
	int len;

	if (!media || media->rtp_socket <= 0)
		return -EIO;

	rc = rtp_receive(media->rtp_socket, &payload, &payload_len, &marker, &payload_type, &media->rx_sequence, &media->rx_timestamp, &media->rx_ssrc);
	if (rc < 0)
		return rc;

	/* search for codec */
	for (codec = media->codec_list; codec; codec = codec->next) {
		
		if (codec->payload_type_local == payload_type)
			break;
	}
	if (!codec) {
		PDEBUG(DCC, DEBUG_NOTICE, "Received RTP frame for unknown codec (payload_type = %d).\n", payload_type);
		return 0;
	}

	if (codec->decoder)
		codec->decoder(payload, payload_len, &data, &len, priv);
	else {
		data = payload;
		len = payload_len;
	}

	if (codec->media->receive)
		codec->media->receiver(codec, marker, media->rx_sequence, media->rx_timestamp, media->rx_ssrc, data, len);

	if (codec->decoder)
		free(data);

	return 0;
}

void osmo_cc_rtp_close(osmo_cc_session_media_t *media)
{
	if (media->rtp_socket) {
		close(media->rtp_socket);
		media->rtp_socket = 0;
	}
	if (media->rtcp_socket) {
		close(media->rtcp_socket);
		media->rtcp_socket = 0;
	}
}

