#ifndef __IPROXY_H__
#define __IPROXY_H__


#define IPROXY_IPC_SOCK_PATH 	"/tmp/iproxy.socket"

#define IPROXY_FACTORY_LOCK_PATH "/tmp/.iproxy.factory.lock"
#define IPROXY_NVRAM_LOCK_PATH "/tmp/.iproxy.nvram.lock"
#define IPROXY_FILE_LOCK_PATH "/tmp/.iproxy.file.lock"
#define IPROXY_RAM_LOCK_PATH "/tmp/.iproxy.ram.lock"

#define MAX_IPC_CLIENT_FDS	50
#define MAX_REGISTER_FD	10

#define MAX_KEY_LEN		32
#define MAX_VALUE_LEN	64

#define BACKEND_SYNC_INTERVAL	300

#define MAX_BUF_SIZE 1024
#define	IPROXY	"MII\0"
//magic-cmdid-key-value

typedef struct
{
	char magic[4];
	int length;
	unsigned char crc;
	int backup[9];
} sync_head_t;

typedef enum
{
	IPROXY_RAM_LEVEL = 0,
	IPROXY_FILE_LEVEL,
	IPROXY_NVRAM_LEVEL,
	IPROXY_FACTORY_LEVEL,
	IPROXY_TOTAL_LEVEL,
} iproxy_cmd_level_t;

typedef enum
{
	IPROXY_SET = 1000,
	IPROXY_GET,
	IPROXY_SUB,
	IPROXY_UNSUB,
	IPROXY_UNSET,
	IPROXY_LIST,
	IPROXY_SYNC,
	IPROXY_PUB,
	IPROXY_CLEAR,
	IPROXY_UNKNOW,
} iproxy_cmd_id_t;

typedef struct
{
	char magic[4];
	iproxy_cmd_level_t level;
	iproxy_cmd_id_t id;
	int key_len;
	int value_len;
	char key[0];
	char value[0];
}iproxy_cmd_t;

typedef struct fd_node
{
	int fd;
	struct fd_node *next;
}iproxy_fd_node_t;

typedef void (*func_cb)(char *);

typedef struct
{
	iproxy_cmd_level_t level;
	func_cb func;
}iproxy_func_node_t;

typedef struct
{
	char *key;
	iproxy_func_node_t *func_node;
}iproxy_key_func_t;

typedef struct
{
	char *value;
	int fd[MAX_REGISTER_FD + 1];
}iproxy_data_node_t;

typedef struct
{
	char *key;
	iproxy_data_node_t data;
}iproxy_key_value_t;

#endif