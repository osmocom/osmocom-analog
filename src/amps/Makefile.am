AM_CPPFLAGS = -Wall -Wextra -Wmissing-prototypes -g $(all_includes)

bin_PROGRAMS = \
	amps tacs jtacs

noinst_LIBRARIES = libamps.a libusatone.a

libamps_a_SOURCES = \
	main_common.c \
	amps.c \
	transaction.c \
	frame.c \
	dsp.c \
	sysinfo.c \
	esn.c

libusatone_a_SOURCES = \
	usa_tones.c \
	usa_noanswer.c \
	usa_outoforder.c \
	usa_invalidnumber.c \
	usa_congestion.c

amps_SOURCES = \
	amps_stations.c \
	amps_image.c \
	amps_main.c

amps_LDADD = \
	$(COMMON_LA) \
	libamps.a \
	libusatone.a \
	$(top_builddir)/src/liboptions/liboptions.a \
	$(top_builddir)/src/libmobile/libmobile.a \
	$(top_builddir)/src/libdisplay/libdisplay.a \
	$(top_builddir)/src/libcompandor/libcompandor.a \
	$(top_builddir)/src/libgoertzel/libgoertzel.a \
	$(top_builddir)/src/libjitter/libjitter.a \
	$(top_builddir)/src/libsamplerate/libsamplerate.a \
	$(top_builddir)/src/libemphasis/libemphasis.a \
	$(top_builddir)/src/libfm/libfm.a \
	$(top_builddir)/src/libfilter/libfilter.a \
	$(top_builddir)/src/libwave/libwave.a \
	$(top_builddir)/src/libsample/libsample.a \
	$(top_builddir)/src/libaaimage/libaaimage.a \
	$(top_builddir)/src/liblogging/liblogging.a \
	$(LIBOSMOCORE_LIBS) \
	$(LIBOSMOCC_LIBS) \
	-lm

if HAVE_ALSA
amps_LDADD += \
	$(top_builddir)/src/libsound/libsound.a \
	$(ALSA_LIBS)
endif

if HAVE_SDR
amps_LDADD += \
	$(top_builddir)/src/libsdr/libsdr.a \
	$(top_builddir)/src/libam/libam.a \
	$(top_builddir)/src/libfft/libfft.a \
	$(UHD_LIBS) \
	$(SOAPY_LIBS)
endif

tacs_SOURCES = \
	tacs_tones.c \
	tacs_outoforder.c \
	tacs_stations.c \
	tacs_image.c \
	tacs_main.c

tacs_LDADD = \
	$(COMMON_LA) \
	libamps.a \
	$(top_builddir)/src/liboptions/liboptions.a \
	$(top_builddir)/src/libmobile/libmobile.a \
	$(top_builddir)/src/libdisplay/libdisplay.a \
	$(top_builddir)/src/libcompandor/libcompandor.a \
	$(top_builddir)/src/libgoertzel/libgoertzel.a \
	$(top_builddir)/src/libjitter/libjitter.a \
	$(top_builddir)/src/libsamplerate/libsamplerate.a \
	$(top_builddir)/src/libemphasis/libemphasis.a \
	$(top_builddir)/src/libfm/libfm.a \
	$(top_builddir)/src/libfilter/libfilter.a \
	$(top_builddir)/src/libwave/libwave.a \
	$(top_builddir)/src/libsample/libsample.a \
	$(top_builddir)/src/libaaimage/libaaimage.a \
	$(top_builddir)/src/liblogging/liblogging.a \
	$(LIBOSMOCORE_LIBS) \
	$(LIBOSMOCC_LIBS) \
	-lm

if HAVE_ALSA
tacs_LDADD += \
	$(top_builddir)/src/libsound/libsound.a \
	$(ALSA_LIBS)
endif

if HAVE_SDR
tacs_LDADD += \
	$(top_builddir)/src/libsdr/libsdr.a \
	$(top_builddir)/src/libam/libam.a \
	$(top_builddir)/src/libfft/libfft.a \
	$(UHD_LIBS) \
	$(SOAPY_LIBS)
endif

jtacs_SOURCES = \
	jtacs_tones.c \
	jtacs_stations.c \
	jtacs_image.c \
	jtacs_main.c

jtacs_LDADD = \
	$(COMMON_LA) \
	libamps.a \
	$(top_builddir)/src/liboptions/liboptions.a \
	$(top_builddir)/src/libmobile/libmobile.a \
	$(top_builddir)/src/libdisplay/libdisplay.a \
	$(top_builddir)/src/libcompandor/libcompandor.a \
	$(top_builddir)/src/libgoertzel/libgoertzel.a \
	$(top_builddir)/src/libjitter/libjitter.a \
	$(top_builddir)/src/libsamplerate/libsamplerate.a \
	$(top_builddir)/src/libemphasis/libemphasis.a \
	$(top_builddir)/src/libfm/libfm.a \
	$(top_builddir)/src/libfilter/libfilter.a \
	$(top_builddir)/src/libwave/libwave.a \
	$(top_builddir)/src/libsample/libsample.a \
	$(top_builddir)/src/libaaimage/libaaimage.a \
	$(top_builddir)/src/liblogging/liblogging.a \
	$(LIBOSMOCORE_LIBS) \
	$(LIBOSMOCC_LIBS) \
	-lm

if HAVE_ALSA
jtacs_LDADD += \
	$(top_builddir)/src/libsound/libsound.a \
	$(ALSA_LIBS)
endif

if HAVE_SDR
jtacs_LDADD += \
	$(top_builddir)/src/libsdr/libsdr.a \
	$(top_builddir)/src/libam/libam.a \
	$(top_builddir)/src/libfft/libfft.a \
	$(UHD_LIBS) \
	$(SOAPY_LIBS)
endif

