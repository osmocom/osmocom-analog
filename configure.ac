dnl Process this file with autoconf to produce a configure script
AC_INIT([abcnetz],
	m4_esyscmd([./git-version-gen .tarball-version]),
	[authors@their.domains])

AM_INIT_AUTOMAKE([dist-bzip2])

AC_CONFIG_MACRO_DIR([m4])

dnl kernel style compile messages
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl checks for programs
AC_PROG_MAKE_SET
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
LT_INIT

dnl checks for header files
AC_HEADER_STDC

dnl Checks for typedefs, structures and compiler characteristics

AC_CANONICAL_HOST

AC_CHECK_LIB([m], [main])
AC_CHECK_LIB([pthread], [main])

PKG_CHECK_MODULES(ALSA, alsa >= 1.0)

# disabled due to problems with api compatibilty with imagemagick
#AC_ARG_ENABLE(graphicsmagick,
#	[AS_HELP_STRING(
#		[--disable-graphicsmagick],
#		[Disable building graphicsmagick]
#	)],
#	[enable_graphicsmagick=$enableval], [enable_graphicsmagick="yes"])
#if test x"$enable_graphicsmagick" = x"yes"
#then
#	PKG_CHECK_MODULES(GRAPHICSMAGICK, GraphicsMagick >= 1.3.16, , enable_graphicsmagick=no)
#fi
#if test x"$enable_graphicsmagick" = x"yes"
#then
#	somethingmagick=yes
#fi

AC_ARG_ENABLE(imagemagick,
	[AS_HELP_STRING(
		[--disable-imagemagick],
		[Disable building imagemagick]
	)],
	[enable_imagemagick=$enableval], [enable_imagemagick="yes"])
if test x"$enable_imagemagick" = x"yes"
then
	PKG_CHECK_MODULES(IMAGEMAGICK, ImageMagick >= 6.0.0, , enable_imagemagick=no)
fi
if test x"$enable_imagemagick" = x"yes"
then
	somethingmagick=yes
fi

AM_CONDITIONAL(ENABLE_MAGICK, test x"$somethingmagick" = x"yes")

with_sdr=no
AC_ARG_WITH([uhd], [AS_HELP_STRING([--with-uhd], [compile with UHD driver @<:@default=check@:>@]) ], [], [with_uhd="check"])
AC_ARG_WITH([soapy], [AS_HELP_STRING([--with-soapy], [compile with SoapySDR driver @<:@default=check@:>@]) ], [], [with_soapy="check"])
AS_IF([test "x$with_uhd" != xno], [PKG_CHECK_MODULES(UHD, uhd >= 3.0.0, with_sdr=yes with_uhd=yes, with_uhd=no)])
AS_IF([test "x$with_soapy" != xno], [PKG_CHECK_MODULES(SOAPY, SoapySDR >= 0.6.0, with_sdr=yes with_soapy=yes, with_soapy=no)])
AM_CONDITIONAL(HAVE_UHD, test "x$with_uhd" == "xyes" )
AM_CONDITIONAL(HAVE_SOAPY, test "x$with_soapy" == "xyes" )
AM_CONDITIONAL(HAVE_SDR, test "x$with_sdr" == "xyes" )
AS_IF([test "x$with_uhd" == "xyes"],[AC_MSG_NOTICE( Compiling with UHD SDR support )], [AC_MSG_NOTICE( UHD SDR not supported. Consider adjusting the PKG_CONFIG_PATH environment variable if you installed software in a non-standard prefix. )])
AS_IF([test "x$with_soapy" == "xyes"],[AC_MSG_NOTICE( Compiling with SoapySDR support )], [AC_MSG_NOTICE( SoapySDR not supported. Consider adjusting the PKG_CONFIG_PATH environment variable if you installed software in a non-standard prefix. )])
AS_IF([test "x$somethingmagick" == "xyes"],[AC_MSG_NOTICE( Compiling with ImageMagick )],[AC_MSG_NOTICE( ImageMagick not supported )])

AC_OUTPUT(
    src/libtimer/Makefile
    src/libsamplerate/Makefile
    src/libfilter/Makefile
    src/common/Makefile
    src/anetz/Makefile
    src/bnetz/Makefile
    src/cnetz/Makefile
    src/nmt/Makefile
    src/amps/Makefile
    src/tacs/Makefile
    src/jtacs/Makefile
    src/r2000/Makefile
    src/tv/Makefile
    src/test/Makefile
    src/Makefile
    sim/Makefile
    Makefile)


