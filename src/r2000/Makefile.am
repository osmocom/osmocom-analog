AM_CPPFLAGS = -Wall -Wextra -g $(all_includes)

bin_PROGRAMS = \
	radiocom2000

radiocom2000_SOURCES = \
	r2000.c \
	dsp.c \
	frame.c \
	tones.c \
	image.c \
	main.c
radiocom2000_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/libdebug/libdebug.a \
	$(top_builddir)/src/libmobile/libmobile.a \
	$(top_builddir)/src/libdisplay/libdisplay.a \
	$(top_builddir)/src/libcompandor/libcompandor.a \
	$(top_builddir)/src/libjitter/libjitter.a \
	$(top_builddir)/src/libhagelbarger/libhagelbarger.a \
	$(top_builddir)/src/libtimer/libtimer.a \
	$(top_builddir)/src/libsamplerate/libsamplerate.a \
	$(top_builddir)/src/libemphasis/libemphasis.a \
	$(top_builddir)/src/libfsk/libfsk.a \
	$(top_builddir)/src/libfm/libfm.a \
	$(top_builddir)/src/libfilter/libfilter.a \
	$(top_builddir)/src/libwave/libwave.a \
	$(top_builddir)/src/libmncc/libmncc.a \
	$(top_builddir)/src/libsound/libsound.a \
	$(top_builddir)/src/libsample/libsample.a \
	$(ALSA_LIBS) \
	$(UHD_LIBS) \
	$(SOAPY_LIBS) \
	-lm

if HAVE_SDR
radiocom2000_LDADD += \
	$(top_builddir)/src/libsdr/libsdr.a \
	$(top_builddir)/src/libfft/libfft.a
endif

