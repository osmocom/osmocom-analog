
void dsp_init(void);
int dsp_init_sender(anetz_t *anetz);
void dsp_cleanup_sender(anetz_t *anetz);
void dsp_set_paging(anetz_t *anetz, double *freq);
void anetz_set_dsp_mode(anetz_t *anetz, enum dsp_mode mode);

