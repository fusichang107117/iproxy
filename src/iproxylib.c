#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ev.h>

#include "iproxy.h"
#include "iproxylib.h"
#include "hashmap.h"

static void periodic_cb(struct ev_loop *loop, ev_periodic *watcher, int revents)
{
	//log_debug("%s(), ot_sockfd: %d\n", __func__, __LINE__, ot_sockfd);
	iproxy_handle_t *iproxy_handle = (iproxy_handle_t *)watcher->data;
	if (iproxy_handle->sockfd < 0)
		iproxy_handle->sockfd = iproxyd_connect();
}

static void iproxy_recv_handle(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
	char buf[MAX_BUF_SIZE] = { 0 };
	int cmd_len = sizeof(iproxy_cmd_t);

	iproxy_handle_t *iproxy_handle = (iproxy_handle_t *)watcher->data;

	int ret = read(watcher->fd, buf, MAX_BUF_SIZE);
	//printf("%s(), ret: %d\n", __func__, ret);
	if (ret <= 0) {
		perror("read");
		ev_io_stop(loop, watcher);
		close(watcher->fd);
		iproxy_handle->sockfd = -1;
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
	ret = hashmap_get(iproxy_handle->hashmap, key, (void**)(&func_node));
	if (ret == MAP_OK) {
		func_node->func(value);
	}
}

void iproxy_close(iproxy_handle_t *iproxy_handle)
{
	if (iproxy_handle) {
		ev_io_stop(EV_DEFAULT_ &iproxy_handle->iproxy_client);
		ev_periodic_stop(EV_DEFAULT_ &iproxy_handle->periodic_tick);
		if (iproxy_handle->sockfd >= 0)
			close(iproxy_handle->sockfd);
		hashmap_free_free(iproxy_handle->hashmap);
		free(iproxy_handle);
	}
}

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

iproxy_handle_t *iproxy_open(void)
{
	int sockfd = iproxyd_connect();
	if (sockfd < 0) {
		return NULL;
	}
	printf("connect iproxyd success...\n");

	iproxy_handle_t *iproxy_handle = (iproxy_handle_t *)malloc(sizeof(iproxy_handle_t));
	iproxy_handle->hashmap = hashmap_new();
	if (!iproxy_handle->hashmap) {
		printf("new iproxy_handle_t hashmap error.\n");
		iproxy_close(iproxy_handle);
		return NULL;
	}

	iproxy_handle->sockfd = sockfd;
	ev_periodic_init(&iproxy_handle->periodic_tick, periodic_cb, 0., 5., 0);
	iproxy_handle->periodic_tick.data = iproxy_handle;
	ev_periodic_start(EV_DEFAULT_ &iproxy_handle->periodic_tick);

	ev_io_init(&iproxy_handle->iproxy_client, iproxy_recv_handle, sockfd, EV_READ);
	iproxy_handle->iproxy_client.data = iproxy_handle;
	ev_io_start(EV_DEFAULT_ &iproxy_handle->iproxy_client);

	return iproxy_handle;
}

static char *construct_packet(iproxy_cmd_id_t cmd_id, char *key, char *value, int *length)
{
	int key_len=0, value_len=0;

	if (key)
		key_len= strlen(key) + 1;
	if (value)
		value_len = strlen(value) + 1;

	if (key_len > MAX_KEY_LEN || value_len > MAX_VALUE_LEN) {
		printf("key max length is %d, value max length is %d\n", MAX_KEY_LEN, MAX_VALUE_LEN);
		return NULL;
	}

	iproxy_cmd_t cmd;

	int cmd_len = sizeof(cmd);

	memcpy(cmd.magic,IPROXY,4);
	cmd.id = cmd_id;
	cmd.key_len = key_len;
	cmd.value_len = value_len;

	int total_len = key_len + value_len + cmd_len;
	char *buf = malloc(total_len);
	if (!buf) {
		printf("%s, %d,malloc error\n", __func__, __LINE__);
		return NULL;
	}

	memcpy(buf,(char *)&cmd, cmd_len);
	if (key)
		snprintf(buf + cmd_len, key_len,"%s",key);
	if(value)
		snprintf(buf + cmd_len + key_len, value_len,"%s",value);

	*length = total_len;
	return buf;
}

static int iproxy_send(iproxy_cmd_id_t cmd_id, char *key, char *value)
{
	int length = 0;
	char *send_buf = construct_packet(cmd_id, key, value, &length);
	if (!send_buf) {
		printf("construct_packet error\n");
		return -1;
	}

	int sockfd = iproxyd_connect();
	if (sockfd < 0) {
		free(send_buf);
		return -1;
	}
	write(sockfd, send_buf, length);
	close(sockfd);
	free(send_buf);
	return 0;
}

static int iproxy_send_recv(iproxy_cmd_id_t cmd_id, char *key, char *value)
{
	int length = 0;
	char *send_buf = construct_packet(cmd_id, key, NULL, &length);
	if (!send_buf) {
		printf("construct_packet error\n");
		return -1;
	}

	int sockfd = iproxyd_connect();
	if (sockfd < 0) {
		free(send_buf);
		return -1;
	}
	write(sockfd, send_buf, length);
	free(send_buf);

	int recv_len = read(sockfd, value, MAX_VALUE_LEN);

	close(sockfd);
	return (recv_len >= 0) ? 0 : -1;
}

int iproxy_set(char *key, char *value)
{
	if (!key || *key == '\0' || !value ) {
		printf("key and value must not be null\n");
		return -1;
	} else
		return iproxy_send(IPROXY_SET, key, value);
}

int iproxy_unset(char *key)
{
	if (!key || *key == '\0') {
		printf("key and value must not be null\n");
		return -1;
	} else
		return iproxy_send(IPROXY_UNSET, key, NULL);
}

int iproxy_get(char *key, char *value)
{
	if (!key || *key == '\0' || !value ) {
		printf("key and value must not be null\n");
		return -1;
	} else
		return iproxy_send_recv(IPROXY_GET, key, value);
}

int iproxy_sub(iproxy_handle_t *iproxy_handle, char *key, void (*func)(char *))
{
	if (!key || *key == '\0' || !iproxy_handle) {
		printf("key or handle must not be null\n");
		return -1;
	}

	int length = 0;
	char *send_buf = construct_packet(IPROXY_SUB, key, NULL, &length);
	if (!send_buf) {
		printf("construct_packet error\n");
		return -1;
	}
	write(iproxy_handle->sockfd, send_buf, length);
	free(send_buf);

	iproxy_func_node_t *func_node;
	func_node = malloc(sizeof(iproxy_func_node_t));
	func_node->func = func;

	int key_len = strlen(key) + 1;
	char *key1 = malloc(key_len);
	snprintf(key1, key_len, "%s", key);

	int ret = hashmap_put(iproxy_handle->hashmap, key1, func_node);
	//printf("hashmap_put ret: %d\n", ret);
	printf("SUB: key: %s\n", key);
	return ret;
}

int iproxy_sub_and_get(char *key, char *value)
{
	return 0;
}

int iproxy_unsub(iproxy_handle_t *iproxy_handle, char *key)
{
	if (!key || *key == '\0' || !iproxy_handle) {
		printf("key and handle must not be null\n");
		return -1;
	}

	int length = 0;
	char *send_buf = construct_packet(IPROXY_UNSUB, key, NULL, &length);
	if (!send_buf) {
		printf("construct_packet error\n");
		return -1;
	}

	write(iproxy_handle->sockfd, send_buf, length);
	free(send_buf);

	int ret = hashmap_remove(iproxy_handle->hashmap, key);
	printf("UNSUB: key: %s\n", key);
	return ret;
}

int iproxy_sync(void)
{
	return iproxy_send(IPROXY_SYNC, NULL, NULL);
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

	int sockfd = iproxyd_connect();
	if (sockfd < 0) {
		return -1;
	}

	usleep(1000);
	int ret = write(sockfd, buf, cmd_len);
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

int iproxy_factory_get(char *key, char *value)
{
	if (!key || *key == '\0' || !value ) {
		printf("key and value must not be null\n");
		return -1;
	}

	return iproxy_send_recv(FACTORY_GET, key, value);
}

int iproxy_factory_list(void)
{
	iproxy_cmd_t cmd;
	char buf[1024] = {'\0'};

	int cmd_len = sizeof(cmd);

	memcpy(cmd.magic,IPROXY,4);
	cmd.id = FACTORY_LIST;
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
