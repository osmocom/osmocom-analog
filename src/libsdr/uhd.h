
int uhd_open(size_t channel, const char *_device_args, const char *_stream_args, const char *_tune_args, const char *tx_antenna, const char *rx_antenna, const char *clock_source, double tx_frequency, double rx_frequency, double lo_offset, double rate, double tx_gain, double rx_gain, double bandwidth, int timestamps);
int uhd_start(void);
void uhd_close(void);
int uhd_send(float *buff, int num);
int uhd_receive(float *buff, int max);
int uhd_get_tosend(int buffer_size);

