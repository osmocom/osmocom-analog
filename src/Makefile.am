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
	libclipper \
	libserial \
	libv27 \
	libmtp \
	libosmocc \
	libg711 \
	libaaimage

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
	r2000 \
	imts \
	mpt1327 \
	jolly \
	eurosignal \
	pocsag \
	fuenf \
	tv \
	radio \
	zeitansage \
	sim \
	magnetic \
	fuvst \
	dcf77

if HAVE_ALSA
if HAVE_FUSE
SUBDIRS += \
	datenklo
endif
endif

SUBDIRS += \
	test

