
#define NMT_MESSAGE_1a	0
#define NMT_MESSAGE_1b	1
#define NMT_MESSAGE_2a	2
#define NMT_MESSAGE_2b	3
#define NMT_MESSAGE_2c	4
#define NMT_MESSAGE_2d	5
#define NMT_MESSAGE_2f	6
#define NMT_MESSAGE_3a	7
#define NMT_MESSAGE_3b	8
#define NMT_MESSAGE_3c	9
#define NMT_MESSAGE_4	10
#define NMT_MESSAGE_5a	11
#define NMT_MESSAGE_5b	12
#define NMT_MESSAGE_6	13
#define NMT_MESSAGE_7	14
#define NMT_MESSAGE_10a	15
#define NMT_MESSAGE_10b	16
#define NMT_MESSAGE_10c	17
#define NMT_MESSAGE_11a	18
#define NMT_MESSAGE_11b	19
#define NMT_MESSAGE_12	20
#define NMT_MESSAGE_13a	21
#define NMT_MESSAGE_13b	22
#define NMT_MESSAGE_14a	23
#define NMT_MESSAGE_14b	24
#define NMT_MESSAGE_15	25
#define NMT_MESSAGE_16	26
#define NMT_MESSAGE_20a	27
#define NMT_MESSAGE_20b	28
#define NMT_MESSAGE_20c	29
#define NMT_MESSAGE_20d	30
#define NMT_MESSAGE_20e	31
#define NMT_MESSAGE_21b	32
#define NMT_MESSAGE_21c	33
#define NMT_MESSAGE_22	34
#define NMT_MESSAGE_25a	35
#define NMT_MESSAGE_25b	36
#define NMT_MESSAGE_25c	37
#define NMT_MESSAGE_25d	38
#define NMT_MESSAGE_26	39
#define NMT_MESSAGE_27	40
#define NMT_MESSAGE_28	41
#define NMT_MESSAGE_30	42

typedef struct frame {
	uint8_t		index;
	uint16_t	channel_no;
	uint16_t	tc_no;
	uint8_t		traffic_area;
	uint8_t		ms_country;
	uint32_t	ms_number;
	uint8_t		tariff_class;
	uint32_t	line_signal;
	uint32_t	digit;
	uint64_t	idle;
	uint8_t		chan_act;
	uint32_t	meas_order;
	uint32_t	meas;
	uint8_t		prefix;
	uint32_t	supervisory;
	uint16_t	ms_password;
	uint8_t		area_info;
	uint64_t	additional_info;
	uint32_t	rand;
	uint64_t	sres;
	uint16_t	limit_strength_eval;
	uint16_t	c;
} frame_t;

int init_frame(void);

uint64_t nmt_encode_channel(int channel, int power);
int nmt_decode_channel(uint64_t value, int *channel, int *power);
void nmt_value2digits(uint64_t value, char *digits, int num);
uint64_t nmt_digits2value(const char *digits, int num);
char nmt_value2digit(uint64_t value);
uint16_t nmt_encode_area_no(uint8_t area_no);

const char *nmt_frame_name(int index);

const char *encode_frame(frame_t *frame, int debug);
int decode_frame(frame_t *frame, const char *bits, enum nmt_direction direction, int callack);

