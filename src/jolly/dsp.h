
void dsp_init(void);
int dsp_init_sender(jolly_t *jolly, int nbfm, double squelch_db, int repeater);
void dsp_cleanup_sender(jolly_t *jolly);
void set_speech_string(jolly_t *jolly, char anouncement, const char *number);
void reset_speech_string(jolly_t *jolly);

