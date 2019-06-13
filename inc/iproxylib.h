#ifndef __IPROXYLIB_H__
#define __IPROXYLIB_H__

#include "hashmap.h"
#include <ev.h>

typedef struct {
	map_t hashmap;
	volatile int sockfd;
	struct ev_periodic periodic_tick;
	struct ev_io iproxy_client;
}iproxy_handle_t;

int iproxyd_connect(void);
iproxy_handle_t *iproxy_open(void);
void iproxy_close(iproxy_handle_t *iproxy_handle);

int iproxy_set(iproxy_cmd_level_t level, char *key, char *value);
int iproxy_get(iproxy_cmd_level_t level, char *key, char *value);
int iproxy_sub(iproxy_handle_t *iproxy_handle, iproxy_cmd_level_t level, char *key, void (*func)(char *));
int iproxy_sub_and_get(iproxy_cmd_level_t level, char *key, char *value);
int iproxy_unsub(iproxy_handle_t *iproxy_handle, iproxy_cmd_level_t level, char *key);
int iproxy_unset(iproxy_cmd_level_t level, char *key);
int iproxy_sync(iproxy_cmd_level_t level);
int iproxy_clear(iproxy_cmd_level_t level);
int iproxy_list(iproxy_cmd_level_t level);

#endif