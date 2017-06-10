
typedef struct frame {
	uint8_t		voie;
	uint8_t		channel;
	uint16_t	relais;
	uint8_t		message;
	uint16_t	deport;
	uint16_t	agi;
	uint16_t	sm_power;
	uint16_t	taxe;
	uint8_t		sm_type;
	uint16_t	sm_relais;
	uint16_t	sm_flotte;
	uint16_t	sm_mor;
	uint16_t	sm_mop_demandee;
	uint8_t		chan_assign;
	uint8_t		crins;		/* inscription response DANGER: never set to 3, it will brick the phone! */
	uint16_t	sequence;
	uint16_t	invitation;
	uint8_t		nconv;		/* supervisory digit 0..7 to send via 50 Baud modem */
	uint8_t		digit[10];
} frame_t;

#define REL_TO_SM	0
#define SM_TO_REL	1

const char *param_agi(uint64_t value);
const char *param_aga(uint64_t value);
const char *param_crins(uint64_t value);
const char *r2000_frame_name(int message, int dir);
int decode_frame(frame_t *frame, const char *bits);
const char *encode_frame(frame_t *frame, int debug);

