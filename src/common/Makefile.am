AM_CPPFLAGS = -Wall -Wextra -g $(all_includes) $(UHD_CFLAGS) $(SOAPY_CFLAGS)

noinst_LIBRARIES = libcommon.a libmobile.a

libcommon_a_SOURCES = \
	sample.c \
	debug.c \
	timer.c \
	sound_alsa.c \
	wave.c \
	goertzel.c \
	jitter.c \
	loss.c \
	iir_filter.c \
	dtmf.c \
	samplerate.c \
	emphasis.c \
	compandor.c \
	fft.c \
	fm_modulation.c \
	fsk.c \
	display_wave.c

libmobile_a_SOURCES = \
	sender.c \
	cause.c \
	call.c \
	testton.c \
	mncc_sock.c \
	hagelbarger.c \
	display_status.c \
	main_mobile.c

if HAVE_SDR
AM_CPPFLAGS += -DHAVE_SDR

libcommon_a_SOURCES += \
	sdr.c \
	display_iq.c \
	display_spectrum.c
endif

if HAVE_UHD
AM_CPPFLAGS += -DHAVE_UHD

libcommon_a_SOURCES += \
	uhd.c
endif

if HAVE_SOAPY
AM_CPPFLAGS += -DHAVE_SOAPY

libcommon_a_SOURCES += \
	soapy.c
endif

