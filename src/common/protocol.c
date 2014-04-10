#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <semaphore.h>
#include "protocol.h"

char *gen_client_fifo_str(pid_t pid)
{
	char *client_fifo = malloc(sizeof(char) * CLIENT_FIFO_MAX_NAME);
	
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

char *gen_mq_name_str(pid_t pid)
{
    char *mq_name = malloc(sizeof(char)* CHT_MAX_MQ_NAME);
    
    if (mq_name == NULL )
    {
        return NULL;
    }
    
    char pid_str[7];
    
    sprintf(pid_str, "%u", pid);
    strcpy(mq_name, CHT_MQ_PREFIX);
    strcat(mq_name, pid_str);
    
    return mq_name;
    
}


int write_server(int fd, void *req_struct, size_t req_size, int req_type)
{
    sem_t *sem = sem_open(SERVER_SEMAPHORE, 0);
    int status;
    
    if (sem == SEM_FAILED)
    {
        return -1;
    }
    sem_wait(sem);
    
    status = write(fd, &req_type, sizeof(int));
	if (status != sizeof(int))
	{
		return -1;
	}

	status = write(fd, req_struct, req_size);
	if (status != req_size)
	{
		return -1;
	}

    sem_post(sem);
    sem_close(sem);
    
    return 0;
}
    
    