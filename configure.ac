#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
#  Copyright 2010 Alan Manuel K. Gloria
#  
#  This file is part of Orb C Implementation
#
#  Orb C Implementation is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  Orb C Implementation is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with Orb C Implementation.  If not, see <http://www.gnu.org/licenses/>.

AC_PREREQ([2.63])
AC_INIT([Orb], [0.1], [almkglor@gmail.com])
AM_INIT_AUTOMAKE([foreign])

AC_CONFIG_SRCDIR([src/symbol.c])
AC_CONFIG_HEADERS([inc/liborb-config.h])

LT_INIT

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

# Checks for libraries.
# check for pthreads
AX_PTHREAD
LIBS="$PTHREAD_LIBS $LIBS"
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
CC="$PTHREAD_CC"
AC_SUBST([LIBS], [$LIBS])
AC_SUBST([CFLAGS], [$CFLAGS])
AC_SUBST([CC], [$CC])

# Checks for header files.
AC_CHECK_HEADERS([stdint.h stdlib.h gc/gc.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_SIZE_T

# Checks for library functions.

AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT
