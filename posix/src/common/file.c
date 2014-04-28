#include <fcntl.h>
#include <unistd.h>

#include "file.h"

int get_file_lock(int fd, short int type)
{

    /* Exclusive lock on whole file */
    struct flock lck = create_lock(type);

    return fcntl(fd, F_SETLKW, &lck);
}

int unlock_file(int fd)
{
    /* Remove lock for file */
    struct flock lck = create_lock(F_UNLCK);

    return fcntl(fd, F_SETLKW, &lck);
}


struct flock create_lock(short int type)
{
    struct flock lck;
    lck.l_type = type;
    lck.l_whence = SEEK_SET;
    lck.l_start = 0;
    lck.l_len = 0;
    lck.l_pid = getpid();
    return lck;
}