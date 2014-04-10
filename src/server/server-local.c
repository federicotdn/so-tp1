#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include "server-local.h"
#include "chatroom-local.h"

#define TRUE 1
#define FALSE 0

client_t *get_client(client_t *head, pid_t pid);
int send_create_response(server_state_t *svstate, client_t *client, int code);	
int chatroom_exists(chatroom_t *head, char *name);
int fork_chat(server_state_t *svstate, char *name, pid_t creator);

int init_server_local()
{
	int status;
	server_state_t sv_state;
	
	sv_state.list_head = NULL;
	sv_state.chat_head = NULL;
	
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
	
	status = start_server(&sv_state);
	
	close_db(sv_state.db);
	free_sv_users(sv_state.list_head);
	unlink(SERVER_FIFO_IN);
	return status;
}

int start_server(server_state_t *svstate)
{
	int error = FALSE;

	printf("Server: loop principal.\n");
	while (!error)
	{
		int req_type;
		
		int status = read(svstate->fifo_in, &req_type, sizeof(int));
		if (status < sizeof(int))
		{
			error = TRUE;
			break;
		}

		printf("\nServer: codigo %d recibido.\n", req_type);

		switch (req_type)
		{
			case SV_LOGIN_REQ:
				
				status = login_user(svstate);
				if (status == -1)
				{
					error = TRUE;
				}
			
			break;
			
			case SV_JOIN_REQ:
			
			break;
			
			case SV_CREATE_REQ:

				status = create_chatroom(svstate);
				if (status == -1)
				{
					error = TRUE;
				}

			break;

			case SV_EXIT_REQ:

				status = exit_user(svstate);
				if (status == -1)
				{
					error = TRUE;
				}

			break;

			case SV_DESTROY_REQ:

			break;
			
			default:
				error = TRUE;
			break;
		}
	}

	return 0;
}


int create_chatroom(server_state_t *svstate)
{
	struct sv_create_req req;
	int status = read(svstate->fifo_in, &req, sizeof(struct sv_create_req));

	if (status != sizeof(struct sv_create_req))
	{
		return -1;
	}	

	client_t *client = get_client(svstate->list_head, req.pid);

	if (client == NULL)
	{
		return -1;
	}

	status = SV_CREATE_SUCCESS;

	if (client->type != DB_TEACHER) 
	{
		status = SV_CREATE_ERROR_PRIV;
	}

	if (chatroom_exists(svstate->chat_head, req.name))
	{
		status = SV_CREATE_ERROR_NAME;
	}

	if (status == SV_CREATE_SUCCESS)
	{
		fork_chat(svstate, req.name, client->pid);
	}
	

	send_create_response(svstate, client, status);

	return 0;

}

int fork_chat(server_state_t *svstate, char *name, pid_t creator)
{
	chatroom_t *chatroom = malloc(sizeof(chatroom_t));
	int file_des[2];
	int fork_pid, status;

	if (chatroom == NULL)
	{
		return -1;
	} 

	chatroom->name = strdup(name);
	if (chatroom->name == NULL)
	{
		free(chatroom);
		return -1;
	}

	chatroom->next = svstate->chat_head;
	svstate->chat_head = chatroom;

	if (pipe(file_des) == -1)
	{
		return -1;
	}

	chatroom->pipe_write = file_des[1];

	switch (fork_pid = fork())
	{
		case -1:
			return -1;

		case 0:

			if (close(file_des[1]) == -1)
				{
	        		exit(1);
				}
				status = init_chatroom(file_des[0], name, creator);

				exit(status);
		break;

		default:

			if (close(file_des[0]) == -1)
			{
        		return -1;
			}
			chatroom->pid = fork_pid;

		break;
	}

	return 0;
}

int send_create_response(server_state_t *svstate, client_t *client, int code)
{

}

int setup_fifo()
{	
	unlink(SERVER_FIFO_IN);
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

	int type = db_check_login(sv_state->db, req.username, req.password);

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
		aux  = aux->next;
	}

	return FALSE;
}

int exit_user(server_state_t *svstate)
{
	struct sv_exit_req req;
	int status = read(svstate->fifo_in, &req, sizeof(struct sv_exit_req));
	if (status != sizeof(struct sv_exit_req))
	{
		return -1;
	}

	printf("Server: usuario PID: %u termino la sesion.\n", req.pid);
	remove_user(svstate, req.pid);

	return 0;
}

void remove_user(server_state_t *svstate, pid_t pid)
{
	client_t *aux = svstate->list_head;

	if (aux == NULL)
	{
		return;
	}
	
	if (aux->pid == pid)
	{
		svstate->list_head = aux->next;
		free(aux->username);
		close(aux->fifo);
		free(aux);

		printf("--> PID: %u eliminado.\n", pid);

		return;
	}

	while (aux->next != NULL)
	{
		client_t *next = aux->next;
		if (next->pid == pid)
		{
			aux->next = next->next;
			free(next->username);
			close(next->fifo);
			free(next);

			printf("--> PID: %u eliminado.\n", pid);

			return;
		}

		aux = aux->next;
	}


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

client_t *get_client(client_t *head, pid_t pid)
{
	while (head != NULL)
	{
		if (head->pid == pid)
		{
			return head;
		}
		head = head->next;
	}

	return NULL;
}

int chatroom_exists(chatroom_t *head, char *name)
{

	while (head != NULL)
	{
		if (strcmp(head->name, name) == 0)
		{
			return TRUE;
		}
		head = head->next;
	}

	return FALSE;
}

