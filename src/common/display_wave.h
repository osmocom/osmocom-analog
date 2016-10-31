typedef struct sender sender_t;

typedef struct display_wave {
	int	interval_pos;
	int	interval_max;
	int	offset;
	int16_t buffer[256];
} dispwav_t;

void display_wave_init(sender_t *sender, int samplerate);
void display_wave_on(int on);
void display_wave(sender_t *sender, int16_t *samples, int length);

