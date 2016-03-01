
typedef struct loss {
	int			interval;		/* levels in one interval */
	int			interval_num;		/* number of similar intervals until loss */
	double			threshold;		/* how much volume change is accedped during loss */
	double			level_last;		/* received level of last block */
	double			level;			/* received level of current block */
	int			level_count;		/* counter of levels inside interval */
	int			interval_count;		/* counter of cosecutive intervals with loss */
} loss_t;

void audio_init_loss(loss_t *loss, int interval, double threshold, int seconds);
void audio_reset_loss(loss_t *loss);
int audio_detect_loss(loss_t *loss, double level);

