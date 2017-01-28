AC_PREREQ([2.59])
AC_INIT([Light MQTT], [0.1], [romuloceccon AT gmail.com])

AM_INIT_AUTOMAKE([foreign -Wall -Werror])

AM_PROG_AR
AC_PROG_CC
AC_PROG_LIBTOOL

PKG_CHECK_MODULES([CHECK], [check >= 0.9.10])

AC_CONFIG_MACRO_DIRS([m4])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile tests/Makefile])

AC_OUTPUT
