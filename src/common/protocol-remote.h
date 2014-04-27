#ifndef PROTOCOL_REMOTE_H
#define PROTOCOL_REMOTE_H

#include "dbaccess.h"

//client defines
#define CLIENT_IN_SV_PORT  43400
#define CLIENT_IN_CHT_PORT 43401
#define CLIENT_IP_ANY "0.0.0.0"

//chat defines
#define CHT_MAX_NAME_LEN 20

//message sizes
#define SV_MSG_SIZE 200

//server request codes
enum SV_REQ_CODES { SV_LOGIN_REQ = 0, SV_JOIN_REQ, SV_CREATE_REQ, SV_EXIT_REQ, SV_DESTROY_REQ };


#endif