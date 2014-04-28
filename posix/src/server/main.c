#include <stdio.h>
#include <string.h>
#include "server-local.h"
#include "server-remote.h"

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
		if (argc != 4)
		{
			printf("Uso: %s remote [ IP ] [ puerto ]\n", argv[0]);
			return 1;
		}

		unsigned short port;

		if (!sscanf(argv[3], "%hu", &port))
		{
			return 1;
		}

		printf("Server: iniciando servidor remoto.\n");
		int status = init_server_remote(argv[2], port);
		if (status == -1)
		{
			printf("Server error.\n");
			return 1;
		}
	}

	return 0;
}
