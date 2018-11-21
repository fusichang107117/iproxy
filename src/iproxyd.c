/*
 * A unit test and example of how to use the simple C hashmap
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ev.h>

#include "iproxy.h"
#include "hashmap.h"
#include "backend.h"

struct ev_async async;
struct ev_periodic sync_tick;

map_t mymap;

int ipc_client_count = 0;
volatile bool sync_flag = false;

static int ipc_server_init(void)
{
	int fd;
	struct sockaddr_un addr;

	unlink(IPC_SOCK_PATH);

	fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (fd < 0) {
		printf("create ipc server error: %d\n", fd);
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, IPC_SOCK_PATH, sizeof(addr.sun_path) - 1);

	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		printf("bind ipc server error\n");
		return -1;
	}

	if (listen(fd, MAX_IPC_CLIENT_FDS) < 0) {
		printf("listen ipc server error\n");
		return -1;
	}

	return fd;
}

static void sync_cb(struct ev_loop *loop, ev_periodic *watcher, int revents)
{
	char buf[MAX_BUF_SIZE];
	int index = 0;

	ev_periodic_stop(EV_DEFAULT_ watcher);
	printf("sync to file...\n");
	do {
		int real_len = 0;
		memset(buf, 0, MAX_BUF_SIZE);
		index = hashmap_get_from_index(mymap, index, buf, MAX_BUF_SIZE, &real_len);
		//printf("index: %d, real_len: %d\n", index, real_len);
		hashmap_sync(buf, real_len);
	} while (index != MAP_END);
	sync_flag = false;
}

static close_fd(struct ev_io *watcher)
{
	ev_io_stop(EV_DEFAULT_ watcher);
	close(watcher->fd);
	free(watcher);
	ipc_client_count--;
}

static void ipc_recv_handle(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
	char buf[MAX_BUF_SIZE] = { 0 };
	int ret;
	bool close_flag = false;

	int recv_len = read(watcher->fd, buf, MAX_BUF_SIZE);
	if (recv_len <= 0) {
		//printf("errno: %d, recv_len: %d\n",errno, recv_len);
		//if (errno == 2)
			hashmap_remove_spec_fd(mymap, watcher->fd);
		perror("read");
		close_fd(watcher);
		return;
	}
	buf[recv_len] = '\0';
	//printf("ret:%d, %s\n", ret, buf);
	int cmd_len = sizeof(iproxy_cmd_t);

	iproxy_cmd_t *cmd = (iproxy_cmd_t *)buf;

	//printf("cmdid: %d\n", cmd->id);
	//printf("keylen: %d\n", cmd->key_len);
	//printf("valuelen: %d\n", cmd->value_len);

	char *key = buf + cmd_len;
	char *value = buf + cmd_len + cmd->key_len;

	//printf("key: %s\n", key);
	//printf("value: %s\n", value);

	switch(cmd->id) {
		case IPROXY_SET:
		{
			data_node_t *data = NULL;
			printf("IPROXY_SET\n");
			ret = hashmap_get(mymap, key, (void**)(&data));
			if (ret == MAP_OK) {
				if (memcmp(data->value, value, cmd->value_len) == 0) {
					printf("value is same, ignore it\n");
				} else {
					int i = 0;

					char *value1 = malloc(cmd->value_len);
					snprintf(value1, cmd->value_len, "%s", value);
					ret = hashmap_update_one_value(mymap, key, value1);

					iproxy_cmd_t *ack = (iproxy_cmd_t *)buf;
					ack->id = IPROXY_PUB;
					for (i = 1; i <= data->fd[0]; i++) {
						printf(" data->fd[%d]: %d, key: %s, value: %s\n", i, data->fd[i], key, value);
						write(data->fd[i], buf,  recv_len);
					}
					if (sync_flag == false) {
						sync_flag = true;
						ev_periodic_start(loop, &sync_tick);
					}
				}
			} else {
				char *key1 = malloc(cmd->key_len);
				data = malloc(sizeof(data_node_t));
				memset(data, 0, sizeof(data_node_t));
				data->value = malloc(cmd->value_len);
				
				snprintf(key1, cmd->key_len, "%s", key);
				snprintf(data->value, cmd->value_len, "%s", value);
				ret = hashmap_put(mymap, key1, data);
				if (sync_flag == false) {
					sync_flag = true;
					ev_periodic_start(loop, &sync_tick);
				}
				//printf("hashmap_put ret: %d\n", ret);
			}
			close_flag = true;
		}
		break;
		case IPROXY_GET:
		{
			printf("IPROXY_GET\n");
			data_node_t *data;
			ret = hashmap_get(mymap, key, (void**)(&data));
			//printf("hashmap_get ret: %d\n", ret);
			if (ret != MAP_OK) {
				printf("%s not found\n", key);
				buf[0]='\0';
			} else {
				//printf("GET:key: %s\n", key);
				//printf("GET:value: %s\n", data->value);
				snprintf(buf, MAX_BUF_SIZE, "%s", data->value);
			}
			write(watcher->fd,buf, strlen(buf) + 1);
			close_flag = true;
		}
		break;
		case IPROXY_SUB:
		{
			data_node_t *data;
			printf("IPROXY_SUB\n");
			ret = hashmap_get(mymap, key, (void**)(&data));
			printf("hashmap_get ret: %d\n", ret);
			if (ret == MAP_OK) {
				hashmap_add_one_fd(mymap, key, watcher->fd);
			} else {
				char *key1 = malloc(cmd->key_len);
				data = malloc(sizeof(data_node_t));
				data->value = malloc(1);
				data->fd[0] = 1;
				data->fd[1] = watcher->fd;

				snprintf(key1, cmd->key_len, "%s", key);
				snprintf(data->value, 1, "%s", "");
				ret = hashmap_put(mymap, key1, data);
				//printf("hashmap_put ret: %d\n", ret);
			}
		}
		break;
		case IPROXY_SUB_AND_GET:
		break;
		case IPROXY_UNSUB:
		{
			printf("IPROXY_UNSUB\n");
			hashmap_remove_one_fd(mymap, key, watcher->fd);
		}
		break;
		case IPROXY_UNSET:
		{
			printf("IPROXY_UNSET\n");
			hashmap_remove_free(mymap, key);
			close_flag = true;
			if (sync_flag == false) {
				sync_flag = true;
				ev_periodic_start(loop, &sync_tick);
			}
			close_flag = true;
		}
		break;
		case IPROXY_SYNC:
		{
			sync_flag = true;
			ev_feed_event(loop, &sync_tick, EV_PERIODIC);
			close_flag = true;
		}
		break;
		case IPROXY_LIST:
		{
			int index = 0;
			do {
				int real_len = 0;
				memset(buf, 0, MAX_BUF_SIZE);
				index = hashmap_get_from_index(mymap, index, buf, MAX_BUF_SIZE, &real_len);
				//printf("index: %d, real_len: %d\n", index, real_len);
				write(watcher->fd, buf, real_len);
			} while (index != MAP_END);
			close_flag = true;
		}
		break;
		default:
			printf("error commid ID: %d\n", cmd->id);
		break;
	}
	if (close_flag)
		close_fd(watcher);
}

static void ipc_accept_handle(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
	int client_fd;

	if (EV_ERROR & revents) {
		printf("error event in accept\n");
		return;
	}
	if (ipc_client_count >= MAX_IPC_CLIENT_FDS) {
		printf("too many ipc client to track %d.\n", ipc_client_count);
		return;
	}

	client_fd = accept(watcher->fd, NULL, NULL);
	if (client_fd < 0) {
		printf("accept error\n");
		return;
	}

	struct ev_io *w_client = (struct ev_io *)malloc(sizeof(struct ev_io));
	if (!w_client) {
		printf("%s(), %d: malloc error\n", __func__, __LINE__);
		close(client_fd);
		return;
	}

	ipc_client_count++;
	printf("client %d connected,total %d clients.\n", client_fd, ipc_client_count);

	ev_io_init(w_client, ipc_recv_handle, client_fd, EV_READ);
	ev_io_start(loop, w_client);
}

void sig_stop_ev(void)
{
	ev_break(EV_DEFAULT_ EVBREAK_ALL);
}

static void sighandler(int sig)
{
	ev_async_send(EV_DEFAULT_ &async);
}

int main(int argc, char const *argv[])
{
	struct ev_loop *loop = EV_DEFAULT;

	struct ev_io ipc_server;
	int ipc_serverfd;
	ipc_serverfd = ipc_server_init();
	if (ipc_serverfd < 0) {
		return -1;
	}

	signal(SIGINT, sighandler);
	signal(SIGTERM, sighandler);
	signal(SIGSEGV, sighandler);
	signal(SIGPIPE, SIG_IGN);

	ev_io_init(&ipc_server, ipc_accept_handle, ipc_serverfd, EV_READ);
	ev_io_start(loop, &ipc_server);

	ev_periodic_init(&sync_tick, sync_cb, 0., BACKEND_SYNC_INTERVAL, 0);

	ev_async_init(&async, sig_stop_ev);
	ev_async_start(loop, &async);

	mymap = hashmap_new();

	hashmap_value_init(mymap);

	ev_run(loop, 0);

	ev_default_destroy();

	if (ipc_serverfd > 0)
		close(ipc_serverfd);

	unlink(IPC_SOCK_PATH);


	hashmap_free_free_free(mymap);

	return 0;
}