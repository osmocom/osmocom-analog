
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
#define DFRAME		8
#define DCALL		9
#define DMNCC		10
#define DDB		11
#define DTRANS		12

#define PDEBUG(cat, level, fmt, arg...) _printdebug(__FILE__, __FUNCTION__, __LINE__, cat, level, fmt, ## arg)
void _printdebug(const char *file, const char *function, int line, int cat, int level, const char *fmt, ...);

const char *debug_amplitude(double level);

void debug_list_cat(void);
int parse_debug_opt(const char *opt);

extern int debuglevel;

