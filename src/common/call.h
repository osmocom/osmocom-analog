
int call_init(const char *station_id, const char *sounddev, int samplerate, int latency, int loopback);
void call_cleanup(void);
int process_call(void);

/* received messages */
int call_in_setup(int callref, const char *callerid, const char *dialing);
void call_in_alerting(int callref);
void call_in_answer(int callref, const char *connecid);
void call_in_release(int callref, int cause);

/* send messages */
int call_out_setup(int callref, char *dialing);
void call_out_disconnect(int callref, int cause);
void call_out_release(int callref, int cause);

/* send and receive audio */
void call_rx_audio(int callref, int16_t *samples, int count);
void call_tx_audio(int callref, int16_t *samples, int count);

/* receive from mncc */
void call_mncc_recv(uint8_t *buf, int length);
void call_mncc_flush(void);
/* clock to transmit to */
void call_mncc_clock(void);

