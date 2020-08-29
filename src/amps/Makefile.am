AM_CPPFLAGS = -Wall -Wextra -g $(all_includes)

bin_PROGRAMS = \
	amps

noinst_LIBRARIES = libamps.a libusatone.a

libamps_a_SOURCES = \
	amps_tacs_main.c \
	amps.c \
	transaction.c \
	frame.c \
	dsp.c \
	sysinfo.c

libusatone_a_SOURCES = \
	tones.c \
	noanswer.c \
	outoforder.c \
	invalidnumber.c \
	congestion.c

amps_SOURCES = \
	stations.c \
	image.c \
	main.c

amps_LDADD = \
	$(COMMON_LA) \
	libamps.a \
	libusatone.a \
	$(top_builddir)/src/liboptions/liboptions.a \
	$(top_builddir)/src/libdebug/libdebug.a \
	$(top_builddir)/src/libmobile/libmobile.a \
	$(top_builddir)/src/libdisplay/libdisplay.a \
	$(top_builddir)/src/libcompandor/libcompandor.a \
	$(top_builddir)/src/libgoertzel/libgoertzel.a \
	$(top_builddir)/src/libjitter/libjitter.a \
	$(top_builddir)/src/libtimer/libtimer.a \
	$(top_builddir)/src/libsamplerate/libsamplerate.a \
	$(top_builddir)/src/libemphasis/libemphasis.a \
	$(top_builddir)/src/libfm/libfm.a \
	$(top_builddir)/src/libfilter/libfilter.a \
	$(top_builddir)/src/libwave/libwave.a \
	$(top_builddir)/src/libmncc/libmncc.a \
	$(top_builddir)/src/libsample/libsample.a \
	$(top_builddir)/src/libg711/libg711.a \
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

