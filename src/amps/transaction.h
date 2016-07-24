
typedef struct amps amps_t;

enum amps_trans_state {
	TRANS_NULL = 0,
	TRANS_REGISTER_ACK,		/* attach request received, waiting to ack */
	TRANS_REGISTER_ACK_SEND,	/* attach request received, sending ack */
	TRANS_CALL_MO_ASSIGN,		/* assigning channel, waiting to send */
	TRANS_CALL_MO_ASSIGN_SEND,	/* assigning channel, sending assignment */
	TRANS_CALL_MT_ASSIGN,		/* assigning channel, waiting to send */
	TRANS_CALL_MT_ASSIGN_SEND,	/* assigning channel, sending assignment */
	TRANS_CALL_MT_ALERT,		/* ringing the phone, sending alert order until signaling tone is received */
	TRANS_CALL_MT_ALERT_SEND,	/* ringing the phone, signaling tone is received */
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
	uint32_t		min1;			/* current station ID (2 values) */
	uint16_t		min2;
	uint8_t			msg_type;		/* message type (3 values) */
	uint8_t			ordq;
	uint8_t			order;
	uint16_t		chan;			/* channel to assign */
	char			dialing[33];		/* number dialed by the phone */
	enum amps_trans_state	state;			/* state of transaction */
	struct timer		timer;			/* for varous timeouts */
	int			sat_detected;		/* state if we detected SAT */
} transaction_t;

transaction_t *create_transaction(amps_t *amps, enum amps_trans_state trans_state, uint32_t min1, uint16_t min2, uint8_t msg_type, uint8_t ordq, uint8_t order, uint16_t chan);
void destroy_transaction(transaction_t *trans);
void link_transaction(transaction_t *trans, amps_t *amps);
void unlink_transaction(transaction_t *trans);
transaction_t *search_transaction(amps_t *amps, uint32_t state_mask);
transaction_t *search_transaction_number(amps_t *amps, uint32_t min1, uint16_t min2);
void trans_new_state(transaction_t *trans, int state);
void amps_flush_other_transactions(amps_t *amps, transaction_t *trans);
void transaction_timeout(struct timer *timer);

