AM_CPPFLAGS = -Wall -Wextra -g $(all_includes)

if HAVE_ALSA
bin_PROGRAMS = \
	dcf77

dcf77_SOURCES = \
	dcf77.c \
	weather.c \
	cities.c \
	image.c \
	main.c
dcf77_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/liboptions/liboptions.a \
	$(top_builddir)/src/libdisplay/libdisplay.a \
	$(top_builddir)/src/libfilter/libfilter.a \
	$(top_builddir)/src/libwave/libwave.a \
	$(top_builddir)/src/libsample/libsample.a \
	$(top_builddir)/src/libsound/libsound.a \
	$(top_builddir)/src/libaaimage/libaaimage.a \
	$(top_builddir)/src/liblogging/liblogging.a \
	$(LIBOSMOCORE_LIBS) \
	$(ALSA_LIBS) \
	-lm
endif

