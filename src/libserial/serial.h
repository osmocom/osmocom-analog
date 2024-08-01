
#include <termios.h>

extern int	serial_errno;
extern char	*serial_errnostr;

typedef struct _serial {
	/* parameters */
	const char *device;
	int baud;
	int databits;
	char parity;
	int stopbits;
	char xonxoff;
	char rtscts;
	float txtimeout;
	float rxtimeout;

	/* internal variables */
	int handle;
	struct termios com_termios;
	struct termios old_termios;
} serial_t;

serial_t *serial_open(const char *serial_device, int serial_baud, int serial_databits, char serial_parity, int serial_stopbits, char serial_xonxoff, char serial_rtscts, int serial_getbreak, float serial_txtimeout, float serial_rxtimeout);
void serial_close(serial_t *serial);
int serial_read(serial_t *serial, uint8_t *buffer, size_t size);
int serial_write(serial_t *serial, const uint8_t *buffer, size_t size);
int serial_timeout(serial_t *serial, double serial_txtimeout, double serial_rxtimeout);
int serial_cts(serial_t *serial);
int serial_dsr(serial_t *serial);
int serial_dtron(serial_t *serial);
int serial_dtroff(serial_t *serial);
int serial_rtson(serial_t *serial);
int serial_rtsoff(serial_t *serial);
int serial_break(serial_t *serial, int on);
int serial_handle(serial_t *serial);

