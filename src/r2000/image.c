#include <stdio.h>
#include <string.h>
#include "../libmobile/image.h"

const char *image[] = {
	"",
	"                             @B/                     \\",
	"                            /    @W/             \\@B    \\",
	"                           |    @W/   @R/       \\@W   \\@B    |",
	"                           |   @W|   @R|    @y|@R    |@W   |@B   |",
	"                           |    @W\\   @R\\   @y|@R   /@W   /@B    |",
	"          @W__________        @B\\    @W\\     @y/|\\@W     /@B    /",
	"        @W_(     _____)        @B\\        @y|###|@B        /",
	"       @W(_____  )__                     @yHXH",
	"            @W(_____)                    @y:X:",
	"                                       @y:X:",
	"                                       @yIXI                 @W_________",
	"                                       @yIXI             @W___(      ___)",
	"                                       @yHXH            @W(_       __)",
	"                        @W____           @yHXH              @W(______)",
	"                       @W(_   )_        @y'XXX'",
	"                         @W(____)       @y'XXX'",
	"                                      @y:XXX:",
	"                                      @y:XXX:",
	"                                      @yHXXXH",
	"            @WRadiocom 2000            @y.XXXXX.",
	"                                     @y:XXXXX:",
	"                      @W~            @y_/XXXXXYX\\_",
	"                 @W~                 @y\\#########/",
	"                                   @y/XX/XXX\\XX\\",
	"                                  @y/XX/     \\XX\\       @W~",
	"                                @y_/XX/       \\XX\\_",
	"                               @y|/|X/|~|~|~|~|\\X|\\|            @W~   ~",
	"      @G(###)                    @y###################",
	"  @G(####)(#####())             @y/XX/X\\_X_X_X_X_X/\\XX\\               @G(#)",
	" (#################)         @y/XX/\\/           \\/\\XX\\        @G((####)#######)",
	" (#######)(#########)       @y/XX//               \\\\XX\\    @G(#####))############)",
	"(############)(######)    @y./XX/       @wo @t~@y         \\XX\\.@G(####)###############)",
	"(######)))(############) @y/####\\      @w'O'@y          /####\\@G(()(######)(##########)@W",
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
				case 'r': /* red */
					printf("\033[0;31m");
					break;
				case 'R': /* red */
					printf("\033[1;31m");
					break;
				case 'B': /* blue */
					printf("\033[1;34m");
					break;
				case 'w': /* white */
					printf("\033[0;37m");
					break;
				case 't': /* turquoise */
					printf("\033[0;36m");
					break;
				case 'G': /* green */
					printf("\033[0;32m");
					break;
				case 'W': /* white */
					printf("\033[1;37m");
					break;
				case 'y': /* yellow */
					printf("\033[0;33m");
					break;
				}
			} else
				printf("%c", image[i][j]);
		}
		printf("\n");
	}
	printf("\033[0;39m");
}

