#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ev.h>
#include <sys/file.h>

#include "iproxy.h"
#include "iproxylib.h"
#include "hashmap.h"
#include "log.h"

#define SUB_KEY_REORG_TEMPLATE "..%d..%s"
#define SUB_KEY_REORG_OFFSET 5

static int iproxy_resub(iproxy_handle_t *iproxy_handle);

static void iproxy_recv_handle(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
	char buf[MAX_BUF_SIZE] = { 0 };
	int cmd_len = sizeof(iproxy_cmd_t);

	iproxy_handle_t *iproxy_handle = (iproxy_handle_t *)watcher->data;

	//log_debug("%s(), %d, %p, %p, event: %d\n", __func__, __LINE__,  &(iproxy_handle->iproxy_client), watcher);

	int ret = recv(watcher->fd, buf, MAX_BUF_SIZE, MSG_DONTWAIT);
	//log_debug("%s(), ret: %d\n", __func__, ret);
	if (ret <= 0) {
		log_err("%s(), %d, sockfd %d(%d) read error\n", __func__, __LINE__, watcher->fd, iproxy_handle->sockfd);
		ev_io_stop(loop, watcher);
		close(watcher->fd);
		iproxy_handle->sockfd = -1;
		return;
	}
	buf[ret] = '\0';

	iproxy_cmd_t *ack = (iproxy_cmd_t *)buf;
	log_debug("magic:%s\n", ack->magic);
	log_debug("level:%d\n", ack->level);
	log_debug("ID:%d\n", ack->id);
	log_debug("key_len:%d\n", ack->key_len);
	log_debug("value_len:%d\n", ack->value_len);
	char *key = buf + cmd_len;
	char *value = buf + cmd_len + ack->key_len;

	char key1[MAX_KEY_LEN + SUB_KEY_REORG_OFFSET] = {'\0'};
	snprintf(key1, MAX_KEY_LEN, SUB_KEY_REORG_TEMPLATE, ack->level, key);
	log_debug("key:%s, key1: %s\n", key, key1);
	log_debug("value:%s\n", value);

	if (ret != (cmd_len + ack->key_len + ack->value_len) || ack->key_len <= 1 || ack->id != IPROXY_PUB)
		return;

	iproxy_func_node_t *func_node;
	ret = hashmap_get(iproxy_handle->hashmap, key1, (void**)(&func_node));
	if (ret == MAP_OK && func_node->level == ack->level) {
		//log_debug("start run callback: %p\n", func_node->func);
		func_node->func(value);
	}
}

static void periodic_cb(struct ev_loop *loop, ev_periodic *watcher, int revents)
{
	iproxy_handle_t *iproxy_handle = (iproxy_handle_t *)watcher->data;
	if (iproxy_handle->sockfd < 0) {
		iproxy_handle->sockfd = iproxyd_connect();
		log_debug("%s(), %d, iproxy_handle->sockfd: %d, %p\n", __func__, __LINE__, iproxy_handle->sockfd, &(iproxy_handle->iproxy_client));
		if (iproxy_handle->sockfd > 0) {
			ev_io_init(&iproxy_handle->iproxy_client, iproxy_recv_handle, iproxy_handle->sockfd, EV_READ);
			ev_io_start(EV_DEFAULT_ &iproxy_handle->iproxy_client);
			iproxy_resub(iproxy_handle);
		}
	}
}

void iproxy_close(iproxy_handle_t *iproxy_handle)
{
	if (iproxy_handle) {
		ev_io_stop(EV_DEFAULT_ &(iproxy_handle->iproxy_client));
		ev_periodic_stop(EV_DEFAULT_ &(iproxy_handle->periodic_tick));
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
		//log_err("create ipc client error: %d\n", sockfd);
		return -1;
	}

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(struct sockaddr_un));

	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, IPROXY_IPC_SOCK_PATH, sizeof(addr.sun_path) - 1);

	int ret = connect(sockfd, (const struct sockaddr *) &addr, sizeof(struct sockaddr_un));
	if (ret  < 0) {
		//log_err("The server is down.\n");
		return -1;
	}

	//log_debug("%s(), %d, sockfd: %d\n", __func__, __LINE__, sockfd);
	return sockfd;
}

