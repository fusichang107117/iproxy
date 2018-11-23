#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include "iproxylib.h"

void usage(void)
{
	printf("use like this: \n");
	printf("\tfactory get key\n");
	printf("\tfacrory list\n");
}

int main(int argc, char const *argv[])
{
	char buf[1024];

	if (argc == 3 && strcmp(argv[1], "get") == 0) {
		iproxy_factory_get(argv[2], buf);
		printf("%s\n", buf);
	} else if (argc == 2 && strcmp(argv[1], "list") == 0) {
		iproxy_factory_list();
	} else {
		usage();
	}
	return 0;
}
