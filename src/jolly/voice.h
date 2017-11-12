
typedef struct jolly_voice {
	sample_t	*spl[13];
	int		size[13];
} jolly_voice_t;

extern jolly_voice_t jolly_voice;

int init_voice(int samplerate);

