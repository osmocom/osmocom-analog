AM_CPPFLAGS = -Wall -Wextra -g $(all_includes)

bin_PROGRAMS = \
	bnetz \
	bnetz-dialer

bnetz_SOURCES = \
	bnetz.c \
	telegramm.c \
	dsp.c \
	stations.c \
	image.c \
	ansage.c \
	main.c
bnetz_LDADD = \
	$(COMMON_LA) \
	../anetz/libgermanton.a \
	$(top_builddir)/src/liboptions/liboptions.a \
	$(top_builddir)/src/libdebug/libdebug.a \
	$(top_builddir)/src/libmobile/libmobile.a \
	$(top_builddir)/src/libosmocc/libosmocc.a \
	$(top_builddir)/src/libdisplay/libdisplay.a \
	$(top_builddir)/src/libjitter/libjitter.a \
	$(top_builddir)/src/libsquelch/libsquelch.a \
	$(top_builddir)/src/libtimer/libtimer.a \
	$(top_builddir)/src/libselect/libselect.a \
	$(top_builddir)/src/libsamplerate/libsamplerate.a \
	$(top_builddir)/src/libemphasis/libemphasis.a \
	$(top_builddir)/src/libfsk/libfsk.a \
	$(top_builddir)/src/libfm/libfm.a \
	$(top_builddir)/src/libfilter/libfilter.a \
	$(top_builddir)/src/libwave/libwave.a \
	$(top_builddir)/src/libsample/libsample.a \
	$(top_builddir)/src/libg711/libg711.a \
	$(top_builddir)/src/libaaimage/libaaimage.a \
	-lm

bnetz_dialer_SOURCES = \
	telegramm.c \
	dialer.c
bnetz_dialer_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/liboptions/liboptions.a \
	$(top_builddir)/src/libdebug/libdebug.a \
	$(top_builddir)/src/libfsk/libfsk.a \
	$(top_builddir)/src/libfm/libfm.a \
	$(top_builddir)/src/libfilter/libfilter.a \
	$(top_builddir)/src/libwave/libwave.a \
	$(top_builddir)/src/libsample/libsample.a \
	$(ALSA_LIBS)
	-lm

if HAVE_ALSA
bnetz_LDADD += \
	$(top_builddir)/src/libsound/libsound.a \
	$(ALSA_LIBS)

bnetz_dialer_LDADD += \
	$(top_builddir)/src/libsound/libsound.a \
	$(ALSA_LIBS)
endif

if HAVE_SDR
bnetz_LDADD += \
	$(top_builddir)/src/libsdr/libsdr.a \
	$(top_builddir)/src/libam/libam.a \
	$(top_builddir)/src/libfft/libfft.a \
	$(UHD_LIBS) \
	$(SOAPY_LIBS)
endif

if HAVE_ALSA
AM_CPPFLAGS += -DHAVE_ALSA
endif

