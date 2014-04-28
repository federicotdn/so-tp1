#ifndef CLIENT_LOCAL_H
#define CLIENT_LOCAL_H

#include <sys/types.h>

#define ERROR_LENGTH 1
#define ERROR_SERVER_CONNECTION 2
#define ERROR_FIFO_CREAT 3
#define ERROR_SV_SEND 4
#define ERROR_OTHER 5
#define ERROR_FIFO_OPEN 6
#define ERROR_SV_READ 7
#define ERROR_SV_CREDENTIALS 8
#define ERROR_SV_USER_ACTIVE 9

typedef struct client_state client_state_t;

int init_client_local(char *username, char *password);
int read_input(char * buff, size_t min_length, size_t max_length);

#endif
/* CLIENT_LOCAL_H */
