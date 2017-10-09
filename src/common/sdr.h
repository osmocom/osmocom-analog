
int sdr_start(void *inst);
void *sdr_open(const char *audiodev, double *tx_frequency, double *rx_frequency, int channels, double paging_frequency, int samplerate, int latspl, double bandwidth, double sample_deviation);
void sdr_close(void *inst);
int sdr_write(void *inst, sample_t **samples, uint8_t **power, int num, enum paging_signal *paging_signal, int *on, int channels);
int sdr_read(void *inst, sample_t **samples, int num, int channels, double *rf_level_db);
int sdr_get_tosend(void *inst, int latspl);

