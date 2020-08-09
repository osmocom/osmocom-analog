
void console_msg(osmo_cc_call_t *call, osmo_cc_msg_t *msg);
int console_init(const char *station_id, const char *audiodev, int samplerate, int latency, int dial_digits, int loopback, int echo_test, const char *digits);
void console_cleanup(void);
int console_open_audio(int latspl);
int console_start_audio(void);
void console_process(int c);
void process_console(int c);

