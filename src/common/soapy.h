
int soapy_open(const char *device_args, double tx_frequency, double rx_frequency, double rate, double rx_gain, double tx_gain, double bandwidth);
int soapy_start(void);
void soapy_close(void);
int soapy_send(float *buff, int num);
int soapy_receive(float *buff, int max);
int soapy_get_tosend(int latspl);

