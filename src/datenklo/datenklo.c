/* osmo datenklo, the "datenklo" emulator
 *
 * (C) 2019 by Andreas Eversberg <jolly@eversberg.eu>
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <asm-generic/termbits.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>
#include <pthread.h>
#include <ctype.h>
#include <fcntl.h>
#include <math.h>
#include "../libsample/sample.h"
#include <osmocom/core/select.h>
#include <osmocom/core/timer.h>
#include "../libfsk/fsk.h"
#include "../libsound/sound.h"
#include "../libwave/wave.h"
#include "../libdisplay/display.h"
#include "../liblogging/logging.h"
#include "../libmobile/get_time.c"
#include "device.h"
#include "am791x.h"
#include "uart.h"
#include "datenklo.h"

#define level2db(level) (20 * log10(level))

/* Put the state of FD into *TERMIOS_P.  */
extern int tcgetattr (int __fd, struct termios *__termios_p) __THROW;

/* Set the state of FD to *TERMIOS_P.
   Values for OPTIONAL_ACTIONS (TCSA*) are in <bits/termios.h>.  */
extern int tcsetattr (int __fd, int __optional_actions,
                      const struct termios *__termios_p) __THROW;

static int quit = 0;

pthread_mutex_t mutex;

static tcflag_t baud2cflag(double _baudrate)
{
	tcflag_t cflag;
	int baudrate = (int)_baudrate;

	switch (baudrate) {
	case 0:
		cflag = B0;
		break;
	case 50:
		cflag = B50;
		break;
	case 75:
		cflag = B75;
		break;
	case 110:
		cflag = B110;
		break;
	case 134:
		cflag = B134;
		break;
	case 150:
		cflag = B150;
		break;
	case 200:
		cflag = B200;
		break;
	case 300:
		cflag = B300;
		break;
	case 600:
		cflag = B600;
		break;
	default:
		cflag = B1200;
	}

	return cflag;
}

static double cflag2baud(tcflag_t cflag)
{
	double baudrate;

	switch ((cflag & CBAUD)) {
	case B0:
		baudrate = 0;
		break;
	case B50:
		baudrate = 50;
		break;
	case B75:
		baudrate = 75;
		break;
	case B110:
		baudrate = 110;
		break;
	case B134:
		baudrate = 134.5;
		break;
	case B150:
		baudrate = 150;
		break;
	case B200:
		baudrate = 200;
		break;
	case B300:
		baudrate = 300;
		break;
	case B600:
		baudrate = 600;
		break;
	default:
		baudrate = 1200;
	}

	return baudrate;
}

static int cflag2databits(tcflag_t cflag)
{
	int databits;

	switch ((cflag & CSIZE)) {
	case CS5:
		databits = 5;
		break;
	case CS6:
		databits = 6;
		break;
	case CS7:
		databits = 7;
		break;
	default:
		databits = 8;
	}

	return databits;
}

static enum uart_parity cflag2parity(tcflag_t cflag)
{
	enum uart_parity parity;

	if (!(cflag & PARENB))
		parity = UART_PARITY_NONE;
	else
	if (!(cflag & PARODD)) {
		if (!(cflag & CMSPAR))
			parity = UART_PARITY_EVEN;
		else
			parity = UART_PARITY_SPACE;
	} else {
		if (!(cflag & CMSPAR))
			parity = UART_PARITY_ODD;
		else
			parity = UART_PARITY_MARK;
	}

	return parity;
}

static char parity2char(enum uart_parity parity)
{
	switch (parity) {
	case UART_PARITY_NONE:
		return 'N';
	case UART_PARITY_EVEN:
		return 'E';
	case UART_PARITY_ODD:
		return 'O';
	case UART_PARITY_MARK:
		return 'M';
	case UART_PARITY_SPACE:
		return 'S';
	}
	return ' ';
}

static int cflag2stopbits(tcflag_t cflag)
{
	int stopbits;

	if ((cflag & CSTOPB))
		stopbits = 2;
	else
		stopbits = 1;

	return stopbits;
}

/* modem changes CTS state */
static void cts(void *inst, int cts)
{
	datenklo_t *datenklo = (datenklo_t *)inst;

	if (datenklo->tx_back)
		return;

	if (datenklo->auto_rts) {
		LOGP(DDATENKLO, LOGL_INFO, "Received CTS=%d in Automatic RTS Mode.\n", cts);
		datenklo->auto_rts_cts = cts;
		return;
	}

	if (cts)
		datenklo->lines |= TIOCM_CTS;
	else
		datenklo->lines &= ~TIOCM_CTS;
	LOGP(DDATENKLO, LOGL_INFO, "Indicating to terminal that CTS is %s\n", (cts) ? "on" : "off");
}

/* modem changes CTS state (back channel) */
static void bcts(void *inst, int cts)
{
	datenklo_t *datenklo = (datenklo_t *)inst;

	if (!datenklo->tx_back)
		return;

	if (datenklo->auto_rts) {
		LOGP(DDATENKLO, LOGL_INFO, "Received BCTS=%d in Automatic RTS Mode.\n", cts);
		datenklo->auto_rts_cts = cts;
		return;
	}

	if (cts)
		datenklo->lines |= TIOCM_CTS;
	else
		datenklo->lines &= ~TIOCM_CTS;
	LOGP(DDATENKLO, LOGL_INFO, "Indicating to terminal that BCTS is %s\n", (cts) ? "on" : "off");
}

/* modem changes CD state */
static void cd(void *inst, int cd)
{
	datenklo_t *datenklo = (datenklo_t *)inst;

	if (datenklo->rx_back)
		return;

	if (datenklo->auto_rts) {
		LOGP(DDATENKLO, LOGL_INFO, "Received CD=%d in Automatic RTS Mode.\n", cd);
		datenklo->auto_rts_cd = cd;
		return;
	}

	if (cd)
		datenklo->lines |= TIOCM_CD;
	else
		datenklo->lines &= ~TIOCM_CD;
	LOGP(DDATENKLO, LOGL_INFO, "Indicating to terminal that CD is %s\n", (cd) ? "on" : "off");
}

/* modem changes CD state (back channel) */
static void bcd(void *inst, int cd)
{
	datenklo_t *datenklo = (datenklo_t *)inst;

	if (!datenklo->rx_back)
		return;

	if (datenklo->auto_rts) {
		LOGP(DDATENKLO, LOGL_INFO, "Received BCD=%d in Automatic RTS Mode.\n", cd);
		datenklo->auto_rts_cd = cd;
		return;
	}

	if (cd)
		datenklo->lines |= TIOCM_CD;
	else
		datenklo->lines &= ~TIOCM_CD;
	LOGP(DDATENKLO, LOGL_INFO, "Indicating to terminal that BCD is %s\n", (cd) ? "on" : "off");
}

/* modem request bit */
static int td(void *inst)
{
	datenklo_t *datenklo = (datenklo_t *)inst;

	if (datenklo->tx_back)
		return 1;

	if (!uart_is_tx(&datenklo->uart)) {
		if (datenklo->break_bits) {
			--datenklo->break_bits;
			return 0;
		}
		if (datenklo->break_on) {
			return 0;
		}
	}

	return uart_tx_bit(&datenklo->uart);
}

/* modem request bit (back channel) */
static int btd(void *inst)
{
	datenklo_t *datenklo = (datenklo_t *)inst;

	if (!datenklo->tx_back)
		return 1;

	if (!uart_is_tx(&datenklo->uart)) {
		if (datenklo->break_bits) {
			--datenklo->break_bits;
			return 0;
		}
		if (datenklo->break_on) {
			return 0;
		}
	}

	return uart_tx_bit(&datenklo->uart);
}

