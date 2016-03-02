AM_CPPFLAGS = -Wall -g $(all_includes)

bin_PROGRAMS = \
	anetz

anetz_SOURCES = \
	anetz.c \
	dsp.c \
	stations.c \
	image.c \
	main.c
anetz_LDADD = \
	$(COMMON_LA) \
	$(ALSA_LIBS) \
	$(top_builddir)/src/common/libcommon.a \
	-lm

