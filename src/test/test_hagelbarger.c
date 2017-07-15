#include "stdio.h"
#include "stdint.h"
#include "string.h"
#include "../nmt/hagelbarger.h"

int main(void)
{
	uint8_t message[9] = "JollyRog", code[20];

	printf("Message: %s\n", message);

	/* clean tail at code bit 72 and above */
	memset(code, 0, sizeof(code));

	/* encode message */
	hagelbarger_encode(message, code, 72);

	/* corrupt data */
	code[0] ^= 0xfc;
	code[3] ^= 0xfc;
	code[7] ^= 0xfc;

	/* decode */
	hagelbarger_decode(code, message, 64);
	printf("Decoded: %s (must be the same as above)\n", message);

	return 0;
}