/* modem received bit */
static void rd(void *inst, int bit, double quality, double level)
{
	datenklo_t *datenklo = (datenklo_t *)inst;

	if (datenklo->rx_back)
		return;

	/* only show level+quality when bit has not changed */
	if (datenklo->last_bit == bit) {
		display_measurements_update(datenklo->dmp_tone_level, level, 0.0);
		display_measurements_update(datenklo->dmp_tone_quality, quality, 0.0);
	}
	datenklo->last_bit = bit;

	uart_rx_bit(&datenklo->uart, bit);
}

/* modem received bit (back channel) */
static void brd(void *inst, int bit, double quality, double level)
{
	datenklo_t *datenklo = (datenklo_t *)inst;

	if (!datenklo->rx_back)
		return;

	/* only show level+quality when bit has not changed */
	if (datenklo->last_bit == bit) {
		display_measurements_update(datenklo->dmp_tone_level, level, 0.0);
		display_measurements_update(datenklo->dmp_tone_quality, quality, 0.0);
	}
	datenklo->last_bit = bit;

	uart_rx_bit(&datenklo->uart, bit);
}

static void set_termios(datenklo_t *datenklo, const void *buf);

/* helper to flush tx buffer and all tx states */
static void flush_tx(datenklo_t *datenklo)
{
	datenklo->tx_fifo_out = datenklo->tx_fifo_in;
	datenklo->onlcr_char = 0;
}

/* helper to flush rx buffer */
static void flush_rx(datenklo_t *datenklo)
{
	datenklo->rx_fifo_out = datenklo->rx_fifo_in;
}

/* UART requests byte to transmit */
static int tx(void *inst)
{
	datenklo_t *datenklo = (datenklo_t *)inst;
	size_t fill;
	int data;

	if (datenklo->output_off)
		return -1;

	if (!(datenklo->lines & TIOCM_RTS) || !(datenklo->lines & TIOCM_CTS))
		return -1;

	if (datenklo->auto_rts && !datenklo->auto_rts_cts)
		return -1;

	/* nl -> cr+nl mode */
	if (datenklo->onlcr_char) {
		datenklo->onlcr_char = 0;
		data = '\n';
		LOGP(DDATENKLO, LOGL_DEBUG, "ONLCR: sending NL\n");
		goto out;
	}

again:
	fill = (datenklo->tx_fifo_in - datenklo->tx_fifo_out + datenklo->tx_fifo_size) % datenklo->tx_fifo_size;

	if (fill == (size_t)datenklo->tx_fifo_full) {
		/* tell cuse to write again */
		LOGP(DDATENKLO, LOGL_DEBUG, "Set POLLOUT!\n");
		datenklo->revents |= POLLOUT;
		device_set_poll_events(datenklo->device, datenklo->revents);
	}

	if (!fill) {
		if (datenklo->auto_rts)
			datenklo->auto_rts_on = 0;

		if (datenklo->tcsetsw) {
			LOGP(DDATENKLO, LOGL_DEBUG, "Transmission finished, applying termios now.\n");
			memcpy(&datenklo->termios, &datenklo->tcsetsw_termios, sizeof(datenklo->termios));
			if (datenklo->tcsetsw == 2) {
				flush_rx(datenklo);
				if ((datenklo->revents & POLLIN)) {
					LOGP(DDATENKLO, LOGL_DEBUG, "Reset POLLIN (flushed)\n");
					datenklo->revents &= ~POLLIN;
					device_set_poll_events(datenklo->device, datenklo->revents);
				}
			}
			datenklo->tcsetsw = 0;
			set_termios(datenklo,  &datenklo->tcsetsw_termios);
		}
		return -1;
	}

	data = datenklo->tx_fifo[datenklo->tx_fifo_out++];
	datenklo->tx_fifo_out %= datenklo->tx_fifo_size;
	fill--;

	/* in case of blocking: check if there is enough space to write */
	device_write_available(datenklo->device);

	/* process output features */
	if (datenklo->opost) {
		if (datenklo->olcuc) {
			LOGP(DDATENKLO, LOGL_DEBUG, "OLCUC: 0x%02x -> 0x%02x\n", data, toupper(data));
			data = toupper(data);
		}
		if (datenklo->onlret && data == '\r') {
			LOGP(DDATENKLO, LOGL_DEBUG, "ONLRET: ignore CR\n");
			goto again;
		}
		if (datenklo->ocrnl && data == '\r') {
			LOGP(DDATENKLO, LOGL_DEBUG, "OCRNL: CR -> NL\n");
			data = '\n';
		}
		if (datenklo->onlcr && data == '\n') {
			datenklo->onlcr_char = 1;
			data = '\r';
			LOGP(DDATENKLO, LOGL_DEBUG, "ONLCR: sending CR\n");
		}
	}

out:
	LOGP(DDATENKLO, LOGL_DEBUG, "Transmitting byte 0x%02x to UART.\n", data);

	return data;
}

/* UART receives complete byte */
static void rx(void *inst, int data, uint32_t __attribute__((unused)) flags)
{
	datenklo_t *datenklo = (datenklo_t *)inst;
	size_t space;

	LOGP(DDATENKLO, LOGL_DEBUG, "Received byte 0x%02x ('%c') from UART.\n", data, (data >= 32 && data <= 126) ? data : '.');

	/* process input features */
	if (datenklo->ignbrk && (flags & UART_BREAK)) {
		LOGP(DDATENKLO, LOGL_DEBUG, "IGNBRK: ignore BREAK\n");
		return;
	}
	if (datenklo->istrip && (data & 0x80)) {
		LOGP(DDATENKLO, LOGL_DEBUG, "ISTRIP: 0x%02x -> 0x%02x\n", data, data & 0x7f);
		data &= 0x7f;
	}
	if (datenklo->inlcr && data == '\n') {
		LOGP(DDATENKLO, LOGL_DEBUG, "INLCR: NL -> CR\n");
		data = '\r';
	}
	if (datenklo->igncr && data == '\r') {
		LOGP(DDATENKLO, LOGL_DEBUG, "IGNCR: ignore CR\n");
		return;
	}
	if (datenklo->icrnl && data == '\r') {
		LOGP(DDATENKLO, LOGL_DEBUG, "ICRNL: CR -> NL\n");
		data = '\n';
	}
	if (datenklo->iuclc) {
		LOGP(DDATENKLO, LOGL_DEBUG, "IUCLC: 0x%02x -> 0x%02x\n", data, tolower(data));
		data = tolower(data);
	}
	if (datenklo->echo) {
		LOGP(DDATENKLO, LOGL_DEBUG, "ECHO: write to output\n");
		space = (datenklo->tx_fifo_out - datenklo->tx_fifo_in - 1 + datenklo->tx_fifo_size) % datenklo->tx_fifo_size;
		if (space) {
			datenklo->tx_fifo[datenklo->tx_fifo_in++] = data;
			datenklo->tx_fifo_in %= datenklo->tx_fifo_size;
		}
	}

	/* empty buffer gets data */
	if (datenklo->rx_fifo_out == datenklo->rx_fifo_in) {
		/* tell cuse to read again */
		LOGP(DDATENKLO, LOGL_DEBUG, "Set POLLIN!\n");
		datenklo->revents |= POLLIN;
		device_set_poll_events(datenklo->device, datenklo->revents);
	}

	space = (datenklo->rx_fifo_out - datenklo->rx_fifo_in - 1 + datenklo->rx_fifo_size) % datenklo->rx_fifo_size;

	if (!space) {
		err_overflow:
		LOGP(DDATENKLO, LOGL_NOTICE, "RX buffer overflow, dropping!\n");
		return;
	}

	if (datenklo->parmrk) {
		if ((flags & (UART_BREAK | UART_PARITY_ERROR))) {
			LOGP(DDATENKLO, LOGL_DEBUG, "PARMRK: 0x%02x -> 0xff,0x00,0x%02x\n", data, data);
			if (space < 3)
				goto err_overflow;
			datenklo->rx_fifo[datenklo->rx_fifo_in++] = 0xff;
			datenklo->rx_fifo_in %= datenklo->rx_fifo_size;
			space--;
			datenklo->rx_fifo[datenklo->rx_fifo_in++] = 0x00;
			datenklo->rx_fifo_in %= datenklo->rx_fifo_size;
			space--;
		} else if (data == 0xff) {
			LOGP(DDATENKLO, LOGL_DEBUG, "PARMRK: 0xff -> 0xff,0xff\n");
			if (space < 2)
				goto err_overflow;
			datenklo->rx_fifo[datenklo->rx_fifo_in++] = 0xff;
			datenklo->rx_fifo_in %= datenklo->rx_fifo_size;
			space--;
		}
	}

	datenklo->rx_fifo[datenklo->rx_fifo_in++] = data;
	datenklo->rx_fifo_in %= datenklo->rx_fifo_size;
	space--;

	/* in case of blocking: check if there is enough data to read */
	device_read_available(datenklo->device);
}

