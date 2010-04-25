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

LIBS=@LIBS@
AM_CFLAGS=@CFLAGS@
CC=@CC@

AM_LDFLAGS = -lgc

lib_LTLIBRARIES = liborb.la
liborb_la_SOURCES =\
	thread-support.c\
	symbol.c\
	gc.c\
	exception.c\
	bs-tree.c\
	object.c\
	init.c\
	call.c\
	c-functions.c\
	bool.c\
	thread-pool.c
liborb_la_LDFLAGS = -module

check_PROGRAMS =\
	check-symbols\
	check-cf\
	check-bool\
	check-thread-pool
check_symbols_SOURCES =\
	check-symbols.c
check_symbols_LDADD = liborb.la
check_symbols_LDFLAGS = -static
check_cf_SOURCES =\
	check-cf.c
check_cf_LDADD = liborb.la
check_cf_LDFLAGS = -static
check_bool_SOURCES =\
	check-bool.c
check_bool_LDADD = liborb.la
check_bool_LDFLAGS = -static
check_thread_pool_SOURCES =\
	check-thread-pool.c
check_thread_pool_LDADD = liborb.la
check_thread_pool_LDFLAGS = -static

TESTS = check-symbols check-cf check-bool check-thread-pool

