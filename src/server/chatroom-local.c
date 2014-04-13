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

		printf("Chatroom %d: msg codigo %d recibido.\n", st->pid, (int)code);

		switch (code)
		{
			case CHT_MSG_JOIN:
				printf("Chatroom %d: user %s joined.\n", st->pid, content);
			break;

			case CHT_MSG_TEXT:

			break;

			case CHT_MSG_HIST:

			break;

			case CHT_MSG_EXIT:

			break;

			default:

			break;
		}

	}

	return 0;
}	