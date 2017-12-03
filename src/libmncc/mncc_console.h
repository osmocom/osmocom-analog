
int console_init(const char *station_id, const char *audiodev, int samplerate, int latency, int dial_digits, int loopback, int echo_test);
void console_cleanup(void);
int console_open_audio(int latspl);
int console_start_audio(void);
void console_process(int c);
void process_console(int c);

