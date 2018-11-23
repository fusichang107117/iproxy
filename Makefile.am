AUTOMAKE_OPTIONS=foreign

lib_LTLIBRARIES=libiproxy.la
libiproxy_la_SOURCES=src/hashmap.c ./src/iproxylib.c

libiproxy_la_CFLAGS=-I./inc
libiproxy_la_LDFLAGS= -lev -version-info 1:0:0
libiproxydir=@includedir@

include_HEADERS=./inc/*.h

bin_PROGRAMS = iproxyd iproxy iproxy_example factory

iproxyd_DEPENDENCIES=libiproxy.la
iproxyd_SOURCES = src/iproxyd.c src/backend.c src/nvram.c
iproxyd_LDADD = libiproxy.la
iproxyd_LDFLAGS = -lev
iproxyd_CFLAGS = -I./inc

iproxyddir=@includedir@

iproxy_example_DEPENDENCIES=libiproxy.la
iproxy_example_SOURCES = src/example.c
iproxy_example_LDADD = libiproxy.la
iproxy_example_LDFLAGS =  -lev
iproxy_example_CFLAGS = -I./inc

iproxy_exampledir=@includedir@

iproxy_DEPENDENCIES=libiproxy.la
iproxy_SOURCES = src/iproxy.c
iproxy_LDADD = libiproxy.la
iproxy_LDFLAGS = -lev
iproxy_CFLAGS = -I./inc

iproxydir=@includedir@

factory_DEPENDENCIES=libiproxy.la
factory_SOURCES = src/factory.c
factory_LDADD = libiproxy.la
factory_CFLAGS = -I./inc

factorydir=@includedir@

distclean-local:
	rm -rf Makefile.in compile autom4te.cache aclocal.m4 config.h.in configure depcomp install-sh missing config.guess config.sub ltmain.sh
