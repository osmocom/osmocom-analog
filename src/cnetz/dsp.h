
void dsp_init(void);
int dsp_init_sender(cnetz_t *cnetz, int measure_speed, double clock_speed[2], double noise);
void dsp_cleanup_sender(cnetz_t *cnetz);
void calc_clock_speed(cnetz_t *cnetz, uint64_t samples, int tx, int result);
void unshrink_speech(cnetz_t *cnetz, int16_t *speech_buffer, int count);

