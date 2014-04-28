#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <sys/shm.h>

#include "chatroom-local.h"
#include "protocol.h"
#include "msg_text_q.h"

#define TRUE 1
#define FALSE 0

typedef struct client {
	char *name;
	pid_t pid;
	struct client *next;
	int mq;
} client_t;

struct chatroom_state {
	char *shm_str;
	int mq_in;
	char *name;
	pid_t creator;
	pid_t pid;
	int new;
	client_t *head;
	int sv_fifo;
	int shm_key;
	void *shm_addr;
	int shm;
	int sem;
	int sem_key;
	msg_text_q *history;
};

int start_chatroom(chatroom_state_t *state);
int add_client(msg_t *msg, chatroom_state_t *st );
void remove_client(pid_t sender_pid, chatroom_state_t *st);
int send_message_to_all(chatroom_state_t *st, char *content, char code);
int exit_all_users(client_t *head, msg_t *msg);
static client_t *get_client(client_t *head, pid_t pid);

int init_chatroom_local(char *name, pid_t creator, int key)
{
	int status;
	chatroom_state_t state;
	state.new = TRUE;
	state.creator = creator;
	state.head = NULL;
	state.pid = getpid();
	state.sv_fifo = open(SERVER_FIFO_IN, O_WRONLY);

	if (signal(SIGINT, SIG_IGN) == SIG_ERR)
	{
		return -1;
  	}

	state.name = strdup(name);
	if (state.name == NULL)
	{
		struct sv_destroy_cht_req req;
		req.pid = state.pid;
		write_server(state.sv_fifo, &req, sizeof(struct sv_destroy_cht_req), SV_DESTROY_REQ);
		return 1;
	}


	state.mq_in =  msgget(key, IPC_CREAT | S_IRUSR | S_IWUSR);
	if (state.mq_in == -1)
	{
		printf("Chatroom %d: error al abrir MQ. ERRNO: %d\n", state.pid, errno);
		free(state.name);
		struct sv_destroy_cht_req req;
		req.pid = state.pid;
		write_server(state.sv_fifo, &req, sizeof(struct sv_destroy_cht_req), SV_DESTROY_REQ);
		return 1;
	}

	printf("Chatroom %d: listo.\n", state.pid);


	state.sem_key = msgget(IPC_PRIVATE, S_IRUSR | S_IWUSR);



	state.sem = semget(state.sem_key, 1, S_IRUSR | S_IWUSR | IPC_CREAT);
	

	if (state.sem == -1)
	{
		struct sv_destroy_cht_req req;
		req.pid = state.pid;
		write_server(state.sv_fifo, &req, sizeof(struct sv_destroy_cht_req), SV_DESTROY_REQ);
		msgctl(state.mq_in, IPC_RMID, NULL);
		free(state.name);
		return -1;
	}

	if ((semctl(state.sem,0 , SETVAL,1)) == -1)
	{
		struct sv_destroy_cht_req req;
		req.pid = state.pid;
		write_server(state.sv_fifo, &req, sizeof(struct sv_destroy_cht_req), SV_DESTROY_REQ);
		msgctl(state.mq_in, IPC_RMID, NULL);
		semctl(state.sem,0 ,IPC_RMID ,NULL);
		free(state.name);
	}

	state.shm_key = msgget(IPC_PRIVATE, S_IRUSR | S_IWUSR);
	state.shm =  shmget(state.shm_key, CHT_SHM_SIZE, S_IRUSR | S_IWUSR | IPC_CREAT);


	printf("sem key: %d    shm key: %d\n", state.sem_key, state.shm_key);

	if (state.shm == -1) 
	{
		struct sv_destroy_cht_req req;
		req.pid = state.pid;
		write_server(state.sv_fifo, &req, sizeof(struct sv_destroy_cht_req), SV_DESTROY_REQ);
		msgctl(state.mq_in, IPC_RMID, NULL);
		semctl(state.sem,0 ,IPC_RMID ,NULL);
		free(state.name);
	}

	state.shm_addr = shmat(state.shm, NULL, 0);

	if(state.shm_addr == (void *) -1)
	{
		struct sv_destroy_cht_req req;
		req.pid = state.pid;
		write_server(state.sv_fifo, &req, sizeof(struct sv_destroy_cht_req), SV_DESTROY_REQ);
		msgctl(state.mq_in, IPC_RMID, NULL);
		semctl(state.sem,0 ,IPC_RMID ,NULL);
		free(state.name);
		shmctl(state.shm, IPC_RMID, NULL);
	}

	state.history = create_queue();


	status = start_chatroom(&state);


	struct sv_destroy_cht_req req;
	req.pid = state.pid;
	write_server(state.sv_fifo, &req, sizeof(struct sv_destroy_cht_req), SV_DESTROY_REQ);
	msgctl(state.mq_in, IPC_RMID, NULL);
	semctl(state.sem,0 ,IPC_RMID ,NULL);
	shmctl(state.shm, IPC_RMID, NULL);
	shmdt(state.shm_addr);


	free(state.name);

	return 0;
}	

