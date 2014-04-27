#include "chatroom-remote.h"
#include "protocol-remote.h"
#include "msg_text_q.h"

#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

#define TRUE 1
#define FALSE 0

typedef struct client {
	char *name;
	struct client *next;
	struct sockaddr_in addr;
} client_t;

typedef struct chatroom_state {
	char *name;
	struct in_addr creator;
	int new;
	client_t *head;

	int socket_fd;
} chatroom_state_t;

static int start_chatroom(chatroom_state_t *state);
static int setup_sockets(chatroom_state_t *st, char *ip, port_t port);
static int add_client(struct sockaddr_in *cl, char *content, chatroom_state_t *st);
static void remove_client(pid_t sender_pid, chatroom_state_t *st);
static int send_text_to_all(chatroom_state_t *st, char *text);
static int send_message_to_all(int sfd, client_t *head, char *msg_buf);
static int exit_all_users(client_t *head, char *msg_buf);
static client_t *get_client(client_t *head, struct sockaddr_in *cl);

int init_chatroom_remote(char *name, char *ip, port_t port, struct in_addr creator)
{
	int status;
	chatroom_state_t state;
	state.new = TRUE;
	state.creator = creator;
	state.head = NULL;

	if (signal(SIGINT, SIG_IGN) == SIG_ERR)
	{
		return -1;
  	}

	state.name = strdup(name);
	if (state.name == NULL)
	{
		return 1;
	}

	if (setup_sockets(&state, ip, port) == -1)
	{
		return -1;
	}

	printf("Chatroom puerto %u: listo.\n", port);

	status = start_chatroom(&state);

	close(state.socket_fd);

	return 0;
}

int setup_sockets(chatroom_state_t *st, char *ip, port_t port)
{
	int status;

	printf("Chatroom: iniciando con IP: %s, puerto %u.\n", ip, port);

	struct sockaddr_in sock_addr;
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_port = htons(port);
	inet_pton(AF_INET, ip, &sock_addr.sin_addr);

	st->socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (st->socket_fd == -1)
	{
		return -1;
	}

	status = bind(st->socket_fd, (struct sockaddr*) &sock_addr, sizeof(struct sockaddr_in));
	if (status == -1)
	{
		close(st->socket_fd);
		return -1;
	}

	return 0;
}

int start_chatroom(chatroom_state_t *st)
{
	char buf[SV_MSG_SIZE];
	char *content;
	int quit = FALSE;
	ssize_t status;
	char new_content[CHT_MSG_SIZE];
	client_t *client;
	int i;

	/* sender info */
	struct sockaddr sender_addr_raw;
	struct sockaddr_in *sender_addr;
	socklen_t addr_len = sizeof(struct sockaddr);

	printf("Chatroom: loop principal.\n");

	while (!quit)
	{
		char code;
		char *content;

		status = recvfrom(st->socket_fd, buf, SV_MSG_SIZE, 0, &sender_addr_raw, &addr_len);
		if (status != SV_MSG_SIZE || addr_len != sizeof(struct sockaddr))
		{
			quit = TRUE;
			printf("Chatroom %s: error al leer del socket.\n", st->name);
			continue;
		}

		sender_addr = (struct sockaddr_in*)&sender_addr_raw;
		code = buf[0];
		content = &buf[1];

		if (code != CHT_MSG_TEXT)
		{
			printf("Chatroom %s: msg codigo %d recibido.\n", st->name, (int)code);
		}
		
		switch (code)
		{
			case CHT_MSG_JOIN:

				printf("Chatroom %s: usuario %s se unio.\n", st->name, content);

				status = add_client(sender_addr, content, st);
				if (status == -1)
				{
					quit = TRUE;;
					break;
				}

				status = sendto(st->socket_fd, buf, SV_MSG_SIZE, 0, sender_addr, sizeof(struct sockaddr_in));
				if (status != SV_MSG_SIZE)
				{
					quit = TRUE;
					break;
				}

				client = get_client(st->head, sender_addr);
				strcpy(new_content, client->name);
				strcat(new_content, " se ha unido al chatroom.");
				send_text_to_all(st, new_content);

			break;

			case CHT_MSG_TEXT:

				client = get_client(st->head, sender_addr);
				if (client == NULL)
				{
					quit = TRUE;
					break;
				}

				strcpy(new_content, client->name);
				strcat(new_content, ": ");
				strcat(new_content, content);

				send_text_to_all(st, new_content);

			break;

			case CHT_MSG_EXIT:
				// client = get_client(st->head, sender_pid);
				// if (client == NULL)
				// {
				// 	quit = TRUE;
				// 	break;
				// }

				// strcpy(new_content, client->name);
				// strcat(new_content, " salio del chatroom.");
				// send_text_to_all(st, new_content);

				
				// if (client->pid == st->creator)
				// {
				// 	i = 3;
				// 	while (i >= 0)
				// 	{
				// 		sprintf(new_content, "El chatroom se cerrara en %i segundos", i--);
				// 		send_text_to_all(st, new_content);
				// 		sleep(1);
				// 	}
				// 	status = exit_all_users(st->head, msg_buf);
				// 	mq_close(st->in_mq);
				// 	mq_unlink(st->in_mq_str);

				// 	struct sv_destroy_cht_req req;
				// 	req.pid = st->pid;
				// 	write_server(st->sv_fifo, &req, sizeof(struct sv_destroy_cht_req), SV_DESTROY_REQ);

				// 	quit = TRUE;
				// 	break;
				// }

				// status = mq_send(client->mq, msg_buf, CHT_MSG_SIZE, 0);
				// remove_client(sender_pid, st);
				// if (status == -1)
				// {
				// 	quit = TRUE;
				// 	break;
				// }

			break;

			default:

			break;
		}

	}

	return 0;
}	


