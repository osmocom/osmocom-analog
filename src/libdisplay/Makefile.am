AM_CPPFLAGS = -Wall -Wextra -g $(all_includes)

noinst_LIBRARIES = libdisplay.a

libdisplay_a_SOURCES = \
	display_status.c \
	display_wave.c \
	display_measurements.c

if HAVE_SDR
libdisplay_a_SOURCES += \
	display_iq.c \
	display_spectrum.c
endif

if HAVE_SDR
AM_CPPFLAGS += -DHAVE_SDR
endif

