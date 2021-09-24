
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "aaimage.h"

extern const char *aaimage[];

void print_aaimage(void)
{
	int i, j;

	for (i = 0; aaimage[i]; i++) {
		for (j = 0; j < (int)strlen(aaimage[i]); j++) {
			if (aaimage[i][j] == '@') {
				j++;
				switch(aaimage[i][j]) {
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
					printf("This will not work on some terminals, please use 'w'\n");
					abort();
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
				printf("%c", aaimage[i][j]);
		}
		printf("\n");
	}
	printf("\033[0;39m");
}

