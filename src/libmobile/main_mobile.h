
extern int num_kanal;
extern const char *kanal[];
extern int swap_links;
extern int num_device;
extern int allow_sdr;
extern int use_sdr;
extern const char *dsp_device[];
extern int dsp_samplerate;
extern double dsp_interval;
extern int dsp_buffer;
extern int uses_emphasis;
extern int do_pre_emphasis;
extern int do_de_emphasis;
extern double rx_gain;
extern double tx_gain;
extern int send_patterns;
extern int loopback;
extern int rt_prio;
extern int fast_math;
extern const char *write_rx_wave;
extern const char *write_tx_wave;
extern const char *read_rx_wave;
extern const char *read_tx_wave;

struct number_lengths {
	int digits;
	const char *usage;
};

void print_help(const char *);

const char *mobile_number_remove_prefix(const char *number);
const char *mobile_number_check_length(const char *number);
const char *mobile_number_check_digits(const char *number);
extern const char *(*mobile_number_check_valid)(const char *);
int main_mobile_number_ask(const char *number, const char *what);

void main_mobile_init(const char *digits, const struct number_lengths lengths[], const char *prefixes[], const char *(*check_valid)(const char *), const char *toneset);
void main_mobile_exit(void);
void main_mobile_set_number_check_valid(const char *(*check_valid)(const char *));
void main_mobile_print_help(const char *arg0, const char *ext_usage);
void main_mobile_print_hotkeys(void);
void main_mobile_print_station_id(void);
void main_mobile_add_options(void);
int main_mobile_handle_options(int short_option, int argi, char **argv);

#define OPT_ARRAY(num_name, name, value) \
{ \
	if (num_name == MAX_SENDER) { \
		fprintf(stderr, "Too many channels defined!\n"); \
		exit(0); \
	} \
	name[num_name++] = value; \
}

extern int quit;
void sighandler(int sigset);

void main_mobile_loop(const char *name, int *quit, void (*myhandler)(void), const char *station_id);

