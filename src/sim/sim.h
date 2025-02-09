
#define SIM_VERSION_NAME	"TelecardVersion"
#define SIM_VERSION		"4"
#define JOLLY_NAME		"Jolly   Zuhause"
#define JOLLY_PHONE		"04644973171"
#define FUTLN_DEFAULT		"2222001"
#define SICHERUNG_DEFAULT	3103
#define KARTEN_DEFAULT		3
#define SONDER_DEFAULT		0
#define WARTUNG_DEFAULT		65535
#define PIN_DEFAULT		"0000"
#define AUTH_DEFAULT		((uint64_t)0x000000000badefee)

enum l1_state {
	L1_STATE_RESET = 0,	/* reset is held */
	L1_STATE_ATR,		/* answer to reset is sent */
	L1_STATE_IDLE,		/* waiting for message or reset */
	L1_STATE_SEND,		/* sending reply */
	L1_STATE_RECEIVE,	/* receiving message */
	L1_STATE_COMPLETE,	/* received message complete, waiting for card reader to release */
	L1_STATE_GARBAGE,	/* received garbage right after frame, waiting for timeout */
};

enum block_state {
	BLOCK_STATE_ADDRESS = 0,
	BLOCK_STATE_CONTROL,
	BLOCK_STATE_LENGTH,
	BLOCK_STATE_DATA,
};

enum sim_mode {
	SIM_MODE_NONE = 0,	/* SIM from EEPROM */
	SIM_MODE_PHONEBOOK,	/* SIM that contains the phonebook */
	SIM_MODE_PIN,		/* entering SIM via PIN */
	SIM_MODE_ERROR,		/* SIM via PIN failed */
};

#define MAX_PIN_TRY	3
#define MAX_CARDS	8	/* must also be defined at eeprom.h */

typedef struct sim_sim {
	int			card;
	enum l1_state		l1_state;

	/* ATR states */
	int			atr_count;

	/* layer 2 states */
	enum block_state	block_state;
	uint8_t			block_address;
	uint8_t			block_control;
	uint8_t			block_checksum;
	uint8_t			block_count;
	uint8_t			block_rx_data[64];
	uint8_t			block_rx_length;
	uint8_t			block_tx_data[64];
	uint8_t			block_tx_length;
	uint8_t			vs, vr;
	int			reject_count;
	int			resync_sent;

	/* ICL layer states */
	uint8_t			icl_online;
	uint8_t			icl_master;
	uint8_t			icl_chaining;
	uint8_t			icl_error;

	/* layer 7 states */
	uint8_t			addr_src;
	uint8_t			addr_dst;
	uint8_t			sh_appl_count;		/* counts applications for SH_APPL */

	/* CNETZ states */
	uint8_t			pin_required;		/* pin required an not yet validated */
	enum sim_mode		sim_mode;		/* sim mode active (programming mode) */
	char	 		pin_program_futln[9];	/* PIN program mode: FUTLN string with maximum of 8 characters */
	int32_t	 		pin_program_values[4];	/* PIN program mode: other 4 values */
	uint8_t			pin_program_index;	/* PIN program mode: strin to be entered */
	uint8_t			pin_len;		/* length of pin (4 .. 8) */
	uint8_t			pin_try;		/* number of tries left (0 == card locked) */
	uint8_t			app;			/* currently selected APP number */
	uint8_t			app_locked;		/* application locked */
	uint8_t			gebz_locked;		/* metering counter and phonebook locked */
	uint8_t			gebz_full;		/* metering counter full (does this really happen?) */
} sim_sim_t;

/* layer 2 */
enum l2_cmd {
	L2_I,
	L2_REJ,
	L2_RES,
};

/* ICL */
#define ICB1_ONLINE	0x01
#define ICB1_CONFIRM	0x02
#define ICB1_MASTER	0x04
#define ICB1_WT_EXT	0x08
#define ICB1_ABORT	0x10
#define ICB1_ERROR	0x20
#define ICB1_CHAINING	0x40
#define ICB2_BUFFER	0x0f
#define ICB2_DYNAMIC	0x10
#define ICB2_ISO_L2	0x20
#define ICB2_PRIVATE	0x40
#define ICB_EXT		0x80

/* command */
#define CLA_CNTR	0x02
#define SL_APPL		0xf1
#define CL_APPL		0xf2
#define SH_APPL		0xf3

#define CLA_STAT	0x03
#define CHK_KON		0xf1

#define CLA_WRTE	0x04
#define WT_RUFN		0x01

#define CLA_READ	0x05
#define RD_EBDT		0x01
#define RD_RUFN		0x02
#define RD_GEBZ		0x03

#define CLA_EXEC	0x06
#define CHK_PIN		0xf1
#define SET_PIN		0xf2
#define EH_GEBZ		0x01
#define CL_GEBZ		0x02
#define SP_GZRV		0x01
#define FR_GZRV		0x02

#define CLA_AUTO	0x07
#define AUT_1		0x01

/* response */
#define CCRC_PIN_NOK	0x01
#define CCRC_AFBZ_NULL	0x02
#define CCRC_APRC_VALID	0x04
#define CCRC_ERROR	0x40
#define CCRC_IDENT	0x80

#define APRC_PIN_REQ	0x02
#define APRC_APP_LOCKED	0x04
#define APRC_GEBZ_LOCK	0x10
#define APRC_GEBZ_FULL	0x20

/* apps */
#define APP_NETZ_C	3
#define APP_RUFN_GEBZ	4

/* defined for main.c */
size_t eeprom_length(void);

int encode_ebdt(uint8_t *data, const char *futln, int32_t sicherung, int32_t karten, int32_t sonder, int32_t wartung);
void decode_ebdt(uint8_t *data, char *futln, char *sicherung, char *karten, char *sonder, char *wartung);
int directory_size(void);
int save_directory(int location, uint8_t *data);
void load_directory(int location, uint8_t *data);
int encode_directory(uint8_t *data, const char *number, const char *name);
void decode_directory(uint8_t *data, char *number, char *name);

int sim_init_eeprom(void);
void sim_reset(sim_sim_t *sim, int reset);
int sim_rx(sim_sim_t *sim, uint8_t c);
int sim_tx(sim_sim_t *sim);
void sim_timeout(sim_sim_t *sim);
