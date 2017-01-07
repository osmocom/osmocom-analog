
void dsp_init(void);
int dsp_init_sender(nmt_t *nmt);
void dsp_cleanup_sender(nmt_t *nmt);
int fsk_render_frame(nmt_t *nmt, const char *frame, int length, int16_t *sample);
void nmt_set_dsp_mode(nmt_t *nmt, enum dsp_mode mode);
void super_reset(nmt_t *nmt);

