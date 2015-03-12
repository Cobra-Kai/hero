AC_INIT([hero], [0.1], [bug-hero@cobra-kai.com])
AM_INIT_AUTOMAKE([-Wall -W foreign])
AC_PROG_CC
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
 Makefile
])
AM_PATH_SDL2([2.0.0],:,AC_MSG_ERROR([*** SDL version 2.0.0 not found!]))

dnl Requires platforms that use pkg-config
PKG_CHECK_MODULES([GL],[gl])
PKG_CHECK_MODULES([GLU],[glu])

AC_CHECK_LIB([m],[fmod])

AC_OUTPUT
