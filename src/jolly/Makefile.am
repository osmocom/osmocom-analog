AM_CPPFLAGS = -Wall -Wextra -g $(all_includes)

bin_PROGRAMS = \
	jollycom

jollycom_SOURCES = \
	jolly.c \
	dsp.c \
	voice.c \
	main.c
jollycom_LDADD = \
	$(COMMON_LA) \
	../anetz/libgermanton.a \
	$(top_builddir)/src/libdebug/libdebug.a \
	$(top_builddir)/src/libmobile/libmobile.a \
	$(top_builddir)/src/libdisplay/libdisplay.a \
	$(top_builddir)/src/libgoertzel/libgoertzel.a \
	$(top_builddir)/src/libjitter/libjitter.a \
	$(top_builddir)/src/libsquelch/libsquelch.a \
	$(top_builddir)/src/libdtmf/libdtmf.a \
	$(top_builddir)/src/libtimer/libtimer.a \
	$(top_builddir)/src/libsamplerate/libsamplerate.a \
	$(top_builddir)/src/libemphasis/libemphasis.a \
	$(top_builddir)/src/libfilter/libfilter.a \
	$(top_builddir)/src/libwave/libwave.a \
	$(top_builddir)/src/libmncc/libmncc.a \
	$(top_builddir)/src/libsound/libsound.a \
	$(top_builddir)/src/libsample/libsample.a \
	$(top_builddir)/src/libfm/libfm.a \
	$(ALSA_LIBS) \
	-lm

if HAVE_SDR
jollycom_LDADD += \
	$(top_builddir)/src/libsdr/libsdr.a \
	$(top_builddir)/src/libfft/libfft.a \
	$(UHD_LIBS) \
	$(SOAPY_LIBS)
endif

