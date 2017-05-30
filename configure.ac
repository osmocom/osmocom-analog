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

with_sdr=no
AC_ARG_WITH([uhd], [AS_HELP_STRING([--with-uhd], [compile with UHD driver @<:@default=check@:>@]) ], [], [with_uhd="check"])
AC_ARG_WITH([soapy], [AS_HELP_STRING([--with-soapy], [compile with SoapySDR driver @<:@default=check@:>@]) ], [], [with_soapy="check"])
AS_IF([test "x$with_uhd" != xno], [PKG_CHECK_MODULES(UHD, uhd >= 3.0.0, with_sdr=yes with_uhd=yes, with_uhd=no)])
AS_IF([test "x$with_soapy" != xno], [PKG_CHECK_MODULES(SOAPY, SoapySDR >= 0.6.0, with_sdr=yes with_soapy=yes, with_soapy=no)])
AM_CONDITIONAL(HAVE_UHD, test "x$with_uhd" == "xyes" )
AM_CONDITIONAL(HAVE_SOAPY, test "x$with_soapy" == "xyes" )
AM_CONDITIONAL(HAVE_SDR, test "x$with_sdr" == "xyes" )
AS_IF([test "x$with_uhd" == "xyes"],[AC_MSG_NOTICE( Compiling with UHD SDR support )], [])
AS_IF([test "x$with_soapy" == "xyes"],[AC_MSG_NOTICE( Compiling with SoapySDR support )], [])

AC_OUTPUT(
    src/common/Makefile
    src/anetz/Makefile
    src/bnetz/Makefile
    src/cnetz/Makefile
    src/nmt/Makefile
    src/amps/Makefile
    src/tacs/Makefile
    src/test/Makefile
    src/Makefile
    sim/Makefile
    Makefile)


