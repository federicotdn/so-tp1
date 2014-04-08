#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include "server-local.h"

#define TRUE 1
#define FALSE 0

int start_server_local()
{
	int error = FALSE;
	server_state_t sv_state;
	
	sv_state.list_head = NULL;
	
	struct db_handle *db = open_db(DB_NAME);
	if (!db)
	{
		return -1;
	}

	sv_state.fifo_in = setup_fifo();
	if (sv_state.fifo_in == -1)
	{
		close_db(db);
		return -1;
	}
	
	printf("Server: loop principal.\n");
	while (!error)
	{
		int req_type;
		int status = read(sv_state.fifo_in, &req_type, sizeof(int));
		if (status < sizeof(int))
		{
			error = TRUE;
			break;
		}
		
		switch (req_type)
		{
			case SV_LOGIN_REQ:
				
				status = login_user(&sv_state);
			
			break;
			
			case SV_JOIN_REQ:
			
			break;
			
			case SV_CREATE_REQ:
			
			break;

			case SV_EXIT_REQ:

			break;

			case SV_DESTROY_REQ:

			break;
			
			default:
				error = TRUE;
			break;
		}
	}
	
	close_db(db);
	unlink(SERVER_FIFO_IN);
	return 0;
}

int setup_fifo()
{	
	if (mkfifo(SERVER_FIFO_IN, S_IRUSR | S_IWUSR | S_IWGRP) == -1)
	{
		printf("Server: Error al crear FIFO.\n");
		return -1;
	}
	
	printf("Server: creando FIFO de lectura...\n");

	int svfifo = open(SERVER_FIFO_IN, O_RDONLY);
	if (svfifo == -1)
	{
		printf("Server: Error al abrir FIFO.\n");
		return -1;
	}

	printf("--> Creado.\n");
	
	if (open(SERVER_FIFO_IN, O_WRONLY) == -1)
	{
		printf("Server: Error al abrir FIFO (escritura).\n");
		return -1;
	}
	
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
	{
		printf("Server: Error SIGPIPE.\n");
		return -1;
	}
	
	return svfifo;
}

int login_user(server_state_t *sv_state)
{
	struct sv_login_req req;
	int status = read(sv_state->fifo_in, &req, sizeof(struct sv_login_req));

	if (status != sizeof(struct sv_login_req))
	{
		return -1;
	}	

	printf("Server: login usuario '%s'\n", req.username);
	printf("--> PID: %u\n", req.pid);

	return 0;
}
