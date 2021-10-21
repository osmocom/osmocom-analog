
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
#define DFRAME		15
#define DCALL		16
#define DCC		17
#define DDB		18
#define DTRANS		19
#define DDMS		20
#define DSMS		21
#define DSDR		22
#define DUHD		23
#define DSOAPY		24
#define DWAVE		25
#define DRADIO		26
#define DAM791X		27
#define DUART		28
#define DDEVICE		29
#define DDATENKLO	30
#define DZEIT		31
#define DSIM1		32
#define DSIM2		33
#define DSIMI		34
#define DSIM7		35
#define DMTP2		36
#define DMTP3		37
#define DMUP		38
#define DROUTER		39
#define DSTDERR		40
#define DSS5		41
#define DISDN		42
#define DMISDN		43
#define DDSS1		44
#define DSIP		45
#define DTEL		46

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

