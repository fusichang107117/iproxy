#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ev.h>

#include "iproxy.h"
#include "iproxylib.h"
#include "hashmap.h"

static int iproxy_sockfd = -1;
static struct ev_periodic periodic_tick;
static struct ev_io iproxy_client;

map_t mymap;

int iproxyd_connect(void)
{
	int sockfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (sockfd < 0) {
		printf("create ipc client error: %d\n", sockfd);
		return -1;
	}

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(struct sockaddr_un));

	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, IPROXY_IPC_SOCK_PATH, sizeof(addr.sun_path) - 1);

	int ret = connect(sockfd, (const struct sockaddr *) &addr, sizeof(struct sockaddr_un));
	if (ret == -1) {
		printf("The server is down.\n");
		return -1;
	}
	return sockfd;
}

static void periodic_cb(struct ev_loop *loop, ev_periodic *w, int revents)
{
	//log_debug("%s(), ot_sockfd: %d\n", __func__, __LINE__, ot_sockfd);
	if (iproxy_sockfd < 0)
		iproxy_sockfd = iproxyd_connect();
}

static void iproxy_recv_handle(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
	char buf[MAX_BUF_SIZE] = { 0 };
	int cmd_len = sizeof(iproxy_cmd_t);

	int ret = read(watcher->fd, buf, MAX_BUF_SIZE);
	//printf("%s(), ret: %d\n", __func__, ret);
	if (ret <= 0) {
		perror("read");
		ev_io_stop(loop, watcher);
		close(watcher->fd);
		iproxy_sockfd = -1;
		return;
	}
	buf[ret] = '\0';
	//printf("ret:%d\n", ret);

	iproxy_cmd_t *ack = (iproxy_cmd_t *)buf;
	//printf("magic:%s\n", ack->magic);
	//printf("ID:%d\n", ack->id);
	//printf("key_len:%d\n", ack->key_len);
	//printf("value_len:%d\n", ack->value_len);

	char *key = buf + cmd_len;
	char *value = buf + cmd_len + ack->key_len;
	//printf("key:%s\n", key);
	//printf("value:%s\n", value);

	if (ret != (cmd_len + ack->key_len + ack->value_len) || ack->key_len <= 1 || ack->id != IPROXY_PUB)
		return;

	iproxy_func_node_t *func_node;
	ret = hashmap_get(mymap, key, (void**)(&func_node));
	if (ret == MAP_OK) {
		func_node->func(value);
	}
}

int iproxy_open(void)
{
	iproxy_sockfd = iproxyd_connect();
	if (iproxy_sockfd < 0) {
		return -1;
	}

	mymap = hashmap_new();

	printf("connect iproxyd success...\n");
	ev_periodic_init(&periodic_tick, periodic_cb, 0., 5., 0);
	ev_periodic_start(EV_DEFAULT_ &periodic_tick);

	ev_io_init(&iproxy_client, iproxy_recv_handle, iproxy_sockfd, EV_READ);
	ev_io_start(EV_DEFAULT_ &iproxy_client);

	return iproxy_sockfd;
}

void iproxy_close(void)
{
	ev_io_stop(EV_DEFAULT_ &iproxy_client);
	ev_periodic_stop(EV_DEFAULT_ &periodic_tick);
	if (iproxy_sockfd >= 0)
		close(iproxy_sockfd);
	hashmap_free_free(mymap);
}

