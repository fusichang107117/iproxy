#ifndef __BACKEND_H__
#define __BACKEND_H__

#include "hashmap.h"

#define IPROXY_FILE_PATH "/home/fu/project/iproxy/iproxy.iproxy"


int hashmap_value_init(map_t mymap);
int hashmap_sync(char *buf, int buf_len);


#endif