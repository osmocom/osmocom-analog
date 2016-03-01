#define CAUSE_NORMAL		16
#define CAUSE_BUSY		17
#define CAUSE_NOANSWER		19
#define CAUSE_OUTOFORDER	27
#define CAUSE_INVALNUMBER	28
#define CAUSE_NOCHANNEL		34
#define CAUSE_TEMPFAIL		41
#define CAUSE_INVALCALLREF	81

const char *cause_name(int cause);


