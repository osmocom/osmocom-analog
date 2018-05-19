
void option_add(int short_option, const char *long_option, int parameter_count);
int options_config_file(const char *config_file, int (*handle_options)(int short_option, int argi, char *argv[]));
int options_command_line(int argc, char *argv[], int (*handle_options)(int short_option, int argi, char *argv[]));
int option_is_first(void);

