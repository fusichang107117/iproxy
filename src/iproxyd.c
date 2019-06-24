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
#include "nvram.h"
#include "log.h"

struct ev_async async;
struct ev_periodic sync_tick;

nvram_handle_t *factory_handle;
nvram_handle_t *nvram_handle;
file_handle_t *file_handle;
file_handle_t *ram_handle;

int ipc_client_count = 0;
volatile bool sync_flag[IPROXY_TOTAL_LEVEL] = {false};

static int iproxyd_server_init(void)
{
	int fd;
	struct sockaddr_un addr;

	unlink(IPROXY_IPC_SOCK_PATH);

	fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (fd < 0) {
		log_err("create ipc server error: %d\n", fd);
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, IPROXY_IPC_SOCK_PATH, sizeof(addr.sun_path) - 1);

	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		log_err("bind ipc server error\n");
		return -1;
	}

	if (listen(fd, MAX_IPC_CLIENT_FDS) < 0) {
		log_err("listen ipc server error\n");
		return -1;
	}

	return fd;
}

static void sync_cb(struct ev_loop *loop, ev_periodic *watcher, int revents)
{
#if 0
	if (sync_flag[IPROXY_RAM_LEVEL]) {
		log_info("start sync to ram ...\n");
		backend_file_sync(ram_handle, IPROXY_RAM_PATH);
		sync_flag[IPROXY_RAM_LEVEL] = false;
	}
#endif
	if (sync_flag[IPROXY_FILE_LEVEL]) {
		log_info("start sync to file ...\n");
		backend_file_sync(file_handle, IPROXY_FILE_PATH);
		sync_flag[IPROXY_FILE_LEVEL] = false;
	}
	if (sync_flag[IPROXY_NVRAM_LEVEL]) {
		log_info("start sync to nvram ...\n");
		backend_nvram_sync(nvram_handle);
		sync_flag[IPROXY_NVRAM_LEVEL] = false;
	}
}

