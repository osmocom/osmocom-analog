/* Zeitansage main
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
#include "../libdebug/debug.h"
#include "../libmobile/call.h"
#include "../libmobile/main_mobile.h"
#include "../liboptions/options.h"
#include "zeitansage.h"
#include "samples.h"

double audio_level_dBm = -16.0;
int alerting = 0;


void print_help(const char *arg0)
{
	main_mobile_print_help(arg0, "-c hw:0,0 ");
	/*      -                                                                             - */
	printf(" -G --gain\n");
	printf("        Gain of audio level relative to 1 mW on a 600 Ohm line. (default = %.0f)\n", audio_level_dBm);
	printf(" -A --alerting\n");
	printf("        Play as early audio while alerting. Don't send a connect.\n");
	main_mobile_print_hotkeys();
}

static void add_options(void)
{
	main_mobile_add_options();
	option_add('G', "gain", 1);
	option_add('A', "alerting", 0);
}

static int handle_options(int short_option, int argi, char **argv)
{
	switch (short_option) {
	case 'G':
		audio_level_dBm = atof(argv[argi]);
		break;
	case 'A':
		alerting = 1;
		send_patterns = 0;
		break;
	default:
		return main_mobile_handle_options(short_option, argi, argv);
	}

	return 1;
}

static const struct number_lengths number_lengths[] = {
	{ 0, "no number" },
	{ 4, "number '1191'" },
	{ 0, NULL },
};

int main(int argc, char *argv[])
{
	int rc, argi;

	allow_sdr = 0;

	/* init system specific tones */
	init_samples();

	/* init mobile interface */
	main_mobile_init("0123456789", number_lengths, NULL, NULL);

	/* handle options / config file */
	add_options();
	rc = options_config_file(argc, argv, "~/.osmocom/analog/zeitansage.conf", handle_options);
	if (rc < 0)
		return 0;
	argi = options_command_line(argc, argv, handle_options);
	if (argi <= 0)
		return argi;

	/* inits */
	fm_init(fast_math);
	zeit_init(audio_level_dBm, alerting);

	main_mobile_loop("zeitansage", &quit, NULL, "1191");

//fail:
	/* exits */
	zeit_exit();
	fm_exit();

	options_free();

	return 0;
}

