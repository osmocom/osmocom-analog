/* B-Netz Telegramms (message digits)
 *
 * (C) 2016 by Andreas Eversberg <jolly@eversberg.eu>
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
#include <stdlib.h>
#include <string.h>
#include "telegramm.h"

/* List of message digits */
static struct impulstelegramm impulstelegramme[] = {
	/* Ziffern */
	{ '0',	"0111011000000011", 0x0000, "Ziffer 0" },
	{ '1',	"0111010100000101", 0x0000, "Ziffer 1" },
	{ '2',	"0111010010001001", 0x0000, "Ziffer 2" },
	{ '3',	"0111010001010001", 0x0000, "Ziffer 3" },
	{ '4',	"0111001100000110", 0x0000, "Ziffer 4" },
	{ '5',	"0111001010001010", 0x0000, "Ziffer 5" },
	{ '6',	"0111001001010010", 0x0000, "Ziffer 6" },
	{ '7',	"0111000110001100", 0x0000, "Ziffer 7" },
	{ '8',	"0111000101010100", 0x0000, "Ziffer 8" },
	{ '9',	"0111000011011000", 0x0000, "Ziffer 9" },
	/* Signale */
	{ 's',	"0111001000100010", 0x0000, "Funkwahl ohne Gebuehrenuebermittlung" },
	{ 'S',	"0111000100100100", 0x0000, "Funkwahl mit Gebuehrenuebermittlung" },
	{ 'U',	"0111000010101000", 0x0000, "Funkwahl (unbekannte Variante)" },
	{ 'e',	"0111010000100001", 0x0000, "Funkwahlende" },
	{ 't',	"0111010101010101", 0x0000, "Trennsignal/Schlusssignal" },
	/* Kanalbefehl B1 */
	{ 1001,	"0111011000000101", 0x0000, "Kanalbefehl 1" },
	{ 1002,	"0111011000001001", 0x0000, "Kanalbefehl 2" },
	{ 1003,	"0111011000010001", 0x0000, "Kanalbefehl 3" },
	{ 1004,	"0111011000000110", 0x0000, "Kanalbefehl 4" },
	{ 1005,	"0111011000001010", 0x0000, "Kanalbefehl 5" },
	{ 1006,	"0111011000010010", 0x0000, "Kanalbefehl 6" },
	{ 1007,	"0111011000001100", 0x0000, "Kanalbefehl 7" },
	{ 1008,	"0111011000010100", 0x0000, "Kanalbefehl 8" },
	{ 1009,	"0111011000011000", 0x0000, "Kanalbefehl 9" },
	{ 1010,	"0111010100000011", 0x0000, "Kanalbefehl 10" },
	{ 1011,	"0111001100000101", 0x0000, "Kanalbefehl 11" }, /* 41 */
	{ 1012,	"0111010100001001", 0x0000, "Kanalbefehl 12" },
	{ 1013,	"0111010100010001", 0x0000, "Kanalbefehl 13" },
	{ 1014,	"0111010100000110", 0x0000, "Kanalbefehl 14" },
	{ 1015,	"0111010100001010", 0x0000, "Kanalbefehl 15" },
	{ 1016,	"0111010100010010", 0x0000, "Kanalbefehl 16" },
	{ 1017,	"0111010100001100", 0x0000, "Kanalbefehl 17" },
	{ 1018,	"0111010100010100", 0x0000, "Kanalbefehl 18" },
	{ 1019,	"0111010100011000", 0x0000, "Kanalbefehl 19 (illegal)" },
	{ 1020,	"0111010010000011", 0x0000, "Kanalbefehl 20" },
	{ 1021,	"0111010010000101", 0x0000, "Kanalbefehl 21" },
	{ 1022,	"0111001100001001", 0x0000, "Kanalbefehl 22" }, /* 42 */
	{ 1023,	"0111010010010001", 0x0000, "Kanalbefehl 23" },
	{ 1024,	"0111010010000110", 0x0000, "Kanalbefehl 24" },
	{ 1025,	"0111010010001010", 0x0000, "Kanalbefehl 25" },
	{ 1026,	"0111010010010010", 0x0000, "Kanalbefehl 26" },
	{ 1027,	"0111010010001100", 0x0000, "Kanalbefehl 27" },
	{ 1028,	"0111010010010100", 0x0000, "Kanalbefehl 28" },
	{ 1029,	"0111010010011000", 0x0000, "Kanalbefehl 29" },
	{ 1030,	"0111010001000011", 0x0000, "Kanalbefehl 30" },
	{ 1031,	"0111010001000101", 0x0000, "Kanalbefehl 31" },
	{ 1032,	"0111010001001001", 0x0000, "Kanalbefehl 32" },
	{ 1033,	"0111001100010001", 0x0000, "Kanalbefehl 33" }, /* 43 */
	{ 1034,	"0111010001000110", 0x0000, "Kanalbefehl 34" },
	{ 1035,	"0111010001001010", 0x0000, "Kanalbefehl 35" },
	{ 1036,	"0111010001010010", 0x0000, "Kanalbefehl 36" },
	{ 1037,	"0111010001001100", 0x0000, "Kanalbefehl 37" },
	{ 1038,	"0111010001010100", 0x0000, "Kanalbefehl 38" },
	{ 1039,	"0111010001011000", 0x0000, "Kanalbefehl 39" },
	/* Kanalbefehl B2 */
	{ 1050, "0111001010000011", 0x0000, "Kanalbefehl 50" },
	{ 1051, "0111001010000101", 0x0000, "Kanalbefehl 51" },
	{ 1052, "0111001010001001", 0x0000, "Kanalbefehl 52" },
	{ 1053, "0111001010010001", 0x0000, "Kanalbefehl 53" },
	{ 1054, "0111001010000110", 0x0000, "Kanalbefehl 54" },
	{ 1055, "0111001100001010", 0x0000, "Kanalbefehl 55" }, /* 45 */
	{ 1056, "0111001010010010", 0x0000, "Kanalbefehl 56" },
	{ 1057, "0111001010001100", 0x0000, "Kanalbefehl 57" },
	{ 1058, "0111001010010100", 0x0000, "Kanalbefehl 58" },
	{ 1059, "0111001010011000", 0x0000, "Kanalbefehl 59" },
	{ 1060, "0111001001000011", 0x0000, "Kanalbefehl 60" },
	{ 1061, "0111001001000101", 0x0000, "Kanalbefehl 61" },
	{ 1062, "0111001001001001", 0x0000, "Kanalbefehl 62" },
	{ 1063, "0111001001010001", 0x0000, "Kanalbefehl 63" },
	{ 1064, "0111001001000110", 0x0000, "Kanalbefehl 64" },
	{ 1065, "0111001001001010", 0x0000, "Kanalbefehl 65" },
	{ 1066, "0111001100010010", 0x0000, "Kanalbefehl 66" }, /* 46 */
	{ 1067, "0111001001001100", 0x0000, "Kanalbefehl 67" },
	{ 1068, "0111001001010100", 0x0000, "Kanalbefehl 68" },
	{ 1069, "0111001001011000", 0x0000, "Kanalbefehl 69" },
	{ 1070, "0111000110000011", 0x0000, "Kanalbefehl 70" },
	{ 1071, "0111000110000101", 0x0000, "Kanalbefehl 71" },
	{ 1072, "0111000110001001", 0x0000, "Kanalbefehl 72" },
	{ 1073, "0111000110010001", 0x0000, "Kanalbefehl 73" },
	{ 1074, "0111000110000110", 0x0000, "Kanalbefehl 74" },
	{ 1075, "0111000110001010", 0x0000, "Kanalbefehl 75" },
	{ 1076, "0111000110010010", 0x0000, "Kanalbefehl 76" },
	{ 1077, "0111001100001100", 0x0000, "Kanalbefehl 77" }, /* 47 */
	{ 1078, "0111000110010100", 0x0000, "Kanalbefehl 78" },
	{ 1079, "0111000110011000", 0x0000, "Kanalbefehl 79" },
	{ 1080, "0111000101000011", 0x0000, "Kanalbefehl 80" },
	{ 1081, "0111000101000101", 0x0000, "Kanalbefehl 81" },
	{ 1082, "0111000101001001", 0x0000, "Kanalbefehl 82" },
	{ 1083, "0111000101010001", 0x0000, "Kanalbefehl 83" },
	{ 1084, "0111000101000110", 0x0000, "Kanalbefehl 84" },
	{ 1085, "0111000101001010", 0x0000, "Kanalbefehl 85" },
	{ 1086, "0111000101010010", 0x0000, "Kanalbefehl 86" },
	/* Gruppenfreisignale */
	{ 2001,	"0111000011000101", 0x0000, "Gruppenfreisignal 1" }, /* 91 */
	{ 2002,	"0111000011001001", 0x0000, "Gruppenfreisignal 2" }, /* 92 */
	{ 2003,	"0111000011010001", 0x0000, "Gruppenfreisignal 3" }, /* 93 */
	{ 2004,	"0111000011000110", 0x0000, "Gruppenfreisignal 4" }, /* 94 */
	{ 2005,	"0111000011001010", 0x0000, "Gruppenfreisignal 5" }, /* 95 */
	{ 2006,	"0111000011010010", 0x0000, "Gruppenfreisignal 6" }, /* 96 */
	{ 2007,	"0111000011001100", 0x0000, "Gruppenfreisignal 7" }, /* 97 */
	{ 2008,	"0111000011010100", 0x0000, "Gruppenfreisignal 8" }, /* 98 */
	{ 2009,	"0111000011000011", 0x0000, "Gruppenfreisignal 9" }, /* 90 */
	{ 2010,	"0111000101000011", 0x0000, "Gruppenfreisignal 10" }, /* 80 */
	{ 2011,	"0111000101000101", 0x0000, "Gruppenfreisignal 11" }, /* 81 */
	{ 2012,	"0111000101001001", 0x0000, "Gruppenfreisignal 12" }, /* 82 */
	{ 2013,	"0111000101010001", 0x0000, "Gruppenfreisignal 13" }, /* 83 */
	{ 2014,	"0111000101000110", 0x0000, "Gruppenfreisignal 14" }, /* 84 */
	{ 2015,	"0111000101001010", 0x0000, "Gruppenfreisignal 15" }, /* 85 */
	{ 2016,	"0111000101010010", 0x0000, "Gruppenfreisignal 16" }, /* 86 */
	{ 2017,	"0111000101001100", 0x0000, "Gruppenfreisignal 17" }, /* 87 */
	{ 2018,	"0111000101010100", 0x0000, "Gruppenfreisignal 18" }, /* 88 */
	{ 2019,	"0111000101011000", 0x0000, "Gruppenfreisignal 19 (Kanal Kleiner Leistung)" }, /* 89 */
	{ 0, NULL, 0, NULL }
};

/* prepare telegramms structure */
void bnetz_init_telegramm(void)
{
	int i, j;

	for (i = 0; impulstelegramme[i].digit; i++) {
		uint16_t telegramm = 0;
		for (j = 0; j < (int)strlen(impulstelegramme[i].sequence); j++) {
			telegramm <<= 1;
			telegramm |= (impulstelegramme[i].sequence[j] == '1');
		}
		impulstelegramme[i].telegramm = telegramm;
	}
}

/* Return bit sequence string by given digit. */
struct impulstelegramm *bnetz_digit2telegramm(int digit)
{
	int i;

	for (i = 0; impulstelegramme[i].digit; i++) {
		if (impulstelegramme[i].digit == digit)
			return &impulstelegramme[i];
	}

	return NULL;
}

struct impulstelegramm *bnetz_telegramm2digit(uint16_t telegramm)
{
	int i;

	for (i = 0; impulstelegramme[i].digit; i++) {
		if (impulstelegramme[i].telegramm == telegramm)
			return &impulstelegramme[i];
	}

	return NULL;
}

