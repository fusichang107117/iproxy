#ifndef __NVRAM_H__
#define __NVRAM_H__

#include <stdint.h>
#include "hashmap.h"
#include "iproxy.h"

#define CONFIG_PARTITION "config"
#define FACTORY_PARTITION "factory"
#define DATA_PARTITION "/mnt/data"

#define NVRAM_MAGIC         0x48534C46
#define NVRAM_VERSION       1
#define NVRAM_MIN_SPACE     0x8000

#define SDRAM_REF_EN    0x8000 
#define SDRAM_REF(p)    (((p)&0xff) | SDRAM_REF_EN)

#define SDRAM_INIT  	0x419
#define SDRAM_BURSTFULL 0x0000
#define SDRAM_CONFIG    SDRAM_BURSTFULL
#define SDRAM_REFRESH   SDRAM_REF(0x40)

typedef struct {
	map_t hashmap;
	uint32_t magic;
	uint32_t len;
	uint32_t crc_ver_init;
	uint32_t config_refresh;
	uint32_t config_ncdl;
}nvram_handle_t;

nvram_handle_t *backend_nvram_init(iproxy_cmd_level_t level);
void backend_nvram_destory(nvram_handle_t *nvt);

int nvram_get(nvram_handle_t *nvt, char *key);
int nvram_list(nvram_handle_t *nvt);

int backend_nvram_sync(nvram_handle_t *nvt);
int iproxy_sync_command(iproxy_cmd_level_t level, iproxy_cmd_id_t cmd, char *key);

#endif