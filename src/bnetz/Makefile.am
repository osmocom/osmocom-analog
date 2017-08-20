AUTOMAKE_OPTIONS = subdir-objects
AM_CPPFLAGS = -Wall -Wextra -g $(all_includes)

bin_PROGRAMS = \
	bnetz

bnetz_SOURCES = \
	bnetz.c \
	telegramm.c \
	dsp.c \
	stations.c \
	image.c \
	$(top_builddir)/src/anetz/freiton.c \
	$(top_builddir)/src/anetz/besetztton.c \
	ansage.c \
	main.c
bnetz_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/common/libmobile.a \
	$(top_builddir)/src/common/libcommon.a \
	$(ALSA_LIBS) \
	$(UHD_LIBS) \
	$(SOAPY_LIBS) \
	-lm

