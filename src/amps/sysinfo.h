
/* filler */
struct sysinfo_filler {
	uint8_t		cmac;
	uint8_t		sdcc1;
	uint8_t		sdcc2;
	uint8_t		wfom;
};

enum amps_sysinfo_type {
	SYSINFO_WORD1,
	SYSINFO_WORD2,
	SYSINFO_REG_ID,
	SYSINFO_REG_INCR,
	SYSINFO_LOC_AREA,
	SYSINFO_NEW_ACC,
	SYSINFO_OVERLOAD,
	SYSINFO_ACC_TYPE,
	SYSINFO_ACC_ATTEMPT,
};

/* Word 1 */
struct sysinfo_word1 {
	uint16_t	sid1;
	uint8_t		ep;
	uint8_t		auth;
	uint8_t		pci;
};

/* Word 2 */
struct sysinfo_word2 {
	uint8_t		s;
	uint8_t		e;
	uint8_t		regh;
	uint8_t		regr;
	uint8_t		dtx;
	uint8_t		n_1;
	uint8_t		rcf;
	uint8_t		cpa;
	uint8_t		cmax_1;
};

/* registration increment */
struct sysinfo_reg_incr {
	uint16_t	regincr;
};

/* location area */
struct sysinfo_loc_area {
	uint8_t		pureg;
	uint8_t		pdreg;
	uint8_t		lreg;
	uint16_t	locaid;
};

/* new access channel set */
struct sysinfo_new_acc {
	uint16_t	newacc;
};

/* overload control */
struct sysinfo_overload {
	uint8_t		olc[16];
};

/* Acces Tyoe */
struct sysinfo_acc_type {
	uint8_t		bis;
	uint8_t		pci_home;
	uint8_t		pci_roam;
	uint8_t		bspc;
	uint8_t		bscap;
};

/* access attempt parameters */
struct sysinfo_acc_attempt {
	uint8_t		maxbusy_pgr;
	uint8_t		maxsztr_pgr;
	uint8_t		maxbusy_other;
	uint8_t		maxsztr_other;
};

/* registration ID */
struct sysinfo_reg_id {
	uint32_t	regid;
};

typedef struct system_information {
	/* all words */
	uint8_t				dcc;
	/* VC assginment */
	uint8_t				vmac;
	/* broadcast */
	struct sysinfo_filler		filler;
	struct sysinfo_word1		word1;
	struct sysinfo_word2		word2;
	struct sysinfo_reg_incr		reg_incr;
	struct sysinfo_loc_area		loc_area;
	struct sysinfo_new_acc		new_acc;
	struct sysinfo_overload		overload;
	struct sysinfo_acc_type		acc_type;
	struct sysinfo_acc_attempt	acc_attempt;
	struct sysinfo_reg_id		reg_id;

	/* tx state */
	enum amps_sysinfo_type		type[16];	/* list of messages in train */
	int				num;		/* number of messages in train */
	int				count;		/* count message train */
} amps_si;

void init_sysinfo(amps_si *si, int cmac, int vmac, int dcc, int sid1, int regh, int regr, int pureg, int pdreg, int locaid, int regincr, int bis);
void prepare_sysinfo(amps_si *si);
uint64_t get_sysinfo(amps_si *si);

