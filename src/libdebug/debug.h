
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
#define DJOLLY		11
#define DEURO		12
#define DFRAME		13
#define DCALL		14
#define DMNCC		15
#define DDB		16
#define DTRANS		17
#define DDMS		18
#define DSMS		19
#define DSDR		20
#define DUHD		21
#define DSOAPY		22
#define DWAVE		23
#define DRADIO		24
#define DAM791X		25
#define DUART		26
#define DDEVICE		27
#define DDATENKLO	28
#define DZEIT		29

void get_win_size(int *w, int *h);

#define PDEBUG(cat, level, fmt, arg...) _printdebug(__FILE__, __FUNCTION__, __LINE__, cat, level, NULL, fmt, ## arg)
#define PDEBUG_CHAN(cat, level, fmt, arg...) _printdebug(__FILE__, __FUNCTION__, __LINE__, cat, level, CHAN, fmt, ## arg)
void _printdebug(const char *file, const char *function, int line, int cat, int level, const char *chan_str, const char *fmt, ...) __attribute__ ((__format__ (__printf__, 7, 8)));

const char *debug_amplitude(double level);
const char *debug_db(double level_db);

void debug_list_cat(void);
int parse_debug_opt(const char *opt);

extern int debuglevel;

extern void (*clear_console_text)(void);
extern void (*print_console_text)(void);

extern int debug_limit_scroll;

