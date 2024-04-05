/* main function
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

#ifndef ARDUINO

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/time.h>
#include "../liblogging/logging.h"
#include "../liboptions/options.h"
#include "../libserial/serial.h"
#include "../libaaimage/aaimage.h"
#include <osmocom/cc/misc.h>
#include "sim.h"
#include "sniffer.h"
#include "eeprom.h"

int num_kanal = 1;
sim_sniffer_t sim_sniffer;
sim_sim_t sim_sim;
const char *write_pdu_file = NULL;
static int quit = 0;
static const char *serialdev = "/dev/ttyUSB0";
static int baudrate = 9600;

static const char *eeprom_name = NULL;
static const char *futln = NULL;
static const char *sicherung = NULL;
static const char *karten = NULL;
static const char *sonder = NULL;
static const char *wartung = NULL;
static const char *pin = NULL;
#define MAX_DIR_COUNT 64
static int dir_count = 0;
static int dir_location[MAX_DIR_COUNT];
static const char *dir_number[MAX_DIR_COUNT];
static const char *dir_name[MAX_DIR_COUNT];
static const char *auth = NULL;

#define TIMEOUT	0.2

static void print_help(const char *arg0)
{
	printf("Usage: %s [options] <command>\n", arg0);
	/*      -                                                                             - */
	printf("General options:\n");
	printf(" -h --help\n");
	printf("        This help\n");
	printf(" --config [~/]<path to config file>\n");
	printf("        Give a config file to use. If it starts with '~/', path is at home dir.\n");
	printf("        Each line in config file is one option, '-' or '--' must not be given!\n");
	logging_print_help();
	printf(" -s --serial-device <device>\n");
	printf("        Serial device (default = '%s')\n", serialdev);
	printf(" -b --baud-rate <baud>\n");
	printf("        Serial baud rate (default = %d)\n", baudrate);
	printf(" -w --write-pdu <filename>\n");
	printf("        Write PDU to file (sniffer only)\n");
	printf("\nSIM card simulator options:\n");
	printf(" -E --eeprom <name>\n");
	printf("        Stores and reads EEPROM data to/from file. The file is stored at\n");
	printf("        \"~/osmocom/analog/sim_<name>.eeprom\". If the file dos not exit yet,\n");
	printf("        the default values are used. Values are always overwritten with card\n");
	printf("        data, if defined.\n");
	printf(" -F --futln <phone number>\n");
	printf("        Give 7 digits subscriber ID (default = '%s')\n", FUTLN_DEFAULT);
	printf(" --sicherung <security code>\n");
	printf("        Card's security code for simple authentication (default = '%s')\n", SICHERUNG_DEFAULT);
	printf(" --kartenkennung <card ID>\n");
	printf("        Card's ID. Not relevant! (default = '%s')\n", KARTEN_DEFAULT);
	printf(" --sonder <special code>\n");
	printf("        Special codes are used for service cards (default = '%s')\n", SONDER_DEFAULT);
	printf(" --wartung <maitenance code>\n");
	printf("        May define features of service cards (default = '%s')\n", WARTUNG_DEFAULT);
	printf(" -P --pin <pin> | 0000\n");
	printf("        Give 4 .. 8 digits of pin. Use '0000' to disable. (default = '%s')\n", PIN_DEFAULT);
	printf("        This will also reset the PIN error counter and unlocks the card.\n");
	printf(" -D --directory <location> <number> <name> [--directory ...]\n");
	printf("        Give storage location '01' .. '%02d'. To erase give \"\" as number\n", directory_size() - 1);
	printf("        and name. This option can be given multiple times for more entries.\n");
	printf(" -A --authenticate 0x...\n");
	printf("        Give 64 Bit value for authentication response. (default = all bits 1)\n");
	printf("\nCommands are:\n");
	printf("        sniff - To passively sniff SIM card communication\n");
	printf("        sim - To simulate a SIM card\n");
}

#define OPT_SICHERUNG	256
#define OPT_KARTEN	257
#define OPT_SONDER	258
#define OPT_WARTUNG	259

