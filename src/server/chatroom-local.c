#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "chatroom-local.h"
#include "protocol.h"

#define TRUE 1
#define FALSE 0

typedef struct client {
	char *name;
	pid_t pid;
	struct client *next;
	mqd_t mq;
} client_t;

struct chatroom_state {
	int in_pipe;
	char *in_mq_str;
	mqd_t in_mq;
	char *name;
	pid_t creator;
	pid_t pid;
	int new;
	client_t *head;
};

int start_chatroom(chatroom_state_t *state);
int add_client(pid_t sender_pid, char *content, chatroom_state_t *st );
void remove_client(pid_t sender_pid, chatroom_state_t *st);
int send_text_to_all(chatroom_state_t *st, char *text);
static client_t *get_client(client_t *head, pid_t pid);



int init_chatroom_local(int in_pipe, char *name, pid_t creator)
{
	int status;
	chatroom_state_t state;
	state.new = TRUE;
	state.creator = creator;
	state.in_pipe = in_pipe;
	state.head = NULL;
	state.pid = getpid();

	state.name = strdup(name);
	if (state.name == NULL)
	{
		return 1;
	}

	state.in_mq_str = gen_mq_name_str(state.pid);
	if (state.in_mq_str == NULL)
	{
		free(state.name);
		return 1;
	}

	struct mq_attr attr;
	attr.mq_maxmsg = CHT_MSG_Q_COUNT;
	attr.mq_msgsize = CHT_MSG_SIZE;

	state.in_mq = mq_open(state.in_mq_str, O_CREAT | O_RDONLY, S_IRUSR | S_IWUSR | S_IWGRP, &attr);
	if (state.in_mq == -1)
	{
		printf("Chatroom %d: error al abrir MQ. ERRNO: %d\n", state.pid, errno);
		free(state.name);
		free(state.in_mq_str);
		return 1;
	}

	printf("Chatroom %d: listo.\n", state.pid);
	status = start_chatroom(&state);

	return 0;
}	

int start_chatroom(chatroom_state_t *st)
{
	char msg_buf[CHT_MSG_SIZE];
	char *content;
	int quit = FALSE;
	ssize_t status;
	char new_content[CHT_MSG_SIZE + 1];
	client_t *client;


	while (!quit)
	{
		pid_t sender_pid;
		char code;

		status = mq_receive(st->in_mq, msg_buf, CHT_MSG_SIZE, NULL);
		if (status == -1)
		{
			quit = TRUE;
			printf("Chatroom: (%d) error al leer del MQ.\n", st->pid);
			continue;
		}

		content = unpack_msg(msg_buf, &sender_pid, &code);

		if (code != CHT_MSG_TEXT)
		{
			printf("Chatroom %d: msg codigo %d recibido.\n", st->pid, (int)code);
		}
		
		switch (code)
		{
			case CHT_MSG_JOIN:
				printf("Chatroom %d: usuario %s se unio.\n", st->pid, content);
				status =  add_client(sender_pid, content, st);
				if (status == -1)
				{
					quit = TRUE;;
					break;
				}
				status = mq_send((st->head)->mq, msg_buf ,CHT_MSG_SIZE, 0);
				if (status == -1)
				{
					quit = TRUE;;
					break;
				}


			break;

			case CHT_MSG_TEXT:
				client = get_client(st->head, sender_pid);
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

			case CHT_MSG_HIST:

			break;

			case CHT_MSG_EXIT:
				client = get_client(st->head, sender_pid);
				if (client == NULL)
				{
					quit = TRUE;
					break;
				}

				strcpy(new_content, client->name);
				strcat(new_content, " salio del chatroom.");
				send_text_to_all(st, new_content);

				status = mq_send(client->mq, msg_buf, CHT_MSG_SIZE, 0);
				remove_client(sender_pid, st);
				if (status == -1)
				{
					quit = TRUE;
					break;
				}

			break;

			default:

			break;
		}

	}

	return 0;
}	

int send_text_to_all(chatroom_state_t *st, char *text)
{
	client_t *aux = st->head;
	char msg_buf[CHT_MSG_SIZE];
	char *content;
	int status;

	content = pack_msg(msg_buf, 0, CHT_MSG_TEXT);
	strcpy(content, text);

	while (aux != NULL)
	{
		status = mq_send(aux->mq, msg_buf, CHT_MSG_SIZE, 0);
		if (status == -1)
		{
			return -1;
		}
		aux = aux->next;
	}

	return 0;
}

static client_t *get_client(client_t *head, pid_t pid)
{
	while(head != NULL)
	{
		if (head->pid == pid)
		{
			return head;
		}
		head = head->next;
	}

	return NULL;
}

int add_client(pid_t sender_pid, char *content, chatroom_state_t *st )
{
	client_t *client = malloc(sizeof(client_t));
	char *mq_name;

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

	mq_name = gen_mq_name_str(sender_pid);

	if (mq_name == NULL)
	{
		free(client->name);
		free(client);
		return -1;
	}

	client->mq = mq_open(mq_name, O_WRONLY);
	free(mq_name);

	if (client->mq == -1)
	{
		free(client->name);
		free(client);
		return -1;
	}

	client->pid = sender_pid;

	client->next = st->head;
	st->head = client;

	return 0;
}

void remove_client(pid_t sender_pid, chatroom_state_t *st)
{
	client_t *aux = st->head;

	if (aux == NULL)
	{
		return;
	}
	
	if (aux->pid == sender_pid)
	{
		st->head = aux->next;
		free(aux->name);
		mq_close(aux->mq);
		free(aux);

		printf("--> PID: %u eliminado de chatroom.\n", sender_pid);

		return;
	}

	while (aux->next != NULL)
	{
		client_t *next = aux->next;
		if (next->pid == sender_pid)
		{
			aux->next = next->next;
			free(next->name);
			mq_close(next->mq);
			free(next);

			printf("--> PID: %u eliminado de chatroom.\n", sender_pid);

			return;
		}

		aux = aux->next;
	}
}