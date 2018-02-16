AM_CPPFLAGS = -Wall -Wextra -g $(all_includes) $(IMAGEMAGICK_CFLAGS) 

noinst_LIBRARIES = libimage.a

libimage_a_SOURCES = \
	img.c

if HAVE_MAGICK
AM_CPPFLAGS += -DHAVE_MAGICK
endif

