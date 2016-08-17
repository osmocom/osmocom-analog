
/* info about subscriber */
typedef struct nmt_subscriber {
	/* NOTE: country must be followed by number, so both represent a string */
	char			country;		/* country digit */
	char			number[7];		/* phone suffix */
	char			password[4];		/* phone's password + '\0' */
	int			coinbox;		/* phone is a coinbox and accept tariff information */
} nmt_subscriber_t;

/* transaction node */
typedef struct transaction {
	struct transaction	*next;			/* pointer to next node in list */
	nmt_t			*nmt;			/* pointer to nmt instance, if bound to a channel */
	int			callref;		/* callref for transaction */
	struct nmt_subscriber	subscriber;
	struct timer		timer;
	int			page_try;		/* number of paging try */

	/* caller ID */
	char			caller_id[33];		/* caller id digits */
	enum number_type	caller_type;		/* caller id type */

	/* SMS */
	char			sms_string[256];	/* current string to deliver */
} transaction_t;

transaction_t *create_transaction(struct nmt_subscriber *subscriber);
void destroy_transaction(transaction_t *trans);
transaction_t *get_transaction_by_callref(int callref);
transaction_t *get_transaction_by_number(struct nmt_subscriber *subscr);

