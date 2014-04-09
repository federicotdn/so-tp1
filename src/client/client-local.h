#ifndef CLIENT_LOCAL_H
#define CLIENT_LOCAL_H

#include "dbaccess.h"
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

enum usr_commands { USR_EXIT, USR_JOIN, USR_CREATE };

typedef struct client_state {
	int sv_fifo;
	int in_fifo;
	char *username;
	enum db_type_code type;
	pid_t pid;
} client_state_t;

int init_client_local(char *username, char *password);
int start_client(client_state_t *st);
enum db_type_code read_server_login(int fifo, int *status);
int send_server_login(client_state_t *st, char *password);
int send_server_exit(client_state_t *st);
int get_usr_command(char *cht_name);
int read_input(char * buff, size_t min_length, size_t max_length);

#endif
/* CLIENT_LOCAL_H */
