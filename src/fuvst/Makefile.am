AM_CPPFLAGS = -Wall -Wextra -Wmissing-prototypes -g $(all_includes)

if HAVE_ALSA
bin_PROGRAMS = \
	fuvst fuvst_sniffer

fuvst_SOURCES = \
	fuvst.c \
	mup.c \
	systemmeldungen.c \
	image.c \
	main.c
fuvst_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/liboptions/liboptions.a \
	$(top_builddir)/src/libmobile/libmobile.a \
	$(top_builddir)/src/libtones/libtones.a \
	$(top_builddir)/src/libdisplay/libdisplay.a \
	$(top_builddir)/src/libcompandor/libcompandor.a \
	$(top_builddir)/src/libjitter/libjitter.a \
	$(top_builddir)/src/libsamplerate/libsamplerate.a \
	$(top_builddir)/src/libemphasis/libemphasis.a \
	$(top_builddir)/src/libfm/libfm.a \
	$(top_builddir)/src/libv27/libv27.a \
	$(top_builddir)/src/libmtp/libmtp.a \
	$(top_builddir)/src/libfilter/libfilter.a \
	$(top_builddir)/src/libwave/libwave.a \
	$(top_builddir)/src/libsample/libsample.a \
	$(top_builddir)/src/libsound/libsound.a \
	$(top_builddir)/src/libaaimage/libaaimage.a \
	$(top_builddir)/src/liblogging/liblogging.a \
	$(LIBOSMOCORE_LIBS) \
	$(LIBOSMOCC_LIBS) \
	$(ALSA_LIBS) \
	-lm

fuvst_sniffer_SOURCES = \
	sniffer.c
fuvst_sniffer_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/liboptions/liboptions.a \
	$(top_builddir)/src/libmobile/libmobile.a \
	$(top_builddir)/src/libtones/libtones.a \
	$(top_builddir)/src/libdisplay/libdisplay.a \
	$(top_builddir)/src/libcompandor/libcompandor.a \
	$(top_builddir)/src/libjitter/libjitter.a \
	$(top_builddir)/src/libsamplerate/libsamplerate.a \
	$(top_builddir)/src/libemphasis/libemphasis.a \
	$(top_builddir)/src/libfm/libfm.a \
	$(top_builddir)/src/libv27/libv27.a \
	$(top_builddir)/src/libmtp/libmtp.a \
	$(top_builddir)/src/libfilter/libfilter.a \
	$(top_builddir)/src/libwave/libwave.a \
	$(top_builddir)/src/libsample/libsample.a \
	$(top_builddir)/src/libsound/libsound.a \
	$(top_builddir)/src/libaaimage/libaaimage.a \
	$(top_builddir)/src/liblogging/liblogging.a \
	$(LIBOSMOCORE_LIBS) \
	$(LIBOSMOCC_LIBS) \
	$(ALSA_LIBS) \
	-lm

if HAVE_SDR
fuvst_LDADD += \
	$(top_builddir)/src/libsdr/libsdr.a \
	$(top_builddir)/src/libam/libam.a \
	$(top_builddir)/src/libfft/libfft.a \
	$(UHD_LIBS) \
	$(SOAPY_LIBS)

fuvst_sniffer_LDADD += \
	$(top_builddir)/src/libsdr/libsdr.a \
	$(top_builddir)/src/libam/libam.a \
	$(top_builddir)/src/libfft/libfft.a \
	$(UHD_LIBS) \
	$(SOAPY_LIBS)
endif

endif

