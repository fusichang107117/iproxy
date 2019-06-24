#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/file.h>

#include "iproxy.h"
#include "nvram.h"

static void usage(void)
{
	printf("use like this: \n");
	printf("\tfactory get key\n");
	printf("\tfacrory list\n");
}

int main(int argc, char const *argv[])
{
	int ret = -1;
	char *key = NULL;

	iproxy_cmd_id_t cmd = IPROXY_UNKNOW;
	if (argc == 3 && strcmp(argv[1], "get") == 0) {
		cmd = IPROXY_GET;
		key = (char *)argv[2];
	} else if (argc == 2 && strcmp(argv[1], "list") == 0) {
		cmd = IPROXY_LIST;
	} else {
		goto OUT;
	}

	ret = iproxy_sync_command(IPROXY_FACTORY_LEVEL, cmd, key);
OUT:
	if (ret < 0)
		usage();
	return ret;
}
