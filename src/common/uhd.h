
int uhd_open(const char *device_args, double tx_frequency, double rx_frequency, double rate, double rx_gain, double tx_gain);
int uhd_start(void);
void uhd_close(void);
int uhd_send(float *buff, int num);
int uhd_receive(float *buff, int max);
int uhd_get_tosend(int latspl);

