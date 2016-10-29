
void dsp_init(void);
int dsp_init_sender(amps_t *amps, int high_pass, int tolerant);
void dsp_cleanup_sender(amps_t *amps);
void amps_set_dsp_mode(amps_t *amps, enum dsp_mode mode, int frame_length);

