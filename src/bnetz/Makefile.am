AM_CPPFLAGS = -Wall -g $(all_includes)

bin_PROGRAMS = \
	bnetz

bnetz_SOURCES = \
	bnetz.c \
	dsp.c \
	stations.c \
	image.c \
	ansage.c \
	main.c
bnetz_LDADD = \
	$(COMMON_LA) \
	$(ALSA_LIBS) \
	$(top_builddir)/src/common/libcommon.a \
	-lm

