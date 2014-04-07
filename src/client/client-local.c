#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include "client-local.h"
#include "protocol.h"



#define TRUE 1
#define FALSE 0

int start_client_local(char *username, char *password)
{
	pid_t pid = getpid();
	char *client_fifo = gen_client_fifo_str(pid);
	if (client_fifo == NULL)
	{
		return -1;
	}


	
	if (mkfifo(client_fifo, S_IRUSR | S_IWUSR | S_IWGRP) == -1)
	{
		return -1;
	}

	free(client_fifo);

	int svfifo = open(SERVER_FIFO_IN, O_WRONLY | O_NONBLOCK);
	if (svfifo == -1)
	{
		return ERROR_SERVER_CONNECTION;
	}


	return 0;

}


