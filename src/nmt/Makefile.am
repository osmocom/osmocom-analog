AM_CPPFLAGS = -Wall -g $(all_includes)

bin_PROGRAMS = \
	nmt

nmt_SOURCES = \
	nmt.c \
	transaction.c \
	dsp.c \
	frame.c \
	dms.c \
	sms.c \
	image.c \
	tones.c \
	announcement.c \
	main.c
nmt_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/common/libcommon.a \
	$(ALSA_LIBS) \
	-lm

