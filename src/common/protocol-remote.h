#ifndef PROTOCOL_REMOTE_H
#define PROTOCOL_REMOTE_H

#include "dbaccess.h"

typedef unsigned short port_t;

//client defines
#define CLIENT_IN_PORT 43400
#define CLIENT_IP_ANY "0.0.0.0"

//chat defines
#define CHT_MAX_NAME_LEN 20

//server defines
#define DB_NAME "db.txt"

//message sizes
#define SV_MSG_SIZE 200

//server request codes
enum SV_REQ_CODES { SV_LOGIN_REQ = 0, SV_JOIN_REQ, SV_CREATE_REQ, SV_EXIT_REQ, SV_DESTROY_REQ };

//server reply codes
enum SV_RES_CODES { SV_LOGIN_RES = 0, SV_CREATE_JOIN_RES };
enum SV_LOGIN_CODES { SV_LOGIN_SUCCESS, SV_LOGIN_ERROR_CRD, SV_LOGIN_ERROR_ACTIVE };

#endif