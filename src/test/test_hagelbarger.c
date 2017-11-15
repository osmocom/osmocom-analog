#include "stdio.h"
#include "stdint.h"
#include "string.h"
#include "../libhagelbarger/hagelbarger.h"

int main(void)
{
	uint8_t message[9] = "JollyRog", code[20];

	printf("Message: %s\n", message);

	/* clean tail at code bit 70 and above */
	memset(code, 0, sizeof(code));

	/* encode message */
	hagelbarger_encode(message, code, 70);

	/* decode */
	hagelbarger_decode(code, message, 64);
	printf("Decoded without corruption: %s (must be the same as above)\n", message);

	/* corrupt data */
	code[0] ^= 0xfc;
	code[3] ^= 0xfc;
	code[7] ^= 0xfc;

	/* decode */
	hagelbarger_decode(code, message, 64);
	printf("Decoded with corruption: %s (must be the same as above)\n", message);

	return 0;
}

