#ifndef __BACKEND_H__
#define __BACKEND_H__

#include "hashmap.h"

#define IPROXY_FILE_PATH "/mnt/data/.iproxy.config"

int hashmap_value_init(map_t mymap);
int hashmap_sync(map_t mymap);

#endif