AM_CPPFLAGS = -Wall -Wextra -g $(all_includes) $(FUSE_CFLAGS)

bin_PROGRAMS = \
	datenklo

datenklo_SOURCES = \
	am791x.c \
	uart.c \
	device.c \
	datenklo.c \
	main.c
datenklo_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/liboptions/liboptions.a \
	$(top_builddir)/src/libdebug/libdebug.a \
	$(top_builddir)/src/libfsk/libfsk.a \
	$(top_builddir)/src/libtimer/libtimer.a \
	$(top_builddir)/src/libfm/libfm.a \
	$(top_builddir)/src/libfilter/libfilter.a \
	$(top_builddir)/src/libsound/libsound.a \
	$(top_builddir)/src/libwave/libwave.a \
	$(top_builddir)/src/libdisplay/libdisplay.a \
	$(top_builddir)/src/libsample/libsample.a \
	$(ALSA_LIBS) \
	$(FUSE_LIBS) \
	-lm

if HAVE_ALSA
AM_CPPFLAGS += -DHAVE_ALSA
endif

if HAVE_FUSE
AM_CPPFLAGS += -DHAVE_FUSE
endif

