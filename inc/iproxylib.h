#ifndef __IPROXYLIB_H__
#define __IPROXYLIB_H__

#include "hashmap.h"
#include <ev.h>

typedef struct {
	map_t hashmap;
	int sockfd;
	struct ev_periodic periodic_tick;
	struct ev_io iproxy_client;
}iproxy_handle_t;

int iproxyd_connect(void);
iproxy_handle_t *iproxy_open(void);
void iproxy_close(iproxy_handle_t *iproxy_handle);
int iproxy_set(char *key, char *value);
int iproxy_get(char *key, char *value);
int iproxy_sub(iproxy_handle_t *iproxy_handle, char *key, void (*func)(char *));
int iproxy_sub_and_get(char *key, char *value);
int iproxy_unsub(iproxy_handle_t *iproxy_handle, char *key);
int iproxy_unset(char *key);
int iproxy_sync(void);
int iproxy_list(void);

int iproxy_factory_get(char *key, char *value);
int iproxy_factory_list(void);


#endif