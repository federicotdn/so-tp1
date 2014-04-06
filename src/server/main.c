#include <stdio.h>
#include <string.h>
#include "server-local.h"

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
		int status = start_server_local();
	}
	else
	{
		
	}

	return 0;
}
