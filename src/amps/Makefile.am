AM_CPPFLAGS = -Wall -Wextra -g $(all_includes)

noinst_LIBRARIES = libamps.a

bin_PROGRAMS = \
	amps

libamps_a_SOURCES = \
	amps_tacs_main.c \
	amps.c \
	transaction.c \
	frame.c \
	dsp.c \
	sysinfo.c

amps_SOURCES = \
	tones.c \
	noanswer.c \
	outoforder.c \
	invalidnumber.c \
	congestion.c \
	stations.c \
	image.c \
	main.c

amps_LDADD = \
	$(COMMON_LA) \
	libamps.a \
	$(top_builddir)/src/common/libmobile.a \
	$(top_builddir)/src/common/libcommon.a \
	$(top_builddir)/src/libsamplerate/libsamplerate.a \
	$(top_builddir)/src/libfilter/libfilter.a \
	$(ALSA_LIBS) \
	$(UHD_LIBS) \
	$(SOAPY_LIBS) \
	-lm

