#include "server-remote.h"
#include "protocol-remote.h"
#include "dbaccess.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TRUE 1
#define FALSE 0

//#include <sys/stat.h>
//#include <fcntl.h>


typedef struct chatroom {
	pid_t pid;
	char *name;
	port_t port;
	struct chatroom *next;
} chatroom_t;

typedef struct client {
	pid_t pid;
	char *username;
	enum db_type_code type;
	struct client *next;
	struct sockaddr_in addr;
} client_t;

typedef struct server_state {
	struct db_handle *db;
	client_t *list_head;
	chatroom_t *chat_head;
	int chat_count;

	int socket_fd;

} server_state_t;

int init_server_remote(char *ip, unsigned short port);
int setup_sockets(server_state_t *svstate, char *ip, port_t port);
static client_t *get_client(client_t *head, pid_t pid);
static int send_create_join_response(server_state_t *svstate, client_t *client, int code, int cht_pid);
static chatroom_t *chatroom_exists(chatroom_t *head, char *name);
static int fork_chat(server_state_t *svstate, char *name, pid_t creator);
static void free_sv_users(client_t *head);
static void free_sv_chats(chatroom_t *head);
static void exit_cleanup(int sig);
static int start_server(server_state_t *svstate);
static int login_user(server_state_t *svstate, struct sockaddr_in *cl, char *msg);
static int join_user(server_state_t *svstate);
static int exit_user(server_state_t *svstate);
static int create_chatroom(server_state_t *svstate);
static void remove_user(server_state_t *svstate, pid_t pid);
static int send_login_response(server_state_t *sv_state, struct sockaddr_in *cl, int code, enum db_type_code type);
static client_t *sv_add_user(server_state_t *svstate, char *username, struct sockaddr_in *addr, enum db_type_code type);
static int user_logged(server_state_t *svstate, char *username);
static int remove_chatroom(server_state_t *svstate);

static server_state_t *gbl_state = NULL;

int init_server_remote(char *ip, unsigned short port)
{
	int status;
	server_state_t sv_state;
	gbl_state = &sv_state;
	
	sv_state.list_head = NULL;
	sv_state.chat_head = NULL;
	sv_state.chat_count = 0;

	if (setup_sockets(&sv_state, ip, port) == -1)
	{
		printf("Server: error al preparar socket.");
		return -1;
	}

	if (signal(SIGINT, exit_cleanup) == SIG_ERR)
	{
		return -1;
	}
	
	sv_state.db = open_db(DB_NAME);
	if (!sv_state.db)
	{
		return -1;
	}
	
	status = start_server(&sv_state);
	
	gbl_state = NULL;
	close_db(sv_state.db);
	close(sv_state.socket_fd);
	free_sv_users(sv_state.list_head);
	return status;
}

int setup_sockets(server_state_t *svstate, char *ip, port_t port)
{
	int status;

	printf("Server: iniciando con IP: %s, puerto %u.\n", ip, port);

	struct sockaddr_in sock_addr;
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_port = htons(port);
	inet_pton(AF_INET, ip, &sock_addr.sin_addr);

	svstate->socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (svstate->socket_fd == -1)
	{
		return -1;
	}

	status = bind(svstate->socket_fd, (struct sockaddr*) &sock_addr, sizeof(struct sockaddr_in));
	if (status == -1)
	{
		close(svstate->socket_fd);
		return -1;
	}

	return 0;
}

