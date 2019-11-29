dnl Process this file with autoconf to produce a configure script
AC_INIT([abcnetz],
	m4_esyscmd([./git-version-gen .tarball-version]),
	[jolly@eversberg.eu])

AM_INIT_AUTOMAKE([dist-bzip2])

AC_CONFIG_MACRO_DIR([m4])

dnl kernel style compile messages
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl checks for programs
AC_PROG_MAKE_SET
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_RANLIB

dnl checks for header files
AC_HEADER_STDC

dnl Checks for typedefs, structures and compiler characteristics

AC_CANONICAL_HOST

AC_CHECK_LIB([m], [main])
AC_CHECK_LIB([pthread], [main])

with_sdr=no
AC_ARG_WITH([alsa], [AS_HELP_STRING([--with-alsa], [compile with Alsa driver @<:@default=check@:>@]) ], [], [with_alsa="check"])
AC_ARG_WITH([uhd], [AS_HELP_STRING([--with-uhd], [compile with UHD driver @<:@default=check@:>@]) ], [], [with_uhd="check"])
AC_ARG_WITH([soapy], [AS_HELP_STRING([--with-soapy], [compile with SoapySDR driver @<:@default=check@:>@]) ], [], [with_soapy="check"])
AC_ARG_WITH([imagemagick], [AS_HELP_STRING([--with-imagemagick], [compile with ImageMagick support @<:@default=check@:>@]) ], [], [with_imagemagick="check"])
AC_ARG_WITH([fuse], [AS_HELP_STRING([--with-fuse], [compile with FUSE support @<:@default=check@:>@]) ], [], [with_fuse="check"])
AS_IF([test "x$with_alsa" != xno], [PKG_CHECK_MODULES(ALSA, alsa >= 1.0, with_alsa=yes, with_alsa=no)])
AS_IF([test "x$with_uhd" != xno], [PKG_CHECK_MODULES(UHD, uhd >= 3.0.0, with_sdr=yes with_uhd=yes, with_uhd=no)])
AS_IF([test "x$with_soapy" != xno], [PKG_CHECK_MODULES(SOAPY, SoapySDR >= 0.5.0, with_sdr=yes with_soapy=yes, with_soapy=no)])
AS_IF([test "x$with_imagemagick" != xno], [PKG_CHECK_MODULES(IMAGEMAGICK, ImageMagick >= 7.0.0, with_imagemagick=yes, with_imagemagick=no)])
AS_IF([test "x$with_fuse" != xno], with_fuse=check)
AS_IF([test "x$with_fuse" == xcheck], [PKG_CHECK_MODULES(FUSE, fuse3 >= 0.30.0, with_fuse=yes, with_fuse=check)])
AS_IF([test "x$with_fuse" == xcheck], [PKG_CHECK_MODULES(FUSE, fuse2 >= 0.29.0, with_fuse=yes, with_fuse=check)])
AS_IF([test "x$with_fuse" == xcheck], [PKG_CHECK_MODULES(FUSE, fuse >= 0.29.0, with_fuse=yes, with_fuse=check)])
AS_IF([test "x$with_fuse" == xcheck], with_fuse=no)
AM_CONDITIONAL(HAVE_ALSA, test "x$with_alsa" == "xyes" )
AM_CONDITIONAL(HAVE_UHD, test "x$with_uhd" == "xyes" )
AM_CONDITIONAL(HAVE_SOAPY, test "x$with_soapy" == "xyes" )
AM_CONDITIONAL(HAVE_SDR, test "x$with_sdr" == "xyes" )
AM_CONDITIONAL(HAVE_MAGICK, test "x$with_imagemagick" == "xyes" )
AM_CONDITIONAL(HAVE_FUSE, test "x$with_fuse" == "xyes" )
AS_IF([test "x$with_alsa" == "xyes"],[AC_MSG_NOTICE( Compiling with Alsa support )], [AC_MSG_NOTICE( Alsa sound card not supported. Consider adjusting the PKG_CONFIG_PATH environment variable if you installed software in a non-standard prefix. )])
AS_IF([test "x$with_uhd" == "xyes"],[AC_MSG_NOTICE( Compiling with UHD SDR support )], [AC_MSG_NOTICE( UHD SDR not supported. Consider adjusting the PKG_CONFIG_PATH environment variable if you installed software in a non-standard prefix. )])
AS_IF([test "x$with_soapy" == "xyes"],[AC_MSG_NOTICE( Compiling with SoapySDR support )], [AC_MSG_NOTICE( SoapySDR not supported. Consider adjusting the PKG_CONFIG_PATH environment variable if you installed software in a non-standard prefix. )])
AS_IF([test "x$with_imagemagick" == "xyes"],[AC_MSG_NOTICE( Compiling with ImageMagick )],[AC_MSG_NOTICE( ImageMagick not supported. Consider adjusting the PKG_CONFIG_PATH environment variable if you installed software in a non-standard prefix. )])
AS_IF([test "x$with_fuse" == "xyes"],[AC_MSG_NOTICE( Compiling with FUSE )],[AC_MSG_NOTICE( FUSE not supported. There will be no analog modem support. Consider adjusting the PKG_CONFIG_PATH environment variable if you installed software in a non-standard prefix. )])

AS_IF([test "x$with_alsa" != "xyes" -a "x$with_sdr" != "xyes"],[AC_MSG_FAILURE( Without sound nor SDR support this project does not make sense. Please support sound card for analog transceivers or better SDR!" )],[])

AC_OUTPUT(
    src/liboptions/Makefile
    src/libdebug/Makefile
    src/libmobile/Makefile
    src/libdisplay/Makefile
    src/libimage/Makefile
    src/libsendevolumenregler/Makefile
    src/libcompandor/Makefile
    src/libgoertzel/Makefile
    src/libjitter/Makefile
    src/libsquelch/Makefile
    src/libhagelbarger/Makefile
    src/libdtmf/Makefile
    src/libtimer/Makefile
    src/libsamplerate/Makefile
    src/libscrambler/Makefile
    src/libemphasis/Makefile
    src/libfsk/Makefile
    src/libam/Makefile
    src/libfm/Makefile
    src/libfilter/Makefile
    src/libwave/Makefile
    src/libfft/Makefile
    src/libmncc/Makefile
    src/libsound/Makefile
    src/libsdr/Makefile
    src/libsample/Makefile
    src/libclipper/Makefile
    src/anetz/Makefile
    src/bnetz/Makefile
    src/cnetz/Makefile
    src/nmt/Makefile
    src/amps/Makefile
    src/tacs/Makefile
    src/jtacs/Makefile
    src/r2000/Makefile
    src/imts/Makefile
    src/jolly/Makefile
    src/eurosignal/Makefile
    src/tv/Makefile
    src/radio/Makefile
    src/datenklo/Makefile
    src/test/Makefile
    src/Makefile
    sim/Makefile
    Makefile)


