#include "../libmobile/sender.h"
#include "../libv27/modem.h"
#include "../libmtp/mtp.h"
#include "mup.h"

enum fuvst_chan_type {
	CHAN_TYPE_ZZK,		/* SS7 signalling channel */
	CHAN_TYPE_SPK,		/* pure traffic channel */
};

/* instance of fuvst sender */
typedef struct fuvst {
	sender_t		sender;
	v27modem_t		modem;
	mtp_t			mtp;

	int			chan_num; /* number of SPK or ZZK */
	enum fuvst_chan_type	chan_type; /* ZZK or SPK */
	int			callref;
	int			link;	/* MTP l2 link up */
	struct SysMeld		SM; /* collects alarm messages */
} fuvst_t;

const char *cnetz_number_valid(const char *number);
int fuvst_create(const char *kanal, enum fuvst_chan_type chan_type, const char *audiodev, int samplerate, double rx_gain, double tx_gain, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, const char *read_tx_wave, int loopback, int ignore_link_failure, uint8_t sio, uint16_t local_pc, uint16_t remove_pc);
void fuvst_destroy(sender_t *sender);
void add_emergency(const char *number);
void config_init(void);
int config_file(const char *filename);

