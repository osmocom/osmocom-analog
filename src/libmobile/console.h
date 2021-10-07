
void console_msg(osmo_cc_call_t *call, osmo_cc_msg_t *msg);
int console_init(const char *audiodev, int samplerate, int buffer, int loopback, int echo_test, const char *digits, const struct number_lengths *lengths, const char *station_id);
void console_cleanup(void);
int console_open_audio(int buffer_size, double interval);
int console_start_audio(void);
void console_process(int c);
void process_console(int c);

