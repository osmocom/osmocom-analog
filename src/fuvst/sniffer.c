/* FuVSt Sniffer
 *
 * (C) 2020 by Andreas Eversberg <jolly@eversberg.eu>
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
#include <string.h>
#include <errno.h>
#include "../libsample/sample.h"
#include <osmocom/core/timer.h>
#include <osmocom/core/utils.h>
#include "../liblogging/logging.h"
#include "../libmobile/call.h"
#include "../libmobile/main_mobile.h"
#include "../liboptions/options.h"
#include "../libmobile/sender.h"
#include "../libv27/modem.h"
#include "../libmtp/mtp.h"
#include "../libfm/fm.h"

typedef struct sniffer {
	sender_t		sender;
	v27modem_t		modem;
	mtp_t			mtp;
	uint8_t			last_fsn;
} sniffer_t;


void print_help(const char *arg0)
{
	main_mobile_print_help(arg0, "");
	/*      -                                                                             - */
	printf("Use '-k BS -k BSC' to trace both sides of SS7 link, using a stereo input.\n");
	printf("Use '-v 0' for total message logging, including errors.\n");
	printf("Use '-v 1' for all messages, including resends (and weird messages from SAE).\n");
	printf("Use '-v 2' for messages between DKO and MSC.\n");
	main_mobile_print_hotkeys();
}

static void add_options(void)
{
	main_mobile_add_options();
}

static int handle_options(int short_option, int argi, char **argv)
{
	switch (short_option) {
	default:
		return main_mobile_handle_options(short_option, argi, argv);
	}

	return 1;
}

/* FISU is received form L2 */
static void receive_fisu(mtp_t *mtp, uint8_t bsn, uint8_t bib, uint8_t fsn, uint8_t fib)
{
	sniffer_t *sniffer = (sniffer_t *)mtp->inst;

	LOGP(DMTP3, (fsn == sniffer->last_fsn) ? LOGL_INFO : LOGL_NOTICE, "%s FISU Frame: FSN=%d FIB=%d BSN=%d BIB=%d\n", mtp->name, fsn, fib, bsn, bib);

	/* store current FSN */
	sniffer->last_fsn = fsn;
}

/* LSSU is received form L2 */
static void receive_lssu(mtp_t *mtp, uint8_t fsn, uint8_t bib, uint8_t status)
{
	sniffer_t *sniffer = (sniffer_t *)mtp->inst;

	LOGP(DMTP3, LOGL_INFO, "%s LSSU Frame: FSN=%d BIB=%d status=%d\n", mtp->name, fsn, bib, status);

	/* store initial FSN */
	sniffer->last_fsn = fsn;
}

/* MSU is received form L2 */
static void receive_msu(mtp_t *mtp, uint8_t bsn, uint8_t bib, uint8_t fsn, uint8_t fib, uint8_t sio, uint8_t *data, int len)
{
	sniffer_t *sniffer = (sniffer_t *)mtp->inst;
	uint16_t dcp, ocp;
	uint8_t slc, h2h1;
	uint8_t ident, opcode;

	if (len < 4) {
		LOGP(DMTP3, LOGL_NOTICE, "Short frame from layer 2 (len=%d)\n", len);
		return;
	}

	if (fsn == sniffer->last_fsn) {
		LOGP(DMTP3, LOGL_INFO, "%s MSU Frame: FSN=%d FIB=%d BSN=%d BIB=%d data: %02x %s\n", mtp->name, fsn, fib, bsn, bib, sio, osmo_hexdump(data, len));
		return;
	}

	if (len < 6) {
		LOGP(DMTP3, LOGL_NOTICE, "Frame from layer 2 too short to carry an Opcode (len=%d)\n", len);
		return;
	}

	/* parse header */
	dcp = data[0];
	dcp |= (data[1] << 8) & 0x3f00;
	ocp = data[1] >> 6;
	ocp |= data[2] << 2;
	ocp |= (data[3] << 10) & 0x3c00;
	slc = data[3] >> 4;
	h2h1 = data[4];
	ident = (h2h1 << 4) | slc;
	opcode = (data[5] >> 4) | (data[5] << 4);
	data += 6;
	len -= 6;

	if (sio == 0xcd)
		LOGP(DMTP3, LOGL_NOTICE, "%s MuP Frame: FSN=%d FIB=%d BSN=%d BIB=%d SIO=0x%02x DCP=%d OCP=%d Ident=0x%02x OP=%02XH %s\n", mtp->name, fsn, fib, bsn, bib, sio, dcp, ocp, ident, opcode, osmo_hexdump(data, len));
	else
		LOGP(DMTP3, LOGL_NOTICE, "%s MSU Frame: FSN=%d FIB=%d BSN=%d BIB=%d SIO=0x%02x DCP=%d OCP=%d SLC=%d H2/H1=0x%02x %02x %s\n", mtp->name, fsn, fib, bsn, bib, sio, dcp, ocp, slc, h2h1, data[-1], osmo_hexdump(data, len));

	/* store current FSN */
	sniffer->last_fsn = fsn;
}