int iproxy_set(char *key, char *value)
{
	if (!key || *key == '\0') {
		printf("key must not be null\n");
		return -1;
	}

	int key_len = strlen(key) + 1;
	int value_len = strlen(value) + 1;
	if (key_len > MAX_KEY_LEN || value_len > MAX_VALUE_LEN) {
		printf("key max length is %d, value max length is %d\n", MAX_KEY_LEN, MAX_VALUE_LEN);
		return -1;
	}

	int sockfd = iproxyd_connect();
	if (sockfd < 0) {
		return -1;
	}

	iproxy_cmd_t cmd;
	char buf[1024] = {'\0'};

	int cmd_len = sizeof(cmd);

	memcpy(cmd.magic,IPROXY,4);
	cmd.id = IPROXY_SET;
	cmd.key_len = key_len;
	cmd.value_len = value_len;
	memcpy(buf,(char *)&cmd, cmd_len);

	//printf("int: %d\n", sizeof(int));
/*	printf("cmd_len: %d\n", cmd_len);
	printf("key_len: %d\n", cmd.key_len);
	printf("key: %s\n", key);
	printf("value_len: %d\n", cmd.value_len);
	printf("value: %s\n", value);*/

	snprintf(buf + cmd_len,cmd.key_len,"%s",key);
	if(value) {
		snprintf(buf + cmd_len + cmd.key_len,cmd.value_len,"%s",value);
	}

	printf("SET: %s, value: %s\n", key, value);
	int total_len = cmd_len + cmd.key_len + cmd.value_len;

	int len = write(sockfd, buf, total_len);

	close(sockfd);
	//printf("send: %d, str_len: %d\n", len, total_len);
	return (len == (total_len + 1)) ? 0 : -1;
}

int iproxy_unset(char *key)
{
	if (!key || *key == '\0') {
		printf("key must not be null\n");
		return -1;
	}

	int key_len = strlen(key) + 1;
	if (key_len > MAX_KEY_LEN) {
		printf("key max length is %d\n", MAX_KEY_LEN);
		return -1;
	}

	int sockfd = iproxyd_connect();
	if (sockfd < 0) {
		return -1;
	}

	iproxy_cmd_t cmd;
	char buf[1024] = {'\0'};

	int cmd_len = sizeof(cmd);

	memcpy(cmd.magic,IPROXY,4);
	cmd.id = IPROXY_UNSET;
	cmd.key_len = key_len;
	cmd.value_len = 0;
	memcpy(buf,(char *)&cmd, cmd_len);

	//printf("int: %d\n", sizeof(int));
/*	printf("cmd_len: %d\n", cmd_len);
	printf("key_len: %d\n", cmd.key_len);
	printf("key: %s\n", key);
	printf("value_len: %d\n", cmd.value_len);
	printf("value: %s\n", value);*/

	snprintf(buf + cmd_len,cmd.key_len,"%s",key);

	printf("UNSET: %s\n", key);
	int total_len = cmd_len + cmd.key_len + cmd.value_len;

	int len = write(sockfd, buf, total_len);

	close(sockfd);
	//printf("send: %d, str_len: %d\n", len, total_len);
	return (len == (total_len + 1)) ? 0 : -1;
}

int iproxy_get(char *key, char *value)
{
	if (!key || *key == '\0' || !value ) {
		printf("key and value must not be null\n");
		return -1;
	}

	int key_len = strlen(key) + 1;
	if (key_len > MAX_KEY_LEN ) {
		printf("key max length is %d\n", MAX_KEY_LEN);
		return -1;
	}

	iproxy_cmd_t cmd;
	char buf[1024] = {'\0'};

	int cmd_len = sizeof(cmd);

	memcpy(cmd.magic,IPROXY,4);
	cmd.id = IPROXY_GET;
	cmd.key_len = key_len;
	cmd.value_len = 0;
	memcpy(buf,(char *)&cmd, cmd_len);

	snprintf(buf + cmd_len,cmd.key_len,"%s",key);

	int total_len = cmd_len + cmd.key_len;

	int sockfd = iproxyd_connect();
	if (sockfd < 0) {
		return -1;
	}

	usleep(1000);
	int len = write(sockfd, buf, total_len);

	int ret = read(sockfd, buf, 1024);

	close(sockfd);

	snprintf(value, 1024, "%s",buf);

	//printf("GET: value: %s\n", buf);
	return 0;
}

