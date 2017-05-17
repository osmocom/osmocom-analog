
void dsp_init(void);
int dsp_init_sender(cnetz_t *cnetz, int measure_speed, double clock_speed[2], int use_sdr);
void dsp_cleanup_sender(cnetz_t *cnetz);
void calc_clock_speed(cnetz_t *cnetz, double samples, int tx, int result);
void unshrink_speech(cnetz_t *cnetz, sample_t *speech_buffer, int count);
void cnetz_set_dsp_mode(cnetz_t *cnetz, enum dsp_mode mode);
void cnetz_set_sched_dsp_mode(cnetz_t *cnetz, enum dsp_mode mode, int frames_ahead);