int start_server(server_state_t *svstate)
{
	int error = FALSE;
	char buf[SV_MSG_SIZE];

	/* sender info */
	struct sockaddr sender_addr_raw;
	struct sockaddr_in *sender_addr;

	printf("Server: loop principal.\n");
	while (!error)
	{
		int status;
		ssize_t bytes_read;
		char req_type, *content;
		socklen_t addr_len = sizeof(struct sockaddr);
		
		bytes_read = recvfrom(svstate->socket_fd, buf, SV_MSG_SIZE, 0, &sender_addr_raw, &addr_len);
		if (bytes_read != SV_MSG_SIZE || addr_len != sizeof(struct sockaddr))
		{
			error = TRUE;
			break;
		}

		sender_addr = (struct sockaddr_in*)&sender_addr_raw;
		req_type = buf[0];
		content = &buf[1];

		printf("\nServer: codigo %d recibido.\n", req_type);

		switch (req_type)
		{
			case SV_LOGIN_REQ:
				
				status = login_user(svstate, sender_addr, content);
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
	// struct sv_destroy_cht_req req;
	// int status = read(svstate->fifo_in, &req, sizeof(struct sv_destroy_cht_req));

	// if (status != sizeof(struct sv_destroy_cht_req))
	// {
	// 	return -1;
	// }

	// size_t cht_pid = req.pid;

	// chatroom_t *aux = svstate->chat_head;

	// if (aux == NULL)
	// {
	// 	return -1;
	// }

	// if (aux->pid == cht_pid)
	// {
	// 	svstate->chat_head = aux->next;
	// 	free(aux->name);
	// 	free(aux);

	// 	printf("--> PID: %u chatroom eliminado.\n", cht_pid);

	// 	return 0;
	// }

	// while (aux->next != NULL)
	// {
	// 	chatroom_t *next = aux->next;
	// 	if (next->pid == cht_pid)
	// 	{
	// 		aux->next = next->next;
	// 		free(next->name);
	// 		free(next);

	// 		printf("--> PID: %u chatroom eliminado.\n", cht_pid);

	// 		return 0;
	// 	}

	// 	aux = aux->next;
	// }

	// svstate->chat_count--;

	return 0;
}


int create_chatroom(server_state_t *svstate)
{
	// struct sv_create_req req;
	// int status = read(svstate->fifo_in, &req, sizeof(struct sv_create_req));
	// int cht_pid;

	// if (status != sizeof(struct sv_create_req))
	// {
	// 	return -1;
	// }	

	// client_t *client = get_client(svstate->list_head, req.pid);

	// if (client == NULL)
	// {
	// 	return -1;
	// }

	// status = SV_CREATE_SUCCESS;

	// if (client->type != DB_TEACHER) 
	// {
	// 	status = SV_CREATE_ERROR_PRIV;
	// }

	// if (chatroom_exists(svstate->chat_head, req.name) != NULL)
	// {
	// 	status = SV_CREATE_ERROR_NAME;
	// }

	// if (status == SV_CREATE_SUCCESS)
	// {
	// 	cht_pid = fork_chat(svstate, req.name, client->pid);
	// }

	// if (cht_pid == -1)
	// {
	// 	return -1;
	// }
	
	// send_create_join_response(svstate, client, status, cht_pid);

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

			//status = init_chatroom_local(name, creator);

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
    // struct sv_create_join_res res;
    // res.status = code;
    
    // if (code != SV_CREATE_SUCCESS && code != SV_JOIN_SUCCESS)
    // {
    //     res.mq_name[0] = 0;
    // }
    // else
    // {
    //    char *mq_name = gen_mq_name_str(cht_pid);
    //    if (mq_name == NULL)
    //    {
    //        return -1;
    //    }

    //    printf("--> mq_name: %s\n", mq_name);
       
    //    strcpy(res.mq_name, mq_name);
    //    free(mq_name);
    // }
    
    // int res_type = SV_CREATE_JOIN_RES;
    // int status = write(client->fifo, &res_type, sizeof(int));
    // if (status != sizeof(int))
    // {
    //     return -1;
    // }
    
    // status = write(client->fifo, &res, sizeof(struct sv_create_join_res));
    // if (status != sizeof(struct sv_create_join_res))
    // {
    //     return -1;
    // }
    
    return 0;
}

int join_user(server_state_t *svstate)
{
	// struct sv_join_req req;
	// int status;
	// int code = SV_JOIN_REQ;
	// chatroom_t *cht;

	// status = read(svstate->fifo_in, &req, sizeof(struct sv_join_req));
	// if (status != sizeof(struct sv_join_req))
	// {
	// 	return -1;
	// }

	// client_t *client = get_client(svstate->list_head, req.pid);

	// if (client == NULL)
	// {
	// 	return -1;
	// }

	// printf("Server: join usuario PID: %u\n", req.pid);

	// cht = chatroom_exists(svstate->chat_head, req.name);
	// code = SV_JOIN_SUCCESS;

	// if (cht == NULL)
	// {
	// 	code = SV_JOIN_ERROR_NAME;
	// }

	// send_create_join_response(svstate, client, code, cht->pid);

	return 0;
}

int login_user(server_state_t *sv_state, struct sockaddr_in *cl, char *msg)
{
	client_t *new_usr;
	char *password, *username;

	username = password = msg;
	while (*password++)
		;

	printf("Server: login usuario '%s'\n", username);
	printf("--> IP: %s\n", inet_ntoa(cl->sin_addr));

	int type = db_check_login(sv_state->db, username, password);

	if (type == -1)
	{
		printf("Server: usuario o clave invalida.\n");
		send_login_response(sv_state, cl, SV_LOGIN_ERROR_CRD, -1);
		return 0;
	}

	if (user_logged(sv_state, username))
	{
		printf("Server: el usuario ya esta logeado.\n");
		send_login_response(sv_state, cl, SV_LOGIN_ERROR_ACTIVE, -1);
		return 0;
	}

	new_usr = sv_add_user(sv_state, username, cl, type);
	if (new_usr == NULL)
	{
		return -1;
	}

	send_login_response(sv_state, cl, SV_LOGIN_SUCCESS, type);

	return 0;
}

int send_login_response(server_state_t *sv_state, struct sockaddr_in *cl, int code, enum db_type_code type)
{
	int res_type = SV_LOGIN_RES;
	char buf[SV_MSG_SIZE];
	buf[0] = SV_LOGIN_RES;
	buf[1] = (char)code;
	buf[2] = (char)type;

	int written = sendto(sv_state->socket_fd, buf, SV_MSG_SIZE, 0, (struct sockaddr*)cl, sizeof(struct sockaddr_in));
	if (written != SV_MSG_SIZE)
	{
		return -1;
	}

	return 0;
}

client_t *sv_add_user(server_state_t *svstate, char *username, struct sockaddr_in *addr, enum db_type_code type)
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
	
	memcpy(&usr->addr, addr, sizeof(struct sockaddr_in));
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
	// struct sv_exit_req req;
	// int status = read(svstate->fifo_in, &req, sizeof(struct sv_exit_req));
	// if (status != sizeof(struct sv_exit_req))
	// {
	// 	return -1;
	// }

	// printf("Server: usuario PID: %u termino la sesion.\n", req.pid);
	// remove_user(svstate, req.pid);

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
	   // free(aux->mq_name);
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

		printf("\nServer: SIGINT recibido. Exit.\n");
		close_db(gbl_state->db);
		free_sv_users(gbl_state->list_head);

		exit(0);
	}
}