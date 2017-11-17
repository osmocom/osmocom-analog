
int soapy_open(size_t channel, const char *_device_args, const char *_stream_args, const char *_tune_args, const char *tx_antenna, const char *rx_antenna, double tx_frequency, double rx_frequency, double rate, double tx_gain, double rx_gain, double bandwidth);
int soapy_start(void);
void soapy_close(void);
int soapy_send(float *buff, int num);
int soapy_receive(float *buff, int max);
int soapy_get_tosend(int latspl);

