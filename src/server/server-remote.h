#include <netinet/in.h>
#include <sys/socket.h>
#include <signal.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#define PORT_NUM "50000"
#define INT_LEN 30

int init_server_remote();