int iproxy_sub(char *key, void (*func)(char *))
{
	if (!key || *key == '\0' ) {
		printf("key must not be null\n");
		return -1;
	}

	int key_len = strlen(key) + 1;
	if (key_len > MAX_KEY_LEN) {
		printf("key max length is %d\n", MAX_KEY_LEN);
		return -1;
	}

	iproxy_cmd_t cmd;
	char buf[1024] = {'\0'};

	int cmd_len = sizeof(cmd);

	memcpy(cmd.magic,IPROXY,4);
	cmd.id = IPROXY_SUB;
	cmd.key_len = key_len;
	cmd.value_len = 0;
	memcpy(buf,(char *)&cmd, cmd_len);

	snprintf(buf + cmd_len,cmd.key_len,"%s",key);

	int total_len = cmd_len + cmd.key_len;

	int len = write(iproxy_sockfd, buf, total_len);

	iproxy_func_node_t *func_node;
	func_node = malloc(sizeof(iproxy_func_node_t));
	func_node->func = func;

	char *key1 = malloc(cmd.key_len);
	snprintf(key1, cmd.key_len, "%s", key);

	int ret = hashmap_put(mymap, key1, func_node);
	//printf("hashmap_put ret: %d\n", ret);

	printf("SUB: key: %s\n", key);
	return 0;
}

int iproxy_sub_and_get(char *key, char *value)
{
	return 0;
}

int iproxy_unsub(char *key)
{
	if (!key || *key == '\0') {
		printf("key must not be null\n");
		return -1;
	}

	int key_len = strlen(key) + 1;
	if (key_len > MAX_KEY_LEN) {
		printf("key max length is %d\n", MAX_KEY_LEN);
		return -1;
	}
	iproxy_cmd_t cmd;
	char buf[1024] = {'\0'};

	int cmd_len = sizeof(cmd);

	memcpy(cmd.magic,IPROXY,4);
	cmd.id = IPROXY_UNSUB;
	cmd.key_len = key_len;
	cmd.value_len = 0;
	memcpy(buf,(char *)&cmd, cmd_len);

	snprintf(buf + cmd_len,cmd.key_len,"%s",key);

	int total_len = cmd_len + cmd.key_len;

	int len = write(iproxy_sockfd, buf, total_len);

	int ret = hashmap_remove(mymap, key);
	//printf("hashmap_put ret: %d\n", ret);

	printf("UNSUB: key: %s\n", key);
	return 0;
}

int iproxy_sync(void)
{
	int sockfd = iproxyd_connect();
	if (sockfd < 0) {
		return -1;
	}

	iproxy_cmd_t cmd;
	char buf[1024] = {'\0'};

	int cmd_len = sizeof(cmd);

	memcpy(cmd.magic,IPROXY,4);
	cmd.id = IPROXY_SYNC;
	cmd.key_len = 0;
	cmd.value_len = 0;
	memcpy(buf,(char *)&cmd, cmd_len);

	int len = write(sockfd, buf, cmd_len);

	close(sockfd);
	//printf("send: %d, str_len: %d\n", len, total_len);
	return (len == cmd_len) ? 0 : -1;
}

int iproxy_list(void)
{
	iproxy_cmd_t cmd;
	char buf[1024] = {'\0'};

	int cmd_len = sizeof(cmd);

	memcpy(cmd.magic,IPROXY,4);
	cmd.id = IPROXY_LIST;
	cmd.key_len = 0;
	cmd.value_len = 0;
	memcpy(buf,(char *)&cmd, cmd_len);

	int total_len = cmd_len + cmd.key_len;

	int sockfd = iproxyd_connect();
	if (sockfd < 0) {
		return -1;
	}

	usleep(1000);
	int ret = write(sockfd, buf, total_len);
	char *key, *value;
	int key_len, value_len;

	do {
		int offset = 0;
		memset(buf, 0, 1024);
		ret = read(sockfd, buf, 1024);
		while(ret > offset) {
			key = buf + offset;
			key_len = strlen(key) + 1;
			value = key + key_len;
			value_len = strlen(value) + 1;
			offset += (key_len + value_len);

			printf("<%s>\t\t\t<%s>\n", key, value);
		}
	} while (ret > 0);

	close(sockfd);
	return 0;
}