/* configuration */

enum osmo_cc_session_nettype {
	osmo_cc_session_nettype_unknown = 0,
	osmo_cc_session_nettype_inet,
};

enum osmo_cc_session_addrtype {
	osmo_cc_session_addrtype_unknown = 0,
	osmo_cc_session_addrtype_ipv4,
	osmo_cc_session_addrtype_ipv6,
};

typedef struct osmo_cc_session_config {
	enum osmo_cc_session_nettype default_nettype;
	enum osmo_cc_session_addrtype default_addrtype;
	const char *default_unicast_address;
	uint16_t rtp_port_next;
	uint16_t rtp_port_from;
	uint16_t rtp_port_to;
} osmo_cc_session_config_t;

/* session description, global part: */

typedef struct osmo_cc_session_origin {
	const char *username;
	const char *sess_id;
	const char *sess_version;
	const char *nettype;
	const char *addrtype;
	const char *unicast_address;
} osmo_cc_session_origin_t;

/* session instance */
typedef struct osmo_cc_session {
	osmo_cc_session_config_t *config;
	void *priv;
	osmo_cc_session_origin_t origin_local, origin_remote;
	const char *name;
	struct osmo_cc_session_media *media_list;
} osmo_cc_session_t;

/* connection description: */

typedef struct osmo_cc_session_connection_data {
	enum osmo_cc_session_nettype nettype;
	const char *nettype_name;
	enum osmo_cc_session_addrtype addrtype;
	const char *addrtype_name;
	const char *address;
} osmo_cc_session_connection_data_t;

/* one media of session description: */

enum osmo_cc_session_media_type {
	osmo_cc_session_media_type_unknown,
	osmo_cc_session_media_type_audio,
	osmo_cc_session_media_type_video,
};

enum osmo_cc_session_media_proto {
	osmo_cc_session_media_proto_unknown,
	osmo_cc_session_media_proto_rtp,
};

typedef struct osmo_cc_session_media_description {
	enum osmo_cc_session_media_type type;
	const char *type_name;
	uint16_t port_local, port_remote;
	enum osmo_cc_session_media_proto proto;
	const char *proto_name;
} osmo_cc_session_media_description_t;

/* media entry */
typedef struct osmo_cc_session_media {
	struct osmo_cc_session_media *next;
	osmo_cc_session_t *session;
	osmo_cc_session_media_description_t description;
	osmo_cc_session_connection_data_t connection_data_local, connection_data_remote;
	struct osmo_cc_session_codec *codec_list;
	int send, receive;
	void (*receiver)(struct osmo_cc_session_codec *codec, uint8_t marker, uint16_t sequence_number, uint32_t timestamp, uint32_t ssrc, uint8_t *data, int len);
	struct osmo_fd rtp_ofd;
	struct osmo_fd rtcp_ofd;
	uint32_t tx_ssrc, rx_ssrc;
	uint16_t tx_sequence, rx_sequence;
	uint32_t tx_timestamp, rx_timestamp;
	int accepted;
} osmo_cc_session_media_t;

/* codec entry */
typedef struct osmo_cc_session_codec {
	struct osmo_cc_session_codec *next;
	osmo_cc_session_media_t *media;
	uint8_t payload_type_local, payload_type_remote; /* local = towards local, remote = toward remote */
	const char *payload_name;
	uint32_t payload_rate;
	int payload_channels;
	void (*encoder)(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void *priv);
	void (*decoder)(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void *priv);
	int accepted;
} osmo_cc_session_codec_t;

#define osmo_cc_session_for_each_media(head, m) \
	for (m = (head); m; m = m->next)

#define osmo_cc_session_for_each_codec(head, c) \
	for (c = (head); c; c = c->next)

void osmo_cc_set_local_peer(osmo_cc_session_config_t *conf, enum osmo_cc_session_nettype nettype, enum osmo_cc_session_addrtype addrtype, const char *address);
osmo_cc_session_t *osmo_cc_new_session(osmo_cc_session_config_t *conf, void *priv, const char *username, const char *sess_id, const char *sess_version, enum osmo_cc_session_nettype nettype, enum osmo_cc_session_addrtype addrtype, const char *unicast_address, const char *session_name, int debug);
void osmo_cc_free_session(osmo_cc_session_t *session);
osmo_cc_session_media_t *osmo_cc_add_media(osmo_cc_session_t *session, enum osmo_cc_session_nettype nettype, enum osmo_cc_session_addrtype addrtype, const char *address, enum osmo_cc_session_media_type type, uint16_t port, enum osmo_cc_session_media_proto proto, int send, int receive, void (*receiver)(struct osmo_cc_session_codec *codec, uint8_t marker, uint16_t sequence_number, uint32_t timestamp, uint32_t ssrc, uint8_t *data, int len), int debug);
void osmo_cc_free_media(osmo_cc_session_media_t *media);
osmo_cc_session_codec_t *osmo_cc_add_codec(osmo_cc_session_media_t *media, const char *playload_name, uint32_t playload_rate, int playload_channels, void (*encoder)(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void *priv), void (*decoder)(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void *priv), int debug);
void osmo_cc_free_codec(osmo_cc_session_codec_t *codec);
int osmo_cc_session_check(struct osmo_cc_session *session, int remote);
const char *osmo_cc_session_send_offer(osmo_cc_session_t *session);
osmo_cc_session_t *osmo_cc_session_receive_offer(osmo_cc_session_config_t *conf, void *priv, const char *sdp);
void osmo_cc_session_accept_media(osmo_cc_session_media_t *media, enum osmo_cc_session_nettype nettype, enum osmo_cc_session_addrtype addrtype, const char *address, int send, int receive, void (*receiver)(struct osmo_cc_session_codec *codec, uint8_t marker, uint16_t sequence_number, uint32_t timestamp, uint32_t ssrc, uint8_t *data, int len));
void osmo_cc_session_accept_codec(osmo_cc_session_codec_t *codec, void (*encoder)(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void *priv), void (*decoder)(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void *priv));
const char *osmo_cc_session_send_answer(osmo_cc_session_t *session);
int osmo_cc_session_receive_answer(osmo_cc_session_t *session, const char *sdp);
const char *osmo_cc_session_nettype2string(enum osmo_cc_session_nettype nettype);
const char *osmo_cc_session_addrtype2string(enum osmo_cc_session_addrtype addrtype);
const char *osmo_cc_session_media_type2string(enum osmo_cc_session_media_type media_type);
const char *osmo_cc_session_media_proto2string(enum osmo_cc_session_media_proto media_proto);
int osmo_cc_session_if_codec(osmo_cc_session_codec_t *codec, const char *name, uint32_t rate, int channels);

