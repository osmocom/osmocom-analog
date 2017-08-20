
enum bas_type {
	BAS_FUBK,
	BAS_VCR,
	BAS_IMAGE,
};

typedef struct bas {
	double		samplerate;
	enum bas_type	type;
	int		fbas;			/* if color shall be added */
	double		circle_radius;		/* radius of circle in grid units */
	int		color_bar;		/* show only color bar on all lines */
	int		grid_only;		/* show only the grid */
	const char	*station_id;		/* text to display as station id */
	double		color_phase;		/* current phase of color carrier */
	int		v_polarity;		/* polarity of V color vector */
	unsigned short	*img;			/* image data, if it should be used */
	int		img_width, img_height;	/* size of image */
	iir_filter_t	lp_y, lp_u, lp_v;	/* low pass filters */
} bas_t;

void bas_init(bas_t *bas, double samplerate, enum bas_type type, int fbas, double circle_radius, int color_bar, int grid_only, const char *station_id, unsigned short *img, int width, int height);
int bas_generate(bas_t *bas, sample_t *sample);