iproxy_handle_t *iproxy_open(void)
{
	int sockfd = iproxyd_connect();
	if (sockfd < 0) {
		return NULL;
	}
	log_info("connect iproxyd success...\n");

	iproxy_handle_t *iproxy_handle = (iproxy_handle_t *)malloc(sizeof(iproxy_handle_t));
	iproxy_handle->hashmap = hashmap_new();
	if (!iproxy_handle->hashmap) {
		log_info("new iproxy_handle_t hashmap error.\n");
		iproxy_close(iproxy_handle);
		return NULL;
	}

	iproxy_handle->sockfd = sockfd;
	ev_periodic_init(&(iproxy_handle->periodic_tick), periodic_cb, 0., 5., 0);
	iproxy_handle->periodic_tick.data = iproxy_handle;
	ev_periodic_start(EV_DEFAULT_ &(iproxy_handle->periodic_tick));

	ev_io_init(&(iproxy_handle->iproxy_client), iproxy_recv_handle, sockfd, EV_READ);
	iproxy_handle->iproxy_client.data = iproxy_handle;
	ev_io_start(EV_DEFAULT_ &(iproxy_handle->iproxy_client));

	log_debug("%s(), %d, %p\n", __func__, __LINE__,  &(iproxy_handle->iproxy_client));

	return iproxy_handle;
}

