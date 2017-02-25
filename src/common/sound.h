
enum paging_signal;

void *sound_open(const char *audiodev, double *tx_frequency, double *rx_frequency, int channels, double paging_frequency, int samplerate, double bandwidth, double sample_deviation);
int sound_start(void *inst);
void sound_close(void *inst);
int sound_write(void *inst, sample_t **samples, int num, enum paging_signal *paging_signal, int *on, int channels);
int sound_read(void *inst, sample_t **samples, int num, int channels);
int sound_get_inbuffer(void *inst);

