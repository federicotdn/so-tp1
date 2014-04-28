#ifndef CHATROOM_LOCAL_H
#define CHATROOM_LOCAL_H

#include <sys/types.h>

typedef struct chatroom_state chatroom_state_t; 

int init_chatroom_local(char *name, pid_t creator, int key);

#endif
/* CHATROOM_LOCAL_H */