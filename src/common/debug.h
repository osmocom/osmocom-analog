
#define DEBUG_DEBUG	0 /* debug info, not for normal use */
#define DEBUG_INFO	1 /* all info about process */
#define DEBUG_NOTICE	2 /* something unexpected happens */
#define DEBUG_ERROR	3 /* there is an error with this software */

#define DSENDER		0
#define DSOUND		1
#define DDSP		2
#define DANETZ		3
#define DBNETZ		4
#define DCNETZ		5
#define DNMT		6
#define DAMPS		7
#define DR2000		8
#define DFRAME		9
#define DCALL		10
#define DMNCC		11
#define DDB		12
#define DTRANS		13
#define DDMS		14
#define DSMS		15
#define DSDR		16
#define DUHD		17
#define DSOAPY		18

#define PDEBUG(cat, level, fmt, arg...) _printdebug(__FILE__, __FUNCTION__, __LINE__, cat, level, -1, fmt, ## arg)
#define PDEBUG_CHAN(cat, level, fmt, arg...) _printdebug(__FILE__, __FUNCTION__, __LINE__, cat, level, CHAN, fmt, ## arg)
void _printdebug(const char *file, const char *function, int line, int cat, int level, int chan, const char *fmt, ...) __attribute__ ((__format__ (__printf__, 7, 8)));

const char *debug_amplitude(double level);
const char *debug_db(double level_db);

void debug_list_cat(void);
int parse_debug_opt(const char *opt);

extern int debuglevel;

