bin_PROGRAMS = iproxyd iproxy_client ipc_client

iproxyd_SOURCES = src/iproxyd.c src/hashmap.c
iproxyd_LDADD =  -lev
iproxyd_CFLAGS = -I./inc

#iproxylib_SOURCES = src/iproxylib.c src/hashmap.c
#iproxylib_LDADD =  -lev
#iproxylib_CFLAGS = -I./inc -s

iproxy_client_SOURCES = src/example.c src/iproxylib.c src/hashmap.c
iproxy_client_LDADD =  -lev
iproxy_client_CFLAGS = -I./inc

ipc_client_SOURCES = src/ipc_client.c
ipc_client_LDADD =  -lev
ipc_client_CFLAGS = -I./inc -s

distclean-local:
	rm -rf Makefile.in compile autom4te.cache aclocal.m4 config.h.in configure depcomp install-sh missing
