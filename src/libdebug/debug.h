
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
#define DFUENF		15
#define DFRAME		16
#define DCALL		17
#define DCC		18
#define DDB		19
#define DTRANS		20
#define DDMS		21
#define DSMS		22
#define DSDR		23
#define DUHD		24
#define DSOAPY		25
#define DWAVE		26
#define DRADIO		27
#define DAM791X		28
#define DUART		29
#define DDEVICE		30
#define DDATENKLO	31
#define DZEIT		32
#define DSIM1		33
#define DSIM2		34
#define DSIMI		35
#define DSIM7		36
#define DMTP2		37
#define DMTP3		38
#define DMUP		39
#define DROUTER		40
#define DSTDERR		41
#define DSS5		42
#define DISDN		43
#define DMISDN		44
#define DDSS1		45
#define DSIP		46
#define DTEL		47

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

