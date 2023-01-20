
#define DEBUG_DEBUG	0 /* debug info, not for normal use */
#define DEBUG_INFO	1 /* all info about process */
#define DEBUG_NOTICE	2 /* something unexpected happens */
#define DEBUG_ERROR	3 /* there is an error with this software */

#define DOPTIONS	0
#define DSENDER		1
#define DSOUND		2
#define DDSP		3
#define DANETZ		4
#define DBNETZ		5
#define DCNETZ		6
#define DNMT		7
#define DAMPS		8
#define DR2000		9
#define DIMTS		10
#define DMPT1327	11
#define DJOLLY		12
#define DEURO		13
#define DPOCSAG		14
#define DGOLAY		15
#define DFUENF		16
#define DFRAME		17
#define DCALL		18
#define DCC		19
#define DDB		20
#define DTRANS		21
#define DDMS		22
#define DSMS		23
#define DSDR		24
#define DUHD		25
#define DSOAPY		26
#define DWAVE		27
#define DRADIO		28
#define DAM791X		29
#define DUART		30
#define DDEVICE		31
#define DDATENKLO	32
#define DZEIT		33
#define DSIM1		34
#define DSIM2		35
#define DSIMI		36
#define DSIM7		37
#define DMTP2		38
#define DMTP3		39
#define DMUP		40
#define DROUTER		41
#define DSTDERR		42
#define DSS5		43
#define DISDN		44
#define DMISDN		45
#define DDSS1		46
#define DSIP		47
#define DTEL		48
#define DUK0		49
#define DPH		50
#define DDCF77		51
#define DJITTER		52
//NOTE: increment mask array, if 127 is exceeded

void lock_debug(void);
void unlock_debug(void);

void get_win_size(int *w, int *h);

#define PDEBUG(cat, level, fmt, arg...) _printdebug(__FILE__, __FUNCTION__, __LINE__, cat, level, NULL, fmt, ## arg)
#define PDEBUG_CHAN(cat, level, fmt, arg...) _printdebug(__FILE__, __FUNCTION__, __LINE__, cat, level, CHAN, fmt, ## arg)
void _printdebug(const char *file, const char *function, int line, int cat, int level, const char *chan_str, const char *fmt, ...) __attribute__ ((__format__ (__printf__, 7, 8)));

const char *debug_amplitude(double level);
const char *debug_db(double level_db);

void debug_print_help(void);
void debug_list_cat(void);
int parse_debug_opt(const char *opt);

extern int debuglevel;

extern void (*clear_console_text)(void);
extern void (*print_console_text)(void);

extern int debug_limit_scroll;

const char *debug_hex(const uint8_t *data, int len);

#define LOGP		PDEBUG
#define LOGL_DEBUG	DEBUG_DEBUG
#define LOGL_INFO	DEBUG_INFO
#define LOGL_NOTICE	DEBUG_NOTICE
#define LOGL_ERROR	DEBUG_ERROR
#define osmo_hexdump	debug_hex

