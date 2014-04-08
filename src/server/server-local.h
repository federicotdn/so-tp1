#ifndef SERVER_LOCAL_H
#define SERVER_LOCAL_H

#include <unistd.h>
#include "protocol.h"
#include "dbaccess.h"

typedef struct client {
	pid_t pid;
	char *username;
	enum db_type_code type;
	int fifo;
	struct client *next;
} client_t;

typedef struct server_state {
	struct db_handle *db;
	client_t *list_head;
	int fifo_in;
} server_state_t;

int start_server_local();
int login_user(server_state_t *svstate);
int send_login_response(int fifo, int code, enum db_type_code type);
client_t *sv_add_user(server_state_t *svstate, char *username, pid_t pid, enum db_type_code type);
void free_sv_users(client_t *head);
int user_logged(server_state_t *svstate, char *username);
int setup_fifo();

#endif
/* SERVER_LOCAL_H */