/* helper to set line states of modem */
static void set_lines(datenklo_t *datenklo, int new)
{
	int old = datenklo->lines;

	if (!(old & TIOCM_DTR) && (new & TIOCM_DTR)) {
		LOGP(DDATENKLO, LOGL_INFO, "Terminal turns DTR on\n");
		flush_tx(datenklo);
		flush_rx(datenklo);
		am791x_dtr(&datenklo->am791x, 1);
	}
	if ((old & TIOCM_DTR) && !(new & TIOCM_DTR)) {
		LOGP(DDATENKLO, LOGL_INFO, "Terminal turns DTR off\n");
		am791x_dtr(&datenklo->am791x, 0);
	}

	if (!(old & TIOCM_RTS) && (new & TIOCM_RTS)) {
		LOGP(DDATENKLO, LOGL_INFO, "Terminal turns RTS on\n");
		if (datenklo->auto_rts)
			new |= TIOCM_CTS | TIOCM_CD;
		else {
			if (!datenklo->tx_back)
				am791x_rts(&datenklo->am791x, 1);
			else
				am791x_brts(&datenklo->am791x, 1);
		}
	}
	if ((old & TIOCM_RTS) && !(new & TIOCM_RTS)) {
		LOGP(DDATENKLO, LOGL_INFO, "Terminal turns RTS off\n");
		if (datenklo->auto_rts)
			new &= ~(TIOCM_CTS | TIOCM_CD);
		else {
			if (!datenklo->tx_back)
				am791x_rts(&datenklo->am791x, 0);
			else
				am791x_brts(&datenklo->am791x, 0);
		}
	}

	datenklo->lines = new;
}

		/* process Auto RTS */
static void process_auto_rts(datenklo_t *datenklo)
{
	if (!datenklo->auto_rts)
		return;
	if (datenklo->auto_rts_on && !datenklo->auto_rts_rts && !datenklo->auto_rts_cd) {
		LOGP(DDATENKLO, LOGL_INFO, "Automatically raising RTS.\n");
		datenklo->auto_rts_rts = 1;
		if (!datenklo->tx_back)
			am791x_rts(&datenklo->am791x, 1);
		else
			am791x_brts(&datenklo->am791x, 1);
	}
	if (!datenklo->auto_rts_on && datenklo->auto_rts_rts) {
		LOGP(DDATENKLO, LOGL_INFO, "Automatically dropping RTS.\n");
		datenklo->auto_rts_rts = 0;
		if (!datenklo->tx_back)
			am791x_rts(&datenklo->am791x, 0);
		else
			am791x_brts(&datenklo->am791x, 0);
	}
}

/* tty performs all IOCTLs that requests states */
static ssize_t dk_ioctl_get(void *inst, int cmd, void *buf, size_t out_bufsz)
{
	datenklo_t *datenklo = (datenklo_t *)inst;
	int status;
	ssize_t rc = 0;

#ifdef HEAVY_DEBUG
	LOGP(DDATENKLO, LOGL_DEBUG, "Device has been read for ioctl (cmd = %d, size = %zu).\n", cmd, out_bufsz);
#endif

	switch (cmd) {
	case TCGETS:
		rc = sizeof(datenklo->termios);
		if (!out_bufsz)
			break;
#ifdef HEAVY_DEBUG
		LOGP(DDATENKLO, LOGL_DEBUG, "Terminal requests termios.\n");
#endif
		memcpy(buf, &datenklo->termios, rc);
		break;
	case TIOCMGET:
		rc = sizeof(status);
		if (!out_bufsz)
			break;
#ifdef HEAVY_DEBUG
		LOGP(DDATENKLO, LOGL_DEBUG, "Terminal requests line states.\n");
#endif
		status = datenklo->lines | TIOCM_LE | TIOCM_DSR;
		memcpy(buf, &status, rc);
		break;
	case TIOCGWINSZ:
		rc = sizeof(struct winsize);
		if (!out_bufsz)
			break;
#ifdef HEAVY_DEBUG
		LOGP(DDATENKLO, LOGL_DEBUG, "Terminal requests window size.\n");
#endif
		struct winsize *winsize = (struct winsize *)buf;
		winsize->ws_row = 25;
		winsize->ws_col = 80;
		winsize->ws_xpixel = 640;
		winsize->ws_ypixel = 200;
		break;
	case FIONREAD:
		rc = sizeof(status);
		if (!out_bufsz)
			break;
		status = (datenklo->rx_fifo_in - datenklo->rx_fifo_out + datenklo->rx_fifo_size) % datenklo->rx_fifo_size;
		memcpy(buf, &status, rc);
#ifdef HEAVY_DEBUG
		LOGP(DDATENKLO, LOGL_DEBUG, "Terminal requests RX buffer fill states.\n");
#endif
		break;
	case TIOCOUTQ:
		rc = sizeof(status);
		if (!out_bufsz)
			break;
#ifdef HEAVY_DEBUG
		LOGP(DDATENKLO, LOGL_DEBUG, "Terminal requests TX buffer fill states.\n");
#endif
		status = (datenklo->tx_fifo_in - datenklo->tx_fifo_out + datenklo->tx_fifo_size) % datenklo->tx_fifo_size;
		memcpy(buf, &status, rc);
		break;
	default:
		rc = -EINVAL;
	}

	return rc;
}

static double tx_baud_rate(datenklo_t *datenklo)
{
	double baudrate;

	if (datenklo->force_tx_baud)
		baudrate = datenklo->force_tx_baud;
	else
		baudrate = datenklo->baudrate;
	if (baudrate > datenklo->max_baud)
		baudrate = datenklo->max_baud;

	return baudrate;
}

static double rx_baud_rate(datenklo_t *datenklo)
{ 
	double baudrate;

	if (datenklo->force_rx_baud)
		baudrate = datenklo->force_rx_baud;
	else
		baudrate = datenklo->baudrate;
	if (baudrate > datenklo->max_baud)
		baudrate = datenklo->max_baud;

	return baudrate;
}

