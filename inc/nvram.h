#ifndef __NVRAM_H__
#define __NVRAM_H__

#include <stdint.h>
#include "hashmap.h"

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
	map_t map;
	uint32_t magic;
	uint32_t len;
	uint32_t crc_ver_init;
	uint32_t config_refresh;
	uint32_t config_ncdl;
}nvram_handle_t;

nvram_handle_t *backend_nvram_init(int factory);
void backend_nvram_destory(nvram_handle_t *nvt);

#endif