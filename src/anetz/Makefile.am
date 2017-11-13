AM_CPPFLAGS = -Wall -Wextra -g $(all_includes)

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
	$(top_builddir)/src/common/libgermanton.a \
	$(top_builddir)/src/common/libmobile.a \
	$(top_builddir)/src/common/libcommon.a \
	$(top_builddir)/src/libfilter/libfilter.a \
	$(ALSA_LIBS) \
	$(UHD_LIBS) \
	$(SOAPY_LIBS) \
	-lm

