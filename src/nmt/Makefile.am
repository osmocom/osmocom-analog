AM_CPPFLAGS = -Wall -Wextra -g $(all_includes)

bin_PROGRAMS = \
	nmt

nmt_SOURCES = \
	nmt.c \
	countries.c \
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
	$(UHD_LIBS) \
	$(SOAPY_LIBS) \
	-lm

