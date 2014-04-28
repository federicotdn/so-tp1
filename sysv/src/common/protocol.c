#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <semaphore.h>
#include <fcntl.h>

#include "protocol.h"
#include "file.h"

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
    
    int status;
    
    status = get_file_lock(fd, F_WRLCK);

    if (status == -1)
    {
        return -1;
    }
    
    
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

    status = unlock_file(fd);
    if (status == -1)
    {
        return -1;
    }
    
    return 0;
}

void pack_msg(msg_t *msg, pid_t pid, char code)
{
    msg->pid = pid;
    msg->code = code;
    msg->mtype = 1;
}

void pack_text_msg(msg_t *msg, pid_t pid, char code, char *content)
{
    pack_msg(msg, pid, code);
    strcpy(msg->mtext, content);
}


char *pack_content(char *content, int key)
{
    char *ptr = content;
    memcpy(ptr, &key, sizeof(int));
    ptr += sizeof(int);
    return ptr;
}


char *unpack_content(char *content, int *key)
{
    char *ptr = content;
    memcpy(key, ptr, sizeof(int));
    ptr += sizeof(int);
    return ptr;
}
    
    