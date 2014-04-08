#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "protocol.h"

char *gen_client_fifo_str(pid_t pid)
{
	char *client_fifo = malloc(sizeof(char)*30);
	
	if (client_fifo == NULL)
	{
		return NULL;
	}
	char pid_str[8];

	strcpy(client_fifo, CLIENT_FIFO_IN_PREFIX);
	sprintf(pid_str, "%u", pid);
	strcat(client_fifo, pid_str);

	return client_fifo;
}