static char *construct_packet(iproxy_cmd_level_t level, iproxy_cmd_id_t cmd_id, char *key, char *value, int *length)
{
	int key_len=0, value_len=0;

	if (level < IPROXY_RAM_LEVEL || IPROXY_RAM_LEVEL > IPROXY_FACTORY_LEVEL) {
		//log_err("%s(), level is unknow: %d\n", level);
		return NULL;
	}

	if (key)
		key_len= strlen(key) + 1;
	if (value)
		value_len = strlen(value) + 1;

	if (key_len > MAX_KEY_LEN || value_len > MAX_VALUE_LEN) {
		//log_err("key max length is %d, value max length is %d\n", MAX_KEY_LEN, MAX_VALUE_LEN);
		return NULL;
	}

	iproxy_cmd_t cmd;

	int cmd_len = sizeof(cmd);

	memcpy(cmd.magic,IPROXY,4);
	cmd.level = level;
	cmd.id = cmd_id;
	cmd.key_len = key_len;
	cmd.value_len = value_len;

	int total_len = key_len + value_len + cmd_len;
	char *buf = malloc(total_len);
	if (!buf) {
		log_err("%s, %d,malloc error\n", __func__, __LINE__);
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

static int iproxy_send(iproxy_cmd_level_t level, iproxy_cmd_id_t cmd_id, char *key, char *value)
{
	int length = 0;
	char *send_buf = construct_packet(level, cmd_id, key, value, &length);
	if (!send_buf) {
		//log_err("construct_packet error\n");
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

static int iproxy_send_recv(iproxy_cmd_level_t level, iproxy_cmd_id_t cmd_id, char *key, char *value)
{
	int length = 0;
	*value = '\0';
	char *send_buf = construct_packet(level, cmd_id, key, NULL, &length);
	if (!send_buf) {
		//log_err("construct_packet error\n");
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

int iproxy_set(iproxy_cmd_level_t level, char *key, char *value)
{
	if (!key || *key == '\0' || !value ) {
		log_err("key and value must not be null\n");
		return -1;
	}
	return iproxy_send(level, IPROXY_SET, key, value);
}

int iproxy_unset(iproxy_cmd_level_t level, char *key)
{
	if (!key || *key == '\0') {
		log_err("key and value must not be null\n");
		return -1;
	}
	return iproxy_send(level, IPROXY_UNSET, key, NULL);
}

int iproxy_get(iproxy_cmd_level_t level, char *key, char *value)
{
	if (!key || *key == '\0' || !value ) {
		//log_err("key and value must not be null\n");
		return -1;
	}
	return iproxy_send_recv(level, IPROXY_GET, key, value);
}

int iproxy_sub(iproxy_handle_t *iproxy_handle, iproxy_cmd_level_t level, char *key, void (*func)(char *))
{
	if (!key || *key == '\0' || !iproxy_handle) {
		log_err("key or handle must not be null\n");
		return -1;
	}

	int length = 0;
	char *send_buf = construct_packet(level, IPROXY_SUB, key, NULL, &length);
	if (!send_buf) {
		log_err("construct_packet error\n");
		return -1;
	}
	write(iproxy_handle->sockfd, send_buf, length);
	free(send_buf);

	iproxy_func_node_t *func_node;
	func_node = malloc(sizeof(iproxy_func_node_t));
	func_node->level = level;
	func_node->func = func;

	int key_len = strlen(key) + 1;
	char *key1 = malloc(key_len + SUB_KEY_REORG_OFFSET);
	snprintf(key1, key_len + SUB_KEY_REORG_OFFSET, SUB_KEY_REORG_TEMPLATE, level, key);

	log_debug("Sub key: %s, key1: %s\n", key, key1);

	int ret = hashmap_put(iproxy_handle->hashmap, key1, func_node);
	return ret;
}

static int iproxy_resub(iproxy_handle_t *iproxy_handle)
{
	if (!iproxy_handle) {
		log_err(" handle must not be null\n");
		return -1;
	}

	int i = 0, ret = 0;
	char *key = NULL;
	iproxy_func_node_t *func_node = NULL;

	log_debug("%s, %d, fd: %d\n", __func__, __LINE__, iproxy_handle->sockfd);
	do {
		ret =hashmap_get_key_value_index(iproxy_handle->hashmap, i++, (char **)(&key), (void **)(&func_node));
		if (ret == MAP_OK) {
			int length = 0;
			//log_debug("%s(), %d, level: %d, key: %s, value: %p\n", __func__, __LINE__, func_node->level, key, func_node->func);

			char *send_buf = construct_packet(func_node->level, IPROXY_SUB, key + SUB_KEY_REORG_OFFSET, NULL, &length);
			if (!send_buf) {
				log_err("construct_packet error\n");
				return -1;
			}
			int len = write(iproxy_handle->sockfd, send_buf, length);

			log_info("RESUB: key: %s, fd; %d, send_len: %d\n", key + SUB_KEY_REORG_OFFSET, iproxy_handle->sockfd, len);
			free(send_buf);
		}
	} while(ret != MAP_END);
	return ret;
}

int iproxy_unsub(iproxy_handle_t *iproxy_handle, iproxy_cmd_level_t level, char *key)
{
	if (!key || *key == '\0' || !iproxy_handle) {
		log_err("key and handle must not be null\n");
		return -1;
	}

	int length = 0;
	char *send_buf = construct_packet(level, IPROXY_UNSUB, key, NULL, &length);
	if (!send_buf) {
		log_err("construct_packet error\n");
		return -1;
	}

	write(iproxy_handle->sockfd, send_buf, length);
	free(send_buf);

	int ret = hashmap_remove(iproxy_handle->hashmap, key);
	log_info("UNSUB: key: %s\n", key);
	return ret;
}

int iproxy_sync(iproxy_cmd_level_t level)
{
	return iproxy_send(level, IPROXY_SYNC, NULL, NULL);
}

int iproxy_clear(iproxy_cmd_level_t level)
{
	return iproxy_send(level, IPROXY_CLEAR, NULL, NULL);
}

int iproxy_list(iproxy_cmd_level_t level)
{
	iproxy_cmd_t cmd;
	char buf[1024] = {'\0'};

	int cmd_len = sizeof(cmd);

	memcpy(cmd.magic,IPROXY,4);
	cmd.level = level;
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

			fprintf(stderr, "%-32s:\t%s\n", key, value);
		}
	} while (ret > 0);

	close(sockfd);
	return 0;
}
