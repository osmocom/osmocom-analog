AM_CPPFLAGS = -Wall -Wextra -g $(all_includes) $(GRAPHICSMAGICK_CFLAGS) $(IMAGEMAGICK_CFLAGS) 

noinst_LIBRARIES = libimage.a

libimage_a_SOURCES = \
	img.c

if ENABLE_MAGICK
AM_CPPFLAGS += -DWITH_MAGICK
endif

