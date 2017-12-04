
typedef struct sdr_config {
	int		uhd,			/* select UHD API */
			soapy;			/* select Soapy SDR API */
	int		channel;		/* channel number */
	const char	*device_args,		/* arguments */
			*stream_args,
			*tune_args;
	int		samplerate;		/* ADC/DAC sample rate */
	double		lo_offset;		/* LO frequency offset */
	double		bandwidth;		/* IF bandwidth */
	double		tx_gain,		/* gain */
			rx_gain;
	const char	*tx_antenna,		/* list/override antennas */
			*rx_antenna;
	const char	*write_iq_tx_wave;	/* wave recording and playback */
	const char	*write_iq_rx_wave;
	const char	*read_iq_tx_wave;
	const char	*read_iq_rx_wave;
	int		swap_links;		/* swap DL and UL frequency */
	int		uhd_tx_timestamps;	/* use UHD time stamps */
} sdr_config_t;

extern sdr_config_t *sdr_config;

void sdr_config_init(double lo_offset);
void sdr_config_print_help(void);
void sdr_config_print_hotkeys(void);
extern struct option sdr_config_long_options[];
extern const char *sdr_config_optstring;
int sdr_config_opt_switch(int c, int *skip_args);
int sdr_configure(int samplerate);

