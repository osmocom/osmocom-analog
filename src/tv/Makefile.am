AM_CPPFLAGS = -Wall -Wextra -g $(all_includes) $(IMAGEMAGICK_CFLAGS)

bin_PROGRAMS = \
	osmotv

osmotv_SOURCES = \
	bas.c \
	fubk.c \
	ebu.c \
	convergence.c \
	color.c \
	font.c \
	vcr.c \
	image.c \
	sample_image.c \
	tv_modulate.c \
	channels.c \
	main.c
osmotv_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/liboptions/liboptions.a \
	$(top_builddir)/src/libdebug/libdebug.a \
	$(top_builddir)/src/libimage/libimage.a \
	$(top_builddir)/src/libfm/libfm.a \
	$(top_builddir)/src/libfilter/libfilter.a \
	$(top_builddir)/src/libwave/libwave.a \
	$(top_builddir)/src/libsample/libsample.a \
	$(ALSA_LIBS) \
	$(IMAGEMAGICK_LIBS) \
	-lm

if HAVE_SDR
osmotv_LDADD += \
	$(top_builddir)/src/libsdr/libsdr.a \
	$(top_builddir)/src/libam/libam.a
endif

osmotv_LDADD += \
	$(top_builddir)/src/libdisplay/libdisplay.a \
	$(top_builddir)/src/libtimer/libtimer.a

if HAVE_SDR
osmotv_LDADD += \
	$(top_builddir)/src/libfft/libfft.a \
	$(UHD_LIBS) \
	$(SOAPY_LIBS)
endif

if HAVE_SDR
AM_CPPFLAGS += -DHAVE_SDR
endif

if HAVE_MAGICK
AM_CPPFLAGS += -DHAVE_MAGICK
endif

