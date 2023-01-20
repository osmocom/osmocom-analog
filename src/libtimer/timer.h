
struct timer {
	struct timer *next;
	int linked; /* set is timer is initialized and linked */
	double duration;
	double timeout;
	void (*cb)(void *data);
	void *data;
};

double get_time(void);
void timer_init(struct timer *timer, void (*fn)(void *data), void *priv);
void timer_exit(struct timer *timer);
void timer_start(struct timer *timer, double duration);
void timer_stop(struct timer *timer);
int timer_running(struct timer *timer);
double process_timer(void);

#define osmo_timer_list timer
void osmo_timer_schedule(struct osmo_timer_list *ti, time_t sec, long usec);
void osmo_timer_del(struct osmo_timer_list *ti);
int osmo_timer_pending(struct osmo_timer_list *ti);