int start_chatroom(chatroom_state_t *st)
{
	int quit = FALSE;
	ssize_t status = 0 ;
	char new_content[CHT_TEXT_SIZE + 1];
	char *content;
	client_t *client;
	int i;
	msg_t msg;
	pid_t sender_pid;
	struct sembuf op;

	while (!quit)
	{
		char *hist_txt;

		status = msgrcv(st->mq_in, &msg, CHT_TEXT_SIZE + 1, 0, 0);
		if (status == -1)
		{
			quit = TRUE;
			printf("Chatroom: (%d) error al leer del MQ.\n", st->pid);
			continue;
		}

		if (msg.code != CHT_MSG_TEXT)
		{
			printf("Chatroom %d: msg codigo %d recibido.\n", st->pid, (int)msg.code);
		}

		sender_pid = msg.pid;
		
		switch (msg.code)
		{
			case CHT_MSG_JOIN:
				printf("Chatroom %d: usuario %s se unio.\n", st->pid, unpack_content(msg.mtext, &i));

				status =  add_client(&msg, st);
				
				if (status == -1)
				{
					quit = TRUE;;
					break;
				}

				bzero(msg.mtext, CHT_TEXT_SIZE + 1);
				content = pack_content(msg.mtext, st->sem_key);
				pack_content(content, st->shm_key);
				pack_msg(&msg, st->pid, CHT_MSG_JOIN);

				printf("CODE: %d\n", msg.code);
				status = msgsnd(st->head->mq, &msg, CHT_TEXT_SIZE + 1, 0);

				if (status == -1)
				{
					quit = TRUE;;
					break;
				}


				client = get_client(st->head, sender_pid);

				if (client == NULL)
				{				
					quit = TRUE;
					break;
				}

				
				strcpy(new_content, client->name);
				strcat(new_content, " se ha unido al chatroom.");
				status = send_message_to_all(st, new_content, CHT_MSG_TEXT);
			
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

				strcat(new_content, msg.mtext);
				push_message(st->history, new_content);
				send_message_to_all(st, new_content, CHT_MSG_TEXT);



			break;

			case CHT_MSG_HIST:
				
				op.sem_num = 0;
				op.sem_op = -1;
				op.sem_flg = SEM_UNDO;

				semop(st->sem,&op,1);

				iter_reset(st->history);

				i = 0;
				while ((hist_txt = iter_next(st->history)) != NULL) 
				{
					memcpy((st->shm_addr) + (CHT_MSG_SIZE * i++), hist_txt, CHT_MSG_SIZE);
				}

				if(i < CHT_HIST_SIZE)
				{
					memset((st->shm_addr) + (CHT_MSG_SIZE * i), 0, (CHT_HIST_SIZE - i) * CHT_MSG_SIZE );
				}

				op.sem_op = 1;
				semop(st->sem,&op,1);

				client = get_client(st->head, sender_pid);



				
				status = msgsnd(client->mq, &msg, CHT_TEXT_SIZE + 1, 0);
				if (status == -1)
				{
					quit = TRUE;;
					break;
				}



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
				send_message_to_all(st, new_content, CHT_MSG_TEXT);

				
				if (client->pid == st->creator)
				{
					i = 3;
					while (i > 0)
					{
						sprintf(new_content, "El chatroom se cerrara en %i segundos", i--);
						send_message_to_all(st, new_content, CHT_MSG_TEXT);
						sleep(1);
					}
					status = exit_all_users(st->head, &msg);
					msgctl(st->mq_in, IPC_RMID, NULL);

					struct sv_destroy_cht_req req;
					req.pid = st->pid;
					write_server(st->sv_fifo, &req, sizeof(struct sv_destroy_cht_req), SV_DESTROY_REQ);

					quit = TRUE;
					break;
				}

				status = msgsnd(client->mq, &msg, CHT_TEXT_SIZE + 1, 0);
				remove_client(msg.pid, st);
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

	return status;
}	


int exit_all_users(client_t *head, msg_t *msg)
{
	client_t *aux;
	int status;

	msg->code = CHT_MSG_EXIT;


	while (head != NULL)
	{
		status = msgsnd(head->mq, msg, CHT_TEXT_SIZE + 1, 0);


		if (status == -1)
		{
			return -1;
		}
		aux = head->next; 
		free(head->name);
		free(head);
		head = aux;
	}

	return 0;
}


int send_message_to_all(chatroom_state_t *st, char *content, char code)
{	
	msg_t msg;

	pack_text_msg(&msg, st->pid, code, content);

	client_t *aux = st->head;

	int status;
	while (aux != NULL)
	{

		status = msgsnd(aux->mq, &msg, CHT_TEXT_SIZE + 1, 0);
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

int add_client(msg_t *msg, chatroom_state_t *st)
{
	client_t *client = malloc(sizeof(client_t));

	char *name;
	int key;

	if (client == NULL)
	{
		return -1;
	}

	name = unpack_content(msg->mtext, &key);

	client->name = strdup(name);

	if (client->name == NULL)
	{
		free(client);
		return -1;
	}

	client->mq = msgget(key, S_IRUSR | S_IWUSR);

	if (client->mq == -1)
	{
		free(client);
		return -1;
	}

	client->pid = msg->pid;

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
			free(next);

			printf("--> PID: %u eliminado de chatroom.\n", sender_pid);

			return;
		}

		aux = aux->next;
	}
}

