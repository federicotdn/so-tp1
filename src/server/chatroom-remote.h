#ifndef CHATROOM_REMOTE_H
#define CHATROOM_REMOTE_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include "protocol-remote.h"

int init_chatroom_remote(char *name, char *ip, port_t port, struct in_addr creator);

#endif