
# define package name and version:
AC_INIT([KeyVal], [0.2.0], [cheetomonster@gmail.com])

# make sure there's an 'ar' somewhere:
AM_PROG_AR

# convert Makefile.am -> Makefile.in:
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

# we'll generate generic libtool files that could become either
# static or dynamic libs:
AM_PROG_LIBTOOL

# make sure we have a C compiler:
AC_PROG_CC
# I use C99 stuff, so add that flag:
AC_PROG_CC_C99

# make sure our needed include files exist:
AC_CHECK_HEADER([errno.h])
AC_CHECK_HEADER([stdio.h])
AC_CHECK_HEADER([stdlib.h])
AC_CHECK_HEADER([string.h])

# make sure our local files exist:
AC_CONFIG_SRCDIR([KeyVal.c])
AC_CONFIG_SRCDIR([KeyVal_load.c])
AC_CONFIG_SRCDIR([KeyVal.h])
AC_CONFIG_SRCDIR([test.c])

# this is C-language project:
AC_LANG(C)

# convert Makefile.in -> Makefile:
AC_CONFIG_FILES([Makefile])
AC_CONFIG_MACRO_DIR([m4])
AC_OUTPUT