static void add_options(void)
{
	option_add('h', "help", 0);
	option_add('v', "debug", 1);
	option_add('s', "serial-device", 1);
	option_add('b', "baud-rate", 1);
	option_add('w', "write-pdu", 1);
	option_add('E', "eeprom", 1);
	option_add('F', "futln", 1);
	option_add(OPT_SICHERUNG, "sicherung", 1);
	option_add(OPT_KARTEN, "kartenkennung", 1);
	option_add(OPT_SONDER, "sonder", 1);
	option_add(OPT_WARTUNG, "wartung", 1);
	option_add('P', "pin", 1);
	option_add('D', "directory", 3);
	option_add('A', "auth", 1);
};

static int handle_options(int short_option, int argi, char **argv)
{
	int rc;

	switch (short_option) {
	case 'h':
		print_help(argv[0]);
		return 0;
	case 'v':
		rc = parse_logging_opt(argv[argi]);
		if (rc > 0)
			return 0;
		if (rc < 0) {
			fprintf(stderr, "Failed to parse logging option, please use -h for help.\n");
			return rc;
		}
		break;
	case 's':
		serialdev = options_strdup(argv[argi]);
		break;
	case 'b':
		baudrate = atoi(argv[argi]);
		break;
	case 'w':
		write_pdu_file = options_strdup(argv[argi]);
		break;
	case 'E':
		eeprom_name = options_strdup(argv[argi]);
		break;
	case 'F':
		futln = options_strdup(argv[argi]);
		break;
	case OPT_SICHERUNG:
		sicherung = options_strdup(argv[argi]);
		break;
	case OPT_KARTEN:
		karten = options_strdup(argv[argi]);
		break;
	case OPT_SONDER:
		sonder = options_strdup(argv[argi]);
		break;
	case OPT_WARTUNG:
		wartung = options_strdup(argv[argi]);
		break;
	case 'P':
		pin = options_strdup(argv[argi]);
		break;
	case 'D':
		if (dir_count == MAX_DIR_COUNT)
			break;
		dir_location[dir_count] = atoi(argv[argi + 0]);
		dir_number[dir_count] = options_strdup(argv[argi + 1]);
		dir_name[dir_count] = options_strdup(argv[argi + 2]);
		dir_count++;
		break;
	case 'A':
		auth = options_strdup(argv[argi]);
		break;
	default:
		return -EINVAL;
	}

	return 1;
}

/* EERPOM emulation */

static uint8_t eeprom[2048];

uint8_t eeprom_read(enum eeprom_locations loc)
{
	if (loc >= sizeof(eeprom))
		abort();

	return eeprom[loc];
}

void eeprom_write(enum eeprom_locations loc, uint8_t value)
{
	if (loc >= sizeof(eeprom))
		abort();

	eeprom[loc] = value;
}

uint8_t *eeprom_memory(void)
{
	return eeprom;
}

size_t eeprom_length(void)
{
	return sizeof(eeprom);
}

/* main loop for interfacing serial with sim / sniffer */

static int main_loop(serial_t *serial, int sniffer)
{
	int rc, cts = 0, last_cts = 0;
	uint8_t byte;
	int skip_bytes = 0;
	int work = 0;

	struct timeval tv;
	double now, timer = 0;

	quit = 0;

	while (!quit) {
		gettimeofday(&tv, NULL);
		now = (double)tv.tv_usec * 0.000001 + tv.tv_sec;

		/* only check CTS when no work was done
		 * this is because USB query may take some time
		 * and we don't want to block transfer
		 */
		if (!work) {
			cts = serial_cts(serial);
			/* initially AND when CTS becomes 1 (pulled to low by reset line) */
			if (last_cts != cts) {
				if (sniffer == 1)
					sniffer_reset(&sim_sniffer);
				else
					sim_reset(&sim_sim, cts);
				timer = 0;
			}
			last_cts = cts;
		}
		work = 0;

		if (sniffer == 0) {
			rc = sim_tx(&sim_sim);
			if (rc >= 0) {
				byte = rc;
				serial_write(serial, &byte, 1);
				work = 1;
				skip_bytes++;
			}
		}

		rc = serial_read(serial, &byte, 1);
		if (rc > 0)
			work = 1;
		/* ignore while reset is low */
		if (cts)
			continue;
		if (rc == 1) {
			timer = now;
			/* count length, to remove echo from transmission */
			if (!skip_bytes) {
				if (sniffer == 1)
					sniffer_rx(&sim_sniffer, byte);
				else
					sim_rx(&sim_sim, byte);
			} else {
				/* done eliminating TX data, so we reset timer */
				if (--skip_bytes == 0)
					timer = 0;
			}
		} else {
			rc = -1;
			if (timer && now - timer > 12.0 * 5.0 / (double)baudrate) {
				if (sniffer == 1)
					sniffer_timeout(&sim_sniffer);
				else
					sim_timeout(&sim_sim);
				timer = 0;
				skip_bytes = 0;
			}
		}

		if (!work) {
			/* sleep some time if nothing was received */
			usleep(100);
		}
	}

	return quit;
}