/* helper to set termios */
static void set_termios(datenklo_t *datenklo, const void *buf)
{
	double old_baud, new_baud;
	int old_databits, new_databits;
	enum uart_parity old_parity, new_parity;
	int old_stopbits, new_stopbits;
	int old_ignbrk, new_ignbrk;
	int old_parmrk, new_parmrk;
	int old_istrip, new_istrip;
	int old_inlcr, new_inlcr;
	int old_igncr, new_igncr;
	int old_icrnl, new_icrnl;
	int old_iuclc, new_iuclc;
	int old_opost, new_opost;
	int old_onlcr, new_onlcr;
	int old_ocrnl, new_ocrnl;
	int old_onlret, new_onlret;
	int old_olcuc, new_olcuc;
	int old_echo, new_echo;
	int rc;

	old_baud = cflag2baud(datenklo->termios.c_cflag & CBAUD);
	old_databits = cflag2databits(datenklo->termios.c_cflag);
	old_parity = cflag2parity(datenklo->termios.c_cflag);
	old_stopbits = cflag2stopbits(datenklo->termios.c_cflag);
	old_ignbrk = !!(datenklo->termios.c_iflag & IGNBRK);
	old_parmrk = !!(datenklo->termios.c_iflag & PARMRK);
	old_istrip = !!(datenklo->termios.c_iflag & ISTRIP);
	old_inlcr = !!(datenklo->termios.c_iflag & INLCR);
	old_igncr = !!(datenklo->termios.c_iflag & IGNCR);
	old_icrnl = !!(datenklo->termios.c_iflag & ICRNL);
	old_iuclc = !!(datenklo->termios.c_iflag & IUCLC);
	old_opost = !!(datenklo->termios.c_oflag & OPOST);
	old_onlcr = !!(datenklo->termios.c_oflag & ONLCR);
	old_ocrnl = !!(datenklo->termios.c_oflag & OCRNL);
	old_onlret = !!(datenklo->termios.c_oflag & ONLRET);
	old_olcuc = !!(datenklo->termios.c_oflag & OLCUC);
	old_echo = !!(datenklo->termios.c_lflag & ECHO);

	memcpy(&datenklo->termios, buf, sizeof(datenklo->termios));

	new_baud = cflag2baud(datenklo->termios.c_cflag & CBAUD);
	new_databits = cflag2databits(datenklo->termios.c_cflag);
	new_parity = cflag2parity(datenklo->termios.c_cflag);
	new_stopbits = cflag2stopbits(datenklo->termios.c_cflag);
	new_ignbrk = !!(datenklo->termios.c_iflag & IGNBRK);
	new_parmrk = !!(datenklo->termios.c_iflag & PARMRK);
	new_istrip = !!(datenklo->termios.c_iflag & ISTRIP);
	new_inlcr = !!(datenklo->termios.c_iflag & INLCR);
	new_igncr = !!(datenklo->termios.c_iflag & IGNCR);
	new_icrnl = !!(datenklo->termios.c_iflag & ICRNL);
	new_iuclc = !!(datenklo->termios.c_iflag & IUCLC);
	new_opost = !!(datenklo->termios.c_oflag & OPOST);
	new_onlcr = !!(datenklo->termios.c_oflag & ONLCR);
	new_ocrnl = !!(datenklo->termios.c_oflag & OCRNL);
	new_onlret = !!(datenklo->termios.c_oflag & ONLRET);
	new_olcuc = !!(datenklo->termios.c_oflag & OLCUC);
	new_echo = !!(datenklo->termios.c_lflag & ECHO);

	if (old_baud != new_baud && (!datenklo->force_tx_baud || !datenklo->force_rx_baud)) {
		LOGP(DDATENKLO, LOGL_INFO, "Terminal changes baud rate to %.1f Baud.\n", new_baud);
		if ((datenklo->lines & TIOCM_DTR) && !new_baud) {
			LOGP(DDATENKLO, LOGL_INFO, "Baudrate is set to 0, we drop DTR\n");
			am791x_dtr(&datenklo->am791x, 0);
		}
		datenklo->baudrate = new_baud;
		am791x_mc(&datenklo->am791x, datenklo->mc, datenklo->samplerate, tx_baud_rate(datenklo), rx_baud_rate(datenklo));
		if ((datenklo->lines & TIOCM_DTR) && !old_baud) {
			LOGP(DDATENKLO, LOGL_INFO, "Baudrate is set from 0, we raise DTR\n");
			am791x_dtr(&datenklo->am791x, 1);
		}
	}

	if (old_databits != new_databits
	 || old_parity != new_parity
	 || old_stopbits != new_stopbits) {
		LOGP(DDATENKLO, LOGL_INFO, "Terminal changes serial mode to %d%c%d.\n", cflag2databits(datenklo->termios.c_cflag), parity2char(cflag2parity(datenklo->termios.c_cflag)), cflag2stopbits(datenklo->termios.c_cflag));
		rc = uart_init(&datenklo->uart, datenklo, cflag2databits(datenklo->termios.c_cflag), cflag2parity(datenklo->termios.c_cflag), cflag2stopbits(datenklo->termios.c_cflag), tx, rx);
		if (rc < 0)
			LOGP(DDATENKLO, LOGL_ERROR, "Failed to initialize UART.\n");
	}

	if (old_stopbits != new_stopbits
	 || old_ignbrk != new_ignbrk
	 || old_parmrk != new_parmrk
	 || old_istrip != new_istrip
	 || old_inlcr != new_inlcr
	 || old_igncr != new_igncr
	 || old_icrnl != new_icrnl
	 || old_iuclc != new_iuclc
	 || old_opost != new_opost
	 || old_onlcr != new_onlcr
	 || old_ocrnl != new_ocrnl
	 || old_onlret != new_onlret
	 || old_olcuc != new_olcuc
	 || old_echo != new_echo) {
		datenklo->ignbrk = new_ignbrk;
		datenklo->parmrk = new_parmrk;
		datenklo->istrip = new_istrip;
		datenklo->inlcr = new_inlcr;
		datenklo->igncr = new_igncr;
		datenklo->icrnl = new_icrnl;
		datenklo->iuclc = new_iuclc;
		datenklo->opost = new_opost;
		datenklo->onlcr = new_onlcr;
		datenklo->ocrnl = new_ocrnl;
		datenklo->onlret = new_onlret;
		datenklo->olcuc = new_olcuc;
		datenklo->echo = new_echo;
		LOGP(DDATENKLO, LOGL_INFO, "Terminal sets serial flags:\n");
		LOGP(DDATENKLO, LOGL_INFO, "%cignbrk %cparmrk %cistrip %cinlcr %cigncr %cicrnl %ciuclc %copost %conlcr %cocrnl %conlret %colcuc %cecho\n",
			(datenklo->ignbrk) ? '+' : '-',
			(datenklo->parmrk) ? '+' : '-',
			(datenklo->istrip) ? '+' : '-',
			(datenklo->inlcr) ? '+' : '-',
			(datenklo->igncr) ? '+' : '-',
			(datenklo->icrnl) ? '+' : '-',
			(datenklo->iuclc) ? '+' : '-',
			(datenklo->opost) ? '+' : '-',
			(datenklo->onlcr) ? '+' : '-',
			(datenklo->ocrnl) ? '+' : '-',
			(datenklo->onlret) ? '+' : '-',
			(datenklo->olcuc) ? '+' : '-',
			(datenklo->echo) ? '+' : '-');
	}
}

