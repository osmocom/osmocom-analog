AM_CPPFLAGS = -Wall -Wextra -g $(all_includes)

bin_PROGRAMS = \
	anetz

noinst_LIBRARIES = libgermanton.a

libgermanton_a_SOURCES = \
	freiton.c\
	besetztton.c

anetz_SOURCES = \
	anetz.c \
	dsp.c \
	stations.c \
	image.c \
	main.c
anetz_LDADD = \
	$(COMMON_LA) \
	libgermanton.a \
	$(top_builddir)/src/libgoertzel/libgoertzel.a \
	$(top_builddir)/src/libjitter/libjitter.a \
	$(top_builddir)/src/libsquelch/libsquelch.a \
	$(top_builddir)/src/common/libmobile.a \
	$(top_builddir)/src/common/libcommon.a \
	$(top_builddir)/src/libtimer/libtimer.a \
	$(top_builddir)/src/libsamplerate/libsamplerate.a \
	$(top_builddir)/src/libemphasis/libemphasis.a \
	$(top_builddir)/src/libfm/libfm.a \
	$(top_builddir)/src/libfilter/libfilter.a \
	$(top_builddir)/src/libwave/libwave.a \
	$(top_builddir)/src/libfft/libfft.a \
	$(ALSA_LIBS) \
	$(UHD_LIBS) \
	$(SOAPY_LIBS) \
	-lm