/* a bit is sent to the modem */
static int send_bit(void *inst)
{
	sniffer_t *sniffer = (sniffer_t *)inst;

	if (sniffer->sender.loopback)
		return mtp_send_bit(&sniffer->mtp);
	else
		return 0;
}

/* a bit is received from the modem */
static void receive_bit(void *inst, int bit)
{
	sniffer_t *sniffer = (sniffer_t *)inst;

	mtp_receive_bit(&sniffer->mtp, bit);
}

/* Destroy transceiver instance and unlink from list. */
static void sniffer_destroy(sender_t *sender)
{
	sniffer_t *sniffer = (sniffer_t *) sender;

	LOGP(DCNETZ, LOGL_DEBUG, "Destroying 'Sniffer' instance for 'Kanal' = %s.\n", sender->kanal);

	mtp_exit(&sniffer->mtp);

	v27_modem_exit(&sniffer->modem);

	sender_destroy(&sniffer->sender);
	free(sniffer);
}

int main(int argc, char *argv[])
{
	int rc, argi;
	int i = 0;
	sniffer_t *sniffer;

	/* forward L2 messages here */
	func_mtp_receive_fisu = receive_fisu;
	func_mtp_receive_lssu = receive_lssu;
	func_mtp_receive_msu = receive_msu;

	/* init mobile interface */
	allow_sdr = 0;
	uses_emphasis = 0;
	check_channel = 0;
	main_mobile_init(NULL, NULL, NULL, NULL, NULL);

	/* handle options / config file */
	add_options();
	argi = options_command_line(argc, argv, handle_options);
	if (argi <= 0)
		return argi;

	/* inits */
	fm_init(fast_math);

	if (!num_kanal) {
		printf("No channel (\"Kanal\") is specified, I suggest to add two channels 'MSC' and 'BS'.\n\n");
		goto fail;
	}

	if (num_device <= 1)
		dsp_device[1] = dsp_device[0];
	for (i = 0; i < num_kanal; i++) {
		LOGP(DCNETZ, LOGL_DEBUG, "Creating 'Sniffer' instance for 'Kanal' = %s (sample rate %d).\n", kanal[i], dsp_samplerate);

		sniffer = calloc(1, sizeof(sniffer_t));
		if (!sniffer) {
			LOGP(DCNETZ, LOGL_ERROR, "No memory!\n");
			goto fail;
		}
		rc = sender_create(&sniffer->sender, kanal[i], 131, 131, dsp_device[i], 0, dsp_samplerate, rx_gain, tx_gain, 0, 0, write_rx_wave, write_tx_wave, read_rx_wave, read_tx_wave, loopback, PAGING_SIGNAL_NONE);
		if (rc < 0) {
			fprintf(stderr, "Failed to create \"Sniffer\" instance. Quitting!\n");
			goto fail;
		}

		rc = mtp_init(&sniffer->mtp, kanal[i], sniffer, NULL, 4800, 1, 0, 0, 0);
		if (rc < 0)
			goto fail;

		sender_set_fm(&sniffer->sender, 1.0, 4000.0, 1.0, 1.0);

		rc = v27_modem_init(&sniffer->modem, sniffer, send_bit, receive_bit, dsp_samplerate, 1);
		if (rc < 0)
			goto fail;
	}

	main_mobile_loop(NULL, &quit, NULL, NULL);

fail:
	/* destroy transceiver instance */
	while (sender_head)
		sniffer_destroy(sender_head);

	/* exits */
	main_mobile_exit();
	fm_exit();

	return 0;
}

/* don't send anything */
void sender_send(sender_t __attribute__((unused)) *sender, sample_t *samples, uint8_t *power, int length)
{
        memset(power, 0, length);

	memset(samples, 0, sizeof(*samples) * length);
}

/* we receive everything */
void sender_receive(sender_t *sender, sample_t *samples, int length, double __attribute__((unused)) rf_level_db)
{
	sniffer_t *sniffer = (sniffer_t *) sender;

	v27_modem_receive(&sniffer->modem, samples, length);
}

void call_down_audio(void __attribute__((unused)) *decoder, void __attribute__((unused)) *decoder_priv, int __attribute__((unused)) callref, uint16_t __attribute__((unused)) sequence, uint8_t __attribute__((unused)) marker, uint32_t __attribute__((unused)) timestamp, uint32_t __attribute__((unused)) ssrc, uint8_t __attribute__((unused)) *payload, int __attribute__((unused)) payload_len) { }

void call_down_clock(void) {}

int call_down_setup(int __attribute__((unused)) callref, const char __attribute__((unused)) *caller_id, enum number_type __attribute__((unused)) caller_type, const char __attribute__((unused)) *dialing) { return 0; }

void call_down_answer(int __attribute__((unused)) callref, struct timeval __attribute__((unused)) *tv_meter) { }

void call_down_disconnect(int __attribute__((unused)) callref, int __attribute__((unused)) cause) { }

void call_down_release(int __attribute__((unused)) callref, int __attribute__((unused)) cause) { }

const char *aaimage[] = { NULL };

void dump_info(void) {}

