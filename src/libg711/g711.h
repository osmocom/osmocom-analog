void g711_init(void);
void g711_encode_alaw_flipped(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void *priv);
void g711_encode_ulaw_flipped(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void *priv);
void g711_decode_alaw_flipped(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void *priv);
void g711_decode_ulaw_flipped(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void *priv);
void g711_encode_alaw(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void *priv);
void g711_encode_ulaw(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void *priv);
void g711_decode_alaw(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void *priv);
void g711_decode_ulaw(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void *priv);
void g711_transcode_alaw_to_ulaw(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void *priv);
void g711_transcode_alaw_flipped_to_ulaw(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void *priv);
void g711_transcode_alaw_to_ulaw_flipped(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void *priv);
void g711_transcode_ulaw_to_alaw(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void *priv);
void g711_transcode_ulaw_flipped_to_alaw(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void *priv);
void g711_transcode_ulaw_to_alaw_flipped(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void *priv);
void g711_transcode_flipped(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void *priv);

