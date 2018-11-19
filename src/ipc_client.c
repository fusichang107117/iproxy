#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <ev.h>
#include "iproxy.h"


struct ev_async async;
struct ev_periodic periodic_tick;
struct ev_io ipc_client;

int ipc_client_sockfd = -1;

static void ipc_recv_handle(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
	char buf[MAX_BUF_SIZE] = { 0 };

	int ret = read(watcher->fd, buf, MAX_BUF_SIZE);
	if (ret <= 0) {
		perror("read");
		ev_io_stop(loop, watcher);
		close(watcher->fd);
		free(watcher);
		ipc_client_sockfd = -1;
		return;
	}
	buf[ret] = '\0';
	printf("ret:%d, %s\n", ret, buf);
}

static int ipc_client_init(void)
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
	ev_io_init(&ipc_client, ipc_recv_handle, sockfd, EV_READ);
	ev_io_start(EV_DEFAULT_ &ipc_client);

	return sockfd;
}

char send_buf[MAX_BUF_SIZE] = { 0 };

static int send_test(void)
{
	if (ipc_client_sockfd > 0) {
		write(ipc_client_sockfd, send_buf, strlen(send_buf) + 1);
	}
}

static int ipc_client_reconnect(void)
{
	if (ipc_client_sockfd < 0) {
		ipc_client_sockfd = ipc_client_init();
	}
}

static void periodic_cb(struct ev_loop *loop, ev_periodic *w, int revents)
{
	//log_debug("%s(), ot_sockfd: %d\n", __func__, __LINE__, ot_sockfd);
	if (ipc_client_sockfd <= 0)
		ipc_client_reconnect();

	send_test();
}

void sig_stop_ev(void)
{
	ev_async_send(EV_DEFAULT_ &async);
	ev_break(EV_DEFAULT_ EVBREAK_ALL);
}

int main(int argc, char const *argv[])
{
	struct ev_loop *loop = EV_DEFAULT;

	if (argc < 2) return -1;
	snprintf(send_buf, MAX_BUF_SIZE, "%s", argv[1]);

	printf("send_buf is %s\n", send_buf);

	ev_periodic_init(&periodic_tick, periodic_cb, 0., 1., 0);
	ev_periodic_start(loop, &periodic_tick);
	ev_feed_event(EV_DEFAULT_ &periodic_tick, EV_PERIODIC);

	ev_async_init(&async, sig_stop_ev);
	ev_async_start(loop, &async);

	ev_run(loop, 0);

	ev_default_destroy();

	if (ipc_client_sockfd > 0)
		close(ipc_client_sockfd);

	return 0;
}

