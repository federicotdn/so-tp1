#include <stdio.h>
#include <string.h>
#include "server-local.h"
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <sys/stat.h>

int main(int argc, char *argv[])
{
	int status = init_server_local();
	if (status == -1)
	{
		printf("Server error.\n");
		return 1;
	}


	return 0;
}
