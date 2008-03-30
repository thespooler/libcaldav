#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)

AC_INIT([libcaldav], m4_esyscmd([sh ./version.sh | tr -d '\n']), [mir@datanom.net])

LIBVERSION=m4_esyscmd([sh ./version.sh LIBTOOL | tr -d '\n'])
AC_SUBST(LIBVERSION)

AC_CANONICAL_SYSTEM

# Generate documentation
# API
DX_HTML_FEATURE(OFF)
DX_CHM_FEATURE(OFF)
DX_CHI_FEATURE(OFF)
DX_MAN_FEATURE(ON)
DX_RTF_FEATURE(OFF)
DX_XML_FEATURE(OFF)
DX_PDF_FEATURE(ON)
DX_PS_FEATURE(OFF)
DX_INIT_DOXYGEN(libcaldav, doxygen-api.cfg, doc)

AM_INIT_AUTOMAKE([dist-bzip2 1.9.6])
AM_MAINTAINER_MODE
AC_CONFIG_HEADER([config.h])

if test $USE_MAINTAINER_MODE = yes; then
		CFLAGS="-g -Wall"
else
		CFLAGS="$CFLAGS -O2 -Wall"
fi

# Checks for programs.
AC_PROG_CC
AC_LANG_C
AC_ISC_POSIX
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_HEADER_STDC
AC_PROG_CPP
AM_PROG_LIBTOOL
AC_PROG_INSTALL

# Checks for libraries.
PKG_CHECK_MODULES(CURL, [libcurl >= 7.15.5])
AC_SUBST(CURL_CFLAGS)
AC_SUBST(CURL_LIBS)

PKG_CHECK_MODULES(GLIB, [glib-2.0 >= 2.6])
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

#PKG_CHECK_MODULES(OPENSSL, [openssl >= 0.9.8])
#AC_SUBST(OPENSSL_CFLAGS)
#AC_SUBST(OPENSSL_LIBS)

# Checks for header files.
AC_HEADER_STDC

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_STRUCT_TM

# Checks for library functions.

# Should we build test program
AC_ARG_ENABLE([caldav], 
		[AC_HELP_STRING(--enable-caldav, Build test application (default=no))],
		[case "${enableval}" in
			yes) caldav=true ;;
			no) caldav=false ;;
			*) AC_MSG_ERROR([bad value "${enableval}" for --enable-caldav]) ;;
			esac], [caldav=false])
AM_CONDITIONAL([BUILD_CALDAV], [test x$caldav = xtrue])

AC_CONFIG_COMMANDS(
    summary,
    [echo ""
    echo "libcaldav will be compiled with these settings:"
    echo ""
    echo -e "\tBuild caldav-test:\t$caldav"
	echo ""
	echo -e "Now run make to build the library"
	echo ""
	echo -e "Please send bugs or feature requests to the maintainer(s)."
	echo -e "Email addresses can be found in the AUTHORS file."
    echo ""],
    [caldav=${caldav}]
)

AC_OUTPUT([
	Makefile
	libcaldav.pc
	src/Makefile
	test/Makefile
	test/src/Makefile
	test/ics/Makefile
])