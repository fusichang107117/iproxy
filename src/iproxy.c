#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include "iproxy.h"
#include "iproxylib.h"

void usage(void)
{
	printf("use like this: \n");
	printf("\tiproxy set level key value\n");
	printf("\tiproxy get level key\n");
	printf("\tiproxy list level \n");
	printf("\tiproxy unset level key\n");
	printf("\tiproxy sync level\n");

	printf("\n\n\tLEVEL must be: ram, file, nvram, factory\n");
}

int main(int argc, char const *argv[])
{
	int ret = -1;

	if (argc < 2)
		goto OUT;

	iproxy_cmd_level_t level = IPROXY_TOTAL_LEVEL;
	if (strcmp((char *)argv[2], "ram") == 0)
		level = IPROXY_RAM_LEVEL;
	else if (strcmp((char *)argv[2], "file") == 0)
		level = IPROXY_FILE_LEVEL;
	else if (strcmp((char *)argv[2], "nvram") == 0)
		level = IPROXY_NVRAM_LEVEL;
	else if (strcmp((char *)argv[2], "factory") == 0)
		level = IPROXY_FACTORY_LEVEL;
	else
		goto OUT;

	ret = 0;
	if (argc == 5 && strcmp((char *)argv[1], "set") == 0 ) {
		iproxy_set(level, (char *)argv[3], (char *)argv[4]);
	} else if (argc == 4 && strcmp(argv[1], "get") == 0) {
		char buf[MAX_VALUE_LEN];
		iproxy_get(level, (char *)argv[3], buf);
		printf("%s\n", buf);
	} else if (argc == 4 && strcmp((char *)argv[1], "unset") == 0) {
		iproxy_unset(level, (char *)argv[3]);
	} else if (argc == 3 && strcmp((char *)argv[1], "list") == 0) {
		iproxy_list(level);
	} else if (argc == 3 && strcmp((char *)argv[1], "sync") == 0) {
		iproxy_sync(level);
	} else if (argc == 3 && strcmp((char *)argv[1], "clear") == 0) {
		iproxy_clear(level);
	} else {
		ret = -1;
	}
OUT:
	if (ret < 0)
		usage();
	return ret;
}
