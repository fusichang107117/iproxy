#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ev.h>

#include "iproxy.h"
#include "iproxylib.h"

struct ev_async async;
iproxy_handle_t *iproxy_handle;

char key[64];
iproxy_cmd_level_t level = IPROXY_TOTAL_LEVEL;

static void sig_stop_ev(void)
{
	printf("%s(), %d\n", __func__, __LINE__);
	ev_async_send(EV_DEFAULT_ &async);
	ev_break(EV_DEFAULT_ EVBREAK_ALL);
}

static void callback1(char  *value)
{
	printf("%s, %d, key:%s, value: %s\n", __func__, __LINE__,key, value);
}

static void timer_cb (struct ev_loop *loop, ev_timer *w, int revents)
{
	iproxy_sub(iproxy_handle, level, key, callback1);
}

int main(int argc, char const *argv[])
{
#if 1
	if (argc != 3) {
		printf("use like this: ./iproxy_sub level key\n");
		return -1;
	}

	if (strcmp((char *)argv[1], "ram") == 0)
		level = IPROXY_RAM_LEVEL;
	else if (strcmp((char *)argv[1], "file") == 0)
		level = IPROXY_FILE_LEVEL;
	else if (strcmp((char *)argv[1], "nvram") == 0)
		level = IPROXY_NVRAM_LEVEL;
	else {
		printf("use like this: ./iproxy_sub level key\n");
		return -1;
	}

	snprintf(key, 64, "%s", argv[2]);

	printf("sub level: %d key: %s\n",level, key);

	iproxy_handle= iproxy_open();
	if (!iproxy_handle) {
		printf("iproxyd connect error\n");
		return -1;
	}
#endif
	struct ev_loop *loop = EV_DEFAULT;

	ev_async_init(&async, sig_stop_ev);
	ev_async_start(loop, &async);

	iproxy_sub(iproxy_handle, level, key, callback1);

	printf("start mainloop,ev_backend is %d\n", ev_backend(loop));
	ev_run(loop, 0);

	printf("%s(),%d\n",__func__, __LINE__);

	iproxy_close(iproxy_handle);
	ev_default_destroy();
	return 0;
}