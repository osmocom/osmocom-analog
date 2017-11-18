AM_CPPFLAGS = -Wall -Wextra -g $(all_includes)

noinst_LIBRARIES = libmobile.a

libmobile_a_SOURCES = \
	sender.c \
	call.c \
	display_status.c \
	main_mobile.c

if HAVE_SDR
AM_CPPFLAGS += -DHAVE_SDR
endif

