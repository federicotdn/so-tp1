#ifndef SERVER_LOCAL_H
#define SERVER_LOCAL_H

#include <unistd.h>
#include "protocol.h"
#include "dbaccess.h"

typedef struct client {
	pid_t pid;
	char *username;
	enum db_type_code type;
	char *fifo_name;
	struct client *next;
} client_t;

typedef struct server_state {
	client_t *list_head;
	int fifo_in;
} server_state_t ;

int start_server_local();
int login_user(server_state_t *sv_state);
int setup_fifo();

#endif
/* SERVER_LOCAL_H */
