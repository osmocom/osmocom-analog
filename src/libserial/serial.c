/* serial port access
 *
 * (C) 2001-2018 by Andreas Eversberg <jolly@eversberg.eu>
 * All Rights Reserved
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "serial.h"

/*
serial = serial_open("/dev/ttyS0",9600,8,n,1,'d','e',1,2);

the return value will be a "serial handle" (!NOT THE FILE HANDLE) for all routines. on failure, a NULL will be returned.

the parameters are as followed:

char *device;		tty name
int baud;		baud rate (bits/s)
int databits;		5-7
char parity;		n=no,e=even,o=odd
int stopbits;		1-2
char xonxoff;		e=disable,e=enable
char rtscts;		e=disable,e=enable
float txtimeout;	seconds timeout for transmit
float rxtimeout;	seconds timeout for receive

to access the real tty handle use:

serial->handle

NOTE: the device is in non-blocking mode.
*/
serial_t *serial_open(const char *serial_device, int serial_baud, int serial_databits, char serial_parity, int serial_stopbits, char serial_xonxoff, char serial_rtscts, int serial_getbreak, float serial_txtimeout, float serial_rxtimeout)
{
	int baud;
	int flags;
    	int handshake_lines;
	serial_t *serial;

	if (serial_databits < 5 || serial_databits > 8) {
		serial_errno = -EINVAL;
		serial_errnostr = "Only 5 through 8 stopbits supported.";
		return NULL;
	}
	if (serial_stopbits < 1 || serial_stopbits > 2) {
		serial_errno = -EINVAL;
		serial_errnostr = "Only 1 through 2 stopbits supported.";
		return NULL;
	}
	if (serial_xonxoff !='e' && serial_xonxoff != 'd') {
		serial_errno = -EINVAL;
		serial_errnostr = "Enable or disable Xon/Xoff?";
		return NULL;
	}
	if (serial_rtscts !='e' && serial_rtscts != 'd') {
		serial_errno = -EINVAL;
		serial_errnostr = "Enable or disable RTS/CTS?";
		return NULL;
	}
	if (serial_parity != 'n' && serial_parity != 'e' && serial_parity != 'o') {
		serial_errno = -EINVAL;
		serial_errnostr = "Unsopported parity.";
		return NULL;
	}
	switch (serial_baud) {
	        case     50: baud = B50;        break;
	        case     75: baud = B75;        break;
	        case    110: baud = B110;       break;
	        case    134: baud = B134;       break;
	        case    150: baud = B150;       break;
	        case    200: baud = B200;       break;
	        case    300: baud = B300;       break;
	        case    600: baud = B600;       break;
	        case   1200: baud = B1200;      break;
	        case   2400: baud = B2400;      break;
	        case   4800: baud = B4800;      break;
	        case   9600: baud = B9600;      break;
	        case  19200: baud = B19200;     break;
	        case  38400: baud = B38400;     break;
	        case  57600: baud = B57600;     break;	
	        case 115200: baud = B115200;    break;
	        case 230400: baud = B230400;    break;
       		default:
			serial_errno = -EINVAL;
			serial_errnostr = "Baudrate not supported.";
		return NULL;
	}

	/* allocate handle */
	if ((serial = calloc(1, sizeof(*serial)))) {

		/* set parameters */
		serial->device = serial_device;
		serial->baud = serial_baud;
		serial->databits = serial_databits;
		serial->parity = serial_parity;
		serial->stopbits = serial_stopbits;
		serial->xonxoff = serial_xonxoff;
		serial->rtscts = serial_rtscts;
		serial->txtimeout = serial_txtimeout;
		serial->rxtimeout = serial_rxtimeout;


		if ((serial->handle = open(serial->device, O_RDWR | O_NONBLOCK)) >= 0) {
			if (isatty(serial->handle)) {
				/* get termios */
				tcgetattr(serial->handle, &serial->old_termios);
				tcgetattr(serial->handle, &serial->com_termios);
				/* set flags */
				serial->com_termios.c_iflag =
					((serial->databits == 7) ? ISTRIP : 0) |
					((serial->xonxoff == 'e') ? (IXON | IXOFF) : 0) ;
				if (serial_getbreak)
					serial->com_termios.c_iflag |= (PARMRK | INPCK);
				else
					serial->com_termios.c_iflag |= (IGNBRK | IGNPAR);
				serial->com_termios.c_oflag = 0;
				serial->com_termios.c_cflag =
					CREAD |
					HUPCL |
					((serial->databits == 5) ? CS5 : 0) |
					((serial->databits == 6) ? CS6 : 0) |
					((serial->databits == 7) ? CS7 : 0) |
					((serial->databits == 8) ? CS8 : 0) |
					((serial->parity == 'e') ? PARENB : 0) |
					((serial->parity == 'o') ? (PARENB | PARODD) : 0) |
					((serial->stopbits == 2) ? CSTOPB : 0) |
					((serial->rtscts =='e' ) ? CRTSCTS : 0) | 
					((serial->rtscts =='d' ) ? CLOCAL : 0) ;
				serial->com_termios.c_lflag = 0;
				serial->com_termios.c_cc[VSTART] = 0x11;
				serial->com_termios.c_cc[VSTOP] = 0x13;

				/* set baud */
				serial->com_termios.c_cflag |= baud;
				cfsetispeed(&serial->com_termios, baud);
				cfsetospeed(&serial->com_termios, baud);

				serial->com_termios.c_cc[VMIN] = 0;
				serial->com_termios.c_cc[VTIME] = (int)(serial->rxtimeout / 0.1 + 0.5);
	
				if (tcsetattr(serial->handle, TCSANOW, &serial->com_termios) >= 0) {
					handshake_lines = TIOCM_DTR;
					if (serial->rtscts == 'd') {
						handshake_lines |= TIOCM_RTS;
					}
	 				if (ioctl(serial->handle, TIOCMBIS, &handshake_lines) >= 0) {
						if ((flags = fcntl(serial->handle, F_GETFL, 0)) >= 0) {
							flags &= ~O_NONBLOCK;
							if (fcntl(serial->handle, F_SETFL, flags) >= 0) {
								serial_errno = 0;
								serial_errnostr = "ok";
								return serial;
							} else {
								serial_errno = -EIO;
								serial_errnostr = "Cannot set fcntl.";
							}
						} else {
							serial_errno = -EIO;
							serial_errnostr = "Cannot read fnctl.";
						}
					} else {
						serial_errno = -EIO;
						serial_errnostr = "Cannot set handshake lines.";
					}
				} else {
					serial_errno = -EIO;
					serial_errnostr = "TTY refuses settings.";
				}
			} else {
				serial_errno = -EIO;
				serial_errnostr = "Device is not a tty!";
			}
			tcsetattr(serial->handle, TCSANOW, &serial->old_termios);
			close(serial->handle);
		} else {
			serial_errno = -EIO;
			serial_errnostr = "Error opening serial interface.";
		}
		free(serial);
	} else {
		serial_errno = -ENOMEM;
		serial_errnostr = "not enough memory for handle";
	}

	return NULL;
}


