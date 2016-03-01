#include <stdio.h>
#include <string.h>
#include "image.h"

const char *image[] = {
	"@w",
	"",
	"                             A-NETZ",
	"@g                                         /",
	"           @w~@g                            /",
	"                @w~@g                      /                      @G/|\\@g",
	"       @w~@g                   ___________/_______               @G//|\\\\@g",
	"   @G/|\\@g                    /|       |        |\\\\           @w~@g  @G//|\\\\@g",
	"@B___@G/|\\@B___________________@g/ |       |        | \\\\@B_____________@G//|\\\\@B__",
	"  @G//|\\\\@g  _/_____________/_(|_______|________|__\\\\________   @G///|\\\\\\@g",
	"  @G//|\\\\@g (                         -            -         \\  @G///|\\\\\\@g",
	"   @G_|_@g  |     _____                           _____       ) @G/  |  \\@g",
	"        =____/@b/   \\@g\\_________________________/@b/   \\@g\\______=   @G_|_",
	"@w_____________@b( (@w*@b) )@w_________________________@b( (@w*@b) )@w________________",
	"              @b\\___/@w                           @b\\___/@w",
	"  =====      ======      ======      ======      ======      ======",
	"",
	"____________________________________________________________________",
	NULL
};

void print_image(void)
{
	int i, j;

	for (i = 0; image[i]; i++) {
		for (j = 0; j < strlen(image[i]); j++) {
			if (image[i][j] == '@') {
				j++;
				switch(image[i][j]) {
				case 'g': /* gray */
					printf("\033[0;37m");
					break;
				case 'G': /* green */
					printf("\033[0;32m");
					break;
				case 'w': /* white */
					printf("\033[1;37m");
					break;
				case 'b': /* brown (yellow) */
					printf("\033[0;33m");
					break;
				case 'B': /* blue */
					printf("\033[0;34m");
					break;
				}
			} else
				printf("%c", image[i][j]);
		}
		printf("\n");
	}
	printf("\033[0;39m");
}

