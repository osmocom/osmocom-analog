AM_CPPFLAGS = -Wall -g $(all_includes)

noinst_PROGRAMS = \
	test_filter \
	test_compandor \
	test_emphasis \
	test_dms \
	test_sms \
	test_performance \
	test_hagelbarger

test_filter_SOURCES = test_filter.c dummy.c

test_filter_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/libmobile/libmobile.a \
	$(top_builddir)/src/libdisplay/libdisplay.a \
	$(top_builddir)/src/common/libcommon.a \
	$(top_builddir)/src/libjitter/libjitter.a \
	$(top_builddir)/src/libtimer/libtimer.a \
	$(top_builddir)/src/libsamplerate/libsamplerate.a \
	$(top_builddir)/src/libemphasis/libemphasis.a \
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
test_filter_LDADD += \
	$(top_builddir)/src/libsdr/libsdr.a \
	$(top_builddir)/src/libfft/libfft.a \
	$(top_builddir)/src/libfm/libfm.a
endif

test_compandor_SOURCES = dummy.x test_compandor.c

test_compandor_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/libmobile/libmobile.a \
	$(top_builddir)/src/libcompandor/libcompandor.a \
	$(top_builddir)/src/common/libcommon.a \
	$(ALSA_LIBS) \
	$(UHD_LIBS) \
	$(SOAPY_LIBS) \
	-lm

test_emphasis_SOURCES = test_emphasis.c dummy.c

test_emphasis_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/libmobile/libmobile.a \
	$(top_builddir)/src/libdisplay/libdisplay.a \
	$(top_builddir)/src/common/libcommon.a \
	$(top_builddir)/src/libjitter/libjitter.a \
	$(top_builddir)/src/libtimer/libtimer.a \
	$(top_builddir)/src/libsamplerate/libsamplerate.a \
	$(top_builddir)/src/libemphasis/libemphasis.a \
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
test_emphasis_LDADD += \
	$(top_builddir)/src/libsdr/libsdr.a \
	$(top_builddir)/src/libfft/libfft.a \
	$(top_builddir)/src/libfm/libfm.a
endif

test_dms_SOURCES = test_dms.c dummy.c

test_dms_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/libmobile/libmobile.a \
	$(top_builddir)/src/libdisplay/libdisplay.a \
	$(top_builddir)/src/nmt/libdmssms.a \
	$(top_builddir)/src/common/libcommon.a \
	$(top_builddir)/src/libjitter/libjitter.a \
	$(top_builddir)/src/libtimer/libtimer.a \
	$(top_builddir)/src/libsamplerate/libsamplerate.a \
	$(top_builddir)/src/libemphasis/libemphasis.a \
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
test_dms_LDADD += \
	$(top_builddir)/src/libsdr/libsdr.a \
	$(top_builddir)/src/libfft/libfft.a \
	$(top_builddir)/src/libfm/libfm.a
endif

test_sms_SOURCES = dummy.c test_sms.c

test_sms_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/libmobile/libmobile.a \
	$(top_builddir)/src/libdisplay/libdisplay.a \
	$(top_builddir)/src/nmt/libdmssms.a \
	$(top_builddir)/src/common/libcommon.a \
	$(top_builddir)/src/libjitter/libjitter.a \
	$(top_builddir)/src/libtimer/libtimer.a \
	$(top_builddir)/src/libsamplerate/libsamplerate.a \
	$(top_builddir)/src/libemphasis/libemphasis.a \
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
test_sms_LDADD += \
	$(top_builddir)/src/libsdr/libsdr.a \
	$(top_builddir)/src/libfft/libfft.a \
	$(top_builddir)/src/libfm/libfm.a
endif

test_performance_SOURCES = dummy.c test_performance.c

test_performance_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/libmobile/libmobile.a \
	$(top_builddir)/src/common/libcommon.a \
	$(top_builddir)/src/libfm/libfm.a \
	$(top_builddir)/src/libfilter/libfilter.a \
	$(ALSA_LIBS) \
	$(UHD_LIBS) \
	$(SOAPY_LIBS) \
	-lm

test_hagelbarger_SOURCES = dummy.c test_hagelbarger.c

test_hagelbarger_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/libmobile/libmobile.a \
	$(top_builddir)/src/libhagelbarger/libhagelbarger.a \
	$(top_builddir)/src/common/libcommon.a \
	$(ALSA_LIBS) \
	$(UHD_LIBS) \
	$(SOAPY_LIBS) \
	-lm

