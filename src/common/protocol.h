#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <unistd.h>
#include "dbaccess.h"

#define DB_NAME "db.txt"
#define SERVER_FIFO_IN "sv_fifo_in"
#define CLIENT_FIFO_IN_PREFIX "cl_fifo"
#define CHT_MAX_NAME_LEN 20


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
	char name[CHT_MAX_NAME_LEN + 1];
};

struct sv_exit_req {
	pid_t pid;
};

struct sv_destroy_cht_req {

};

/* SERVER -> CLIENT responses */

enum SV_RES_CODES { SV_LOGIN_RES = 0, SV_CREATE_JOIN_RES };

enum SV_LOGIN_CODES { SV_LOGIN_SUCCESS, SV_LOGIN_ERROR_CRD, SV_LOGIN_ERROR_ACTIVE };
enum SV_CREATE_CODES { SV_CREATE_SUCCESS, SV_CREATE_ERROR_NAME, SV_CREATE_ERROR_PRIV };
struct sv_login_res {
	int status;
	enum db_type_code usr_type;
};

struct sv_create_join_res {

};

char *gen_client_fifo_str(pid_t pid);

#endif
/* PROTOCOL_H */
