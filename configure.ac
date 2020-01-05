AC_INIT(jush, 0.1, https://github.com/troglobit/jush/issues,, https://github.com/troglobit/jush/)
AC_CONFIG_AUX_DIR(aux)
AM_INIT_AUTOMAKE([1.11 foreign subdir-objects no-dist-gzip dist-xz])

AC_CONFIG_SRCDIR([src/jush.c])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])

AC_PROG_CC
AC_PROG_INSTALL
AC_HEADER_STDC

# Check for required packages
PKG_PROG_PKG_CONFIG

# Check for usually missing API's, which we can replace
AC_REPLACE_FUNCS([strlcpy strlcat])
AC_CONFIG_LIBOBJ_DIR([lib])

# The Debian packaged version is too old
PKG_CHECK_MODULES([editline], [libeditline >= 1.13.0])

AC_OUTPUT
