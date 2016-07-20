
enum nmt_mt {
	NMT_MESSAGE_1a = 0,
	NMT_MESSAGE_1b,
	NMT_MESSAGE_2a,
	NMT_MESSAGE_2b,
	NMT_MESSAGE_2c,
	NMT_MESSAGE_2d,
	NMT_MESSAGE_2f,
	NMT_MESSAGE_3a,
	NMT_MESSAGE_3b,
	NMT_MESSAGE_3c,
	NMT_MESSAGE_4,
	NMT_MESSAGE_5a,
	NMT_MESSAGE_5b,
	NMT_MESSAGE_6,
	NMT_MESSAGE_7,
	NMT_MESSAGE_8,
	NMT_MESSAGE_10a,
	NMT_MESSAGE_10b,
	NMT_MESSAGE_10c,
	NMT_MESSAGE_11a,
	NMT_MESSAGE_11b,
	NMT_MESSAGE_12,
	NMT_MESSAGE_13a,
	NMT_MESSAGE_13b,
	NMT_MESSAGE_14a,
	NMT_MESSAGE_14b,
	NMT_MESSAGE_15,
	NMT_MESSAGE_16,
	NMT_MESSAGE_20_1,
	NMT_MESSAGE_20_2,
	NMT_MESSAGE_20_3,
	NMT_MESSAGE_20_4,
	NMT_MESSAGE_20_5,
	NMT_MESSAGE_21b,
	NMT_MESSAGE_21c,
	NMT_MESSAGE_22,
	NMT_MESSAGE_25_1,
	NMT_MESSAGE_25_2,
	NMT_MESSAGE_25_3,
	NMT_MESSAGE_25_4,
	NMT_MESSAGE_26,
	NMT_MESSAGE_27,
	NMT_MESSAGE_28,
	NMT_MESSAGE_30,
	NMT_MESSAGE_UKN_MTX,
	NMT_MESSAGE_UKN_B,
};
	
typedef struct frame {
	enum nmt_mt	mt;
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
	uint8_t		seq_number;
	uint16_t	checksum;
} frame_t;

int init_frame(void);

uint64_t nmt_encode_channel(int channel, int power);
int nmt_decode_channel(uint64_t value, int *channel, int *power);
void nmt_value2digits(uint64_t value, char *digits, int num);
uint64_t nmt_digits2value(const char *digits, int num);
char nmt_value2digit(uint64_t value);
uint16_t nmt_encode_area_no(uint8_t area_no);

const char *nmt_frame_name(enum nmt_mt mt);

const char *encode_frame(frame_t *frame, int debug);
int decode_frame(frame_t *frame, const char *bits, enum nmt_direction direction, int callack);

