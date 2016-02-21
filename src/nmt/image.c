#include <stdio.h>
#include <string.h>
#include "image.h"

const char *image[] = {
	"",
	"@g       ______                               @r####@w##@r########",
	"      @g/      \\___   @r__                      @r####@w##@r########",
	"     @g/  Hej!   __\\ @r/  )                     @r####@w##@r########",
	"     @g\\        /   @r/  /       @wNMT            @w##############",
	"      @g\\______/   @r/  /                       @r####@w##@r########",
	"              @y_@G_@r/@G__@r/@y_               @b####@y##@b##@r####@w##@r########",
	"             @y/@G/    \\ @y\\              @b####@y##@b##@r####@w##@r########",
	"            @y/.@G\\____/@y//              @b####@y##@b########",
	"           @y/@G_@r/@G__@r/@y..//               @y##############",
	"          @y/@G/    \\@y.//                @b####@y##@b########",
	"         @y/.@G\\____/@y//         @r####@w|@b#@w|@r#@b####@y##@b########",
	"        @y/@G_@r/@G__@r/@y..//          @r####@w|@b#@w|@r#@b####@y##@b########",
	"       @y/@G/    \\@y.//           @r####@w|@b#@w|@r#######",
	"       @y\\@G\\____/@y//            @b##############",
	"       @r/  /@y__/              @r####@w|@b#@w|@r#######",
	"      @r/  /          @w####@b##@w##@r####@w|@b#@w|@r#######",
	"     @r/  /           @w####@b##@w##@r####@w|@b#@w|@r#######",
	"    @r(__/            @w####@b##@w########",
	"                    @b##############",
	"                    @w####@b##@w########",
	"                    @w####@b##@w########",
	"                    @w####@b##@w########",
	"",
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
				case 'r': /* red */
					printf("\033[0;31m");
					break;
				case 'g': /* gray */
					printf("\033[0;37m");
					break;
				case 'G': /* green */
					printf("\033[0;32m");
					break;
				case 'w': /* white */
					printf("\033[1;37m");
					break;
				case 'y': /* yellow */
					printf("\033[0;33m");
					break;
				case 'b': /* blue */
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