static void sighandler(int sigset)
{
	if (sigset == SIGHUP)
		return;
	if (sigset == SIGPIPE)
		return;

	printf("Signal received: %d\n", sigset);

	quit = -1;
}

int main(int argc, char *argv[])
{
	const char *home;
	char eeprom_file[128];
	FILE *fp;
	serial_t *serial = NULL;
	uint8_t ebdt_data[9];
	int rc, argi;
	int sniffer = 0;
	int i;

	loglevel = LOGL_INFO;
	logging_init();

	add_options();
	rc = options_config_file(argc, argv, "~/.osmocom/analog/sim.conf", handle_options);
	if (rc < 0)
		return 0;

	rc = sim_init_eeprom();
	if (rc < 0)
		return rc;

	/* parse command line */
	argi = options_command_line(argc, argv, handle_options);
	if (argi <= 0)
		return argi;

	/* read from eeprom file, if defined and exists */
	if (eeprom_name) {
		/* open config file */
		home = getenv("HOME");
		if (home == NULL)
			return 1;
		sprintf(eeprom_file, "%s/.osmocom/analog/sim_%s.eeprom", home, eeprom_name);
		
		fp = fopen(eeprom_file, "r");
		if (fp) {
			rc = fread(eeprom_memory(), eeprom_length(), 1, fp);
			fclose(fp);
		} else
			LOGP(DOPTIONS, LOGL_INFO, "EEPROM file '%s' does not exist yet.\n", eeprom_file);

	}

	/* check version */
	if (eeprom_read(EEPROM_MAGIC + 0) != 'C' || eeprom_read(EEPROM_MAGIC + 1) != '0' + EEPROM_VERSION) {
		LOGP(DOPTIONS, LOGL_ERROR, "EEPROM file '%s' is not compatible with this version of program, please remove it!\n", eeprom_file);
		return 1;
	}

	/* apply config to eeprom, if defined */
	ebdt_data[0] = eeprom_read(EEPROM_FUTLN_H);
	ebdt_data[1] = eeprom_read(EEPROM_FUTLN_M);
	ebdt_data[2] = eeprom_read(EEPROM_FUTLN_L);
	ebdt_data[3] = eeprom_read(EEPROM_SICH_H);
	ebdt_data[4] = eeprom_read(EEPROM_SICH_L);
	ebdt_data[5] = eeprom_read(EEPROM_SONDER_H);
	ebdt_data[6] = eeprom_read(EEPROM_SONDER_L);
	ebdt_data[7] = eeprom_read(EEPROM_WARTUNG_H);
	ebdt_data[8] = eeprom_read(EEPROM_WARTUNG_L);
	rc = encode_ebdt(ebdt_data, futln, sicherung, karten, sonder, wartung);
	if (rc < 0)
		return 0;
	eeprom_write(EEPROM_FUTLN_H, ebdt_data[0]);
	eeprom_write(EEPROM_FUTLN_M, ebdt_data[1]);
	eeprom_write(EEPROM_FUTLN_L, ebdt_data[2]);
	eeprom_write(EEPROM_SICH_H, ebdt_data[3]);
	eeprom_write(EEPROM_SICH_L, ebdt_data[4]);
	eeprom_write(EEPROM_SONDER_H, ebdt_data[5]);
	eeprom_write(EEPROM_SONDER_L, ebdt_data[6]);
	eeprom_write(EEPROM_WARTUNG_H, ebdt_data[7]);
	eeprom_write(EEPROM_WARTUNG_L, ebdt_data[8]);
	if (pin) {
		if (strlen(pin) < 4 || strlen(pin) > 8) {
			LOGP(DSIM7, LOGL_NOTICE, "Given PIN '%s' has invalid length. (Must be 4 .. 8)\n", pin);
			return 0;
		}
		eeprom_write(EEPROM_FLAGS, (strlen(pin) << EEPROM_FLAG_PIN_LEN) | (MAX_PIN_TRY << EEPROM_FLAG_PIN_TRY));
		for (i = 0; i < (int)strlen(pin); i++)
			eeprom_write(EEPROM_PIN_DATA + i, pin[i]);
	}
	for (i = 0; i < dir_count; i++) {
		uint8_t data[24];
		rc = encode_directory(data, dir_number[i], dir_name[i]);
		if (rc < 0)
			return 0;
		rc = save_directory(dir_location[i], data);
		if (rc < 0)
			return 0;
	}
	if (auth) {
		uint64_t value = strtoull(auth, NULL, 0);
		for (i = 0; i < 8; i++)
			eeprom_write(EEPROM_AUTH_DATA, value >> (8 * (7 - i)));
	}

	if (argi >= argc) {
		fprintf(stderr, "Expecting command, use '-h' for help!\n");
		return 0;
	} else if (!strcmp(argv[argi], "sniff")) {
		sniffer = 1;
	} else if (!strcmp(argv[argi], "sim")) {
		sniffer = 0;
	} else {
		print_help(argv[0]);
		return -EINVAL;
	}

	/* open serial device */
	serial = serial_open(serialdev, baudrate, 8, 'e', 2, 'd', 'd', 0, 1.0, 0.0);
	if (!serial) {
		printf("Serial failed: %s\n", serial_errnostr);
		goto error;
	}

	if (sniffer == 1)
		printf("SIM analyzer ready, please start the phone!\n");
	else {
		char temp[5][16];
		print_aaimage();
		decode_ebdt(ebdt_data, temp[0], temp[1], temp[2], temp[3], temp[4]);
		printf("FUTLN=%s, Sicherungscode=%s, Kartekennung=%s, Sonderheitenschluessel=%s, Wartungsschluessel=%s\n", temp[0], temp[1], temp[2], temp[3], temp[4]);
		for (i = 0; i < 8; i++)
			temp[0][i] = eeprom_read(EEPROM_PIN_DATA + i);
		temp[0][(eeprom_read(EEPROM_FLAGS) >> EEPROM_FLAG_PIN_LEN) & 0xf] = '\0';
		uint64_t auth_value = 0;
		for (i = 0; i < 8; i++)
			auth_value |= eeprom_read(EEPROM_AUTH_DATA + i) << (8 * (7 - i));
		printf("PIN=%s, auth response=0x%016" PRIx64 "\n", temp[0], auth_value);
		printf("Telephone directory has %d entries.\n", directory_size() - 1);
		for (i = 0; i < directory_size() - 1; i++) {
			uint8_t data[24];
			char number[32], name[32];
			load_directory(i + 1, data);
			decode_directory(data, number, name);
			if (number[0])
				printf(" -> %02d %16s %s\n", i + 1, number, name);
		}
		printf("SIM emulator ready, please start the phone!\n");
	}

	/* catch signals */
	signal(SIGINT, sighandler);
	signal(SIGHUP, sighandler);
	signal(SIGTERM, sighandler);
	signal(SIGPIPE, sighandler);

	/* run main loop until terminated by user */
	main_loop(serial, sniffer);

	/* reset signals */
	signal(SIGINT, SIG_DFL);
	signal(SIGHUP, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);

	/* write to eeprom file, if defined */
	if (eeprom_name) {
		fp = fopen(eeprom_file, "w");
		if (fp) {
			fwrite(eeprom_memory(), eeprom_length(), 1, fp);
			fclose(fp);
			LOGP(DOPTIONS, LOGL_INFO, "EEPROM file '%s' written.\n", eeprom_file);
		} else
			LOGP(DOPTIONS, LOGL_INFO, "EEPROM file '%s' cannot be written. (errno = %d)\n", eeprom_file, errno);
	}

error:
	if (serial)
		serial_close(serial);

	options_free();

	return 0;
}

void osmo_cc_set_log_cat(int __attribute__((unused)) cc_log_cat) {}

#endif /* ARDUINO */
