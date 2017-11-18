AM_CPPFLAGS = -Wall -Wextra -g $(all_includes)

bin_PROGRAMS = \
	tacs

tacs_SOURCES = \
	tones.c \
	outoforder.c \
	stations.c \
	image.c \
	main.c

tacs_LDADD = \
	$(COMMON_LA) \
	../amps/libamps.a \
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
tacs_LDADD += \
	$(top_builddir)/src/libsdr/libsdr.a \
	$(top_builddir)/src/libfft/libfft.a \
	$(top_builddir)/src/libfm/libfm.a
endif

