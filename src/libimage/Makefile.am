AM_CPPFLAGS = -Wall -Wextra -Wmissing-prototypes -g $(all_includes) \
	$(IMAGEMAGICK6_CFLAGS) $(IMAGEMAGICK7_CFLAGS)

noinst_LIBRARIES = libimage.a

libimage_a_SOURCES = \
	img.c

if HAVE_MAGICK6
AM_CPPFLAGS += -DHAVE_MAGICK6
endif
if HAVE_MAGICK7
AM_CPPFLAGS += -DHAVE_MAGICK7
endif

