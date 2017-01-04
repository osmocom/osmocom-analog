AM_CPPFLAGS = -Wall -Wextra -g $(all_includes)

bin_PROGRAMS = \
	cnetz

cnetz_SOURCES = \
	cnetz.c \
	transaction.c \
	database.c \
	sysinfo.c \
	telegramm.c \
	dsp.c \
	fsk_fm_demod.c \
	scrambler.c \
	image.c \
	ansage.c \
	main.c
cnetz_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/common/libcommon.a \
	$(ALSA_LIBS) \
	$(UHD_LIBS) \
	-lm

