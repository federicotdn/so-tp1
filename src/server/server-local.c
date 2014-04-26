#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>

#include "server-local.h"
#include "chatroom-local.h"
#include "protocol.h"
#include "dbaccess.h"

#define TRUE 1
#define FALSE 0

typedef struct chatroom {
	pid_t pid;
	char *name;
	char *mq_name;
	struct chatroom *next;
} chatroom_t;

typedef struct client {
	pid_t pid;
	char *username;
	enum db_type_code type;
	int fifo;
	struct client *next;
} client_t;

typedef struct server_state {
	struct db_handle *db;
	client_t *list_head;
	int fifo_in;
	chatroom_t *chat_head;
	int chat_count;
} server_state_t;

client_t *get_client(client_t *head, pid_t pid);
int send_create_join_response(server_state_t *svstate, client_t *client, int code, int cht_pid);
chatroom_t *chatroom_exists(chatroom_t *head, char *name);
int fork_chat(server_state_t *svstate, char *name, pid_t creator);
void free_sv_users(client_t *head);
void free_sv_chats(chatroom_t *head);
void exit_cleanup(int sig);
int start_server(server_state_t *svstate);
int login_user(server_state_t *svstate);
int join_user(server_state_t *svstate);
int exit_user(server_state_t *svstate);
int create_chatroom(server_state_t *svstate);
void remove_user(server_state_t *svstate, pid_t pid);
int send_login_response(int fifo, int code, enum db_type_code type);
client_t *sv_add_user(server_state_t *svstate, char *username, pid_t pid, enum db_type_code type);
int user_logged(server_state_t *svstate, char *username);
int setup_fifo();
int remove_chatroom(server_state_t *svstate);

static server_state_t *gbl_state = NULL;

int init_server_local()
{
	int status;
	server_state_t sv_state;
	gbl_state = &sv_state;
	
	sv_state.list_head = NULL;
	sv_state.chat_head = NULL;
	sv_state.chat_count = 0;

	if (signal(SIGINT, exit_cleanup) == SIG_ERR)
	{
		return -1;
	}
	printf("Server: Creando semaforo\n");
	sem_t *sem = sem_open(SERVER_SEMAPHORE, O_CREAT, S_IRUSR | S_IWUSR , 1);
	if (sem == SEM_FAILED)
	{
	    return -1;
	}
	
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
	
	gbl_state = NULL;
	close_db(sv_state.db);
	free_sv_users(sv_state.list_head);
	close(sv_state.fifo_in);
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
			
				status = join_user(svstate);
				if (status == -1)
				{
					error = TRUE;
				}


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

				status = remove_chatroom(svstate);
				if (status == -1)
				{
					error = TRUE;
				}
			break;
			
			default:
				printf("Server: codigo invalido recibido.");
				error = TRUE;
			break;
		}
	}

	return 0;
}


int remove_chatroom(server_state_t *svstate)
{
	struct sv_destroy_cht_req req;
	int status = read(svstate->fifo_in, &req, sizeof(struct sv_destroy_cht_req));

	if (status != sizeof(struct sv_destroy_cht_req))
	{
		return -1;
	}

	size_t cht_pid = req.pid;

	chatroom_t *aux = svstate->chat_head;

	if (aux == NULL)
	{
		return;
	}

	if (aux->pid == cht_pid)
	{
		svstate->chat_head = aux->next;
		free(aux->name);
		mq_close(aux->mq_name);
		free(aux);

		printf("--> PID: %u chatroom eliminado.\n", cht_pid);

		return;
	}

	while (aux->next != NULL)
	{
		chatroom_t *next = aux->next;
		if (next->pid == cht_pid)
		{
			aux->next = next->next;
			free(next->name);
			mq_close(next->mq_name);
			free(next);

			printf("--> PID: %u chatroom eliminado.\n", cht_pid);

			return;
		}

		aux = aux->next;
	}

	svstate->chat_count--;

	return 0;
}


