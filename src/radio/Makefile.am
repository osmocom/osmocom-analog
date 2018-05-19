AM_CPPFLAGS = -Wall -Wextra -g $(all_includes)

if HAVE_SDR

bin_PROGRAMS = \
	osmoradio

osmoradio_SOURCES = \
	radio.c \
	main.c
osmoradio_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/liboptions/liboptions.a \
	$(top_builddir)/src/libdebug/libdebug.a \
	$(top_builddir)/src/libwave/libwave.a \
	$(top_builddir)/src/libsample/libsample.a \
	$(top_builddir)/src/libsdr/libsdr.a \
	$(top_builddir)/src/libclipper/libclipper.a \
	$(top_builddir)/src/libfm/libfm.a \
	$(top_builddir)/src/libam/libam.a \
	$(top_builddir)/src/libemphasis/libemphasis.a \
	$(top_builddir)/src/libsamplerate/libsamplerate.a \
	$(top_builddir)/src/libjitter/libjitter.a \
	$(top_builddir)/src/libfilter/libfilter.a \
	$(top_builddir)/src/libdisplay/libdisplay.a \
	$(top_builddir)/src/libfft/libfft.a \
	$(top_builddir)/src/libtimer/libtimer.a \
	$(UHD_LIBS) \
	$(SOAPY_LIBS) \
	-lm

if HAVE_ALSA
osmoradio_LDADD += \
	$(top_builddir)/src/libsound/libsound.a \
	$(ALSA_LIBS)
endif

if HAVE_ALSA
AM_CPPFLAGS += -DHAVE_ALSA
endif

AM_CPPFLAGS += -DHAVE_SDR

endif

