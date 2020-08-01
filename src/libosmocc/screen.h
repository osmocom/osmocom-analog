
void osmo_cc_help_screen(void);
char *osmo_cc_strtok_quotes(const char **text_p);
int osmo_cc_add_screen(osmo_cc_endpoint_t *ep, const char *text);
void osmo_cc_flush_screen(osmo_cc_screen_list_t *list);
osmo_cc_msg_t *osmo_cc_screen_msg(osmo_cc_endpoint_t *ep, osmo_cc_msg_t *old_msg, int in, const char **routing_p);

