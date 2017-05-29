AC_PREREQ([2.59])
AC_INIT([Light MQTT], [0.1], [romuloceccon AT gmail.com])

AM_INIT_AUTOMAKE([foreign -Wall -Werror])

AM_PROG_AR
AC_PROG_CC
AC_PROG_LIBTOOL

AX_CHECK_COMPILE_FLAG([-std=c89])
AX_CHECK_COMPILE_FLAG([-std=gnu99])
AX_APPEND_COMPILE_FLAGS([-pedantic])

PKG_CHECK_MODULES([CHECK], [check >= 0.9.10])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile examples/Makefile tests/Makefile])

AC_OUTPUT
