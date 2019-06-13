#ifndef __BACKEND_H__
#define __BACKEND_H__

#include "hashmap.h"

#define IPROXY_FILE_PATH "/mnt/data/.iproxy.file"
#define IPROXY_RAM_PATH "/tmp/.iproxy.ram"

typedef struct {
	map_t hashmap;
	char magic[4];
	unsigned char crc;
}file_handle_t;

void backend_file_destory(file_handle_t *file_handle);
file_handle_t *backend_file_init(char *path);
int backend_file_sync(file_handle_t *file_handle, char *path);

#endif