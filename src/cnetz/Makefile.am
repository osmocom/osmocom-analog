AM_CPPFLAGS = -Wall -Wextra -Wmissing-prototypes -g $(all_includes)

bin_PROGRAMS = \
	cnetz

noinst_LIBRARIES = libcnetztones.a

libcnetztones_a_SOURCES = \
	ansage.c

cnetz_SOURCES = \
	cnetz.c \
	transaction.c \
	database.c \
	sysinfo.c \
	telegramm.c \
	dsp.c \
	fsk_demod.c \
	image.c \
	stations.c \
	main.c
cnetz_LDADD = \
	$(COMMON_LA) \
	../anetz/libgermanton.a \
	libcnetztones.a \
	$(top_builddir)/src/liboptions/liboptions.a \
	$(top_builddir)/src/libmobile/libmobile.a \
	$(top_builddir)/src/libdisplay/libdisplay.a \
	$(top_builddir)/src/libcompandor/libcompandor.a \
	$(top_builddir)/src/libjitter/libjitter.a \
	$(top_builddir)/src/libsamplerate/libsamplerate.a \
	$(top_builddir)/src/libscrambler/libscrambler.a \
	$(top_builddir)/src/libemphasis/libemphasis.a \
	$(top_builddir)/src/libfm/libfm.a \
	$(top_builddir)/src/libfilter/libfilter.a \
	$(top_builddir)/src/libwave/libwave.a \
	$(top_builddir)/src/libsample/libsample.a \
	$(top_builddir)/src/libaaimage/libaaimage.a \
	$(top_builddir)/src/liblogging/liblogging.a \
	$(LIBOSMOCORE_LIBS) \
	$(LIBOSMOCC_LIBS) \
	-lm

if HAVE_ALSA
cnetz_LDADD += \
	$(top_builddir)/src/libsound/libsound.a \
	$(ALSA_LIBS)
endif

if HAVE_SDR
cnetz_LDADD += \
	$(top_builddir)/src/libsdr/libsdr.a \
	$(top_builddir)/src/libam/libam.a \
	$(top_builddir)/src/libfft/libfft.a \
	$(UHD_LIBS) \
	$(SOAPY_LIBS)
endif