int exit_all_users(client_t *head, char *msg_buf)
{
	// client_t *aux;
	// int status;

	// while (head != NULL)
	// {
	// 	status = mq_send(head->mq, msg_buf, CHT_MSG_SIZE, 0);
	// 	mq_close(head->mq);

	// 	if (status == -1)
	// 	{
	// 		return -1;
	// 	}
	// 	aux = head->next; 
	// 	free(head->name);
	// 	free(head);
	// 	head = aux;
	// }

	return 0;
}

int send_text_to_all(chatroom_state_t *st, char *text)
{
	char msg_buf[SV_MSG_SIZE];
	char *content = msg_buf;
	int status;

	*content++ = CHT_MSG_TEXT;
	strcpy(content, text);

	return send_message_to_all(st->socket_fd, st->head, msg_buf);
}

int send_message_to_all(int sfd, client_t *head, char *msg_buf)
{	
	ssize_t status;
	while (head != NULL)
	{
		status = sendto(sfd, msg_buf, SV_MSG_SIZE, 0, (struct sockaddr*)&head->addr, sizeof(struct sockaddr_in));
		if (status != SV_MSG_SIZE)
		{
			return -1;
		}

		head = head->next;
	}

	return 0;
}

client_t *get_client(client_t *head, struct sockaddr_in *cl)
{
	while(head != NULL)
	{
		if (head->addr.sin_addr.s_addr == cl->sin_addr.s_addr)
		{
			return head;
		}
		head = head->next;
	}

	return NULL;
}

int add_client(struct sockaddr_in *cl, char *content, chatroom_state_t *st)
{
	client_t *client = malloc(sizeof(client_t));
	if (client == NULL)
	{
		return -1;
	}

	client->name = strdup(content);

	if (client->name == NULL)
	{
		free(client);
		return -1;
	}

	memcpy(&client->addr, cl, sizeof(struct sockaddr_in));

	client->next = st->head;
	st->head = client;

	return 0;
}

void remove_client(pid_t sender_pid, chatroom_state_t *st)
{
	// client_t *aux = st->head;

	// if (aux == NULL)
	// {
	// 	return;
	// }
	
	// if (aux->pid == sender_pid)
	// {
	// 	st->head = aux->next;
	// 	free(aux->name);
	// 	mq_close(aux->mq);
	// 	free(aux);

	// 	printf("--> PID: %u eliminado de chatroom.\n", sender_pid);

	// 	return;
	// }

	// while (aux->next != NULL)
	// {
	// 	client_t *next = aux->next;
	// 	if (next->pid == sender_pid)
	// 	{
	// 		aux->next = next->next;
	// 		free(next->name);
	// 		mq_close(next->mq);
	// 		free(next);

	// 		printf("--> PID: %u eliminado de chatroom.\n", sender_pid);

	// 		return;
	// 	}

	// 	aux = aux->next;
	// }
}