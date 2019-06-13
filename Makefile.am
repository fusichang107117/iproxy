AUTOMAKE_OPTIONS=foreign

lib_LTLIBRARIES=libiproxy.la
libiproxy_la_SOURCES=src/hashmap.c ./src/iproxylib.c

libiproxy_la_CFLAGS=-I./inc
libiproxy_la_LDFLAGS= -lev -version-info 1:0:0
libiproxydir=@includedir@

include_HEADERS=./inc/*.h

bin_PROGRAMS = iproxyd iproxy iproxy_sub factory nvram

iproxyd_DEPENDENCIES=libiproxy.la
iproxyd_SOURCES = src/iproxyd.c src/backend.c src/nvram.c
iproxyd_LDADD = libiproxy.la
iproxyd_LDFLAGS = -lev
iproxyd_CFLAGS = -I./inc

iproxyddir=@includedir@

iproxy_sub_DEPENDENCIES=libiproxy.la
iproxy_sub_SOURCES = src/example.c
iproxy_sub_LDADD = libiproxy.la
iproxy_sub_LDFLAGS =  -lev
iproxy_sub_CFLAGS = -I./inc

iproxy_subdir=@includedir@

iproxy_DEPENDENCIES=libiproxy.la
iproxy_SOURCES = src/iproxy.c
iproxy_LDADD = libiproxy.la
iproxy_LDFLAGS = -lev
iproxy_CFLAGS = -I./inc

iproxydir=@includedir@

factory_DEPENDENCIES=libiproxy.la
factory_SOURCES = src/factory_cmd.c src/nvram.c
factory_LDADD = libiproxy.la
factory_CFLAGS = -I./inc

factorydir=@includedir@

nvram_DEPENDENCIES=libiproxy.la
nvram_SOURCES = src/nvram_cmd.c src/nvram.c
nvram_LDADD = libiproxy.la
nvram_CFLAGS = -I./inc

nvramdir=@includedir@

distclean-local:
	rm -rf Makefile.in compile autom4te.cache aclocal.m4 config.h.in configure depcomp install-sh missing config.guess config.sub ltmain.sh
