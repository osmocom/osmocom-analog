
typedef struct squelch {
	const char *kanal;	/* channel number */
	double	threshold_db;	/* threshold level to mute or loss of signal */
	double	init_count;	/* duration counter for starting squelch process */
	int	auto_state;	/* set if auto threshold calibration is performed */
	double	auto_count;	/* duration counter for calibration process */
	double	auto_level_sum;	/* sum of rf level while calibrating */
	int	auto_level_count; /* counter for rf levels that are summed */
	double	mute_time;	/* time to indicate mute after being below threshold */
	int	mute_state;	/* set, if we are currently at mute condition */
	double	mute_count;	/* duration counter for mute condition */
	double	loss_time;	/* time to indicate loss after being below threshold */
	int	loss_state;	/* set, if we are currently at 'signal loss' condition */
	double 	loss_count;	/* duration counter for 'signal loss' condition */
} squelch_t;

enum squelch_result {
	SQUELCH_OPEN,
	SQUELCH_MUTE,
	SQUELCH_LOSS,
};

void squelch_init(squelch_t *squelch, const char *kanal, double threshold_db, double mute_time, double loss_time);
enum squelch_result squelch(squelch_t *squelch, double rf_level_db, double duration);

