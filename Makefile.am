AUTOMAKE_OPTIONS=foreign

lib_LTLIBRARIES=libiproxy.la
libiproxy_la_SOURCES=src/hashmap.c ./src/iproxylib.c

libiproxy_la_CFLAGS=-I./inc
libiproxy_la_LDFLAGS= -lev -version-info 1:0:0

include_HEADERS=./inc/*.h

bin_PROGRAMS = iproxyd iproxy iproxy_example

iproxyd_SOURCES = src/iproxyd.c src/backend.c
iproxyd_LDADD =  -lev -liproxy
iproxyd_CFLAGS = -I./inc

iproxy_example_SOURCES = src/example.c
iproxy_example_LDADD =  -lev -liproxy
iproxy_example_CFLAGS = -I./inc

iproxy_SOURCES = src/iproxy.c
iproxy_LDADD =  -lev -liproxy
iproxy_CFLAGS = -I./inc


distclean-local:
	rm -rf Makefile.in compile autom4te.cache aclocal.m4 config.h.in configure depcomp install-sh missing config.guess config.sub ltmain.sh
