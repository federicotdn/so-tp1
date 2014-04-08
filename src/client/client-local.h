#ifndef CLIENT_LOCAL_H
#define CLIENT_LOCAL_H

#include "dbaccess.h"

#define ERROR_LENGTH 1
#define ERROR_SERVER_CONNECTION 2
#define ERROR_FIFO_CREAT 3
#define ERROR_SV_SEND 4
#define ERROR_OTHER 5
#define ERROR_FIFO_OPEN 6
#define ERROR_SV_READ 7
#define ERROR_SV_CREDENTIALS 8
#define ERROR_SV_USER_ACTIVE 9

int start_client_local(char *username, char *password);
enum db_type_code read_server_login(int fifo, int *status);
int send_server_login(int sv_fifo, char *username, char *password);

#endif
/* CLIENT_LOCAL_H */
