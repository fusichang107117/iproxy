#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include "iproxy.h"

void usage(void)
{
	printf("use like this: \n");
	printf("\tiproxy set key value\n");
	printf("\tiproxy get key\n");
	printf("\tiproxy list\n");
	printf("\tiproxy unset key\n");
	printf("\tiproxy sync\n");
}

int main(int argc, char const *argv[])
{
	char buf[1024];

	if (argc == 4 && strcmp(argv[1], "set") == 0) {
		iproxy_set(argv[2], argv[3]);
	} else if (argc == 3 && strcmp(argv[1], "get") == 0) {
		iproxy_get(argv[2], buf);
		printf("%s\n", buf);
	} else if (argc == 3 && strcmp(argv[1], "unset") == 0) {
		iproxy_unset(argv[2]);
	} else if (argc == 2 && strcmp(argv[1], "list") == 0) {
		iproxy_list();
	} else {
		usage();
	}
	return 0;
}
