#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <linux/limits.h>

#include "nvram.h"
#include "iproxy.h"
#include "log.h"

static char *nvram_find_mtd(char *name, int *part_size)
{
	FILE *fp = fopen("/proc/mtd", "r");
	if (!fp) {
		return NULL;
	}
	char dev[PATH_MAX] = {0x00};
	char *path = NULL;
	int i=0;
	struct stat s;
	while( fgets(dev, sizeof(dev), fp) ) {
		if (strcasestr(dev, name) &&
			sscanf(dev, "mtd%d: %08x", &i, part_size) ) {
			snprintf(dev, sizeof(dev), "/dev/mtdblock%d", i);
			if ((stat(dev, &s) == 0) && (s.st_mode & S_IFBLK) ) {
				path = (char *)malloc(strlen(dev)+1);
				strncpy(path, dev, strlen(dev)+1);
				break;
			}
		}
	}
	fclose(fp);
	return path;
}

static int backend_factory_read(nvram_handle_t *nvt, char *partition, int partition_size)
{
	if (!nvt || !nvt->hashmap || !partition || partition_size <= 0) {
		//log_err("Not init the nvram_handle_t or partition was error.\n");
		return -1;
	}

	int fd = open(partition, O_RDONLY);
	if (fd < 0) {
		//log_err("Can't open the partition:%s\n", partition);
		return -1;
	}
	int result = -1;
	unsigned char *buf = malloc(partition_size);
	int ret = 0, read_len = 0;
	while((read_len < partition_size) && 
		((ret=read(fd, buf+read_len, partition_size-read_len)) > 0 )) {
		if (ret <= 0) {
			break;
		}
		read_len += ret;
	}
 	if (read_len > partition_size) {
		//log_err("The read partition(%s) size was error.\n", partition);
		goto read_end;
	}
	int i = 0;
	int offset = -1;
	for( i = 0; i <= ((read_len - NVRAM_MIN_SPACE) / sizeof(uint32_t)); i++ ) {
		 if( ((uint32_t *)buf)[i] == NVRAM_MAGIC ) {
			 offset = i * sizeof(uint32_t);
			 break;
		 }
	}
	if (offset < 0) {
		//log_err("Can't find the NVRAM_MAGIC.\n");
		goto read_end;
	}

	nvt->magic = *(uint32_t *)(buf+offset);
	nvt->len = *(uint32_t*)(buf+offset+sizeof(uint32_t*));
	nvt->crc_ver_init=*(uint32_t*)(buf+offset+sizeof(uint32_t*)*2);
	nvt->config_refresh=*(uint32_t*)(buf+offset+sizeof(uint32_t*)*3);
	nvt->config_ncdl = *(uint32_t*)(buf+offset+sizeof(uint32_t*)*4);
	char *name = NULL, *value = NULL, *eq = NULL;
	if (nvt->magic == NVRAM_MAGIC &&
			nvt->len < partition_size-offset) {
		name = buf+offset+sizeof(uint32_t*)*5;	
		value = NULL, eq = NULL;
		//log_err("Get key value by partition %s:\n", partition);
		for( ; *name; name=value+strlen(value)+1) {
			if (*name == 0xFF) {
				break;
			}
			if ( !(eq = (char *)strchr(name, '=')) ) {
				break;
			}
			*eq = '\0';
			value = eq+1;
			//log_info("%s:%s\n", name, value);
			int key_len = strlen(name) + 1;
			int value_len = strlen(value) + 1;

			//log_debug("key_len:%d, value_len: %d\n", key_len, value_len);

			iproxy_data_node_t *data = NULL;
			char *key1 = malloc(key_len);
			data = malloc(sizeof(iproxy_data_node_t));
			memset(data, 0, sizeof(iproxy_data_node_t));
			data->value = malloc(value_len);
	
			snprintf(key1, key_len, "%s", name);
			snprintf(data->value, value_len, "%s", value);
			ret = hashmap_put(nvt->hashmap, key1, data);

		}
		result = 0;
	}
read_end:
	free(buf);
	close(fd);
	return result;
}

static int backend_nvram_read(nvram_handle_t *nvt, char *partition, int partition_size)
{
	if (!nvt || !nvt->hashmap || !partition || partition_size <= 0) {
		//log_err("Not init the nvram_handle_t or partition was error.\n");
		return -1;
	}

	int fd = open(partition, O_RDONLY);
	if (fd < 0) {
		//log_err("Can't open the partition:%s\n", partition);
		return -1;
	}

	sync_head_t sync_head;
	int offset = sizeof(sync_head_t);
	int ret = read(fd, &sync_head, offset);

	if(ret != offset || strcmp(sync_head.magic, IPROXY) != 0 || sync_head.length > partition_size - offset ) {
		close(fd);
		return -1;
	}

	unsigned char *buf = malloc(sync_head.length);
	if(!buf) {
		close(fd);
		return -1;
	}

	int read_len = 0;
	do {
		ret=read(fd, buf + read_len,  sync_head.length - read_len);
		if (ret <= 0) {
			break;
		}
		read_len += ret;
	} while( sync_head.length != read_len);
	close(fd);

	unsigned char crc = crc8(buf, sync_head.length);
	if (read_len != sync_head.length || sync_head.crc != crc) {
		//log_err("The read partition(%s) size was error %d(%d).\n", partition, read_len, sync_head.length);
		//log_err("crc %lu(%lu) is not match\n",sync_head.crc, crc);
		free(buf);
		return -1;
	}

	char *key, *value;
	int key_len, value_len;

	offset = 0;
	map_t hashmap = nvt->hashmap;

	//printf("%s(), %d, sync_head.length: %d\n", __func__, __LINE__, sync_head.length);

	while (offset < sync_head.length) {
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
		hashmap_put(hashmap, key1, data);
	}

	free(buf);
	return 0;
}

