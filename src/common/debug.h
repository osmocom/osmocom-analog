
#define DEBUG_DEBUG	0 /* debug info, not for normal use */
#define DEBUG_INFO	1 /* all info about process */
#define DEBUG_NOTICE	2 /* something unexpected happens */
#define DEBUG_ERROR	3 /* there is an error with this software */

#define DSENDER		0
#define DSOUND		1
#define DFSK		2
#define DAUDIO		3
#define DANETZ		4
#define DBNETZ		5
#define DCALL		6
#define DMNCC		7

#define PDEBUG(cat, level, fmt, arg...) _printdebug(__FILE__, __FUNCTION__, __LINE__, cat, level, fmt, ## arg)
void _printdebug(const char *file, const char *function, int line, int cat, int level, const char *fmt, ...);

extern int debuglevel;

