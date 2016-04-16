
struct timer {
	struct timer *next;
	int linked; /* set is timer is initialized and linked */
	double duration;
	double timeout;
	void (*fn)(struct timer *timer);
	void *priv;
};

double get_time(void);
void timer_init(struct timer *timer, void (*fn)(struct timer *timer), void *priv);
void timer_exit(struct timer *timer);
void timer_start(struct timer *timer, double duration);
void timer_stop(struct timer *timer);
int timer_running(struct timer *timer);
void process_timer(void);