int create_chatroom(server_state_t *svstate)
{
	struct sv_create_req req;
	int status = read(svstate->fifo_in, &req, sizeof(struct sv_create_req));
	int cht_pid;

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

	if (chatroom_exists(svstate->chat_head, req.name) != NULL)
	{
		status = SV_CREATE_ERROR_NAME;
	}

	if (status == SV_CREATE_SUCCESS)
	{
		cht_pid = fork_chat(svstate, req.name, client->pid);
	}

	if (cht_pid == -1)
	{
		return -1;
	}
	
	send_create_join_response(svstate, client, status, cht_pid);

	return 0;

}

int fork_chat(server_state_t *svstate, char *name, pid_t creator)
{
	chatroom_t *chatroom = malloc(sizeof(chatroom_t));
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


	switch (fork_pid = fork())
	{
		case -1:

			free(chatroom);
			return -1;

		break;

		case 0:

			status = init_chatroom_local(name, creator);

			exit(status);

		break;

		default:
			printf("Server: Chatroom creado, pid: %d\n", fork_pid);
			chatroom->pid = fork_pid;
			svstate->chat_count++;

		break;
	}

	return fork_pid;
}

int send_create_join_response(server_state_t *svstate, client_t *client, int code, int cht_pid)
{
    struct sv_create_join_res res;
    res.status = code;
    
    if (code != SV_CREATE_SUCCESS && code != SV_JOIN_SUCCESS)
    {
        res.mq_name[0] = 0;
    }
    else
    {
       char *mq_name = gen_mq_name_str(cht_pid);
       if (mq_name == NULL)
       {
           return -1;
       }

       printf("--> mq_name: %s\n", mq_name);
       
       strcpy(res.mq_name, mq_name);
       free(mq_name);
    }
    
    int res_type = SV_CREATE_JOIN_RES;
    int status = write(client->fifo, &res_type, sizeof(int));
    if (status != sizeof(int))
    {
        return -1;
    }
    
    status = write(client->fifo, &res, sizeof(struct sv_create_join_res));
    if (status != sizeof(struct sv_create_join_res))
    {
        return -1;
    }
    
    return 0;
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

int join_user(server_state_t *svstate)
{
	struct sv_join_req req;
	int status;
	int code = SV_JOIN_REQ;
	chatroom_t *cht;

	status = read(svstate->fifo_in, &req, sizeof(struct sv_join_req));
	if (status != sizeof(struct sv_join_req))
	{
		return -1;
	}

	client_t *client = get_client(svstate->list_head, req.pid);

	if (client == NULL)
	{
		return -1;
	}

	printf("Server: join usuario PID: %u\n", req.pid);

	cht = chatroom_exists(svstate->chat_head, req.name);
	code = SV_JOIN_SUCCESS;

	if (cht == NULL)
	{
		code = SV_JOIN_ERROR_NAME;
	}

	send_create_join_response(svstate, client, code, cht->pid);

	return 0;
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

void free_sv_chats(chatroom_t *head)
{
	while (head != NULL)
	{
	    chatroom_t *aux = head;
	    head = head->next;
	    free(aux->name);
	    free(aux->mq_name);
	    free(aux);
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

chatroom_t *chatroom_exists(chatroom_t *head, char *name)
{

	while (head != NULL)
	{
		if (strcmp(head->name, name) == 0)
		{
			return head;
		}
		head = head->next;
	}

	return NULL;
}

void exit_cleanup(int sig)
{
	static int warned = FALSE;
	if (sig == SIGINT)
	{
		if (gbl_state == NULL)
		{
			return;
		}

		if (gbl_state->chat_count > 0 && !warned)
		{
			printf("\n-- Advertencia: Hay chatrooms abiertos.\n-- Presionar CTRL + C nuevamente para cerrar el servidor.\n");
			warned = TRUE;
			return;
		}

		if (gbl_state->chat_count == 0)
		{
			warned = FALSE;
			return;
		}

		printf("\nServer: SIGINT recibido. Exit.\n");
		close_db(gbl_state->db);
		free_sv_users(gbl_state->list_head);
		//free_sv_chats(gbl_state->chat_head);
		close(gbl_state->fifo_in);
		unlink(SERVER_FIFO_IN);
		sem_unlink(SERVER_SEMAPHORE);

		exit(0);
	}
}