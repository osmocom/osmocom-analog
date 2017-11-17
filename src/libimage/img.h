extern int save_depth;
unsigned short *load_img(int *width, int *height, const char *filename, int index);
int save_img(unsigned short *img, int width, int height, int alpha, const char *filename, int index);
int save_img_array(double *array, int width, int height, int alpha, const char *filename, int index);
void img2array_short(unsigned short *img, int iw, int ih, double *array, int aw, int ah);
void array2img_short(double *array, int aw, int ah, unsigned short *img, int iw, int ih, int alpha);
void scale_img(unsigned short *img, int width, int height, int scale);
