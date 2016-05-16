AM_CPPFLAGS = -Wall -g $(all_includes)

bin_PROGRAMS = \
	cnetz

cnetz_SOURCES = \
	cnetz.c \
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
	-lm

