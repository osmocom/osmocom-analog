AUTOMAKE_OPTIONS = foreign

SUBDIRS = \
	liboptions \
	libdebug \
	libmobile \
	libdisplay \
	libsample \
	libimage \
	libsendevolumenregler \
	libcompandor \
	libgoertzel \
	libjitter \
	libsquelch \
	libhagelbarger \
	libdtmf \
	libtimer \
	libsamplerate \
	libscrambler \
	libemphasis \
	libfsk \
	libam \
	libfm \
	libfilter \
	libwave \
	libfft \
	libmncc \
	libclipper \
	libserial \
	libv27 \
	libmtp \
	libosmocc \
	libg711

if HAVE_ALSA
SUBDIRS += \
	libsound
endif

if HAVE_SDR
SUBDIRS += \
	libsdr
endif

SUBDIRS += \
	anetz \
	bnetz \
	cnetz \
	nmt \
	amps \
	tacs \
	jtacs \
	r2000 \
	imts \
	jolly \
	eurosignal \
	tv \
	radio \
	zeitansage \
	sim \
	fuvst

if HAVE_SDR
if HAVE_FUSE
SUBDIRS += \
	datenklo
endif
endif

SUBDIRS += \
	test

