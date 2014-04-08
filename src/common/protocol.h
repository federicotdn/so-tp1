#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <unistd.h>
#include "dbaccess.h"

#define DB_NAME "db.txt"
#define SERVER_FIFO_IN "sv_fifo_in"
#define CLIENT_FIFO_IN_PREFIX "cl_fifo"

/* CLIENT/CHAT -> SERVER requests */

enum SV_REQ_CODES { SV_LOGIN_REQ = 0, SV_JOIN_REQ, SV_CREATE_REQ, SV_EXIT_REQ, SV_DESTROY_REQ };

struct sv_login_req {
	pid_t pid;
	char password[DB_MAX_PASSLEN + 1];
	char username[DB_MAX_USERLEN + 1];
};

struct sv_join_req {
	pid_t pid;
};

struct sv_create_req {
	pid_t pid;
};

struct sv_exit_req {
	pid_t pid;
};

struct sv_destroy_cht_req {

};

/* SERVER -> CLIENT responses */

enum SV_RES_CODES { SV_LOGIN_RES = 0, SV_CREATE_JOIN_RES, SV_EXIT_RES };

struct sv_login_res {

};

struct sv_create_join_res {

};

struct sv_exit_res {

};

char *gen_client_fifo_str( pid_t pid);

#endif
/* PROTOCOL_H */
