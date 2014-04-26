#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <semaphore.h>
#include "protocol.h"

char *gen_name(pid_t pid, char *prefix, int size);

char *gen_client_fifo_str(pid_t pid)
{
	return gen_name(pid, CLIENT_FIFO_IN_PREFIX, CLIENT_FIFO_MAX_NAME);
}

char *gen_mq_name_str(pid_t pid)
{
    return gen_name(pid, CHT_MQ_PREFIX, CHT_MAX_MQ_NAME);
}

char *gen_shm_name_str(pid_t pid)
{
    return gen_name(pid, CHT_SHM_PREFIX, CHT_MAX_SHM_NAME);
}

char *gen_sem_name_str(pid_t pid)
{
    return gen_name(pid, CHT_SEM_PREFIX, CHT_MAX_SEM_NAME);
}

char *gen_name(pid_t pid, char *prefix, int size)
{
    char *name = malloc(sizeof(char)* size);
    
    if (name == NULL )
    {
        return NULL;
    }
    
    char pid_str[7];
    
    sprintf(pid_str, "%u", pid);
    strcpy(name, prefix);
    strcat(name, pid_str);
    
    return name;
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

char *pack_msg(char *msg_buf, pid_t pid, char code)
{
    char *ptr = msg_buf;
    memcpy(ptr, &pid, sizeof(pid_t));
    ptr += sizeof(pid_t);
    *ptr++ = code;
    return ptr;
}

char *unpack_msg(char *msg_buf, pid_t *pid, char *code)
{
    char *ptr = msg_buf;
    memcpy(pid, ptr, sizeof(pid_t));
    ptr += sizeof(pid_t);
    *code = *ptr++;
    return ptr;
}
    
    