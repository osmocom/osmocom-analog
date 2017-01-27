typedef struct nmt nmt_t;

struct dms_frame {
	struct dms_frame	*next;
	uint8_t			s;			/* CT/DT frame */
	uint8_t			n;			/* sequence number */
	uint8_t			data[8];		/* data */
};

struct dms_state {
	int			started;		/* did we start conversation ? */
	int			rand_sent;		/* if we sent rand as an initiator? */
	int			established;		/* DT frames after RAND (ack) */
	int			tx_pending;		/* uper layer sent data, but not yet sent and acked */
	uint8_t			n_r;			/* next expected frame to be received */
	uint8_t			n_s;			/* next frame to be sent */
	uint8_t			n_a;			/* next frame to be acked */
	uint8_t			n_count;		/* counts frames that are stored in list */
	uint8_t			dir;			/* direction */
	int			eight_bits;		/* what mode are used for DT frames */
	struct dms_frame	*frame_list;		/* list of frames to transmit */
	int			send_rr;		/* RR must be sent next */
};

typedef struct dms {
	/* DMS transmission */
	int			frame_valid;		/* set, if there is a valid frame in sample buffer */
	sample_t		*frame_spl;		/* 127 * fsk_bit_length */
	int			frame_size;		/* total size of buffer */
	int			frame_pos;		/* current sample position in frame_spl */
	int			frame_length;		/* number of samples currently in frame_spl */
	uint16_t		rx_sync;		/* shift register to detect sync */
	double			rx_sync_level[256];	/* level infos */
	double			rx_sync_quality[256];	/* quality infos */
	int			rx_sync_count;		/* next bit to receive */
	int			rx_in_sync;		/* if we are in sync and receive bits */
	uint8_t			rx_frame[12];		/* receive frame, including label at the start and 3 words crc */
	double			rx_frame_level[12];	/* level info */
	double			rx_frame_quality[12];	/* quality info */
	int			rx_bit_count;		/* next bit to receive */
	int			rx_frame_count;		/* next character to receive */
	struct {
		uint8_t		d;			/* direction */
		uint8_t		s;			/* selection */
		uint8_t		p;			/* prefix */
		uint8_t		n;			/* number of DT frame */
	} rx_label;					/* used while receiving frame */

	/* DMS protocol states */
	struct dms_state	state;
} dms_t;

int dms_init_sender(nmt_t *nmt);
void dms_cleanup_sender(nmt_t *nmt);
int fsk_dms_frame(nmt_t *nmt, sample_t *samples, int length);
void fsk_receive_bit_dms(nmt_t *nmt, int bit, double quality, double level);
void dms_reset(nmt_t *nmt);

void dms_send(nmt_t *nmt, const uint8_t *data, int length, int eight_bits);
void dms_all_sent(nmt_t *nmt);
void dms_receive(nmt_t *nmt, const uint8_t *data, int length, int eight_bits);

