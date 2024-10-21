AM_CPPFLAGS = -Wall -Wextra -Wmissing-prototypes -g $(all_includes)

noinst_LIBRARIES = libdmssms.a

bin_PROGRAMS = \
	nmt

libdmssms_a_SOURCES = \
	dms.c \
	sms.c

nmt_SOURCES = \
	nmt.c \
	countries.c \
	transaction.c \
	dsp.c \
	frame.c \
	image.c \
	main.c
nmt_LDADD = \
	$(COMMON_LA) \
	libdmssms.a \
	$(top_builddir)/src/liboptions/liboptions.a \
	$(top_builddir)/src/libmobile/libmobile.a \
	$(top_builddir)/src/libtones/libtones.a \
	$(top_builddir)/src/libdisplay/libdisplay.a \
	$(top_builddir)/src/libcompandor/libcompandor.a \
	$(top_builddir)/src/libgoertzel/libgoertzel.a \
	$(top_builddir)/src/libjitter/libjitter.a \
	$(top_builddir)/src/libhagelbarger/libhagelbarger.a \
	$(top_builddir)/src/libdtmf/libdtmf.a \
	$(top_builddir)/src/libsamplerate/libsamplerate.a \
	$(top_builddir)/src/libemphasis/libemphasis.a \
	$(top_builddir)/src/libfsk/libfsk.a \
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
nmt_LDADD += \
	$(top_builddir)/src/libsound/libsound.a \
	$(ALSA_LIBS)
endif

if HAVE_SDR
nmt_LDADD += \
	$(top_builddir)/src/libsdr/libsdr.a \
	$(top_builddir)/src/libam/libam.a \
	$(top_builddir)/src/libfft/libfft.a \
	$(UHD_LIBS) \
	$(SOAPY_LIBS)
endif

