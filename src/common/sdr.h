
int sdr_init(int sdr_uhd, int sdr_soapy, int channel, const char *device_args, const char *stream_args, const char *tune_args, const char *tx_antenna, const char *rx_antenna, double tx_gain, double rx_gain, int samplerate, double bandwidth, const char *write_iq_tx_wave, const char *write_iq_rx_wave, const char *read_iq_tx_wave, const char *read_iq_rx_wave, int latspl, int swap_links, int uhd_tx_timestamps);
int sdr_start(void *inst);
void *sdr_open(const char *audiodev, double *tx_frequency, double *rx_frequency, int channels, double paging_frequency, int samplerate, double bandwidth, double sample_deviation);
void sdr_close(void *inst);
int sdr_write(void *inst, sample_t **samples, int num, enum paging_signal *paging_signal, int *on, int channels);
int sdr_read(void *inst, sample_t **samples, int num, int channels);
int sdr_get_tosend(void *inst, int latspl);

