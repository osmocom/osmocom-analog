
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
#define DFRAME		14
#define DCALL		15
#define DCC		16
#define DDB		17
#define DTRANS		18
#define DDMS		19
#define DSMS		20
#define DSDR		21
#define DUHD		22
#define DSOAPY		23
#define DWAVE		24
#define DRADIO		25
#define DAM791X		26
#define DUART		27
#define DDEVICE		28
#define DDATENKLO	29
#define DZEIT		30
#define DSIM1		31
#define DSIM2		32
#define DSIMI		33
#define DSIM7		34
#define DMTP2		35
#define DMTP3		36
#define DMUP		37
#define DROUTER		38
#define DSTDERR		39
#define DSS5		40
#define DISDN		41
#define DMISDN		42
#define DDSS1		43
#define DSIP		44

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

const char *debug_hex(const uint8_t *data, int len);

