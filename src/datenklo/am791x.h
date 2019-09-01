
enum am791x_type {
	AM791X_TYPE_7910 = 0,
	AM791X_TYPE_7911 = 1,
};

enum am791x_st {
	AM791X_STATE_INIT = 0,
	AM791X_STATE_RCON,
	AM791X_STATE_CDON,
	AM791X_STATE_DATA,
	AM791X_STATE_RCOFF,
	AM791X_STATE_CDOFF,
	AM791X_STATE_STO_OFF,
	AM791X_STATE_SQ_OFF,
	AM791X_STATE_BRCON,
	AM791X_STATE_BCDON,
	AM791X_STATE_BDATA,
	AM791X_STATE_BRCOFF,
	AM791X_STATE_BCDOFF,
};

typedef struct am791x {
	/* settings */
	void *inst;					/* upper layer instance */
	enum am791x_type type;
	int samplerate;
	double tx_baud, rx_baud;

	/* callbacks */
	void (*cts_cb)(void *inst, int cts);
	void (*bcts_cb)(void *inst, int cts);
	void (*cd_cb)(void *inst, int cd);
	void (*bcd_cb)(void *inst, int cd);
	int (*td_cb)(void *inst);
	int (*btd_cb)(void *inst);
	void (*rd_cb)(void *inst, int bit, double quality, double level);
	void (*brd_cb)(void *inst, int bit, double quality, double level);

	/* modes */
	uint8_t		mc;				/* current mode setting */
	int		fullduplex;                     /* duplex */
	int		loopback_main, loopback_back;   /* loopback */
	int		equalizer, sto;                 /* equalizer & STO */
	int		bell_202;			/* is BELL 202 */

	/* states */
	enum am791x_st	tx_state, rx_state;
	int		tx_silence;			/* no audio transmitted */
	int		tx_sto;				/* no STO transmitted */
	int		block_td;			/* "TD IGNORED" */
	int		block_rd;			/* "RD = MARK" */
	int		line_cd;			/* 1 = CD is low */
	int		block_cd;			/* "SET CD HIGH" (CD is ignored) */
	int		block_btd;			/* "BTD IGNORED" */
	int		block_brd;			/* "BRD = MARK" */
	int		line_bcd;			/* 1 = BCD is low */
	int		block_bcd;			/* "SET BCD HIGH" (BCD is ignored) */
	int		squelch;			/* "SQUELCH" (mute received audio) */
	int		line_dtr;			/* 1 = DTR is low */
	int		line_rts;			/* 1 = RTS is low */
	int		line_brts;			/* 1 = BRTS is low */
	int		line_ring;			/* 1 = ring is low */

	/* frequencies */
	int		f0_tx, f1_tx;
	int		f0_rx, f1_rx;

	/* timers */
	struct timer	tx_timer, rx_timer;
	double		t_rcon;
	double		t_rcoff;
	double		t_brcon;
	double		t_brcoff;
	double		t_cdon;
	double		t_cdoff;
	double		t_bcdon;
	double		t_bcdoff;
	double		t_at;
	double		t_sil1;
	double		t_sil2;
	double		t_sq;
	double	   	t_sto;

	/* FSK/STO signal */
	int		rx_back_channel;		/* indikates if receiver is tuned to back channel */
	fsk_mod_t	fsk_tx;				/* FSK modulator */
	fsk_demod_t	fsk_rx;				/* FSK demodulator */
	double		tx_level;			/* level of TX */
	double		cd_on, cd_off;			/* levels for CD */
	int		cd, bcd;			/* carrier detected */
	double		sto_phaseshift65536;		/* STO tone phase shift */
} am791x_t;

void am791x_send(am791x_t *am791x, sample_t *samples, int length);
void am791x_receive(am791x_t *am791x, sample_t *samples, int length);
void am791x_list_mc(enum am791x_type type);
int am791x_init(am791x_t *am791x, void *inst, enum am791x_type type, uint8_t mc, int samplerate, double tx_baud, double rx_baud, void (*cts)(void *inst, int cts), void (*bcts)(void *inst, int cts), void (*cd)(void *inst, int cd), void (*bcd)(void *inst, int cd), int (*td)(void *inst), int (*btd)(void *inst), void (*rd)(void *inst, int bit, double quality, double level), void (*brd)(void *inst, int bit, double quality, double level));
void am791x_exit(am791x_t *am791x);
double am791x_max_baud(uint8_t mc);
int am791x_mc(am791x_t *am791x, uint8_t mc, int samplerate, double tx_baud, double rx_baud);
void am791x_reset(am791x_t *am791x);
void am791x_dtr(am791x_t *am791x, int dtr);
void am791x_rts(am791x_t *am791x, int rts);
void am791x_brts(am791x_t *am791x, int brts);
void am791x_ring(am791x_t *am791x, int ring);

