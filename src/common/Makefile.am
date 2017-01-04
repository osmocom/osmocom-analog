AUTOMAKE_OPTIONS = subdir-objects
AM_CPPFLAGS = -Wall -Wextra -g $(all_includes)

noinst_LIBRARIES = libcommon.a

libcommon_a_SOURCES = \
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
	../common/sender.c \
	../common/display_wave.c \
	../common/main_common.c

if HAVE_SDR
AM_CPPFLAGS += -DHAVE_SDR

libcommon_a_SOURCES += \
	../common/sdr.c
endif

