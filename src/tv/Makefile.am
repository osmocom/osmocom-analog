AM_CPPFLAGS = -Wall -Wextra -g $(all_includes) -fstack-check

bin_PROGRAMS = \
	osmotv

osmotv_SOURCES = \
	bas.c \
	fubk.c \
	font.c \
	vcr.c \
	image.c \
	tv_modulate.c \
	channels.c \
	main.c
osmotv_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/libimage/libimage.a \
	$(top_builddir)/src/common/libcommon.a \
	$(top_builddir)/src/libfm/libfm.a \
	$(top_builddir)/src/libfilter/libfilter.a \
	$(top_builddir)/src/libwave/libwave.a \
	$(top_builddir)/src/libsample/libsample.a \
	$(ALSA_LIBS) \
	$(UHD_LIBS) \
	$(SOAPY_LIBS) \
	$(GRAPHICSMAGICK_LIBS) $(IMAGEMAGICK_LIBS) \
	-lm

if HAVE_SDR
osmotv_LDADD += \
	$(top_builddir)/src/libsdr/libsdr.a \
	$(top_builddir)/src/libdisplay/libdisplay.a \
	$(top_builddir)/src/libfft/libfft.a
endif

osmotv_LDADD += \
	$(top_builddir)/src/libtimer/libtimer.a

if HAVE_SDR
AM_CPPFLAGS += -DHAVE_SDR
endif

if ENABLE_MAGICK
AM_CPPFLAGS += -DWITH_MAGICK
endif

