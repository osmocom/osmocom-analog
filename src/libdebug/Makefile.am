AM_CPPFLAGS = -Wall -Wextra -g $(all_includes)

noinst_LIBRARIES = libdebug.a

libdebug_a_SOURCES = \
	debug.c

if HAVE_SDR
AM_CPPFLAGS += -DHAVE_SDR
endif

