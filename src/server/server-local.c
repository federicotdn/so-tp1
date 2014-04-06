#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include "server-local.h"

int sv_req_sizes[] = {
	sizeof(struct sv_login_req),
	sizeof(struct sv_join_req),
	sizeof(struct sv_create_rq)
};

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
		printf("Error al abrir la base de datos.\n");
		return -1;
	}

	int svfifo = setup_fifo();
	if (svfifo == -1)
	{
		close_db(db);
		return -1;
	}
	
	while (!error)
	{
		int req_type;
		int status = read(svfifo, &req_type, sizeof(int));
		if (status < sizeof(int))
		{
			error = TRUE;
			break;
		}
		
		struct sv_login_req login_req;
		
		switch (req_type)
		{
			case SV_LOGIN:
			
				status = read(svfifo, &login_req, sv_req_sizes[SV_LOGIN]);
				if (status < sv_req_sizes[SV_LOGIN])
				{
					error = TRUE;
					break;
				}
				
				status = login_user(&sv_state, &login_req);
			
			break;
			
			case SV_JOIN:
			
			break;
			
			case SV_CREATE:
			
			break;
			
			default:
				error = TRUE;
			break;
		}
	}
	
	close_db(db);
	return 0;
}

int setup_fifo()
{	
	if (mkfifo(SERVER_FIFO_IN, S_IRUSR | S_IWUSR | S_IWGRP) == -1)
	{
		return -1;
	}
	
	int svfifo = open(SERVER_FIFO_IN, O_RDONLY);
	if (svfifo == -1)
	{
		return -1;
	}
	
	if (open(SERVER_FIFO_IN, O_WRONLY) == -1)
	{
		return -1;
	}
	
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
	{
		return -1;
	}
	
	return svfifo;
}

int login_user(server_state_t *sv_state, struct sv_login_req *req)
{
	
}
