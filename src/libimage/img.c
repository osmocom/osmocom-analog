#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "img.h"

int save_depth = 16;

#ifdef HAVE_MAGICK
#include <magick/api.h>

/* load given image to memory. return short RGB values */
unsigned short *load_img(int *width, int *height, const char *filename, int index)
{
	Image *image = NULL;
	ImageInfo *imageinfo = NULL;
	ExceptionInfo exception;
	unsigned short *img = NULL;

	MagickCoreGenesis(NULL, MagickFalse);
//	InitializeMagick(NULL);
	imageinfo = CloneImageInfo(0);
	GetExceptionInfo(&exception);

	sprintf(imageinfo->filename, filename, index);

	image = ReadImage(imageinfo, &exception);
	if (!image) {
//		printf("failed to read image '%s' via *magick\n", filename);
		goto exit;
	}

	*width = image->columns;
	*height = image->rows;

	img = (unsigned short *)malloc((*width) * (*height) * 3 * 2);
	if (!img) {
		printf("%s:failed to allocate image data\n", __func__);
		goto exit;
	}

	ExportImagePixels(image, 0, 0, *width, *height, "RGB", ShortPixel, img, NULL);
//	DispatchImage(image, 0, 0, *width, *height, "RGB", ShortPixel, img, NULL);

exit:
	if (image)
		DestroyImage(image);

	if (imageinfo)
		DestroyImageInfo(imageinfo);

	MagickCoreTerminus();
//	DestroyMagick();

	return img;
}

/* save given image */
int save_img(unsigned short *img, int width, int height, int alpha, const char *filename, int index)
{
	int rc = -1;
	Image *image = NULL;
	ImageInfo *imageinfo = NULL;
	ExceptionInfo exception;

	MagickCoreGenesis(NULL, MagickFalse);
//	InitializeMagick(NULL);
	imageinfo = CloneImageInfo(0);
	GetExceptionInfo(&exception);

	imageinfo->quality = 100;
	if (strlen(filename) >= 4 && !strcmp(filename + strlen(filename) - 4, ".png"))
		imageinfo->quality = 1;

	image=ConstituteImage(width, height, (alpha)?"RGBA":"RGB", ShortPixel, img, &exception);
	if (!image) {
		printf("%s:failed to prepare to write image\n", __func__);
		goto exit;
	}

	/* store as 16 bit, if lib and format supports it */
	image->depth = save_depth;

	sprintf(image->filename, filename, index); /* ACHTUNG: nicht imageinfo!!! */
	if (!WriteImage(imageinfo, image)) {
		printf("%s:failed to write image\n", __func__);
		goto exit;
	}

	rc = 0;

exit:
	if (image)
		DestroyImage(image);

	if (imageinfo)
		DestroyImageInfo(imageinfo);

	MagickCoreTerminus();
//	DestroyMagick();

	return rc;
}
#else

/* load given image to memory. return short RGB values */
unsigned short *load_img(int *width, int *height, const char *filename, int index)
{
	FILE *fp = NULL;
	unsigned short *img = NULL;
	char line[256];
	int words, i;

	sprintf(line, filename, index);
//	printf("reading image: %s\n", line);
	fp = fopen(line, "r");
	if (!fp) {
//		printf("failed to read ppm image '%s'\n", filename);
		goto exit;
	}
again1:
	if (!fgets(line, sizeof(line), fp)) {
		printf("%s:failed to read image depth\n", __func__);
		goto exit;
	}
	line[sizeof(line)-1] = '\0';
	if (line[0]) line[strlen(line)-1] = '\0';
	if (line[0] == '#')
		goto again1;
	if (!!strcmp(line, "P6")) {
		printf("%s:expecting image depth 'P6'\n", __func__);
		goto exit;
	}
again2:
	if (!fgets(line, sizeof(line), fp)) {
		printf("%s:failed to read image size\n", __func__);
		goto exit;
	}
	line[sizeof(line)-1] = '\0';
	if (line[0]) line[strlen(line)-1] = '\0';
	if (line[0] == '#')
		goto again2;
	sscanf(line, "%d %d", width, height);
//	printf("Image size: w=%d h=%d\n", *width, *height);
again3:
	if (!fgets(line, sizeof(line), fp)) {
		printf("%s:failed to read line '255' or '65535'\n", __func__);
		goto exit;
	}
	line[sizeof(line)-1] = '\0';
	if (line[0]) line[strlen(line)-1] = '\0';
	if (line[0] == '#')
		goto again3;
	if (!strcmp(line, "255")) {
		words = 1;
	} else
	if (!strcmp(line, "65535")) {
		words = 2;
	} else {
		printf("%s:expecting line '255' or '65535'\n", __func__);
		goto exit;
	}

	img = (unsigned short *)malloc((*width) * (*height) * 3 * 2);
	if (!img) {
		printf("%s:failed to allocate image data\n", __func__);
		goto exit;
	}
	if (fread(img, (*width) * (*height) * 3 * words, 1, fp) != 1) {
		printf("%s:failed to read image data\n", __func__);
		goto exit;
	}

	/* char to short (255 -> 65535) */
	if (words == 1) {
		unsigned char *from = (unsigned char *)img, c;
		for (i = (*width) * (*height) * 3 - 1; i >= 0; i--) {
			c = from[i];
			img[i] = (c << 8) | c;
		}
	} else {
		/* correct byte order */
		unsigned short v;
		unsigned char *from = (unsigned char *)img;
		for (i = 0; i < (*width) * (*height) * 3; i++) {
			v = ((*from++) << 8);
			v |= (*from++);
			img[i] = v;
		}
	}

exit:
	if (fp)
		fclose(fp);

	return img;
}

