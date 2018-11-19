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
	strncpy(addr.sun_path, IPC_SOCK_PATH, sizeof(addr.sun_path) - 1);

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

	int ret = read(watcher->fd, buf, MAX_BUF_SIZE);
	if (ret <= 0) {
		perror("read");
		ev_io_stop(loop, watcher);
		close(watcher->fd);
		free(watcher);
		iproxy_sockfd = -1;
		return;
	}
	buf[ret] = '\0';
	printf("ret:%d, %s\n", ret, buf);
}

int iproxy_open(void)
{
	struct ev_loop *loop = EV_DEFAULT;

	iproxy_sockfd = iproxyd_connect();
	if (iproxy_sockfd < 0) {
		return -1;
	}

	mymap = hashmap_new();

	printf("connect iproxyd success...\n");
	ev_periodic_init(&periodic_tick, periodic_cb, 0., 5., 0);
	ev_periodic_start(loop, &periodic_tick);

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

	iproxy_cmd_t cmd;
	char buf[1024] = {'\0'};

	int cmd_len = sizeof(cmd);

	memcpy(cmd.magic,IPROXY,4);
	cmd.id = IPROXY_SET;
	cmd.key_len = strlen(key) + 1;
	cmd.value_len = strlen(value) + 1;
	memcpy(buf,(char *)&cmd, cmd_len);

	//printf("int: %d\n", sizeof(int));
	printf("cmd_len: %d\n", cmd_len);
	printf("key_len: %d\n", cmd.key_len);
	printf("key: %s\n", key);
	printf("value_len: %d\n", cmd.value_len);
	printf("value: %s\n", value);

	snprintf(buf + cmd_len,cmd.key_len,"%s",key);
	if(value) {
		snprintf(buf + cmd_len + cmd.key_len,cmd.value_len,"%s",value);
	}

	int total_len = cmd_len + cmd.key_len + cmd.value_len;

	int len = write(iproxy_sockfd, buf, total_len);
	printf("send: %d, str_len: %d\n", len, total_len);
	return (len == (total_len + 1)) ? 0 : -1;
}

int iproxy_get(char *key, char *value)
{
	if (!key || *key == '\0' || !value ) {
		printf("key must not be null\n");
		return -1;
	}

	iproxy_cmd_t cmd;
	char buf[1024] = {'\0'};

	int cmd_len = sizeof(cmd);

	memcpy(cmd.magic,IPROXY,4);
	cmd.id = IPROXY_GET;
	cmd.key_len = strlen(key) + 1;
	cmd.value_len = 0;
	memcpy(buf,(char *)&cmd, cmd_len);

	snprintf(buf + cmd_len,cmd.key_len,"%s",key);

	int total_len = cmd_len + cmd.key_len;

	int len = write(iproxy_sockfd, buf, total_len);

	int ret = read(iproxy_sockfd, buf, 1024);

	printf("GET: ret: %d, result: %s\n", ret, buf);
	return 0;
}

int iproxy_register(char *key, void (*func)(char *))
{
	if (!key || *key == '\0' ) {
		printf("key must not be null\n");
		return -1;
	}

	iproxy_cmd_t cmd;
	char buf[1024] = {'\0'};

	int cmd_len = sizeof(cmd);

	memcpy(cmd.magic,IPROXY,4);
	cmd.id = IPROXY_REGISTER;
	cmd.key_len = strlen(key) + 1;
	cmd.value_len = 0;
	memcpy(buf,(char *)&cmd, cmd_len);

	snprintf(buf + cmd_len,cmd.key_len,"%s",key);

	int total_len = cmd_len + cmd.key_len;

	int len = write(iproxy_sockfd, buf, total_len);

	data_node_t *data;
	data = malloc(sizeof(data_node_t));
	data->value = func;
	char *key1 = malloc(cmd.key_len);

	snprintf(key1, cmd.key_len, "%s", key);

	int ret = hashmap_put(mymap, key1, data);
	printf("hashmap_put ret: %d\n", ret);

	return 0;
}

int iproxy_register_and_get(char *key, char *value)
{
	return 0;
}

int iproxy_unregister(char *key)
{
	return 0;
}

int iproxy_commit()
{
	return 0;
}

void *callback(char  *value)
{
	printf("%s, %d\n", __func__, __LINE__);
}

int main(int argc, char const *argv[])
{
	/* code */
/*	if (argc != 3) {
		printf("use like this: ./iproxylib key value\n");
		return -1;
	}*/
	char buf[1024];
	if(strcmp(argv[1], "set") == 0)
		iproxy_set(argv[2], argv[3]);
	else if(strcmp(argv[1], "get") == 0)
		iproxy_get(argv[2],buf);
	else if(strcmp(argv[1], "register") == 0)
		iproxy_register(argv[2], callback);
	else if (strcmp(argv[1], "unregister") == 0)
		iproxy_unregister(argv[2]);
	else if (strcmp(argv[1], "commit") == 0)
		iproxy_commit();
	return 0;
}