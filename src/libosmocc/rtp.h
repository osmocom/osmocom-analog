
void osmo_cc_set_rtp_ports(osmo_cc_session_config_t *conf, uint16_t from, uint16_t to);
int osmo_cc_rtp_open(osmo_cc_session_media_t *media);
int osmo_cc_rtp_connect(osmo_cc_session_media_t *media);
void osmo_cc_rtp_send(osmo_cc_session_codec_t *codec, uint8_t *data, int len, uint8_t marker, int inc_sequence, int inc_timestamp);
int osmo_cc_rtp_receive(osmo_cc_session_media_t *media);
void osmo_cc_rtp_close(osmo_cc_session_media_t *media);

