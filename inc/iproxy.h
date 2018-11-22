#ifndef __IPROXY_H__
#define __IPROXY_H__


#define IPROXY_IPC_SOCK_PATH 	"/tmp/iproxy.socket"
#define MAX_IPC_CLIENT_FDS	50
#define MAX_REGISTER_FD	10

#define MAX_KEY_LEN		32
#define MAX_VALUE_LEN	64
#define BACKEND_SYNC_INTERVAL	30

#define MAX_BUF_SIZE 1024
#define	IPROXY	"MII\0"
//magic-cmdid-key-value

typedef struct
{
	char magic[4];
	unsigned long crc;
} sync_head_t;

typedef enum
{
	IPROXY_SET = 1000,
	IPROXY_GET,
	IPROXY_SUB,
	IPROXY_SUB_AND_GET,
	IPROXY_UNSUB,
	IPROXY_UNSET,
	IPROXY_LIST,
	IPROXY_SYNC,
	IPROXY_PUB,
}iproxy_cmd_id_t;

typedef struct
{
	char magic[4];
	int id;
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
	func_cb func;
}iproxy_func_node_t;

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