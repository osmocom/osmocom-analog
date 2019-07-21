
void dsp_init(void);
int dsp_init_transceiver(imts_t *imts, double squelch_db, int ptt);
void dsp_cleanup_transceiver(imts_t *imts);
void imts_set_dsp_mode(imts_t *imts, enum dsp_mode mode, int tone, double duration, int reset_demod);

