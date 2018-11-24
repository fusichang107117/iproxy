#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ev.h>

#include "iproxy.h"
#include "iproxylib.h"
#include "hashmap.h"


struct ev_async async;
struct ev_periodic periodic_tick;
iproxy_handle_t *iproxy_handle;

char key[64];

void sig_stop_ev(void)
{
	printf("%s(), %d\n", __func__, __LINE__);
	ev_break(EV_DEFAULT_ EVBREAK_ALL);
}

void callback1(char  *value)
{
	printf("%s, %d, key:%s, value: %s\n", __func__, __LINE__,key, value);
}

void callback2(char  *value)
{
	printf("%s, %d, value: %s\n", __func__, __LINE__,value);
}

static void periodic_cb(struct ev_loop *loop, ev_periodic *watcher, int revents)
{
	static int flag = 1;

	ev_periodic_stop(EV_DEFAULT_ watcher);

	//iproxy_set("key1", "on");

	//iproxy_set("key2", "on");

	if (flag) {

		iproxy_sub(iproxy_handle, key, callback1);

		//iproxy_sub("key2", callback2);

		flag = 0;
	}

	//iproxy_set("key2", "off");

	//iproxy_set("key1", "off");
}

static void sighandler(int sig)
{
	ev_async_send(EV_DEFAULT_ &async);
}

int main(int argc, char const *argv[])
{
	if (argc != 2) {
		printf("use like this: ./iproxy_example key\n");
		return -1;
	}
	snprintf(key, 64, "%s", argv[1]);

	printf("sub key: %s\n", key);

	iproxy_handle= iproxy_open();
	if (!iproxy_handle) {
		printf("iproxyd connect error\n");
		return -1;
	}

	signal(SIGINT, sighandler);
	signal(SIGTERM, sighandler);
	signal(SIGSEGV, sighandler);
	signal(SIGPIPE, SIG_IGN);

	struct ev_loop *loop = EV_DEFAULT;
	ev_periodic_init(&periodic_tick, periodic_cb, 0., 3., 0);
	ev_periodic_start(loop, &periodic_tick);
	ev_feed_event(loop, &periodic_tick, EV_PERIODIC);

	ev_async_init(&async, sig_stop_ev);
	ev_async_start(loop, &async);

	printf("start mainloop,ev_backend is %d\n", ev_backend(loop));
	ev_run(loop, 0);

	printf("%s(),%d\n",__func__, __LINE__);

	iproxy_close(iproxy_handle);
	ev_default_destroy();
	return 0;
}