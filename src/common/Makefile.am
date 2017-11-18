AM_CPPFLAGS = -Wall -Wextra -g $(all_includes)

noinst_LIBRARIES = libcommon.a

libcommon_a_SOURCES = \
	debug.c \
	display_wave.c \
	display_measurements.c

if HAVE_SDR
AM_CPPFLAGS += -DHAVE_SDR
endif

