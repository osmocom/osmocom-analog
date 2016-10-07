#include <stdio.h>
#include <string.h>
#include "image.h"

const char *image[] = {
	"@g                                                       _\n"
	"                           @y______________@g             ( )\n"
	"                          @y/              \\@g           / /\n"
	"                         @y(@w Die Mauer ist@y  )@g         / /\n"
	"                          @y\\@w   gefallen!@y  /@g         / /\n"
	"                           @y\\_______   __/@g         / /\n"
	"                                   @y\\ |@g           / /\n"
	"         @wC-NETZ@g                     @y\\|@g          / /\n"
	"                             __________________/_/_\n"
	"                            /         oo          /|\n"
	"                           /        o o o        / |\n"
	"                          /          oo         /  |\n"
	"                         /  ________________   /   |\n"
	"                        /  /   @G021250993@g   /  /    /\n"
	"                       /  /_______________/  /    /\n"
	"                      /  @b______    ______@g   /    /   @c___@g\n"
	"                     /  @b/_@G(@b_@G)@b_/   /_@r(@b_@r)@b_/@g  /    /    @c\\  \\__    @r___/@g\n"
	"                    /  @b____  ____  ____@g   /    /  @c_  )    / @r__/    )@g\n"
	"                   /  @b/_@w1@b_/ /_@w2@b_/ /_@w3@b_/@g  /    /  @c( \\/     \\@r/       |@g\n"
	"                  /  @b____  ____  ____@g   /    /   @c|        @r|         \\@g\n"
	"                 /  @b/_@w4@b_/ /_@w5@b_/ /_@w6@b_/@g  /    /   @c/         @r\\         |@g\n"
	"                /  @b____  ____  ____@g   /    /   @c|   BRD    @r/   DDR    )@g\n"
	"               /  @b/_@w7@b_/ /_@w8@b_/ /_@w9@b_/@g  /    /   @c_|         @r/           |@g\n"
	"              /  @b____  ____  ____@g   /    /    @c\\         @r|            |@g\n"
	"             /  @b/_@w*@b_/ /_@w0@b_/ /_@w#@b_/@g  /    /     @c/        @r/         ___/@g\n"
	"            /                     /    /     @c|         @r\\________/@g\n"
	"           /         o o         /    /      @c\\                \\@g\n"
	"          /_____________________/    /       @c|                 \\@g\n"
	"         |                      |   /         @c\\___              \\_@g\n"
	"         |   =              =   |  /             @c/               /@g\n"
	"         |   =              =   | /             @c/            __ (@g\n"
	"         |______________________|/              @c|___________/  \\)@g\n"
	"@w",
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
				case 'g': /* gray */
					printf("\033[0;37m");
					break;
				case 'G': /* green */
					printf("\033[0;32m");
					break;
				case 'c': /* cyan */
					printf("\033[0;36m");
					break;
				case 'w': /* white */
					printf("\033[1;37m");
					break;
				case 'y': /* yellow */
					printf("\033[0;33m");
					break;
				case 'r': /* red */
					printf("\033[0;31m");
					break;
				case 'b': /* blue */
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