/*
serial_close(serial);

closes the com port and frees all memory used by "serial"
*/
void serial_close(serial_t *serial)
{
	serial_errno = 0;
	if (serial == 0)
		return;

	tcsetattr(serial->handle,TCSANOW,&serial->old_termios);
	close(serial->handle);
	free(serial);
}

/*
read = serial_read(serial, &buffer, size);

reads until buffer "size" has reached or until timeout has occurred.
"read" gives the number of bytes read.
*/

int serial_read(serial_t *serial, uint8_t *buffer, int size)
{
	int n;

	serial_errno = 0;
	if (!serial)
		return 0;

	n = read(serial->handle, buffer, size);
	if (n < 0) {
		serial_errno = n;
		n = 0;
	}

	return n;
}

/*
wrote = serial_write(serial, &buffer, size);

writes until buffer "size" has reached or until timeout has occurred.
"wrote" gives the number of bytes written.
*/

int serial_write(serial_t *serial, uint8_t *buffer, int size)
{
	int n;
	fd_set Desc;
	struct timeval Timeout;

	serial_errno = 0;
	if (!serial)
		return 0;

	Timeout.tv_usec = (int)(serial->txtimeout * 1000000) % 1000000;
	Timeout.tv_sec  = serial->txtimeout;


	FD_ZERO (&Desc);
	FD_SET (serial->handle, &Desc);

	// Use select to check if write is possible

	if (select(serial->handle + 1, NULL, &Desc, NULL, &Timeout)) {
		// Descriptor is ready for writing
		n = write(serial->handle, buffer, size);
		if (n < 0) {
			serial_errno = n;
			n = 0;
		}
		return n;
	} else {
		// Timeout or signal. Return an timeout error code when a signal
		// occurs
		return 0;
	}
}

