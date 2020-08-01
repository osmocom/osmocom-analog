
void osmo_cc_convert_cause(struct osmo_cc_ie_cause *cause);
void osmo_cc_convert_cause_msg(osmo_cc_msg_t *msg);
uint8_t osmo_cc_collect_cause(uint8_t old_cause, uint8_t new_cause);

