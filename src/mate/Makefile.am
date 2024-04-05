AM_CPPFLAGS = -Wall -Wextra -Wmissing-prototypes -g $(all_includes)

bin_PROGRAMS = \
	matesimulator

matesimulator_SOURCES = \
	matesimulator.c

matesimulator_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/liboptions/liboptions.a \
	$(top_builddir)/src/libdebug/libdebug.a \
	$(top_builddir)/src/libwave/libwave.a \
	$(top_builddir)/src/libsample/libsample.a \
	$(top_builddir)/src/liblogging/liblogging.a \
	$(LIBOSMOCORE_LIBS) \
	$(LIBOSMOCC_LIBS) \
	-lm

if HAVE_ALSA
matesimulator_LDADD += \
	$(top_builddir)/src/libsound/libsound.a \
	$(ALSA_LIBS)
endif

if HAVE_ALSA
AM_CPPFLAGS += -DHAVE_ALSA
endif

