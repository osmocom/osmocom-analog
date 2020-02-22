#include "psk.h"
#include "scrambler.h"

typedef struct v27modem {
	int		(*send_bit)(void *inst);
	void		(*receive_bit)(void *inst, int bit);
	void		*inst;

	v27scrambler_t	scrambler, descrambler;
	psk_mod_t	psk_mod;
	psk_demod_t	psk_demod;
} v27modem_t;

int v27_modem_init(v27modem_t *modem, void *inst, int (*send_bit)(void *inst), void (*receive_bit)(void *inst, int bit), int samplerate, int bis);
void v27_modem_exit(v27modem_t *modem);
void v27_modem_send(v27modem_t *modem, sample_t *sample, int length);
void v27_modem_receive(v27modem_t *modem, sample_t *sample, int length);

