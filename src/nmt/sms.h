
#define SMS_TYPE_UKNOWN		0x0
#define SMS_TYPE_INTERNATIONAL	0x1
#define SMS_TYPE_NATIONAL	0x2
#define SMS_TYPE_NETWORK	0x3
#define SMS_TYPE_SUBSCRIBER	0x4
#define SMS_TYPE_ALPHANUMERIC	0x5
#define SMS_TYPE_ABBREVIATED	0x6
#define SMS_TYPE_RESERVED	0x7

#define SMS_PLAN_UNKOWN		0x0
#define SMS_PLAN_ISDN_TEL	0x1
#define SMS_PLAN_DATA		0x3
#define SMS_PLAN_TELEX		0x4
#define SMS_PLAN_NATIONAL	0x8
#define SMS_PLAN_PRIVATE	0x9
#define SMS_PLAN_ERMES		0xa
#define SMS_PLAN_RESERVED	0xf

typedef struct sms {
	uint8_t			rx_buffer[1024];		/* data received from MS */
	int			rx_count;			/* number of bytes in buffer */
	int			data_sent;			/* all pending data have been sent and was acked */
	int			mt;				/* mobile terminating SMS */
} sms_t;

int sms_init_sender(nmt_t *nmt);
void sms_cleanup_sender(nmt_t *nmt);
void sms_submit(nmt_t *nmt, uint8_t ref, const char *orig_address, uint8_t orig_type, uint8_t orig_plan, int msg_ref, const char *dest_address, uint8_t dest_type, uint8_t dest_plan, const char *message);
void sms_deliver_report(nmt_t *nmt, uint8_t ref, int error, uint8_t cause);
int sms_deliver(nmt_t *nmt, uint8_t ref, const char *orig_address, uint8_t type, uint8_t plan, time_t timestamp, const char *message);
void sms_release(nmt_t *nmt);
void sms_reset(nmt_t *nmt);

