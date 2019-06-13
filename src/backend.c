#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h> 
#include <fcntl.h>

#include "hashmap.h"
#include "iproxy.h"
#include "backend.h"
#include "log.h"

void backend_file_destory(file_handle_t *file_handle)
{
	if(file_handle) {
		hashmap_free_free_free(file_handle->hashmap);
		free(file_handle);
	}
}

file_handle_t *backend_file_init(char *path)
{
	file_handle_t *file_handle = (file_handle_t *)malloc(sizeof(file_handle_t));
	file_handle->hashmap = hashmap_new();
	if (!file_handle->hashmap) {
		//log_err("new file_handle_t hashmap error.\n");
		return NULL;
	}

	FILE *fp = fopen(path, "rb");
	if (!fp) {
		//log_warning("file not exsit\n");
		return file_handle;
	}

	fseek(fp, 0, SEEK_END);
	int file_len = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	char *buf = malloc(file_len);
	memset(buf, 0, file_len);
	int read_len = fread(buf, 1, file_len, fp);
	fclose(fp);
	if (file_len != read_len || file_len == 0) {
		//log_err("file read error %d(%d)\n", file_len, read_len);
		goto OUT;
	}

	int offset = sizeof(sync_head_t);
	sync_head_t *sync_head = (sync_head_t *)buf;

	if(strcmp(sync_head->magic, IPROXY) != 0 || sync_head->length != file_len - offset ) {
		//log_err("magic %s(%s) or length %lu(%lu) is not match\n",sync_head->magic, IPROXY,sync_head->length, file_len - offset);
		goto OUT;
	}

	unsigned char crc = crc8(buf + offset, sync_head->length);
	if (sync_head->crc != crc) {
		//log_err("crc %lu(%lu) is not match\n",sync_head->crc, crc);
		goto OUT;
	}
	char *key, *value;
	int key_len, value_len;

	do {
		key = buf + offset;
		key_len = strlen(key) + 1;
		value = key + key_len;
		value_len = strlen(value) + 1;

		//printf("key_len: %d, value_len: %d, offset: %d\n", key_len,value_len,offset);
		offset += (value_len + key_len);

		char *key1 = malloc(key_len);
		iproxy_data_node_t *data = malloc(sizeof(iproxy_data_node_t));
		memset(data, 0, sizeof(iproxy_data_node_t));
		data->value = malloc(value_len);
		snprintf(key1, key_len, "%s", key);
		snprintf(data->value, value_len, "%s", value);

		//printf("<%s>\t\t\t<%s>\n", key1, value);
		hashmap_put(file_handle->hashmap, key1, data);
	} while (offset < file_len);
OUT:
	free(buf);
	return file_handle;
}

int backend_file_sync(file_handle_t *file_handle, char *path)
{
	map_t hashmap = file_handle->hashmap;
	int fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, 0660);
	if (fd <0) {
		log_err("file not exsit\n");
		return -1;
	}

	lseek(fd, sizeof(sync_head_t), SEEK_SET);
	unsigned char crc_value = 0;
	int length = 0;

	char buf[MAX_BUF_SIZE];
	int index = 0;
	do {
		int real_len = 0;
		memset(buf, 0, MAX_BUF_SIZE);
		index = hashmap_get_from_index(hashmap, index, buf, MAX_BUF_SIZE, &real_len);

		crc_value = crc8_start(buf, real_len, crc_value);
		write(fd, buf, real_len);
		length += real_len;
	} while (index != MAP_END);

	lseek(fd, 0, SEEK_SET);

	sync_head_t sync_head;
	memset(&sync_head, 0, sizeof(sync_head));
	snprintf(sync_head.magic, 4, "%s", IPROXY);
	sync_head.crc = crc_value;
	sync_head.length = length;

	write(fd, &sync_head, sizeof(sync_head_t));

	log_info("SYNC: magic: %s, length: %lu, crc: %lu\n", sync_head.magic, sync_head.length, sync_head.crc);
	close(fd);
	return 0;
}
