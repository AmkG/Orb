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
	object.c\
	init.c\
	thread-pool.c\
	tree.c
liborb_la_LDFLAGS = -module

check_PROGRAMS =\
	test-symbols

test_symbols_SOURCES =\
	test-symbols.c
test_symbols_LDADD = liborb.la
test_symbols_LDFLAGS = -static

TESTS = $(check_PROGRAMS)

