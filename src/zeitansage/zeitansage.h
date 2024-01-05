#include "../libfm/fm.h"
#include "../libmobile/sender.h"
#include <osmocom/core/timer.h>

/* current state of incoming call */
enum zeit_call_state {
	ZEIT_CALL_NULL = 0,
	ZEIT_CALL_BEEP,		/* play beep at the beginnung of each 10 seconds period */
	ZEIT_CALL_INTRO,	/* play intro sample */
	ZEIT_CALL_HOUR,		/* play hour sample */
	ZEIT_CALL_MINUTE,	/* play minute sample */
	ZEIT_CALL_SECOND,	/* play second sample */
	ZEIT_CALL_PAUSE,	/* pause until next 10 seconds period */
};

/* instance of incoming call */
typedef struct zeit_call {
	struct zeit_call	*next;
	int			callref;		/* call reference */
	struct osmo_timer_list		timer;
	enum zeit_call_state	state;			/* current state */
	char			caller_id[32];		/* caller id to be displayed */
	int			spl_time;		/* sample offset within 10 seconds */
	int			h, m, s;		/* what hour, minute, second to play */
} zeit_call_t;

int zeit_init(double audio_level_dBm, int alerting);
void zeit_exit(void);