/* tty performs all IOCTLs that sets states or performs actions */
static ssize_t dk_ioctl_set(void *inst, int cmd, const void *buf, size_t in_bufsz)
{
	datenklo_t *datenklo = (datenklo_t *)inst;
	int status;
	ssize_t rc = 0;
	size_t space;

#ifdef HEAVY_DEBUG
	LOGP(DDATENKLO, LOGL_DEBUG, "Device has been written for ioctl (cmd = %d, size = %zu).\n", cmd, in_bufsz);
#endif

	switch (cmd) {
	case TCSETS:
		rc = sizeof(datenklo->termios);
		if (!in_bufsz)
			break;
		LOGP(DDATENKLO, LOGL_DEBUG, "Terminal sets termios now.\n");
		set_termios(datenklo, buf);
		break;
	case TCSETSW:
	case TCSETSF:
		rc = sizeof(datenklo->termios);
		if (!in_bufsz)
			break;
		LOGP(DDATENKLO, LOGL_DEBUG, "Terminal sets termios after draining output buffer.\n");
		if (1 || datenklo->tx_fifo_out == datenklo->tx_fifo_in) {
			LOGP(DDATENKLO, LOGL_DEBUG, "Output buffer empty, applying termios now.\n");
			set_termios(datenklo, buf);
			break;
		}
		memcpy(&datenklo->tcsetsw_termios, buf, rc);
		if (cmd == TCSETSW)
			datenklo->tcsetsw = 1;
		else
			datenklo->tcsetsw = 2;
		break;
	case TCFLSH:
		rc = sizeof(status);
		if (!in_bufsz)
			break;
		memcpy(&status, buf, rc);
		LOGP(DDATENKLO, LOGL_DEBUG, "Terminal flushes buffer (status = %d).\n", status);
		if (status == TCIOFLUSH || status == TCOFLUSH) {
			flush_tx(datenklo);
			if (!(datenklo->revents & POLLOUT)) {
				/* tell cuse to write again */
				LOGP(DDATENKLO, LOGL_DEBUG, "Set POLLOUT (flushed)\n");
				datenklo->revents |= POLLOUT;
				device_set_poll_events(datenklo->device, datenklo->revents);
			}
		}
		if (status == TCIOFLUSH || status == TCIFLUSH) {
			flush_rx(datenklo);
			if ((datenklo->revents & POLLIN)) {
				LOGP(DDATENKLO, LOGL_DEBUG, "Reset POLLIN (flushed)\n");
				datenklo->revents &= ~POLLIN;
				device_set_poll_events(datenklo->device, datenklo->revents);
			}
		}
		break;
	case TCSBRK:
		rc = 0;
		LOGP(DDATENKLO, LOGL_DEBUG, "Terminal sends break\n");
		datenklo->break_bits = tx_baud_rate(datenklo) * 3 / 10;
		break;
	case TCSBRKP:
		rc = sizeof(status);
		if (!in_bufsz)
			break;
		memcpy(&status, buf, rc);
		LOGP(DDATENKLO, LOGL_DEBUG, "Terminal sends break (duration = %d).\n", status);
		if (status == 0)
			status = 3;
		if (status > 30)
			status = 30;
		datenklo->break_bits = tx_baud_rate(datenklo) * status / 10;
		break;
	case TIOCSBRK:
		rc = 0;
		LOGP(DDATENKLO, LOGL_DEBUG, "Terminal turns break on\n");
		datenklo->break_on = 1;
		break;
	case TIOCCBRK:
		rc = 0;
		LOGP(DDATENKLO, LOGL_DEBUG, "Terminal turns break off\n");
		datenklo->break_on = 0;
		break;
	case TIOCMBIS:
		rc = sizeof(status);
		if (!in_bufsz)
			break;
		memcpy(&status, buf, rc);
		LOGP(DDATENKLO, LOGL_DEBUG, "Terminal sets line status (0x%x).\n", status);
		status = datenklo->lines | status;
		set_lines(datenklo, status);
		break;
	case TIOCMBIC:
		rc = sizeof(status);
		if (!in_bufsz)
			break;
		memcpy(&status, buf, rc);
		LOGP(DDATENKLO, LOGL_DEBUG, "Terminal clears line status (0x%x).\n", status);
		status = datenklo->lines & ~status;
		set_lines(datenklo, status);
		break;
	case TIOCMSET:
		rc = sizeof(status);
		if (!in_bufsz)
			break;
		memcpy(&status, buf, rc);
		LOGP(DDATENKLO, LOGL_DEBUG, "Terminal specifies line status (0x%x).\n", status);
		set_lines(datenklo, status);
		break;
	case TIOCGSID:
		LOGP(DDATENKLO, LOGL_DEBUG, "TIOGSID -> ENOTTY\n");
		rc = -ENOTTY;
		break;
	case TIOCGPGRP:
		LOGP(DDATENKLO, LOGL_DEBUG, "TIOCGPGRP -> ENOTTY\n");
		rc = -ENOTTY;
		break;
	case TIOCSCTTY:
		LOGP(DDATENKLO, LOGL_DEBUG, "TIOCSCTTY -> ENOTTY\n");
		rc = -ENOTTY;
		break;
	case TIOCSPGRP:
		LOGP(DDATENKLO, LOGL_DEBUG, "TIOCSPGRP -> ENOTTY\n");
		rc = -ENOTTY;
		break;
	case TIOCSWINSZ:
		rc = sizeof(struct winsize);
		if (!in_bufsz)
			break;
		LOGP(DDATENKLO, LOGL_DEBUG, "Terminal sets window size.\n");
		break;
	case TCXONC:
		rc = sizeof(status);
		if (!in_bufsz)
			break;
		memcpy(&status, buf, rc);
		switch (status) {
		case TCOOFF:
			LOGP(DDATENKLO, LOGL_DEBUG, "Terminal turns off output.\n");
			datenklo->output_off = 1;
			break;
		case TCOON:
			LOGP(DDATENKLO, LOGL_DEBUG, "Terminal turns on output.\n");
			datenklo->output_off = 0;
			break;
		case TCIOFF:
			LOGP(DDATENKLO, LOGL_DEBUG, "Terminal turns off input.\n");
			space = (datenklo->rx_fifo_out - datenklo->rx_fifo_in - 1 + datenklo->rx_fifo_size) % datenklo->rx_fifo_size;
			if (space < 1)
				break;
			datenklo->rx_fifo[datenklo->rx_fifo_in++] = datenklo->termios.c_cc[VSTOP];
			datenklo->rx_fifo_in %= datenklo->rx_fifo_size;
			break;
		case TCION:
			LOGP(DDATENKLO, LOGL_DEBUG, "Terminal turns on input.\n");
			space = (datenklo->rx_fifo_out - datenklo->rx_fifo_in - 1 + datenklo->rx_fifo_size) % datenklo->rx_fifo_size;
			if (space < 1)
				break;
			datenklo->rx_fifo[datenklo->rx_fifo_in++] = datenklo->termios.c_cc[VSTART];
			datenklo->rx_fifo_in %= datenklo->rx_fifo_size;
			break;
		}
		break;
	default:
		rc = -EINVAL;
	}

	return rc;
}

/* tty has been opened */
static int dk_open(void *inst, int flags)
{
	datenklo_t *datenklo = (datenklo_t *)inst;

	if (datenklo->open_count) {
		LOGP(DDATENKLO, LOGL_NOTICE, "Device is busy.\n");
		return -EBUSY;
	}
	datenklo->open_count++;
	datenklo->flags = flags;
	LOGP(DDATENKLO, LOGL_INFO, "Device has been opened.\n");
	int status = datenklo->lines | TIOCM_DTR | TIOCM_RTS;
	set_lines(datenklo, status);

	return 0;
}

/* tty has been closed */
static void dk_close(void *inst)
{
	datenklo_t *datenklo = (datenklo_t *)inst;

	LOGP(DDATENKLO, LOGL_INFO, "Device has been closed.\n");
	datenklo->open_count--;
	int status = datenklo->lines & ~(TIOCM_DTR | TIOCM_RTS);
	set_lines(datenklo, status);
	datenklo->output_off = 0;
}

/* helper to debug buffer content */
static void debug_data(const char *buf, int count)
{
	char text[41];
	size_t i;

	while (count) {
		for (i = 0; count && i < sizeof(text) - 1; i++) {
			text[i] = (*buf >= 32 && *buf <= 126) ? *buf : '.';
			buf++;
			count--;
		}
		text[i] = '\0';
		LOGP(DDATENKLO, LOGL_DEBUG, "  \"%s\"\n", text);
	}
}

