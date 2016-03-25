
extern int kanal;
extern const char *sounddev;
extern const char *call_sounddev;
extern int samplerate;
extern int latency;
extern int use_mncc_sock;
extern int send_patterns;
extern int loopback;
extern int rt_prio;
extern const char *read_wave;
extern const char *write_wave;

void print_help(const char *arg0);
void print_help_common(const char *arg0, const char *ext_usage);
extern struct option *long_options;
extern char *optstring;
void set_options_common(const char *optstring_special, struct option *long_options_special);
void opt_switch_common(int c, char *arg0, int *skip_args);


extern int quit;
void sighandler(int sigset);
