#ifndef __BACKEND_H__
#define __BACKEND_H__

#include "hashmap.h"

#define IPROXY_FILE_PATH "/mnt/data/.iproxy.config"

typedef struct {
	map_t hashmap;
	char magic[4];
	unsigned long crc;
}file_handle_t;

void backend_file_destory(file_handle_t *file_handle);
file_handle_t *backend_file_init(void);
int backend_file_sync(file_handle_t *file_handle);


#endif