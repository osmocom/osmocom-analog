
const char *print_message(const char *message, int message_length);
int64_t get_codeword(pocsag_t *pocsag);
void put_codeword(pocsag_t *pocsag, uint32_t word, int8_t slot, int8_t subslot);