/* tty performs read */
static ssize_t dk_read(void *inst, char *buf, size_t size, int flags)
{
	datenklo_t *datenklo = (datenklo_t *)inst;
	size_t fill, space;
	size_t i, count;
	unsigned char vtime = datenklo->termios.c_cc[VTIME];
	unsigned char vmin = datenklo->termios.c_cc[VMIN];

	fill = (datenklo->rx_fifo_in - datenklo->rx_fifo_out + datenklo->rx_fifo_size) % datenklo->rx_fifo_size;
	space = (datenklo->tx_fifo_out - datenklo->tx_fifo_in - 1 + datenklo->tx_fifo_size) % datenklo->tx_fifo_size;

	/* both MIN and TIME are nonzero */
	if (vmin && vtime) {
		/* first: start timer */
		if (!datenklo->vtimeout)
			datenklo->vtimer_us = vtime * 100000; /* start timer in main loop */
		/* no data: block (in blocking IO) */
		if (fill == 0) {
			/* special value to tell device there is no data right now, we have to block */
			return -EAGAIN;
		}
		/* not enough data and no timeout and blocking IO: block */
		if (fill < vmin && !datenklo->vtimeout && !(flags & O_NONBLOCK)) {
			/* special value to tell device there is no data right now, we have to block */
			return -EAGAIN;
		}
		/* enough data or timeout or nonblocking IO: stop timer and return what we have */
		datenklo->vtimeout = 0;
		datenklo->vtimer_us = -1; /* stop timer in main loop */
	}
	/* both MIN and TIME are zero */
	if (!vmin && !vtime) {
		/* no data: return 0 */
		if (fill == 0)
			return 0;
		/* data: return what we have */
	}
	/* MIN is zero, TIME is nonzero */
	if (!vmin && vtime) {
		/* first: start timer */
		if (!datenklo->vtimeout)
			datenklo->vtimer_us = vtime * 100000; /* start timer in main loop */
		if (fill == 0) {
			/* no data and no timeout: block (in blocking IO) */
			if (!datenklo->vtimeout) {
				/* special value to tell device there is no data right now, we have to block */
				return -EAGAIN;
			}
			/* no data and timeout: return 0 */
			datenklo->vtimeout = 0;
			return 0;
		}
		/* data: stop timer and return what we have */
		datenklo->vtimeout = 0;
		datenklo->vtimer_us = -1; /* stop timer in main loop */
	}
	/* MIN is nonzero, TIME is zero */
	if (vmin && !vtime) {
		/* less data than vmin (or buffer full): block (in blocking IO) */
		if (fill < vmin || !space) {
			/* special value to tell device there is no data right now, we have to block */
			return -EAGAIN;
		}
		/* enough data in buffer: return what we have */
	}

	LOGP(DDATENKLO, LOGL_DEBUG, "Device has been read from. (fill = %zu)\n", fill);

	/* get data from fifo */
	count = 0;
	for (i = 0; i < size; i++) {
		if (!fill)
			break;
		fill--;
		buf[i] = datenklo->rx_fifo[datenklo->rx_fifo_out++];
		datenklo->rx_fifo_out %= datenklo->rx_fifo_size;
		count++;
	}

	debug_data(buf, count);

	if (!fill) {
		/* tell cuse not to read anymore */
		if ((datenklo->revents & POLLIN)) {
			LOGP(DDATENKLO, LOGL_DEBUG, "Reset POLLIN (now empty)!\n");
			datenklo->revents &= ~POLLIN;
			device_set_poll_events(datenklo->device, datenklo->revents);
		}
	}

	return count;
}

/* tty performs write */
static ssize_t dk_write(void *inst, const char *buf, size_t size, int __attribute__((unused)) flags)
{
	datenklo_t *datenklo = (datenklo_t *)inst;
	size_t space, fill;
	size_t i;

	if (!(datenklo->lines & TIOCM_DTR)) {
		LOGP(DDATENKLO, LOGL_INFO, "Dropping data, DTR is off!\n");
		return -EIO;
	}

	if (!(datenklo->lines & TIOCM_RTS)) {
		LOGP(DDATENKLO, LOGL_INFO, "Dropping data, RTS is off!\n");
		return -EIO;
	}

	if (size > (size_t)datenklo->tx_fifo_size - 1) {
		LOGP(DDATENKLO, LOGL_NOTICE, "Device sends us too many data. (size = %zu)\n", size);
		return -EIO;
	}

	space = (datenklo->tx_fifo_out - datenklo->tx_fifo_in - 1 + datenklo->tx_fifo_size) % datenklo->tx_fifo_size;

	/* block if not enough space AND buffer is not completely empty */
	if (space < size && datenklo->tx_fifo_out != datenklo->tx_fifo_in) {
		/* special value to tell device there is no data right now, we have to block */
		return -EAGAIN;
	}

	LOGP(DDATENKLO, LOGL_DEBUG, "Device has been written to. (space = %zu)\n", space);
	debug_data(buf, size);

	if (datenklo->auto_rts)
		datenklo->auto_rts_on = 1;

	/* put data to fifo */
	for (i = 0; i < size; i++) {
		datenklo->tx_fifo[datenklo->tx_fifo_in++] = buf[i];
		datenklo->tx_fifo_in %= datenklo->tx_fifo_size;
	}

	fill = (datenklo->tx_fifo_in - datenklo->tx_fifo_out + datenklo->tx_fifo_size) % datenklo->tx_fifo_size;

	if ((datenklo->revents & POLLOUT) && fill >= (size_t)datenklo->tx_fifo_full) {
		/* tell cuse not to write */
		LOGP(DDATENKLO, LOGL_DEBUG, "Reset POLLOUT (buffer full)\n");
		datenklo->revents &= ~POLLOUT;
		device_set_poll_events(datenklo->device, datenklo->revents);
	}

	return size;
}

static void dk_flush_tx(void *inst)
{
	datenklo_t *datenklo = (datenklo_t *)inst;

	LOGP(DDATENKLO, LOGL_INFO, "Terminal sends interrupt while writing, flushing TX buffer\n");
	flush_tx(datenklo);
}

/* tty locks main thread to call our functions */
static void dk_lock(void)
{
	pthread_mutex_lock(&mutex);
}

/* tty unlocks main thread */
static void dk_unlock(void)
{
	pthread_mutex_unlock(&mutex);
}

/* signal handler to exit */
void sighandler(int sigset)
{
	if (sigset == SIGHUP)
		return;
	if (sigset == SIGPIPE)
		return;

	printf("Signal received: %d\n", sigset);

	quit = 1;
}

/* vtimer */
static void vtime_timeout(void *data)
{
        datenklo_t *datenklo = data;

	/* check if there is enough data to read */
	datenklo->vtimeout = 1;
	device_read_available(datenklo->device);
}

/* global init is required for the mutex */
void datenklo_init_global(void)
{
	if (pthread_mutex_init(&mutex, NULL)) {
		LOGP(DDATENKLO, LOGL_ERROR, "Failed to init mutex.\n");
		exit(0);
	}
}

