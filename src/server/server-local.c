#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include "server-local.h"

#define TRUE 1
#define FALSE 0

int start_server_local()
{
	int error = FALSE;
	server_state_t sv_state;
	
	sv_state.list_head = NULL;
	
	sv_state.db = open_db(DB_NAME);
	if (!sv_state.db)
	{
		return -1;
	}

	sv_state.fifo_in = setup_fifo();
	if (sv_state.fifo_in == -1)
	{
		close_db(sv_state.db);
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
				if (status == -1)
				{
					error = TRUE;
				}
			
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
	
	close_db(sv_state.db);
	free_sv_users(sv_state.list_head);
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
	client_t *new_usr;
	int client_fifo;
	char *client_fifo_str;
	int status = read(sv_state->fifo_in, &req, sizeof(struct sv_login_req));

	if (status != sizeof(struct sv_login_req))
	{
		return -1;
	}	

	printf("Server: login usuario '%s'\n", req.username);
	printf("--> PID: %u\n", req.pid);

	client_fifo_str = gen_client_fifo_str(req.pid);
	if (client_fifo_str == NULL)
	{
		printf("Server: Error al generar nombre de FIFO.\n");
		return 0;
	}

	printf("--> Client FIFO: %s\n", client_fifo_str);
	client_fifo = open(client_fifo_str, O_WRONLY);
	if (client_fifo == -1)
	{
		printf("Server: Error al abrir FIFO de cliente.\n");
		return 0;
	}

	enum db_type_code type = db_check_login(sv_state->db, req.username, req.password);
	if (type == -1)
	{
		printf("Server: usuario o clave invalida.\n");
		send_login_response(client_fifo, SV_LOGIN_ERROR_CRD, -1);
		close(client_fifo);
		return 0;
	}

	if (user_logged(sv_state, req.username))
	{
		printf("Server: el usuario ya esta logeado.\n");
		send_login_response(client_fifo, SV_LOGIN_ERROR_ACTIVE, -1);
		close(client_fifo);
		return 0;
	}

	new_usr = sv_add_user(sv_state, req.username, req.pid, type);
	if (new_usr == NULL)
	{
		return -1;
	}

	new_usr->fifo = client_fifo;
	send_login_response(client_fifo, SV_LOGIN_SUCCESS, type);

	return 0;
}

int send_login_response(int fifo, int code, enum db_type_code type)
{
	int res_type = SV_LOGIN_RES;
	struct sv_login_res res;
	res.status = code;
	res.usr_type = type;

	int status = write(fifo, &res_type, sizeof(int));
	if (status != sizeof(int))
	{
		return -1;
	}

	status = write(fifo, &res, sizeof(struct sv_login_res));
	if (status != sizeof(struct sv_login_res))
	{
		return -1;
	}

	return 0;
}

client_t *sv_add_user(server_state_t *svstate, char *username, pid_t pid, enum db_type_code type)
{
	client_t *usr = malloc(sizeof(client_t));
	if (usr == NULL)
	{
		return NULL;
	}
	
	usr->username = strdup(username);
	if (usr->username == NULL)
	{
		free(usr);
		return NULL;
	}
	
	usr->pid = pid;
	usr->type = type;
	usr->next = svstate->list_head;
	svstate->list_head = usr;
	
	return usr;
}

int user_logged(server_state_t *svstate, char *username)
{
	client_t *aux = svstate->list_head;
	while (aux != NULL)
	{
		if (strcmp(username, aux->username) == 0)
		{
			return TRUE;
		}
	}

	return FALSE;
}

void free_sv_users(client_t *head)
{
	client_t *aux = head;
	while (aux != NULL)
	{
		free(aux->username);
		client_t *next = aux->next;
		free(aux);
		aux = next;
	}
}
