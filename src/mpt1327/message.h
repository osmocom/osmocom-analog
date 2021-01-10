
#define IDENT_ALLI	8191	/* System-wide ident */
#define IDENT_TSCI	8190	/* Ident of TSC */
#define IDENT_IPFIXI	8189	/* Interprefix ident */
#define IDENT_SDMI	8188	/* Short data message ident */
#define IDENT_DIVERTI	8187	/* Divert ident */
#define IDENT_INCI	8186	/* Include ident */
#define IDENT_REGI	8185	/* Registration ident */
#define IDENT_PSTNSI1	8121	/* Short-form PSTN idents */
#define IDENT_NETSI1	8121	/* Short-form data Network idents */
#define IDENT_DNI	8103	/* Data Network gateway ident */
#define IDENT_PABXI	8102	/* PABX gateway ident */
#define IDENT_PSTNGI	8101	/* General PSTN gateway ident */
#define IDENT_DUMMYI	0	/* Dummy ident */

#define OPER_PRESSEL_ON		0
#define OPER_PRESSEL_OFF	1
#define OPER_PERIODIC		2
#define OPER_DISCONNECT		3
#define OPER_SPARE		4
#define OPER_RESERVED		5
#define OPER_CLEAR		6
#define OPER_DISABLE		7

enum mpt1327_codeword_dir {
	MPT_DOWN,
	MPT_UP,
	MPT_BOTH,
};

enum mpt1327_codeword_type {
	MPT_FILLER = 0,
	MPT_GTC,
	MPT_ALH,
	MPT_ALHS,
	MPT_ALHD,
	MPT_ALHE,
	MPT_ALHR,
	MPT_ALHX,
	MPT_ALHF,
	MPT_ACK,
	MPT_ACKI,
	MPT_ACKQ,
	MPT_ACKX,
	MPT_ACKV,
	MPT_ACKE,
	MPT_ACKT,
	MPT_ACKB,
	MPT_RQS,
	MPT_RQSpare,
	MPT_RQX,
	MPT_RQT,
	MPT_RQE,
	MPT_RQR,
	MPT_RQQ,
	MPT_RQC,
	MPT_AHY,
	MPT_AHYSpare,
	MPT_AHYX,
	MPT_AHYP,
	MPT_AHYQ,
	MPT_AHYC,
	MPT_MARK,
	MPT_MAINT,
	MPT_CLEAR,
	MPT_MOVE,
	MPT_BCAST0,
	MPT_BCAST1,
	MPT_BCAST2,
	MPT_BCAST3,
	MPT_BCAST4,
	MPT_BCAST5,
	MPT_SAMO,
	MPT_SAMIU,
	MPT_SAMIS,
	MPT_HEAD,
	MPT_RQD,
	MPT_AHYD,
	MPT_GTT,
	MPT_DRUGI,
	MPT_DACKD,
	MPT_DACK_DAL,
	MPT_DACK_DALG,
	MPT_DACK_DALN,
	MPT_DACK_GO,
	MPT_DACKZ,
	MPT_DAHY,
	MPT_DAHYZ,
	MPT_DAHYX,
	MPT_RLA,
	MPT_DRQG,
	MPT_DRQZ,
	MPT_DRQX,
	MPT_SACK,
	MPT_SITH_I,
	MPT_SITH_G,
	MPT_START_SYNC,
	MPT_CCSC,
	MPT_START_SYNT,
	MPT_ACKT_DT1,
	MPT_ACKT_DT2,
	MPT_ACKT_DT3,
	MPT_ACKT_DT4,
	MPT_AHY_DT,
	MPT_AHYQ_DT,
	MPT_SAMIS_DT,
	MPT_HEAD_DT,
	MPT_AHYD_DT,
	MPT_SACK_DT,
	_NUM_MPT_DEFINITIONS
};

enum mpt1327_parameters {
	MPT_CONSTANT = 0,
	MPT_PFIX,
	MPT_IDENT1,
	MPT_D,
	MPT_CHAN,
	MPT_IDENT2,
	MPT_N,
	MPT_P,
	MPT_CAT,
	MPT_TYPE,
	MPT_FUNC,
	MPT_CHAN4,
	MPT_WT,
	MPT_RSVD,
	MPT_M,
	MPT_QUAL,
	MPT_DT,
	MPT_LEVEL,
	MPT_EXT,
	MPT_FLAG1,
	MPT_FLAG2,
	MPT_PARAMETERS,
	MPT_SD,
	MPT_DIV,
	MPT_INFO,
	MPT_STATUS,
	MPT_SLOTS,
	MPT_POINT,
	MPT_CHECK,
	MPT_E,
	MPT_AD,
	MPT_DESC,
	MPT_A,
	MPT_B,
	MPT_SPARE,
	MPT_REVS,
	MPT_OPER,
	MPT_SYS,
	MPT_CONT,
	MPT_SYSDEF,
	MPT_PER,
	MPT_IVAL,
	MPT_PON,
	MPT_ID,
	MPT_ADJSITE,
	MPT_SOL,
	MPT_LEN,
	MPT_PREFIX2,
	MPT_KIND,
	MPT_PORT,
	MPT_FAD,
	MPT_INTER,
	MPT_HADT,
	MPT_MODEM,
	MPT_O_R,
	MPT_RATE,
	MPT_TRANS,
	MPT_RNITEL,
	MPT_TNITEL,
	MPT_JOB,
	MPT_REASON,
	MPT_ATRANS,
	MPT_EFLAGS,
	MPT_TASK,
	MPT_ONES,
	MPT_ITENUM,
	MPT_USERDATA,
	MPT_I_G,
	MPT_MORE,
	MPT_LASTBIT,
	MPT_FRAGL,
	MPT_RTRANS,
	MPT_W_F,
	MPT_P_N,
	MPT_DN,
	MPT_SPRE,
	MPT_SX,
	MPT_CAUSE,
	MPT_I_T,
	MPT_RESP,
	MPT_TOC,
	MPT_CCS,
	MPT_LET,
	MPT_PREAMBLE,
	MPT_PARAMETERS1,
	MPT_PARAMETERS2,
	MPT_BCD11,
	MPT_RSA,
	MPT_FCW,
	MPT_SP,
	MPT_EXCHANGE,
	MPT_Number,
	MPT_GF,
	MPT_PFIXT,
	MPT_IDENTT,
	MPT_FORM,
	MPT_PFIX2,
	_NUM_MPT_PARAMETERS
};

extern char *mpt1327_bcd;

typedef struct mpt1327_codeword {
	enum mpt1327_codeword_dir dir;
	enum mpt1327_codeword_type type;
	const char *short_name, *long_name;
	uint64_t params[_NUM_MPT_PARAMETERS];
} mpt1327_codeword_t;

void init_codeword(void);
uint16_t mpt1327_checkbits(uint64_t bits, uint16_t *parityp);
uint64_t mpt1327_encode_codeword(mpt1327_codeword_t *codeword);
int mpt1327_decode_codeword(mpt1327_codeword_t *codeword, int specific, enum mpt1327_codeword_dir dir, uint64_t bits);