static void close_fd(struct ev_io *watcher)
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

	int recv_len = recv(watcher->fd, buf, MAX_BUF_SIZE, MSG_DONTWAIT);
	if (recv_len <= 0) {
		//log_err("errno: %d, recv_len: %d\n",errno, recv_len);
		//if (errno == 2)
		hashmap_remove_spec_fd(ram_handle->hashmap, watcher->fd);
		hashmap_remove_spec_fd(file_handle->hashmap, watcher->fd);
		hashmap_remove_spec_fd(nvram_handle->hashmap, watcher->fd);
		log_err("read");
		close_fd(watcher);
		return;
	}
	buf[recv_len] = '\0';
	int cmd_len = sizeof(iproxy_cmd_t);

	iproxy_cmd_t *cmd = (iproxy_cmd_t *)buf;

	log_debug("cmdid: %d\n", cmd->id);
	log_debug("level: %d\n", cmd->level);
	log_debug("keylen: %d\n", cmd->key_len);
	log_debug("valuelen: %d\n", cmd->value_len);

	map_t hashmap = NULL;
	switch(cmd->level) {
		case IPROXY_RAM_LEVEL:
			hashmap = ram_handle->hashmap;
		break;
		case IPROXY_FILE_LEVEL:
			hashmap = file_handle->hashmap;
		break;
		case IPROXY_NVRAM_LEVEL:
			hashmap = nvram_handle->hashmap;
		break;
		case IPROXY_FACTORY_LEVEL:
			if (cmd->id == IPROXY_GET || cmd->id == IPROXY_LIST)
				hashmap = factory_handle->hashmap;
		break;
		default:
			log_err("unknow level : %d\n", cmd->level);
		break;
	}

	if (!hashmap) {
		close_fd(watcher);
		return;
	}

	char *key = buf + cmd_len;
	char *value = buf + cmd_len + cmd->key_len;

	log_debug("key: %s\n", key);
	log_debug("value: %s\n", value);

	switch(cmd->id) {
		case IPROXY_SET:
		{
			iproxy_data_node_t *data = NULL;
			log_info("IPROXY_SET\n");
			ret = hashmap_get(hashmap, key, (void**)(&data));
			if (ret == MAP_OK) {
				if (strncmp(data->value, value, cmd->value_len) == 0) {
					log_warning("value is same, ignore it\n");
				} else {
					int i = 0;

					char *value1 = malloc(cmd->value_len);
					snprintf(value1, cmd->value_len, "%s", value);
					ret = hashmap_update_one_value(hashmap, key, value1);

					iproxy_cmd_t *ack = (iproxy_cmd_t *)buf;
					ack->id = IPROXY_PUB;
					for (i = 1; i <= data->fd[0]; i++) {
						log_info(" data->fd[%d]: %d, level: %d, key: %s, value: %s\n", i, data->fd[i], ack->level, key, value);
						int send_len = write(data->fd[i], buf,  recv_len);
						if(send_len < 0) {
							close(data->fd[i]);
							hashmap_remove_spec_fd(hashmap, data->fd[i]);
						}
					}
					sync_flag[cmd->level] = true;
				}
			} else {
				char *key1 = malloc(cmd->key_len);
				data = malloc(sizeof(iproxy_data_node_t));
				memset(data, 0, sizeof(iproxy_data_node_t));
				data->value = malloc(cmd->value_len);
				
				snprintf(key1, cmd->key_len, "%s", key);
				snprintf(data->value, cmd->value_len, "%s", value);
				ret = hashmap_put(hashmap, key1, data);
				sync_flag[cmd->level] = true;
				//log_debug("hashmap_put ret: %d\n", ret);
			}
			close_flag = true;
		}
		break;
		case IPROXY_GET:
		{
			log_info("IPROXY_GET\n");
			iproxy_data_node_t *data;
			ret = hashmap_get(hashmap, key, (void**)(&data));
			//log_debug("hashmap_get ret: %d\n", ret);
			if (ret != MAP_OK) {
				log_warning("%s not found\n", key);
				buf[0]='\0';
			} else {
				//log_debug("GET:key: %s\n", key);
				//log_debug("GET:value: %s\n", data->value);
				snprintf(buf, MAX_BUF_SIZE, "%s", data->value);
			}
			write(watcher->fd,buf, strlen(buf) + 1);
			close_flag = true;
		}
		break;
		case IPROXY_SUB:
		{
			iproxy_data_node_t *data;
			iproxy_cmd_t *ack = (iproxy_cmd_t *)buf;
			ack->id = IPROXY_PUB;

			log_info("IPROXY_SUB\n");
			ret = hashmap_get(hashmap, key, (void**)(&data));
			if (ret == MAP_OK) {
				hashmap_add_one_fd(hashmap, key, watcher->fd);
				int get_value_len = strlen(data->value) + 1;
				ack->value_len =  get_value_len;
				memcpy(value, data->value, get_value_len);
			} else {
				char *key1 = malloc(cmd->key_len);
				data = malloc(sizeof(iproxy_data_node_t));
				data->value = malloc(1);
				data->fd[0] = 1;
				data->fd[1] = watcher->fd;

				snprintf(key1, cmd->key_len, "%s", key);
				snprintf(data->value, 1, "%s", "");
				ret = hashmap_put(hashmap, key1, data);
				ack->value_len =  1;
				*value = '\0';
				//log_debug("hashmap_put ret: %d\n", ret);
			}
			write(watcher->fd, buf, cmd_len + ack->key_len + ack->value_len);
			log_info("value: %s\n", ack->value);
			log_info("value_len: %d\n", ack->value_len);
		}
		break;
		case IPROXY_UNSUB:
		{
			log_info("IPROXY_UNSUB\n");
			hashmap_remove_one_fd(hashmap, key, watcher->fd);
		}
		break;
		case IPROXY_UNSET:
		{
			log_info("IPROXY_UNSET\n");
			iproxy_data_node_t *data = NULL;
			ret = hashmap_get(hashmap, key, (void**)(&data));
			if (ret == MAP_OK) {
				int i = 0;

				iproxy_cmd_t *ack = (iproxy_cmd_t *)buf;
				ack->id = IPROXY_PUB;
				for (i = 1; i <= data->fd[0]; i++) {
					log_info(" data->fd[%d]: %d, level: %d, key: %s, value: %s\n", i, data->fd[i], ack->level, key, value);
					int send_len = write(data->fd[i], buf,  recv_len);
					if(send_len < 0) {
						close(data->fd[i]);
						hashmap_remove_spec_fd(hashmap, data->fd[i]);
					}
				}
				if (data->fd[0] >= 1)
					*(data->value)='\0';
				else
					hashmap_remove_free(hashmap, key);
				sync_flag[cmd->level] = true;
			}
			close_flag = true;
		}
		break;
		case IPROXY_SYNC:
		{
			log_info("IPROXY_SYNC\n");
			sync_flag[cmd->level] = true;
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
				index = hashmap_get_from_index(hashmap, index, buf, MAX_BUF_SIZE, &real_len);
				//log_debug("index: %d, real_len: %d\n", index, real_len);
				write(watcher->fd, buf, real_len);
			} while (index != MAP_END);
			close_flag = true;
		}
		break;
		case IPROXY_CLEAR:
		{
			log_info("clear hashmap of level:%d\n",cmd->level);
			hashmap_clear(hashmap);
			sync_flag[cmd->level] = true;
			ev_feed_event(loop, &sync_tick, EV_PERIODIC);
			close_flag = true;
		}
		break;
		default:
			log_err("error commid ID: %d\n", cmd->id);
		break;
	}
	if (close_flag)
		close_fd(watcher);
}

