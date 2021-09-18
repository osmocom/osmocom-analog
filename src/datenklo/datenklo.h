
enum datenklo_auto_mc {
	DATENKLO_AUTO_MC_NONE = 0,
	DATENKLO_AUTO_MC_BELL_ORIGINATE,
	DATENKLO_AUTO_MC_BELL_ANSWER,
	DATENKLO_AUTO_MC_BELL_4WIRE,
	DATENKLO_AUTO_MC_CCITT_ORIGINATE,
	DATENKLO_AUTO_MC_CCITT_ANSWER,
	DATENKLO_AUTO_MC_CCITT_4WIRE,
};

typedef struct datenklo {
	struct datenklo *slave;

	/* settings */
	uint8_t		mc;			/* modem chip mode */
	int		auto_rts;		/* automatic RTS controlling for half duplex */
	double		max_baud;		/* limit to what the mode supports */
	double		force_tx_baud, force_rx_baud; /* override IOCTL */
	int		tx_back, rx_back;	/* set if back channel is used for path */
	int		samplerate;		/* audio sample rate */
	int		buffer_size;		/* audio buffer size */
	int		loopback;		/* loopback mode */

	/* states */
	int		flags;			/* open() flags */
	struct termios	termios;		/* current termios */
	double		baudrate;		/* current baud rate */
	int		lines;			/* state of lines (from IOCTL) */
	int		break_on;		/* currently sending a break */
	int		break_bits;		/* counts bits while sending a break signal */
	int		tcsetsw;		/* send new termios after TX buffer is flused */
	struct termios	tcsetsw_termios;	/* new termios after TX buffer is flused */
	int		ignbrk;			/* IGNBRK option enabled */
	int		parmrk;			/* PARMRK option enabled */
	int		istrip;			/* ISTRIP option enabled */
	int		inlcr;			/* INLCR option enabled */
	int		igncr;			/* IGNCR option enabled */
	int		icrnl;			/* ICRNL option enabled */
	int		iuclc;			/* IUCLC option enabled */
	int		opost;			/* OPOST option to enable all post options */
	int		onlcr;			/* ONLCR option enabled */
	int		onlcr_char;		/* CR transmitted, next up is NL */
	int		ocrnl;			/* OCRNL option enabled */
	int		onlret;			/* ONLRET option enabled */
	int		olcuc;			/* OLCUC option enabled */
	int		echo;			/* ECHO option enabled */
	short		revents;		/* current set of poll reply events */
	int		open_count;		/* to see if device is in use */
	int		auto_rts_on;		/* Data available */
	int		auto_rts_rts;		/* RTS was raised */
	int		auto_rts_cts;		/* CTS was indicated */
	int		auto_rts_cd;		/* CD was indicated */
	int		output_off;		/* output stopped by flow control */
	struct timer	vtimer;			/* VTIME timer */
	int		vtimeout;		/* when timeout has fired */

	/* data fifos */
	uint8_t		*tx_fifo;
	int		tx_fifo_in, tx_fifo_out;
	int		tx_fifo_size;
	int		tx_fifo_full;		/* watermark to change POLLOUT flag */
	uint8_t		*rx_fifo;
	int		rx_fifo_in, rx_fifo_out;
	int		rx_fifo_size;

	/* instances */
	am791x_t	am791x;			/* da great modem IC */
	uart_t		uart;			/* soft uart */
	void		*audio;			/* sound interface */
	void		*device;		/* CUSE device */
        wave_rec_t	wave_rx_rec;		/* wave recording (from RX) */
        wave_rec_t	wave_tx_rec;		/* wave recording (from TX) */
        wave_play_t	wave_rx_play;		/* wave playback (as RX) */
        wave_play_t	wave_tx_play;		/* wave playback (as TX) */
	dispwav_t	dispwav;		/* wave display */
	dispmeas_t	dispmeas;		/* measurements display */
	double		rx_level_abs;		/* measure peak level of received audio */
	int		rx_level_count;		/* samples measured so far */
	int		rx_level_max;		/* number of samples per measurement */
	dispmeasparam_t	*dmp_rx_level;		/* current rx level */
	dispmeasparam_t	*dmp_tone_level;	/* level of tone */
	dispmeasparam_t	*dmp_tone_quality;	/* quality of tone */
	int		last_bit;		/* to check if we have valid quality */
} datenklo_t;

void datenklo_main(datenklo_t *datenklo, int loopback);
int datenklo_init(datenklo_t *datenklo, const char *dev_name, enum am791x_type am791x_type, uint8_t mc, int auto_rts, double force_tx_baud, double force_rx_baud, int samplerate, int loopback);
int datenklo_open_audio(datenklo_t *datenklo, const char *audiodev, int buffer, const char *write_rx_wave, const char *write_tx_wave, const char *read_rx_wave, const char *read_tx_wave);
void datenklo_exit(datenklo_t *datenklo);
void datenklo_init_global(void);

