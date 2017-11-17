AM_CPPFLAGS = -Wall -Wextra -g $(all_includes)

noinst_LIBRARIES = libcommon.a libmobile.a

libcommon_a_SOURCES = \
	sample.c \
	debug.c \
	display_wave.c \
	display_measurements.c

libmobile_a_SOURCES = \
	sender.c \
	call.c \
	display_status.c \
	main_mobile.c

if HAVE_SDR
AM_CPPFLAGS += -DHAVE_SDR
endif

