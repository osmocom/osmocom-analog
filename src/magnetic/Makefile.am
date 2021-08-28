AM_CPPFLAGS = -Wall -Wextra -g $(all_includes)

bin_PROGRAMS = \
	cnetz_magnetic

cnetz_magnetic_SOURCES = \
	iso7811.c \
	image.c \
	main.c

cnetz_magnetic_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/libdebug/libdebug.a \
	$(top_builddir)/src/liboptions/liboptions.a \
	$(top_builddir)/src/libwave/libwave.a \
	$(top_builddir)/src/libaaimage/libaaimage.a \
	-lm

if HAVE_ALSA
cnetz_magnetic_LDADD += \
        $(top_builddir)/src/libsound/libsound.a \
        $(ALSA_LIBS)
endif

if HAVE_ALSA
AM_CPPFLAGS += -DHAVE_ALSA
endif

