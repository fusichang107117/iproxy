#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hashmap.h"
#include "iproxy.h"
#include "backend.h"

int hashmap_value_init(map_t mymap)
{
	map_t hashmap = mymap;
	FILE *fp = fopen(IPROXY_FILE_PATH, "rb");
	if (!fp) {
		printf("file not exsit\n");
		return -1;
	}

	fseek(fp, 0, SEEK_END);
	int file_len = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	char *buf = malloc(file_len);
	memset(buf, 0, file_len);
	int read_len = fread(buf, 1, file_len, fp);
	fclose(fp);
	if (file_len != read_len || file_len == 0) {
		printf("file read error %d(%d)\n", file_len, read_len);
		free(buf);
		return -1;
	}

	int offset = 0;
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
		data_node_t *data = malloc(sizeof(data_node_t));
		memset(data, 0, sizeof(data_node_t));
		data->value = malloc(value_len);
		snprintf(key1, key_len, "%s", key);
		snprintf(data->value, value_len, "%s", value);

		printf("<%s>\t\t\t<%s>\n", key1, value);

		hashmap_put(mymap, key1, data);
	} while (offset < file_len);

	free(buf);
	return 0;
}

int hashmap_sync(char *buf, int buf_len)
{
	FILE *fp = fopen(IPROXY_FILE_PATH, "wb");
	if (!fp) {
		printf("file not exsit\n");
		return -1;
	}
	fwrite(buf, 1, buf_len, fp);
	fclose(fp);
}


