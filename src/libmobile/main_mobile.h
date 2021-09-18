
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
extern const char *console_digits;

void main_mobile_init(void);
void main_mobile_print_help(const char *arg0, const char *ext_usage);
void main_mobile_print_hotkeys(void);
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

void main_mobile(const char *name, int *quit, void (*myhandler)(void), const char *station_id, int station_id_digits);

void dump_info(void);

