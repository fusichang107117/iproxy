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

static int backend_nvram_read(nvram_handle_t *nvt, char *partition, int partition_size)
{
	if (!nvt || !nvt->map || !partition || partition_size <= 0) {
		printf("Not init the nvram_handle_t or partition was error.\n");
		return -1;
	}

	int fd = open(partition, O_RDONLY);
	if (fd < 0) {
		printf("Can't open the partition:%s\n", partition);
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
		printf("The read partition(%s) size was error.\n", partition);
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
		printf("Can't find the NVRAM_MAGIC.\n");
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
		printf("Get key value by partition %s:\n", partition);
		for( ; *name; name=value+strlen(value)+1) {
			if (*name == 0xFF) {
				break;
			}
			if ( !(eq = (char *)strchr(name, '=')) ) {
				break;
			}
			*eq = '\0';
			value = eq+1;
			printf("%s:%s\n", name, value);
			int key_len = strlen(name) + 1;
			int value_len = strlen(value) + 1;

			printf("key_len:%d, value_len: %d\n", key_len, value_len);

			iproxy_data_node_t *data = NULL;
			char *key1 = malloc(key_len);
			data = malloc(sizeof(iproxy_data_node_t));
			memset(data, 0, sizeof(iproxy_data_node_t));
			data->value = malloc(value_len);
	
			snprintf(key1, key_len, "%s", name);
			snprintf(data->value, value_len, "%s", value);
			ret = hashmap_put(nvt->map, key1, data);

		}
		result = 0;
	}
read_end:
	free(buf);
	close(fd);
	return result;
}

void backend_nvram_destory(nvram_handle_t *nvt)
{
	if (nvt){
		hashmap_free_free_free(nvt->map);
		free(nvt);
	}
	return;
}

nvram_handle_t *backend_nvram_init(int factory)
{
	nvram_handle_t *nvt = (nvram_handle_t *)malloc(sizeof(nvram_handle_t));
	nvt->map = hashmap_new();
	if (!nvt->map) {
		printf("new nvram_handle_t map error.\n");
		backend_nvram_destory(nvt);
		return NULL;
	}
	int result = 0;
	int part_size = 0;
	if (factory) {
		char *factory = nvram_find_mtd(FACTORY_PARTITION, &part_size);
		backend_nvram_read(nvt, factory, part_size);
		free(factory);
	}
#if 0
	else {
		char config_file[PATH_MAX] = {0x00};
		snprintf(config_file, sizeof(config_file),
				"%s/.%s.nvram", DATA_PARTITION, CONFIG_PARTITION); 
		if (access(config_file, R_OK) == 0) {
			struct stat st;
			stat(config_file, &st);
			_nvram_read(nvt, config_file, st.st_size);
		}else {
			part_size = 0;
			char *config = nvram_find_mtd(CONFIG_PARTITION, &part_size);
			_nvram_read(nvt, config, part_size);
			xfree(config);
		}
	}
#endif
	return nvt;
}

int _nvram_write(nvram_handle_t *nvt, char *partition, int partition_size)
{
	int result = 0;
	if (!partition||!nvt) {
		return -1;
	}
#if 0
	int i = 0;
	int fd = open(partition, O_WRONLY|O_CREAT);
	if (fd < 0 ) {
		printf("Can't open partition(%s) to write: %m.\n", partition);
		return -1;
	}
	char *init = NULL, *config = NULL, *refresh = NULL, *ncdl = NULL;
	uint32_t magic = 0, crc_ver_init = 0, config_refresh=0, config_ncdl = 0;
	nvt->magic = NVRAM_MAGIC;
	nvt->crc_ver_init = (NVRAM_VERSION << 8);

	if (!(init = (char *)hashmap_get(nvt->map, "sdram_init")) ||
		!(config = (char *)hashmap_get(nvt->map, "sdram_config")) ||
		!(refresh = (char *)hashmap_get(nvt->map, "sdram_refresh")) ||
		!(ncdl = (char *)hashmap_get(nvt->map, "sdram_ncdl"))) {
		nvt->crc_ver_init |= SDRAM_INIT << 16;
		nvt->config_refresh = SDRAM_CONFIG | (SDRAM_REFRESH << 16);
		nvt->config_ncdl = 0;
	} else {
		nvt->crc_ver_init |= (strtoul(init, NULL, 0) & 0xFFFF) << 16;
		nvt->config_refresh = (strtoul(config, NULL, 0) & 0xFFFF) |
							(strtoul(refresh, NULL, 0) & 0xFFFF << 16);
		nvt->config_ncdl = strtoul(ncdl, NULL, 0);
	}
	char *partition_buf = (char *)xmalloc0(partition_size);
	char *ptr = partition_buf+sizeof(uint32_t)*5;
	char *end = partition_buf + partition_size - sizeof(uint32_t)*5 - 2;
	memset(ptr, 0xFF, partition_size-sizeof(uint32_t)*5);
	void *key = NULL, *value = NULL;
	iterator it;
	HASHMAP_FOREACH_KEY(value, key, nvt->map, it) {
		if ((ptr+xstrlen((char *)key)+xstrlen((char *)value)+ 2) > end) {
			break;
		}
		ptr += sprintf(ptr, "%s=%s", (char *)key, (char *)value) + 1;
	}
	*ptr = '\0';
	ptr++;
	if ( (int)ptr%4 ) {
		memset(ptr, 0, 4-((int)ptr%4));
	}
	ptr++;
	
	nvt->len = NVRAM_ROUNDUP(ptr-(partition_buf+sizeof(uint32_t)*5), 4);
	uint32_t header[3] = {0x00};
	header[0] = nvt->crc_ver_init;
	header[1] = nvt->config_refresh;
	header[2] = nvt->config_ncdl;
	
	uint8_t crc = hndcrc8((unsigned char *)header+1, 
			sizeof(uint32_t)*3-1, 0xff);
	crc = hndcrc8(partition_buf+sizeof(uint32_t)*5,
			partition_size-sizeof(uint32_t)*5, crc);
	nvt->crc_ver_init |= crc;
	ptr = partition_buf;
	memcpy(ptr, &(nvt->magic), sizeof(uint32_t)); ptr+=sizeof(uint32_t);
	memcpy(ptr, &(nvt->len), sizeof(uint32_t));ptr+=sizeof(uint32_t);
	memcpy(ptr, &(nvt->crc_ver_init), sizeof(uint32_t));ptr+=sizeof(uint32_t);
	memcpy(ptr, &(nvt->config_refresh), sizeof(uint32_t));ptr+=sizeof(uint32_t);
	memcpy(ptr, &(nvt->config_ncdl), sizeof(uint32_t));ptr+=sizeof(uint32_t);

	int ret = 0, write_len = 0;
	while( write_len < partition_size ) {
		ret = write(fd, partition_buf+write_len, partition_size-write_len);
		if (ret <= 0) {
			log_error("write to file faild.\n");
			result = -1;
			break;
		}
		write_len += ret;
	}
	close(fd);
	xfree(partition_buf);
	return result;
#endif
}