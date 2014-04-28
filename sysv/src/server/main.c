#include <stdio.h>
#include <string.h>
#include "server-local.h"
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <sys/stat.h>

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		printf("Uso: %s [ local | remote ]\n", argv[0]);
		return 1;
	}
	
	char *mode = argv[1];
	if (strcmp("local", mode) != 0 && strcmp("remote", mode) != 0)
	{
		printf("Uso: %s [ local | remote ]\n", argv[0]);
		return 1;		
	}
	
	if (strcmp("local", mode) == 0)
	{
		int status = init_server_local();
		if (status == -1)
		{
			printf("Server error.\n");
			return 1;
		}
	}
	else
	{
		
	}

	return 0;
}