/* save given image */
int save_img(unsigned short *img, int width, int height, int alpha, const char *filename, int index)
{
	FILE *fp = NULL;
	int rc = -1;
	char line[256];
	int i;
	unsigned short v;
	unsigned char *to;

	if (alpha) {
		printf("%s:cannot save alpha component with PPM support only\n", __func__);
		alpha = 0;
		goto exit;
	}

	sprintf(line, filename, index);
//	printf("writing image: %s\n", line);
	fp = fopen(line, "w");
	if (!fp) {
		printf("%s:failed to write image\n", __func__);
		goto exit;
	}
	fprintf(fp, "P6\n%d %d\n65535\n", width, height);

	/* correct byte order, write and restore byte order */
	to = (unsigned char *)img;
	for (i = 0; i < width * height * 3; i++) {
		v = img[i];
		if (i/100*i == i) { printf("%04x ", v); }
		(*to++) = v >> 8;
		(*to++) = v;
	}
	rc = fwrite(img, width * height * 3 * 2, 1, fp);
	to = (unsigned char *)img;
	for (i = 0; i < width * height * 3; i++) {
		v = (*to++) << 8;
		v |= (*to++);
		img[i] = v;
	}
	if (rc != 1) {
		printf("%s:failed to write image data\n", __func__);
		goto exit;
	}

	rc = 0;

exit:
	if (fp)
		fclose(fp);

	return rc;
}
#endif

int save_img_array(double *array, int width, int height, int alpha, const char *filename, int index)
{
	int rc = -1;
	unsigned short *img = NULL;
	int components;

#ifndef HAVE_MAGICK
	if (alpha) {
		printf("%s:warning, cannot save alpha component with PPM support only\n", __func__);
		alpha = 0;
	}
#endif
	components = (alpha) ? 4 : 3;

	img = (unsigned short *)malloc(width * height * components * 2);
	if (!img) {
		printf("%s:failed to allocate image data\n", __func__);
		goto exit;
	}

	array2img_short(array, width, height, img, width, height, alpha);

	save_img(img, width, height, alpha, filename, index);

	rc = 0;

exit:
	if (img)
		free(img);

	return rc;
}

/* convert an image to a three dimensional array of double
 * the size is: width, height, 3
 */
void img2array_short(unsigned short *img, int iw, int ih, double *array, int aw, int ah)
{
	int x, y;
	int channel;
	double r, g, b;

	channel = aw * ah;

	for (y = 0; y < ih; y++) {
		for (x = 0; x < iw; x++) {
			r = img[(x+iw*y)*3] / 65535.0F;
			g = img[(x+iw*y)*3+1] / 65535.0F;
			b = img[(x+iw*y)*3+2] / 65535.0F;
			array[x+aw*y] = r;
			array[x+aw*y+channel] = g;
			array[x+aw*y+channel+channel] = b;
		}
	}
}

/* convert a three dimensional array of double to an image
 * the size is: width, height, 3
 */
void array2img_short(double *array, int aw, int ah, unsigned short *img, int iw, int ih, int alpha)
{
	int x, y, c;
	int channel, components;
	double r, g, b, a;

	channel = aw * ah;
	components = (alpha) ? 4 : 3;

	for (y = 0; y < ih; y++) {
		for (x = 0; x < iw; x++) {
			r = array[x+aw*y];
			c = (r * 65535.0F + 0.5F);
			if (c < 0)
				c = 0;
			else if (c > 65535)
				c = 65535;
			img[(x+iw*y)*components] = c;
			g = array[x+aw*y+channel];
			c = (g * 65535.0F + 0.5F);
			if (c < 0)
				c = 0;
			else if (c > 65535)
				c = 65535;
			img[(x+iw*y)*components+1] = c;
			b = array[x+aw*y+channel+channel];
			c = (b * 65535.0F + 0.5F);
			if (c < 0)
				c = 0;
			else if (c > 65535)
				c = 65535;
			img[(x+iw*y)*components+2] = c;
			if (alpha) {
				a = array[x+aw*y+channel+channel+channel];
				c = (a * 65535.0F + 0.5F);
				if (c < 0)
					c = 0;
				else if (c > 65535)
					c = 65535;
				img[(x+iw*y)*components+3] = c;
			}
		}
	}
}

/*
 * scale down image in img_buffer by calculating average
 */
void scale_img(unsigned short *img, int width, int height, int scale)
{
	int w, h, i, j, x, y;
	int r, g, b;

	if (scale == 1)
		return;

	w = width / scale;
	h = height / scale;

	for (i = 0; i < h; i++) {
		for (j = 0; j < w; j++) {
			r = g = b = 0;
			for (y = 0; y < scale; y++) {
				for (x = 0; x < scale; x++) {
					r += img[((i*scale+y) * width + j*scale+x) * 3 + 0];
					g += img[((i*scale+y) * width + j*scale+x) * 3 + 1];
					b += img[((i*scale+y) * width + j*scale+x) * 3 + 2];
				}
			}
			img[(i * w + j)*3 + 0] = r / scale / scale;
			img[(i * w + j)*3 + 1] = g / scale / scale;
			img[(i * w + j)*3 + 2] = b / scale / scale;
		}
	}
}


