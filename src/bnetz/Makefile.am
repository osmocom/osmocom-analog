AM_CPPFLAGS = -Wall -Wextra -g $(all_includes)

bin_PROGRAMS = \
	bnetz

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
	$(top_builddir)/src/libjitter/libjitter.a \
	$(top_builddir)/src/libsquelch/libsquelch.a \
	$(top_builddir)/src/common/libmobile.a \
	$(top_builddir)/src/common/libcommon.a \
	$(top_builddir)/src/libtimer/libtimer.a \
	$(top_builddir)/src/libsamplerate/libsamplerate.a \
	$(top_builddir)/src/libemphasis/libemphasis.a \
	$(top_builddir)/src/libfsk/libfsk.a \
	$(top_builddir)/src/libfm/libfm.a \
	$(top_builddir)/src/libfilter/libfilter.a \
	$(top_builddir)/src/libwave/libwave.a \
	$(top_builddir)/src/libfft/libfft.a \
	$(top_builddir)/src/libmncc/libmncc.a \
	$(ALSA_LIBS) \
	$(UHD_LIBS) \
	$(SOAPY_LIBS) \
	-lm

