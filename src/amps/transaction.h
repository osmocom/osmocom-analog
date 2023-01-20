
enum amps_trans_state {
	TRANS_NULL = 0,
	TRANS_REGISTER_ACK,		/* attach request received, waiting to ack */
	TRANS_REGISTER_ACK_SEND,	/* attach request received, sending ack */
	TRANS_CALL_MO_ASSIGN,		/* assigning channel, waiting to send */
	TRANS_CALL_MO_ASSIGN_SEND,	/* assigning channel, sending assignment */
	TRANS_CALL_MO_ASSIGN_CONFIRM,	/* assignment sent, waiting for confirm (SAT) */
	TRANS_CALL_MT_ASSIGN,		/* assigning channel, waiting to send */
	TRANS_CALL_MT_ASSIGN_SEND,	/* assigning channel, sending assignment */
	TRANS_CALL_MT_ASSIGN_CONFIRM,	/* assignment sent, waiting for confirm (SAT) */
	TRANS_CALL_MT_ALERT,		/* ringing the phone, waiting to send alert */
	TRANS_CALL_MT_ALERT_SEND,	/* ringing the phone, sending alert */
	TRANS_CALL_MT_ALERT_CONFIRM,	/* ringing the phone, signaling tone is received */
	TRANS_CALL_MT_ANSWER_WAIT,	/* ringing the phone, waiting for the phone to answer */
	TRANS_CALL_REJECT,		/* rejecting channel, waiting to send */
	TRANS_CALL_REJECT_SEND,		/* rejecting channel, sending reject */
	TRANS_CALL,			/* active call */
	TRANS_CALL_RELEASE,		/* release call towards phone, waiting to send */
	TRANS_CALL_RELEASE_SEND,	/* release call towards phone, sending release */
	TRANS_PAGE,			/* paging phone, waiting to send */
	TRANS_PAGE_SEND,		/* paging phone, sending page order */
	TRANS_PAGE_REPLY,		/* waitring for paging reply */
};

typedef struct transaction {
	struct transaction	*next;			/* pointer to next node in list */
	amps_t			*amps;			/* pointer to amps instance */
	int			callref;		/* call reference */
	int			page_retry;		/* current number of paging (re)try */
	uint32_t		min1;			/* current station ID (2 values) */
	uint16_t		min2;
	uint32_t		esn;			/* ESN */
	uint8_t			msg_type;		/* message type (3 values) */
	uint8_t			ordq;
	uint8_t			order;
	uint16_t		chan;			/* channel to assign */
	int			alert_retry;		/* current number of alter order (re)try */
	char			caller_id[33];		/* id of calling phone */
	char			dialing[33];		/* number dialed by the phone */
	enum amps_trans_state	state;			/* state of transaction */
	struct timer		timer;			/* for varous timeouts */
	int			sat_detected;		/* state if we detected SAT */
	int			dtx;			/* if set, DTX is used with this call */
} transaction_t;

transaction_t *create_transaction(amps_t *amps, enum amps_trans_state trans_state, uint32_t min1, uint16_t min2, uint32_t esn, uint8_t msg_type, uint8_t ordq, uint8_t order, uint16_t chan);
void destroy_transaction(transaction_t *trans);
void link_transaction(transaction_t *trans, amps_t *amps);
void unlink_transaction(transaction_t *trans);
transaction_t *search_transaction(amps_t *amps, uint32_t state_mask);
transaction_t *search_transaction_number(amps_t *amps, uint32_t min1, uint16_t min2);
transaction_t *search_transaction_callref(amps_t *amps, int callref);
void trans_new_state(transaction_t *trans, int state);
void amps_flush_other_transactions(amps_t *amps, transaction_t *trans);
void transaction_timeout(void *data);
const char *trans_short_state_name(int state);

