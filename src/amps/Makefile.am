AM_CPPFLAGS = -Wall -Wextra -g $(all_includes)

bin_PROGRAMS = \
	amps

amps_SOURCES = \
	amps.c \
	transaction.c \
	frame.c \
	dsp.c \
	sysinfo.c \
	image.c \
	tones.c \
	noanswer.c \
	outoforder.c \
	invalidnumber.c \
	congestion.c \
	stations.c \
	main.c
amps_LDADD = \
	$(COMMON_LA) \
	$(top_builddir)/src/common/libcommon.a \
	$(ALSA_LIBS) \
	$(UHD_LIBS) \
	$(SOAPY_LIBS) \
	-lm

