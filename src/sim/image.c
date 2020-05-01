#ifndef ARDUINO

#include <stdio.h>
#include <string.h>
#include "../libmobile/image.h"

const char *image[] = {
	"@w",
	"                         ()",
	"                        //    _______________________________________________",
	"                       //    /                                               \\",
	"     @WC-NETZ SIM@w       //    |                                                 |",
	"           __________//_    |                  @WJ o l l y ' s@w                  |",
	"          /    o o    /|    |                                                 |",
	"         /__________ / |    |    @Y  _ __ _  @w                                   |",
	"        //_________//  /    | @bVCC@Y (_)__(_) @bGND@w                                |",
	"       /@B_@g()@B_/ /_@r()@B_@w/  /     | @bRES@Y (_)__(_) @w                                   |",
	"      /@B_@W1@B_/_@W2@B_/_@W3@B_@w/  /      | @bCLK@Y (_)__(_) @bI/O@w                                |",
	"     /@B_@W4@B_/_@W5@B_/_@W6@B_@w/  /       |                                                 |",
	"    /@B_@W7@B_/_@W8@B_/_@W9@B_@w/  /        |                                                 |",
	"   /@B_@W*@B_/_@W0@B_/_@W#@B_@w/  /         |  @y/|_____@w                                        |",
	"  /___________/  /          | @y/       @w       @WT e l e K a r t e@w                |",
	" |  _      _  | /           | @y\\  _____@w                                        |",
	" |____________|/            |  @y\\|     @w                                        |",
	"                             \\_______________________________________________/",

	"",
	NULL
};

void print_image(void)
{
	int i, j;

	for (i = 0; image[i]; i++) {
		for (j = 0; j < (int)strlen(image[i]); j++) {
			if (image[i][j] == '@') {
				j++;
				switch(image[i][j]) {
				case 'k': /* black */
					printf("\033[0;30m");
					break;
				case 'r': /* red */
					printf("\033[0;31m");
					break;
				case 'g': /* green */
					printf("\033[0;32m");
					break;
				case 'y': /* yellow */
					printf("\033[0;33m");
					break;
				case 'b': /* blue */
					printf("\033[0;34m");
					break;
				case 'm': /* magenta */
					printf("\033[0;35m");
					break;
				case 'c': /* cyan */
					printf("\033[0;36m");
					break;
				case 'w': /* white */
					printf("\033[0;37m");
					break;
				case 'K': /* bright black */
					printf("\033[1;30m");
					break;
				case 'R': /* bright red */
					printf("\033[1;31m");
					break;
				case 'G': /* bright green */
					printf("\033[1;32m");
					break;
				case 'Y': /* bright yellow */
					printf("\033[1;33m");
					break;
				case 'B': /* bright blue */
					printf("\033[1;34m");
					break;
				case 'M': /* bright magenta */
					printf("\033[1;35m");
					break;
				case 'C': /* bright cyan */
					printf("\033[1;36m");
					break;
				case 'W': /* bright white */
					printf("\033[1;37m");
					break;
				}
			} else
				printf("%c", image[i][j]);
		}
		printf("\n");
	}
	printf("\033[0;39m");
}

#endif /* ARDUINO */
