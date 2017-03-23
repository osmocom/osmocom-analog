AUTOMAKE_OPTIONS = subdir-objects
AM_CPPFLAGS = -Wall -Wextra -g $(all_includes) $(UHD_CFLAGS) $(SOAPY_CFLAGS)

noinst_LIBRARIES = libcommon.a

libcommon_a_SOURCES = \
	../common/sample.c \
	../common/debug.c \
	../common/timer.c \
	../common/sound_alsa.c \
	../common/wave.c \
	../common/goertzel.c \
	../common/jitter.c \
	../common/loss.c \
	../common/filter.c \
	../common/dtmf.c \
	../common/samplerate.c \
	../common/call.c \
	../common/freiton.c \
	../common/besetztton.c \
	../common/testton.c \
	../common/mncc_sock.c \
	../common/cause.c \
	../common/emphasis.c \
	../common/compandor.c \
	../common/fft.c \
	../common/fm_modulation.c \
	../common/sender.c \
	../common/display_wave.c \
	../common/main_common.c

if HAVE_SDR
AM_CPPFLAGS += -DHAVE_SDR

libcommon_a_SOURCES += \
	../common/sdr.c \
	../common/display_iq.c \
	../common/display_spectrum.c
endif

if HAVE_UHD
AM_CPPFLAGS += -DHAVE_UHD

libcommon_a_SOURCES += \
	../common/uhd.c
endif

if HAVE_SOAPY
AM_CPPFLAGS += -DHAVE_SOAPY

libcommon_a_SOURCES += \
	../common/soapy.c
endif