int backend_nvram_sync(nvram_handle_t *nvt)
{
	if (!nvt) return -1;


	int partition_size = 0;
	char *partition = nvram_find_mtd(CONFIG_PARTITION, &partition_size);

	if (!partition||partition_size <= 0) {
		return -1;
	}

	log_debug("%s, %d, PATH: %s\n", __func__, __LINE__, partition);
	char *buf = (char *)malloc(partition_size);
	if(!buf)
		return -1;
	memset(buf, 0xFF, partition_size);

	int fd = open(partition, O_WRONLY|O_CREAT, 0660);
	if (fd < 0 ) {
		log_err("Can't open partition(%s) to write: %m.\n", partition);
		free(buf);
		return -1;
	}

	map_t hashmap = nvt->hashmap;
	int offset = sizeof(sync_head_t);
	int real_len = 0;

	hashmap_get_from_index(hashmap, 0, buf + offset, partition_size - offset, &real_len);

	sync_head_t sync_head;
	memset(&sync_head, 0, sizeof(sync_head));
	snprintf(sync_head.magic, 4, "%s", IPROXY);
	sync_head.crc = crc8(buf + offset, real_len);;
	sync_head.length = real_len;
	memcpy(buf, &sync_head, offset);

	int ret = 0, write_len = 0;
	while( write_len < partition_size ) {
		ret = write(fd, buf+write_len, partition_size-write_len);
		if (ret <= 0) {
			log_err("write to file faild.\n");
			ret = -1;
			break;
		}
		write_len += ret;
	}
	close(fd);
	free(buf);

	log_info("NVRAM SYNC: magic: %s, length: %lu, crc: %lu\n", sync_head.magic, sync_head.length, sync_head.crc);
	return (ret < 0) ? -1 : 0;
}

void backend_nvram_destory(nvram_handle_t *nvt)
{
	if (nvt){
		hashmap_free_free_free(nvt->hashmap);
		free(nvt);
	}
	return;
}

nvram_handle_t *backend_nvram_init(iproxy_cmd_level_t level)
{
	if (level != IPROXY_FACTORY_LEVEL && level != IPROXY_NVRAM_LEVEL)
		return NULL;
	nvram_handle_t *nvt = (nvram_handle_t *)malloc(sizeof(nvram_handle_t));
	nvt->hashmap = hashmap_new();
	if (!nvt->hashmap) {
		//log_err("new nvram_handle_t map error.\n");
		backend_nvram_destory(nvt);
		return NULL;
	}
	int part_size = 0;
	if (level == IPROXY_FACTORY_LEVEL) {
		char *factory = nvram_find_mtd(FACTORY_PARTITION, &part_size);
		if (factory) {
			//log_debug("%s(), %d, path: %s\n", __func__, __LINE__, factory);
			backend_factory_read(nvt, factory, part_size);
			free(factory);
		}
	} else {
		char *config = nvram_find_mtd(CONFIG_PARTITION, &part_size);
		if (config) {
			//log_debug("%s(), %d, path: %s\n", __func__, __LINE__, config);
			backend_nvram_read(nvt, config, part_size);
			free(config);
		}

	}
	return nvt;
}

int nvram_get(nvram_handle_t *nvt, char *key)
{
	iproxy_data_node_t *data;
	int ret = hashmap_get(nvt->hashmap, key, (void**)(&data));
	//log_debug("hashmap_get ret: %d\n", ret);
	if (ret == MAP_OK) {
		printf("%s", data->value);
		fflush(stdout);
	}
	return MAP_OK;
}

int nvram_list(nvram_handle_t *nvt)
{
	return hashmap_get_list_all(nvt->hashmap);
}

int iproxy_lock(char *path)
{
	int fd = open(path, O_CREAT|O_CLOEXEC, 0660);
	if ( fd > 0 ) {
		if ( flock(fd, LOCK_EX) ) {
			close(fd);fd = -1;
		}
	}
	return fd;
}

void iproxy_unlock(int fd)
{
	if (fd > 0) {
		flock(fd, LOCK_UN);
		close(fd);
	}
	return;
}

int iproxy_sync_command(iproxy_cmd_level_t level, iproxy_cmd_id_t cmd, char *key)
{
	char *path;

	switch(level) {
#if 0
		case IPROXY_RAM_LEVEL:
			path = IPROXY_RAM_LOCK_PATH;
		break;
		case IPROXY_FILE_LEVEL:
			path = IPROXY_FILE_LOCK_PATH;
		break;
#endif
		case IPROXY_NVRAM_LEVEL:
			path = IPROXY_NVRAM_LOCK_PATH;
		break;
		case IPROXY_FACTORY_LEVEL:
			path = IPROXY_FACTORY_LOCK_PATH;
		break;
		default:
			//log_err("unknow level : %d\n", level);
		return -1;
	}

	int lockfd = iproxy_lock(path);

	nvram_handle_t *handle = backend_nvram_init(level);
	if (!handle) {
		iproxy_unlock(lockfd);
		//printf("%s, %d:create %d handle error\n", __func__, __LINE__, level);
		return -1;
	}

	switch(cmd) {
		case IPROXY_GET:
			nvram_get(handle, key);
		break;
		case IPROXY_LIST:
			nvram_list(handle);
		break;
		default:
		break;
	}

	backend_nvram_destory(handle);
	iproxy_unlock(lockfd);
	return 0;
}