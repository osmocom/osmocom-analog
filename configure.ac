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

PKG_CHECK_MODULES(ALSA, alsa >= 1.0)

AC_OUTPUT(
    src/common/Makefile
    src/anetz/Makefile
    src/bnetz/Makefile
    src/Makefile
    Makefile)


