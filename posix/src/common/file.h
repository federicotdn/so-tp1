#ifndef FILE_H
#define FILE_H

int get_file_lock(int fd, short int type);
int unlock_file(int fd);
struct flock create_lock(short int type);

#endif