/*
ok = serial_timeout(serial, transmit, receive);

will set the transmit and receive timeout. note that the receive timeout
will only use the nearest 1/10s. to disable timeout, use 0 for any value.
*/
int serial_timeout(serial_t *serial, double serial_txtimeout, double serial_rxtimeout)
{
	serial_errno = 0;
	if (!serial)
		return 0;

	serial->rxtimeout = serial_rxtimeout;
	serial->txtimeout = serial_txtimeout;

	serial->com_termios.c_cc[VMIN] = 0;
	serial->com_termios.c_cc[VTIME] = (int)(serial->rxtimeout / 0.1 + 0.5);
	if (tcsetattr(serial->handle, TCSANOW, &serial->com_termios) < 0) {
		serial_errno = - EIO;
		serial_errnostr = "TTY refuses settings.";
		return -1;
	}

	return 0;
}

/*
serial_errno
serial_errnostr

the last call will give an errno while opening, which is described below:

0:	ok
also use serial_errnostr for description
*/
int	serial_errno = 0;
char	*serial_errnostr = "";

int serial_cts(serial_t *serial)
{
	int status = 0;

	serial_errno = 0;
	if (!serial)
		return 0;

	if(ioctl(serial->handle, TIOCMGET, &status) < 0) {
		serial_errno = -EIO;
		serial_errnostr = "Cannot get ioctl.";
		return -1;
	}
	return (status & TIOCM_CTS) != 0;
}

int serial_dsr(serial_t *serial)
{
	int status = 0;

	serial_errno = 0;
	if (!serial)
		return 0;

	if(ioctl(serial->handle, TIOCMGET, &status) < 0) {
		serial_errno = -EIO;
		serial_errnostr = "Cannot get ioctl.";
		return -1;
	}
	return (status & TIOCM_DSR) != 0;
}

/*
ok = serial_dtron(serial);
ok = serial_rtson(serial);
ok = serial_dtroff(serial);
ok = serial_rtsoff(serial);

turn on or off: rts, dtr
*/

int serial_dtron(serial_t *serial)
{
	int status = TIOCM_DTR;

	serial_errno = 0;
	if (!serial)
		return 0;

	if(ioctl(serial->handle, TIOCMBIS, &status) < 0) {
		serial_errno = -EIO;
		serial_errnostr = "Cannot set ioctl.";
		return -1;
	}
	return 0;
}
int serial_dtroff(serial_t *serial)
{
	int status = TIOCM_DTR;

	serial_errno = 0;
	if (!serial)
		return 0;

	if(ioctl(serial->handle, TIOCMBIC, &status) < 0) {
		serial_errno = -EIO;
		serial_errnostr = "Cannot set ioctl.";
		return -1;
	}
	return 0;
}
int serial_rtson(serial_t *serial)
{
	int status = TIOCM_RTS;

	serial_errno = 0;
	if (!serial)
		return 0;

	if(ioctl(serial->handle, TIOCMBIS, &status) < 0) {
		serial_errno = -EIO;
		serial_errnostr = "Cannot set ioctl.";
		return -1;
	}
	return 0;
}
int serial_rtsoff(serial_t *serial)
{
	int status = TIOCM_RTS;

	serial_errno = 0;
	if (!serial)
		return 0;

	if(ioctl(serial->handle, TIOCMBIC, &status) < 0) {
		serial_errno = -EIO;
		serial_errnostr = "Cannot set ioctl.";
		return -1;
	}
	return 0;
}

int serial_break(serial_t *serial, int on)
{
	serial_errno = 0;
	if (!serial)
		return 0;

        if (ioctl(serial->handle, on ? TIOCSBRK : TIOCCBRK, 0) < 0) {
		serial_errno = -EIO;
		serial_errnostr = "Cannot set ioctl.";
		return -1;
	}
	return 0;
}


/*
handle = serial_handle(serial);

get real file handle by giving the serial handle
*/

int serial_handle(serial_t *serial)
{
	if (!serial)
		return 0;

	return serial->handle;
}

