AM_CPPFLAGS = -Wall -Wextra -g $(all_includes)

bin_PROGRAMS = \
	radiocom2000

radiocom2000_SOURCES = \
	r2000.c \
	dsp.c \
	frame.c \
	tones.c \
	image.c \
	main.c
radiocom2000_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/libjitter/libjitter.a \
	$(top_builddir)/src/libhagelbarger/libhagelbarger.a \
	$(top_builddir)/src/common/libmobile.a \
	$(top_builddir)/src/common/libcommon.a \
	$(top_builddir)/src/libtimer/libtimer.a \
	$(top_builddir)/src/libsamplerate/libsamplerate.a \
	$(top_builddir)/src/libfilter/libfilter.a \
	$(top_builddir)/src/libwave/libwave.a \
	$(ALSA_LIBS) \
	$(UHD_LIBS) \
	$(SOAPY_LIBS) \
	-lm

