
void dsp_init(void);
int dsp_init_sender(mpt1327_t *mpt1327, double squelch_db);
void dsp_cleanup_sender(mpt1327_t *mpt1327);
void mpt1327_set_dsp_mode(mpt1327_t *mpt1327, enum dsp_mode mode, int repeater);
void mpt1327_reset_sync(mpt1327_t *mpt1327);;

