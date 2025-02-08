
/* number type, includes presentation info */
enum number_type {
	TYPE_NOTAVAIL,
	TYPE_ANONYMOUS,
	TYPE_UNKNOWN,
	TYPE_SUBSCRIBER,
	TYPE_NATIONAL,
	TYPE_INTERNATIONAL,
};

void encode_l16(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void __attribute__((unused)) *arg);
void decode_l16(uint8_t *src_data, int src_len, uint8_t **dst_data, int *dst_len, void __attribute__((unused)) *arg);

int call_init(const char *name, int _send_patterns, int _release_on_disconnect, int use_socket, int argc, const char *argv[], int no_l16, const char *toneset);
void call_exit(void);
int call_handle(void);
void call_media_handle(void);

/* function pointer to delive MNCC messages to upper layer */
extern int (*mncc_up)(uint8_t *buf, int length);
/* MNCC messages from upper layer */
void mncc_down(uint8_t *buf, int length);
/* flush all calls in case of MNCC socket failure */
void mncc_flush(void);

/* received messages */
int call_up_setup(const char *callerid, const char *dialing, uint8_t network, const char *network_id);
void call_up_alerting(int callref);
void call_up_early(int callref);
void call_up_answer(int callref, const char *connect_id);
void call_up_release(int callref, int cause);
void call_tone_recall(int callref, int on);

/* send messages */
int call_down_setup(int callref, const char *caller_id, enum number_type caller_type, const char *dialing);
void call_down_answer(int callref, struct timeval *tv_meter);
void call_down_disconnect(int callref, int cause);
void call_down_release(int callref, int cause);

/* send and receive audio */
void call_up_audio(int callref, sample_t *samples, int count);
void call_down_audio(void *decoder, void *decoder_priv, int callref, uint16_t sequence, uint8_t marker, uint32_t timestamp, uint32_t ssrc, uint8_t *payload, int payload_len);

/* clock to transmit to */
void call_clock(void); /* from main loop */
void call_down_clock(void); /* towards mobile implementation */

/* display call states */
void dump_info(void);

