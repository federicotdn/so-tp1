#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "client-local.h"
#include "protocol.h"

#define TRUE 1
#define FALSE 0

int start_client_local(char *username, char *password)
{
	pid_t pid = getpid();
	int status;
	int fifo_in;

	char *fifo_str = gen_client_fifo_str(pid);
	if (fifo_str == NULL)
	{
		return ERROR_OTHER;
	}
	
	if (mkfifo(fifo_str, S_IRUSR | S_IWUSR | S_IWGRP) == -1)
	{
		free(fifo_str);
		return ERROR_FIFO_CREAT;
	}

	printf("Client FIFO: %s\n", fifo_str);

	int svfifo = open(SERVER_FIFO_IN, O_WRONLY);
	if (svfifo == -1)
	{
		free(fifo_str);
		return ERROR_SERVER_CONNECTION;
	}

	status = send_server_login(svfifo, username, password);
	if (status == ERROR_SV_SEND)
	{
		free(fifo_str);
		return ERROR_SV_SEND;
	}

	fifo_in = open(fifo_str, O_RDONLY);
	free(fifo_str);
	if (fifo_in == -1)
	{
		return ERROR_FIFO_OPEN;
	}

	enum db_type_code code = read_server_login(fifo_in, &status);
	if (status == -1)
	{
		return ERROR_SV_READ;
	}

	if (status != SV_LOGIN_SUCCESS)
	{
		if (status == SV_LOGIN_ERROR_CRD)
		{
			return ERROR_SV_CREDENTIALS;
		}
		if (status == SV_LOGIN_ERROR_ACTIVE)
		{
			return ERROR_SV_USER_ACTIVE;
		}
	}

	printf("Login completado.  Tipo de usuario: %d\n", code);

	return 0;

}

enum db_type_code read_server_login(int fifo, int *status)
{
	struct sv_login_res res;
	int res_type = -1;
	int bytes = read(fifo, &res_type, sizeof(int));
	if (bytes != sizeof(int) || res_type != SV_LOGIN_RES)
	{
		printf("Client: error al leer tipo de mensaje, o tipo de mensaje no es SV_LOGIN_RES: %d.\n", res_type);
		*status = -1;
		return -1;
	}

	bytes = read(fifo, &res, sizeof(struct sv_login_res));
	if (bytes != sizeof(struct sv_login_res))
	{
		printf("Client: error al leer respuesta de servidor.\n");
		*status = -1;
		return -1;
	}

	*status = res.status;
	return res.usr_type;
}

int send_server_login(int sv_fifo, char *username, char *password)
{
	int req_type = SV_LOGIN_REQ;
	int status;
	struct sv_login_req req;
	req.pid = getpid();
	strcpy(req.username, username);
	strcpy(req.password, password);

	status = write(sv_fifo, &req_type, sizeof(int));
	if (status != sizeof(int))
	{
		return ERROR_SV_SEND;
	}

	status = write(sv_fifo, &req, sizeof(struct sv_login_req));
	if (status != sizeof(struct sv_login_req))
	{
		return ERROR_SV_SEND;
	}

	return 0;
}