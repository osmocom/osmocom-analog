AM_CPPFLAGS = -Wall -Wextra -g $(all_includes)

noinst_LIBRARIES = libdmssms.a

bin_PROGRAMS = \
	nmt

libdmssms_a_SOURCES = \
	dms.c \
	sms.c

nmt_SOURCES = \
	nmt.c \
	countries.c \
	transaction.c \
	dsp.c \
	frame.c \
	image.c \
	tones.c \
	announcement.c \
	main.c
nmt_LDADD = \
	$(COMMON_LA) \
	libdmssms.a \
	$(top_builddir)/src/libjitter/libjitter.a \
	$(top_builddir)/src/libhagelbarger/libhagelbarger.a \
	$(top_builddir)/src/libdtmf/libdtmf.a \
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

