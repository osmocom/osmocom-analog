AM_CPPFLAGS = -Wall -Wextra -g $(all_includes) $(GRAPHICSMAGICK_CFLAGS) $(IMAGEMAGICK_CFLAGS) $(UHD_CFLAGS) $(SOAPY_CFLAGS)

noinst_LIBRARIES = libcommon.a libmobile.a libimage.a

libcommon_a_SOURCES = \
	sample.c \
	debug.c \
	sound_alsa.c \
	goertzel.c \
	jitter.c \
	emphasis.c \
	compandor.c \
	fft.c \
	fm_modulation.c \
	fsk.c \
	display_wave.c \
	display_measurements.c \
	squelch.c

libmobile_a_SOURCES = \
	sender.c \
	cause.c \
	call.c \
	testton.c \
	mncc_console.c \
	mncc_cross.c \
	mncc_sock.c \
	hagelbarger.c \
	display_status.c \
	main_mobile.c

libimage_a_SOURCES = \
	img.c

if HAVE_SDR
AM_CPPFLAGS += -DHAVE_SDR

libcommon_a_SOURCES += \
	sdr_config.c \
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

if ENABLE_MAGICK
AM_CPPFLAGS += -DWITH_MAGICK
endif

