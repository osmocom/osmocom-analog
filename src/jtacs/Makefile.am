AM_CPPFLAGS = -Wall -Wextra -g $(all_includes)

bin_PROGRAMS = \
	jtacs

jtacs_SOURCES = \
	tones.c \
	stations.c \
	image.c \
	main.c

jtacs_LDADD = \
	$(COMMON_LA) \
	../amps/libamps.a \
	$(top_builddir)/src/common/libmobile.a \
	$(top_builddir)/src/common/libcommon.a \
	$(ALSA_LIBS) \
	$(UHD_LIBS) \
	$(SOAPY_LIBS) \
	-lm

