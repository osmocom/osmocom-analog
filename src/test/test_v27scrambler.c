#include "stdio.h"
#include "stdint.h"
#include "string.h"
#include "../libv27/scrambler.h"

static int show_bin(uint8_t *data1, uint8_t *data2, int len)
{
	int i, j, error = 0;;
	uint8_t bit1, bit2;

	for (i = 0; i < len; i++) {
		printf(".");
		for (j = 0; j < 8; j++) {
			bit1 = (data1[i] >> j) & 1;
			bit2 = (data2[i] >> j) & 1;
			if (bit1 == bit2)
				printf("%d", bit1);
			else {
				printf("X");
				error++;
			}
		}
	}

	printf("\n");

	return error;
}

static int check_repetition(uint8_t *data, int len, int repeat, int start)
{
	int i;
	uint8_t b1, b2;

	for (i = start; i < (len * 8 - repeat); i++) {
		b1 = (data[i >> 3] >> (i & 7)) & 1;
		b2 = (data[(i+repeat) >> 3] >> ((i+repeat) & 7)) & 1;
		if (b1 != b2)
			return i - start + repeat;
	}

	return 0;
}

int main(void)
{
	v27scrambler_t scram, descram;

	char message[] = "Jolly Roger~~~~";
	int len = strlen(message);
	uint8_t data[len];
	int ret;

	printf("Message: %s\n", message);

	memcpy(data, message, len);
	show_bin(data, (uint8_t *)message, len);

	v27_scrambler_init(&scram, 1, 0);
	v27_scrambler_block(&scram, data, len);

	printf("Scrambled:\n");
	show_bin(data, data, len);

	v27_scrambler_init(&descram, 1, 1);
	v27_scrambler_block(&descram, data, len);
	
	printf("Descramble without corruption?\n");

	ret = show_bin(data, (uint8_t *)message, len);
	if (ret) {
		printf("Descrambling failed!\n");
		return 1;
	}
	printf("Yes!\n");

	printf("\n");

	v27_scrambler_init(&scram, 1, 0);
	v27_scrambler_block(&scram, data, len);

	data[0] = 'B';
	data[1] = 'U';
	data[2] = 'G';

	v27_scrambler_init(&descram, 1, 1);
	v27_scrambler_block(&descram, data, len);
	
	printf("Descramble with 3 bytes corruption: (should fix itself after 4 bytes)\n");

	show_bin(data, (uint8_t *)message, len);

	printf("\n");

	printf("Descramble a scrambled sequence of 8 bit repetitions with V.27: 01111110\n");

	memset(data, 0x7e, len);

	v27_scrambler_init(&descram, 0, 1);
	v27_scrambler_block(&descram, data, len);

	show_bin(data, (uint8_t *)data, len);

	/* note at position 6 we have no more change towards 8 bit offset */
	ret = check_repetition(data, len, 8, 6);
	if (ret) {
		printf("There's is a change of repetition after %d bits after start %d, please fix!\n", ret, 6);
		return 1;
	}
	printf("Repetition not detected, good!\n");
	
	printf("\n");

	printf("Descramble a scrambled sequence of 8 bit repetitions with V.27bis/ter: 01111110\n");

	memset(data, 0x7e, len);

	v27_scrambler_init(&descram, 1, 1);
	v27_scrambler_block(&descram, data, len);

	show_bin(data, (uint8_t *)data, len);

	/* note at position 6 we have no more change towards 8 bit offset */
	ret = check_repetition(data, len, 8, 6);
	if (ret != 34) {
		printf("There's is NO change of repetition after 34 bits, but after %d bits, which should not happen!\n", ret);
		return 1;
	}
	printf("Repetition detected after %d bits from start %d, good!\n", ret, 6);
	
	return 0;
}

