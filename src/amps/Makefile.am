AM_CPPFLAGS = -Wall -Wextra -g $(all_includes)

noinst_LIBRARIES = libamps.a

bin_PROGRAMS = \
	amps

libamps_a_SOURCES = \
	amps_tacs_main.c \
	amps.c \
	transaction.c \
	frame.c \
	dsp.c \
	sysinfo.c

amps_SOURCES = \
	tones.c \
	noanswer.c \
	outoforder.c \
	invalidnumber.c \
	congestion.c \
	stations.c \
	image.c \
	main.c

amps_LDADD = \
	$(COMMON_LA) \
	libamps.a \
	$(top_builddir)/src/libmobile/libmobile.a \
	$(top_builddir)/src/libcompandor/libcompandor.a \
	$(top_builddir)/src/libgoertzel/libgoertzel.a \
	$(top_builddir)/src/libjitter/libjitter.a \
	$(top_builddir)/src/common/libcommon.a \
	$(top_builddir)/src/libtimer/libtimer.a \
	$(top_builddir)/src/libsamplerate/libsamplerate.a \
	$(top_builddir)/src/libemphasis/libemphasis.a \
	$(top_builddir)/src/libfilter/libfilter.a \
	$(top_builddir)/src/libwave/libwave.a \
	$(top_builddir)/src/libmncc/libmncc.a \
	$(top_builddir)/src/libsound/libsound.a \
	$(top_builddir)/src/libsample/libsample.a \
	$(ALSA_LIBS) \
	$(UHD_LIBS) \
	$(SOAPY_LIBS) \
	-lm

if HAVE_SDR
amps_LDADD += \
	$(top_builddir)/src/libsdr/libsdr.a \
	$(top_builddir)/src/libfft/libfft.a \
	$(top_builddir)/src/libfm/libfm.a
endif

