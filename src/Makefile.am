
LIBS=@LIBS@
CFLAGS=@CFLAGS@
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
	c-functions.c
liborb_la_LDFLAGS = -module

check_PROGRAMS = check-symbols check-cf
check_symbols_SOURCES =\
	check-symbols.c
check_symbols_LDADD = liborb.la
check_symbols_LDFLAGS = -static
check_cf_SOURCES=\
	check-cf.c
check_cf_LDADD = liborb.la
check_cf_LDFLAGS = -static

TESTS = check-symbols check-cf

