#ifndef CLIENT_LOCAL_H
#define CLIENT_LOCAL_H

#define ERROR_LENGTH 1
#define ERROR_SERVER_CONNECTION 2
#define ERROR_FIFO_CREAT 3
#define ERROR_SV_SEND 4
#define ERROR_OTHER 5
#define ERROR_FIFO_OPEN 6

int start_client_local(char *username, char *password);
int send_server_login(int sv_fifo, char *username, char *password);

#endif
/* CLIENT_LOCAL_H */
