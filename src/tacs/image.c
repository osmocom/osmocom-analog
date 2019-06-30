#include <stdio.h>
#include <string.h>
#include "../libmobile/image.h"

const char *image[] = {
	"",
	"               @B(@g#@B)",
	"                @g#",
	"                #",
	"                #",
	"                #   @R_____",
	"               _@g#@R--- _._ ---__",
	"              |_____(_|_)_____|",
	"               |  @WTELEPHONE@R  |",
	"              |###############|",
	"              |###############|",
	"              |##   |   |   ##|",
	"              |## @B1@R | @B2@R | @B3@R ##|",
	"              |##___|___|___##|       @WTACS@R",
	"              |##   |   |   ##|",
	"              |## @B4@R | @B5@R | @B6@R ##|",
	"              |##___|___|___##|",
	"              |##   |   |   ##|",
	"              |## @B7@R | @B8@R | @B9@R ##|",
	"              |##___|___|___##|",
	"              |##   |   |   ##|",
	"              |## @B*@R | @B0@R | @B#@R ##|",
	"              |##___|___|___##|",
	"              |###############|",
	"              |##+---------+##|",
	"              |##+---------+##|",
	"              |###############|",
	"              @g+++++++++++++++++",
	"@W",
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
				case 'R': /* red */
					printf("\033[1;31m");
					break;
				case 'g': /* gray */
					printf("\033[0;37m");
					break;
				case 'W': /* white */
					printf("\033[1;37m");
					break;
				case 'B': /* blue */
					printf("\033[1;34m");
					break;
				}
			} else
				printf("%c", image[i][j]);
		}
		printf("\n");
	}
	printf("\033[0;39m");
}

