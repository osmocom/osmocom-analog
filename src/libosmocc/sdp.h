
char *osmo_cc_session_gensdp(struct osmo_cc_session *session);
struct osmo_cc_session *osmo_cc_session_parsesdp(void *priv, const char *_sdp);
int osmo_cc_payload_type_by_attrs(uint8_t *fmt, const char *name, uint32_t *rate, int *channels);
void osmo_cc_debug_sdp(const char *sdp);