static void ipc_accept_handle(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
	int client_fd;

	if (EV_ERROR & revents) {
		log_err("error event in accept\n");
		return;
	}
	if (ipc_client_count >= MAX_IPC_CLIENT_FDS) {
		log_err("too many ipc client to track %d.\n", ipc_client_count);
		return;
	}

	client_fd = accept(watcher->fd, NULL, NULL);
	if (client_fd < 0) {
		log_err("accept error\n");
		return;
	}

	struct ev_io *w_client = (struct ev_io *)malloc(sizeof(struct ev_io));
	if (!w_client) {
		log_err("%s(), %d: malloc error\n", __func__, __LINE__);
		close(client_fd);
		return;
	}

	ipc_client_count++;
	log_info("client %d connected,total %d clients.\n", client_fd, ipc_client_count);

	ev_io_init(w_client, ipc_recv_handle, client_fd, EV_READ);
	ev_io_start(loop, w_client);
}

void sig_stop_ev(void)
{
	log_debug("%s, %d\n", __func__, __LINE__);
	ev_break(EV_DEFAULT_ EVBREAK_ALL);
}

static void sighandler(int sig)
{
	log_debug("%s, %d, %d\n", __func__, __LINE__, sig);
	exit(-1);
}

int main(int argc, char const *argv[])
{
	struct ev_loop *loop = EV_DEFAULT;

	struct ev_io ipc_server;
	int ipc_serverfd;
	ipc_serverfd = iproxyd_server_init();
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
	ev_periodic_start(loop, &sync_tick);

	ev_async_init(&async, sig_stop_ev);
	ev_async_start(loop, &async);

	ram_handle = backend_file_init(IPROXY_RAM_PATH);
	if (!ram_handle) {
		log_err("create ram_handle error\n");
	}
	file_handle = backend_file_init(IPROXY_FILE_PATH);
	if (!file_handle) {
		log_err("create file_handle error\n");
	}
	nvram_handle = backend_nvram_init(IPROXY_NVRAM_LEVEL);
	if (!nvram_handle) {
		log_err("create nvram_handle error\n");
	}
	factory_handle = backend_nvram_init(IPROXY_FACTORY_LEVEL);
	if (!factory_handle) {
		log_err("create factory_handle error\n");
	}

	ev_run(loop, 0);

OUT:
	ev_default_destroy();

	if (ipc_serverfd > 0)
		close(ipc_serverfd);

	unlink(IPROXY_IPC_SOCK_PATH);

	if(ram_handle)
		backend_file_destory(ram_handle);
	if (file_handle)
		backend_file_destory(file_handle);
	if(nvram_handle)
		backend_nvram_destory(nvram_handle);
	if (factory_handle)
		backend_nvram_destory(factory_handle);

	return 0;
}