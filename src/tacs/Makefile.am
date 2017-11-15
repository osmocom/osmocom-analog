AM_CPPFLAGS = -Wall -Wextra -g $(all_includes)

bin_PROGRAMS = \
	tacs

tacs_SOURCES = \
	tones.c \
	outoforder.c \
	stations.c \
	image.c \
	main.c

tacs_LDADD = \
	$(COMMON_LA) \
	../amps/libamps.a \
	$(top_builddir)/src/common/libmobile.a \
	$(top_builddir)/src/common/libcommon.a \
	$(top_builddir)/src/libtimer/libtimer.a \
	$(top_builddir)/src/libsamplerate/libsamplerate.a \
	$(top_builddir)/src/libfilter/libfilter.a \
	$(top_builddir)/src/libwave/libwave.a \
	$(ALSA_LIBS) \
	$(UHD_LIBS) \
	$(SOAPY_LIBS) \
	-lm

