
	/* login to the network */
#define	TRANS_EM	(1 << 0)	/* attach request received, sending reply */
	/* roaming to different base station/network */
#define	TRANS_UM	(1 << 1)	/* roaming request received, sending reply */
	/* check if phone is still on */
#define	TRANS_MA	(1 << 2)	/* periodic online check, waiting for time slot to send order */
#define	TRANS_MFT	(1 << 3)	/* periodic online check sent, waiting for reply */
	/* mobile originated call */
#define	TRANS_VWG	(1 << 4)	/* received dialing request, waiting for time slot to send dial order */
#define	TRANS_WAF	(1 << 5)	/* dial order sent, waiting for dialing */
#define	TRANS_WBP	(1 << 6)	/* dialing received, waiting for time slot to acknowledge call */
#define	TRANS_WBN	(1 << 7)	/* dialing received, waiting for time slot to reject call */
#define	TRANS_VAG	(1 << 8)	/* establishment of call sent, switching channel */
	/* mobile terminated call */
#define	TRANS_VAK	(1 << 9)	/* establishment of call sent, switching channel */
	/* traffic channel */
#define	TRANS_BQ	(1 << 10)	/* accnowledge channel */
#define	TRANS_VHQ	(1 << 11)	/* hold call */
#define	TRANS_RTA	(1 << 12)	/* hold call and make the phone ring */
#define	TRANS_DS	(1 << 13)	/* establish speech connection */
#define	TRANS_AHQ	(1 << 14)	/* establish speech connection after answer */
	/* release */
#define	TRANS_AF	(1 << 15)	/* release connection by base station */
#define	TRANS_AT	(1 << 16)	/* release connection by mobile station */

typedef struct transaction {
	struct transaction	*next;			/* pointer to next node in list */
	cnetz_t			*cnetz;			/* pointer to cnetz instance */
	int			callref;		/* callref for transaction */
	uint8_t			futln_nat;		/* current station ID (3 values) */
	uint8_t			futln_fuvst;
	uint16_t		futln_rest;
	int			extended;		/* extended frequency capability */
	char			dialing[17];		/* number dialed by the phone */
	int32_t			state;			/* state of transaction */
	int8_t			release_cause;		/* reason for release, (c-netz coding) */
	int			try;			/* counts resending messages */
	int			repeat;			/* counts repeating messages */
	struct timer		timer;			/* for varous timeouts */
	int			mo_call;		/* flags a moile originating call */
	int			mt_call;		/* flags a moile terminating call */
	int			page_failed;		/* failed to get a response from MS */
} transaction_t;

const char *transaction2rufnummer(transaction_t *trans);
transaction_t *create_transaction(cnetz_t *cnetz, uint32_t state, uint8_t futln_nat, uint8_t futln_fuvst, uint16_t futln_rest, int extended);
void destroy_transaction(transaction_t *trans);
void link_transaction(transaction_t *trans, cnetz_t *cnetz);
void unlink_transaction(transaction_t *trans);
transaction_t *search_transaction(cnetz_t *cnetz, uint32_t state_mask);
transaction_t *search_transaction_number(cnetz_t *cnetz, uint8_t futln_nat, uint8_t futln_fuvst, uint16_t futln_rest);
transaction_t *search_transaction_callref(cnetz_t *cnetz, int callref);
void trans_new_state(transaction_t *trans, int state);
void cnetz_flush_other_transactions(cnetz_t *cnetz, transaction_t *trans);
void transaction_timeout(struct timer *timer);

