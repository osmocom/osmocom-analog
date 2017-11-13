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
	$(top_builddir)/src/libgermanton/libgermanton.a \
	$(top_builddir)/src/common/libmobile.a \
	$(top_builddir)/src/common/libcommon.a \
	$(top_builddir)/src/libtimer/libtimer.a \
	$(top_builddir)/src/libsamplerate/libsamplerate.a \
	$(top_builddir)/src/libfilter/libfilter.a \
	$(ALSA_LIBS) \
	$(UHD_LIBS) \
	$(SOAPY_LIBS) \
	-lm