/* init function */
int datenklo_init(datenklo_t *datenklo, const char *dev_name, enum am791x_type am791x_type, uint8_t mc, int auto_rts, double force_tx_baud, double force_rx_baud, int samplerate, int loopback)
{
	int rc = 0;
	tcflag_t flag;
	cc_t *cc;

	LOGP(DDATENKLO, LOGL_DEBUG, "Creating Datenklo instance.\n");

	memset(datenklo, 0, sizeof(*datenklo));

	datenklo->samplerate = samplerate;
	datenklo->loopback = loopback;

	datenklo->mc = mc;
	datenklo->auto_rts = auto_rts;
	datenklo->max_baud = am791x_max_baud(datenklo->mc);
	datenklo->baudrate = datenklo->max_baud;
	datenklo->force_tx_baud = force_tx_baud;
	datenklo->force_rx_baud = force_rx_baud;
	if ((force_tx_baud && force_tx_baud <= 150) || datenklo->max_baud <= 150)
		datenklo->tx_back = 1;
	if ((force_rx_baud && force_rx_baud <= 150) || datenklo->max_baud <= 150)
		datenklo->rx_back = 1;

	/* default termios */
	flag = 0;
	flag |= baud2cflag(datenklo->baudrate);
	flag |= CS8;
	flag |= CREAD;
	datenklo->termios.c_cflag = flag;
	flag = 0;
	flag |= OPOST | ONLCR;
	datenklo->termios.c_oflag = flag;
	cc = datenklo->termios.c_cc;
	cc[VDISCARD] = 017;
//	cc[VDSUSP] = 031;
	cc[VEOF] = 004;
	cc[VEOL] = 0;
	cc[VEOL2] = 0;
	cc[VERASE] = 0177;
	cc[VINTR] = 003;
	cc[VKILL] = 025;
	cc[VLNEXT] = 026;
	cc[VMIN] = 0;
	cc[VQUIT] = 034;
	cc[VREPRINT] = 022;
	cc[VSTART] = 021;
//	cc[VSTATUS] = 024;
	cc[VSTOP] = 023;
	cc[VSUSP] = 032;
	cc[VSWTC] = 0;
	cc[VTIME] = 0;
	cc[VWERASE] = 027;

	datenklo->device = device_init(datenklo, dev_name, dk_open, dk_close, dk_read, dk_write, dk_ioctl_get, dk_ioctl_set, dk_flush_tx, dk_lock, dk_unlock);
	if (!datenklo->device) {
		LOGP(DDATENKLO, LOGL_ERROR, "Failed to attach virtual device '%s' using cuse.\n", dev_name);
		rc = -errno;
		goto error;
	}
	datenklo->revents = POLLOUT;
	device_set_poll_events(datenklo->device, datenklo->revents);

	datenklo->baudrate = cflag2baud(datenklo->termios.c_cflag);
	rc = am791x_init(&datenklo->am791x, datenklo, am791x_type, datenklo->mc, datenklo->samplerate, tx_baud_rate(datenklo), rx_baud_rate(datenklo), cts, bcts, cd, bcd, td, btd, rd, brd);
	if (rc < 0) {
		LOGP(DDATENKLO, LOGL_ERROR, "Failed to initialize AM791X modem chip.\n");
		goto error;
	}

	datenklo->tx_fifo_size = 4097;
	datenklo->tx_fifo_full = 4097; /* poll events disabled if same as size */
	datenklo->rx_fifo_size = 4097;
	datenklo->tx_fifo = calloc(datenklo->tx_fifo_size, 1);
	datenklo->rx_fifo = calloc(datenklo->rx_fifo_size, 1);
	if (!datenklo->tx_fifo || !datenklo->rx_fifo) {
		LOGP(DDATENKLO, LOGL_ERROR, "No mem!\n");
		rc = -ENOMEM;
		goto error;
	}

	osmo_timer_setup(&datenklo->vtimer, vtime_timeout, datenklo);

	rc = uart_init(&datenklo->uart, datenklo, cflag2databits(datenklo->termios.c_cflag), cflag2parity(datenklo->termios.c_cflag), cflag2stopbits(datenklo->termios.c_cflag), tx, rx);
	if (rc < 0) {
		LOGP(DDATENKLO, LOGL_ERROR, "Failed to initialize UART.\n");
		goto error;
	}

	display_wave_init(&datenklo->dispwav, samplerate, dev_name);
	display_measurements_init(&datenklo->dispmeas, samplerate, dev_name);

	datenklo->rx_level_max = samplerate / 20;
	datenklo->dmp_rx_level = display_measurements_add(&datenklo->dispmeas, "Input Level", "%.1f dBm", DISPLAY_MEAS_LAST, DISPLAY_MEAS_LEFT, -50.0, 5.0, 0.0);
	datenklo->dmp_tone_level = display_measurements_add(&datenklo->dispmeas, "Tone Level", "%.1f dBm", DISPLAY_MEAS_LAST, DISPLAY_MEAS_LEFT, -50.0, 5.0, 0.0);
	datenklo->dmp_tone_quality = display_measurements_add(&datenklo->dispmeas, "Tone Quality", "%.1f %%", DISPLAY_MEAS_LAST, DISPLAY_MEAS_LEFT, 0.0, 100.0, 100.0);

	return 0;

error:
	datenklo_exit(datenklo);
	return rc;
}

/* open audio device of one or two datenlo_t instance */
int datenklo_open_audio(datenklo_t *datenklo, const char *audiodev, int buffer, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, const char *read_tx_wave)
{
	int channels = 1;
	int rc;

	/* stereo */
	if (datenklo->slave)
		channels = 2;

	/* size of send buffer in samples */
	datenklo->buffer_size = datenklo->samplerate * buffer / 1000;

#ifdef HAVE_ALSA
	/* init sound */
	datenklo->audio = sound_open(audiodev, NULL, NULL, NULL, channels, 0.0, datenklo->samplerate, datenklo->buffer_size, 1.0, 1.0, 4000.0, 2.0);
	if (!datenklo->audio) {
		LOGP(DDATENKLO, LOGL_ERROR, "No sound device!\n");
		return -EIO;
	}
#endif

	if (write_rx_wave) {
		rc = wave_create_record(&datenklo->wave_rx_rec, write_rx_wave, datenklo->samplerate, channels, 1.0);
		if (rc < 0) {
			LOGP(DDATENKLO, LOGL_ERROR, "Failed to create WAVE recoding instance!\n");
			return rc;
		}
	}
	if (write_tx_wave) {
		rc = wave_create_record(&datenklo->wave_tx_rec, write_tx_wave, datenklo->samplerate, channels, 1.0);
		if (rc < 0) {
			LOGP(DDATENKLO, LOGL_ERROR, "Failed to create WAVE recoding instance!\n");
			return rc;
		}
	}
	if (read_rx_wave) {
		rc = wave_create_playback(&datenklo->wave_rx_play, read_rx_wave, &datenklo->samplerate, &channels, 1.0);
		if (rc < 0) {
			LOGP(DDATENKLO, LOGL_ERROR, "Failed to create WAVE playback instance!\n");
			return rc;
		}
	}
	if (read_tx_wave) {
		rc = wave_create_playback(&datenklo->wave_tx_play, read_tx_wave, &datenklo->samplerate, &channels, 1.0);
		if (rc < 0) {
			LOGP(DDATENKLO, LOGL_ERROR, "Failed to create WAVE playback instance!\n");
			return rc;
		}
	}

	return 0;
}

static int get_char()
{
	struct timeval tv = {0, 0};
	fd_set fds;
	char c = 0;
	int __attribute__((__unused__)) rc;

	FD_ZERO(&fds);
	FD_SET(0, &fds);
	select(0+1, &fds, NULL, NULL, &tv);
	if (FD_ISSET(0, &fds)) {
		rc = read(0, &c, 1);
		return c;
	} else
		return -1;
}

static void display_level(datenklo_t *datenklo, sample_t *samples, int count)
{
	int i;
	sample_t s;

	for (i = 0; i < count; i++) {
		s = *samples++;
		if (s < 0)
			s = -s;
		if (s > datenklo->rx_level_abs)
			datenklo->rx_level_abs = s;
		if (++datenklo->rx_level_count == datenklo->rx_level_max) {
			display_measurements_update(datenklo->dmp_rx_level, level2db(datenklo->rx_level_abs), 0.0);
			datenklo->rx_level_abs = 0.0;
			datenklo->rx_level_count = 0;
		}
	}
}

