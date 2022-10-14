
struct osmo_cc_helper_audio_codecs {
	const char *payload_name;
	uint32_t payload_rate;
	int payload_channels;
	void (*encoder)(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len);
	void (*decoder)(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len);
};

osmo_cc_session_t *osmo_cc_helper_audio_offer(osmo_cc_session_config_t *conf, void *priv, struct osmo_cc_helper_audio_codecs *codecs, void (*receiver)(struct osmo_cc_session_codec *codec, uint8_t marker, uint16_t sequence_number, uint32_t timestamp, uint32_t ssrc, uint8_t *data, int len), osmo_cc_msg_t *msg, int debug);
const char *osmo_cc_helper_audio_accept(osmo_cc_session_config_t *conf, void *priv, struct osmo_cc_helper_audio_codecs *codecs, void (*receiver)(struct osmo_cc_session_codec *codec, uint8_t marker, uint16_t sequence_number, uint32_t timestamp, uint32_t ssrc, uint8_t *data, int len), osmo_cc_msg_t *msg, osmo_cc_session_t **session_p, osmo_cc_session_codec_t **codec_p, int force_our_codec);
int osmo_cc_helper_audio_negotiate(osmo_cc_msg_t *msg, osmo_cc_session_t **session_p, osmo_cc_session_codec_t **codec_p);

