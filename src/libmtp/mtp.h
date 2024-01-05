
enum mtp_prim {
	MTP_PRIM_POWER_ON,
	MTP_PRIM_EMERGENCY,
	MTP_PRIM_EMERGENCY_CEASES,
	MTP_PRIM_LOCAL_PROCESSOR_OUTAGE,
	MTP_PRIM_LOCAL_PROCESSOR_RECOVERED,
	MTP_PRIM_REMOTE_PROCESSOR_OUTAGE,
	MTP_PRIM_REMOTE_PROCESSOR_RECOVERED,
	MTP_PRIM_START,
	MTP_PRIM_STOP,
	MTP_PRIM_DATA,
	MTP_PRIM_IN_SERVICE,
	MTP_PRIM_OUT_OF_SERVICE,
	MTP_PRIM_SIOS,
	MTP_PRIM_SIO,
	MTP_PRIM_SIN,
	MTP_PRIM_SIE,
	MTP_PRIM_SIPO,
	MTP_PRIM_SIB,
	MTP_PRIM_MSU,
	MTP_PRIM_FISU,
	MTP_PRIM_T1_TIMEOUT,
	MTP_PRIM_T2_TIMEOUT,
	MTP_PRIM_T3_TIMEOUT,
	MTP_PRIM_T4_TIMEOUT,
	MTP_PRIM_CORRECT_SU,
	MTP_PRIM_ABORT_PROVING,
	MTP_PRIM_LINK_FAILURE,
};

#define MTP_CAUSE_ALIGNMENT_TIMEOUT		1
#define MTP_CAUSE_LINK_FAILURE_LOCAL		2
#define MTP_CAUSE_LINK_FAILURE_REMOTE		3
#define MTP_CAUSE_PROVING_FAILURE_LOCAL		4
#define MTP_CAUSE_PROVING_FAILURE_REMOTE	5
#define MTP_CAUSE_PROVING_TIMEOUT		6

enum mtp_l2state {
	MTP_L2STATE_POWER_OFF = 0,
	MTP_L2STATE_OUT_OF_SERVICE,
	MTP_L2STATE_NOT_ALIGNED,
	MTP_L2STATE_ALIGNED,
	MTP_L2STATE_PROVING,
	MTP_L2STATE_ALIGNED_READY,
	MTP_L2STATE_ALIGNED_NOT_READY,
	MTP_L2STATE_IN_SERVICE,
	MTP_L2STATE_PROCESSOR_OUTAGE,
};

struct mtp_msg {
	struct mtp_msg	*next;
	uint8_t		sequence;
	int		transmitted;
	int		len;
	uint8_t		sio;
	uint8_t		data[0];
};

typedef struct mtp {
	/* config */
	const char	*name;		/* instance name (channel) */
	int		bitrate;	/* link bit rate (4.8k or 64k) */
	int		ignore_monitor;	/* ignore link monitoring errors */

	/* layer 2 states */
	enum mtp_l2state l2_state;	/* layer 2 state (link & alignment state) */
	int		local_emergency; /* we request emergency alignment */
	int		remote_emergency; /* remote requests emergency alignment */
	int		local_outage;	/* current local processor outage */
	int		remote_outage;	/* current remote processor outage */
	int		tx_lssu;	/* what LSSU status to transmit (-1 for nothing) */
	struct osmo_timer_list	t1;		/* timer "alignment ready" */
	struct osmo_timer_list	t2;		/* timer "not aligned" */
	struct osmo_timer_list	t3;		/* timer "aligned" */
	struct osmo_timer_list	t4;		/* proving period timer */
	int		proving_try;	/* counts number of proving attempts */
	int		further_proving;/* flag that indicates another proving attempt */

	/* frame transmission */
	uint8_t		tx_frame[272];	/* frame memory */
	int		tx_frame_len;	/* number of bytes in frame */
	int		tx_byte_count;	/* count bytes within frame */
	int		tx_bit_count;	/* count bits within byte */
	int		tx_transmitting;/* transmit frame, if 0: transmit flag */
	uint8_t		tx_byte;	/* current byte transmitting */
	uint8_t		tx_stream;	/* output stream to track bit stuffing */

	/* frame reception */
	uint8_t		rx_frame[272];	/* frame memory */
	int		rx_byte_count;	/* count bytes within frame */
	int		rx_bit_count;	/* count bits within byte */
	int		rx_receiving;	/* receive frame, if 0: no flag yet */
	uint8_t		rx_byte;	/* current byte receiving */
	uint8_t		rx_stream;	/* input stream to track bit stuffing/flag/abort */
	int		rx_flag_count;	/* counter to detect exessively received flags */
	int		rx_octet_counting; /* we are in octet counting mode */
	int		rx_octet_count;	/* counter when performing octet counting */

	/* frame sequencing */
	struct mtp_msg	*tx_queue;	/* head of all messages in queue */
	uint8_t		tx_queue_seq;	/* last sequence assigned to a frame in the queue */
	uint8_t		tx_seq;		/* current sequence number transmitting */
	uint8_t		fib;		/* current FIB */
	uint8_t		rx_seq;		/* last accepted seqeuence number */
	uint8_t		bib;		/* current BIB */
	int		tx_nack;	/* next frame shall send a NAK by inverting BIB */

	/* monitor */
	int		 proving_errors;/* counts errors while proving */
	int		 monitor_errors;/* counts link errors */
	int		 monitor_good;	/* counts good frames */


	/* layer 3 */
	void	(*mtp_receive)(void *inst, enum mtp_prim prim, uint8_t slc, uint8_t *data, int len);
	void		*inst;
	uint8_t		sio;
	uint16_t	local_pc, remote_pc;
} mtp_t;

int mtp_init(mtp_t *mtp, const char *name, void *inst, void (*mtp_receive)(void *inst, enum mtp_prim prim, uint8_t slc, uint8_t *data, int len), int bitrate, int ignore_monitor, uint8_t sio, uint16_t local_pc, uint16_t remote_pc);
void mtp_exit(mtp_t *mtp);
void mtp_flush(mtp_t *mtp);

void mtp_l2_new_state(mtp_t *mtp, enum mtp_l2state state);

int mtp_send(mtp_t *mtp, enum mtp_prim prim, uint8_t slc, uint8_t *data, int len);
int mtp_l3l2(mtp_t *mtp, enum mtp_prim prim, uint8_t sio, uint8_t *data, int len);
void mtp_l2l3(mtp_t *mtp, enum mtp_prim prim, uint8_t sio, uint8_t *data, int len);

uint8_t mtp_send_bit(mtp_t *mtp);
void mtp_receive_bit(mtp_t *mtp, uint8_t bit);

void mtp_send_block(mtp_t *mtp, uint8_t *data, int len);
void mtp_receive_block(mtp_t *mtp, uint8_t *data, int len);

/* overload receive functions to redirect for sniffing */
extern void (*func_mtp_receive_lssu)(mtp_t *mtp, uint8_t fsn, uint8_t bib, uint8_t status);
extern void (*func_mtp_receive_msu)(mtp_t *mtp, uint8_t bsn, uint8_t bib, uint8_t fsn, uint8_t fib, uint8_t sio, uint8_t *data, int len);
extern void (*func_mtp_receive_fisu)(mtp_t *mtp, uint8_t bsn, uint8_t bib, uint8_t fsn, uint8_t fib);

