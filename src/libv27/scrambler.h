
typedef struct v27scrambler {
	int		descramble;	/* set if we descramble */

	uint16_t	shift;		/* shift register to hold 13 bits */
	int		counter;	/* counter to guard against repetitions */
	uint16_t	resetmask;	/* bit mask for repition check */
} v27scrambler_t;

void v27_scrambler_init(v27scrambler_t *scram, int bis, int descramble);
uint8_t v27_scrambler_bit(v27scrambler_t *scram, uint8_t in);
void v27_scrambler_block(v27scrambler_t *scram, uint8_t *data, int len);

