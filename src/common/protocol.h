#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <unistd.h>
#include "dbaccess.h"

#define DB_NAME "db.txt"
#define SERVER_FIFO_IN "sv_fifo_in"

enum SV_REQ_CODES { SV_LOGIN = 0, SV_JOIN, SV_CREATE };

struct sv_login_req {
	pid_t pid;
	char password[DB_MAX_PASSLEN + 1];
	char username[DB_MAX_USERLEN + 1];
};

struct sv_join_req {
	pid_t pid;
};

struct sv_create_rq {
	pid_t pid;
};

extern int sv_req_sizes[];

#endif
/* PROTOCOL_H */