/* main loop */
void datenklo_main(datenklo_t *datenklo, int loopback)
{
	int num_chan = 1;
	int interval = 1;
	double begin_time, now, sleep;
	struct termios term, term_orig;
	int c;
	int i;

	/* stereo */
	if (datenklo->slave)
		num_chan = 2;

	sample_t buff[num_chan][datenklo->buffer_size], *samples[num_chan];
	uint8_t pbuff[num_chan][datenklo->buffer_size], *power[num_chan];
	for (i = 0; i < num_chan; i++) {
		samples[i] = buff[i];
		power[i] = pbuff[i];
	}
	double rf_level_db[num_chan];
	int count;
	int __attribute__((unused)) rc;

	pthread_mutex_lock(&mutex);

	/* prepare terminal */
	tcgetattr(0, &term_orig);
	term = term_orig;
	term.c_lflag &= ~(ISIG|ICANON|ECHO);
	term.c_cc[VMIN]=1;
	term.c_cc[VTIME]=2;
	tcsetattr(0, TCSANOW, &term);

	/* catch signals */
	signal(SIGINT, sighandler);
	signal(SIGHUP, sighandler);
	signal(SIGTERM, sighandler);
	signal(SIGPIPE, sighandler);

	sound_start(datenklo->audio);

	while (!quit) {
		begin_time = get_time();

		process_auto_rts(datenklo);
		/* process Auto RTS */
		if (num_chan > 1)
			process_auto_rts(datenklo->slave);

		/* Timers may only be processed in main thread, because libosmocore has timer lists for individual threads. */
		if (datenklo->vtimer_us < 0)
			osmo_timer_del(&datenklo->vtimer);
		if (datenklo->vtimer_us > 0)
			osmo_timer_schedule(&datenklo->vtimer, datenklo->vtimer_us / 1000000,datenklo->vtimer_us % 1000000);
		datenklo->vtimer_us = 0;

		am791x_add_del_timers(&datenklo->am791x);

		osmo_select_main(1);

#ifdef HAVE_ALSA
		count = sound_read(datenklo->audio, samples, datenklo->buffer_size, num_chan, rf_level_db);
		if (count < 0) {
			LOGP(DDSP, LOGL_ERROR, "Failed to read RX data from audio device (rc = %d)\n", count);
			if (count == -EPIPE) {
				LOGP(DDATENKLO, LOGL_ERROR, "Trying to recover!\n");
				continue;
			}
			break;
		}
#endif

		/* record received audio to wave */
		if (datenklo->wave_rx_rec.fp)
			wave_write(&datenklo->wave_rx_rec, samples, count);

		/* replace received audio from wave */
		if (datenklo->wave_rx_play.fp)
			wave_read(&datenklo->wave_rx_play, samples, count);

		/* put audio into modem */
		if (!loopback) {
			display_wave(&datenklo->dispwav, samples[0], count, 1);
			display_level(datenklo, samples[0], count);
			am791x_receive(&datenklo->am791x, samples[0], count);
			if (num_chan > 1) {
				display_wave(&datenklo->slave->dispwav, samples[1], count, 1);
				display_level(datenklo->slave, samples[1], count);
				am791x_receive(&datenklo->slave->am791x, samples[1], count);
			}
		}

#ifdef HAVE_ALSA
		count = sound_get_tosend(datenklo->audio, datenklo->buffer_size);
#else
		count = samplerate / 1000;
#endif
		if (count < 0) {
			LOGP(DDSP, LOGL_ERROR, "Failed to get number of samples in buffer (rc = %d)!\n", count);
			if (count == -EPIPE) {
				LOGP(DDATENKLO, LOGL_ERROR, "Trying to recover!\n");
				continue;
			}
			break;
		}

		/* get audio from modem */
		am791x_send(&datenklo->am791x, samples[0], count);
		if (num_chan > 1) {
			am791x_send(&datenklo->slave->am791x, samples[1], count);
		}
		if (loopback) {
			/* copy buffer to preserve original audio for later use */
			sample_t lbuff[num_chan][datenklo->buffer_size];
			memcpy(lbuff, buff, sizeof(lbuff));
			if (loopback == 2 && num_chan == 2) {
				/* swap */
				samples[0] = lbuff[1];
				samples[1] = lbuff[0];
			} else {
				samples[0] = lbuff[0];
				samples[1] = lbuff[1];
			}
			display_wave(&datenklo->dispwav, samples[0], count, 1);
			display_level(datenklo, samples[0], count);
			am791x_receive(&datenklo->am791x, samples[0], count);
			if (num_chan > 1) {
				display_wave(&datenklo->slave->dispwav, samples[1], count, 1);
				display_level(datenklo->slave, samples[1], count);
				am791x_receive(&datenklo->slave->am791x, samples[1], count);
			}
			samples[0] = buff[0];
			samples[1] = buff[1];
		}
		memset(power[0], 1, count);

		/* write generated audio to wave */
		if (datenklo->wave_tx_rec.fp)
			wave_write(&datenklo->wave_tx_rec, samples, count);

		/* replace generated audio from wave */
		if (datenklo->wave_tx_play.fp)
			wave_read(&datenklo->wave_tx_play, samples, count);

#ifdef HAVE_ALSA
		/* write audio */
		rc = sound_write(datenklo->audio, samples, power, count, NULL, NULL, num_chan);
		if (rc < 0) {
			LOGP(DDSP, LOGL_ERROR, "Failed to write TX data to audio device (rc = %d)\n", rc);
			if (rc == -EPIPE) {
				LOGP(DDATENKLO, LOGL_ERROR, "Trying to recover!\n");
				continue;
			}
			break;
		}
#endif

next_char:
		c = get_char();
		switch (c) {
		case 3:
			/* quit */
			printf("CTRL+c received, quitting!\n");
			quit = 1;
			goto next_char;
		case 'w':
			/* toggle wave display */
			display_measurements_on(0);
			display_wave_on(-1);
			goto next_char;
		case 'm':
			/* toggle measurements display */
			display_wave_on(0);
			display_measurements_on(-1);
			goto next_char;
		}

		display_measurements((double)interval / 1000.0);

		now = get_time();

		/* sleep interval */
		sleep = ((double)interval / 1000.0) - (now - begin_time);

		pthread_mutex_unlock(&mutex);
		if (sleep > 0)
			usleep(sleep * 1000000.0);
		pthread_mutex_lock(&mutex);
	}

	/* reset terminal */
	tcsetattr(0, TCSANOW, &term_orig);

	/* reset signals */
	signal(SIGINT, SIG_DFL);
	signal(SIGHUP, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);

	display_measurements_on(0);
	display_wave_on(0);

	pthread_mutex_unlock(&mutex);
}

/* cleanup function */
void datenklo_exit(datenklo_t *datenklo)
{
	LOGP(DDATENKLO, LOGL_DEBUG, "Destroying Datenklo instance.\n");

	osmo_timer_del(&datenklo->vtimer);

	/* exit device */
	if (datenklo->device)
		device_exit(datenklo->device);

#ifdef HAVE_ALSA
	/* exit sound */
	if (datenklo->audio)
		sound_close(datenklo->audio);
#endif

	wave_destroy_record(&datenklo->wave_rx_rec);
	wave_destroy_record(&datenklo->wave_tx_rec);
	wave_destroy_playback(&datenklo->wave_rx_play);
	wave_destroy_playback(&datenklo->wave_tx_play);
}

