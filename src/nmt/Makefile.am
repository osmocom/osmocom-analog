AM_CPPFLAGS = -Wall -g $(all_includes)

bin_PROGRAMS = \
	nmt

nmt_SOURCES = \
	nmt.c \
	dsp.c \
	frame.c \
	image.c \
	tones.c \
	announcement.c \
	main.c
nmt_LDADD = \
	$(COMMON_LA) \
	$(ALSA_LIBS) \
	$(top_builddir)/src/common/libcommon.a \
	-lm

