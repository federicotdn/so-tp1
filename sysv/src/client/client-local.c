#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <ncurses.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <sys/shm.h>



#include "client-local.h"
#include "protocol.h"
#include "dbaccess.h"

#define TRUE 1
#define FALSE 0
#define CHT_PS1 "-->: "

enum usr_commands { USR_EXIT, USR_JOIN, USR_CREATE, USR_CLEAR };

struct client_state {
	int sv_fifo;
	int in_fifo;
	char *username;
	char *chat_name;
	enum db_type_code type;
	pid_t pid;
	int mq_in;
	int mq_out;
	int sem;
	void *shm_addr;
	int mq_key;
	int shm;


	/* ncurses */
	WINDOW *display;
	WINDOW *input;
	pthread_mutex_t screen_m;

	/* receive window pthread */
	pthread_t rec_thread;
	pthread_mutex_t thread_m;
	int thread_ended;
	int in_chatroom;
};

int start_client(client_state_t *st);
int enter_chat_mode(client_state_t *st, int key);
enum db_type_code read_server_login(int fifo, int *status);
int send_server_login(client_state_t *st, char *password);
int send_server_exit(client_state_t *st);
int send_server_create(client_state_t *st, char *name);
int send_server_join(client_state_t *st, char *name);
int read_create_join_res(client_state_t *st, int *key);
int get_usr_command(client_state_t *st, char *cht_name);
int init_ncurses(client_state_t *st);
int read_input_ncurses(client_state_t *st, char *buf, size_t max_length);
int enter_chat_loop(client_state_t * st);
void *read_mq_loop(void *arg);
void fill_zeros(char *buf, int length);
void exit_cleanup(int sig);

static struct client_state *gbl_state = NULL;

int init_client_local(char *username, char *password)
{
	if (signal(SIGINT, exit_cleanup) == SIG_ERR)
	{
		return -1;
	}

	client_state_t state;
	gbl_state = &state;
	state.pid = getpid();
	state.username = username;
	int status;

	state.in_chatroom = FALSE;

	pthread_mutex_init(&state.thread_m, NULL);
	pthread_mutex_init(&state.screen_m, NULL);
	
	if (init_ncurses(&state) != 0)
	{
		return ERROR_OTHER;
	}

	int key = msgget(IPC_PRIVATE, S_IRUSR | S_IWUSR);

	state.mq_key = key;
	state.mq_in = msgget(key, IPC_CREAT | S_IRUSR | S_IWUSR);

	if (state.mq_in == -1)
	{
		return ERROR_OTHER;
	}

	char *fifo_str = gen_client_fifo_str(state.pid);
	if (fifo_str == NULL)
	{
		return ERROR_OTHER;
	}
	
	unlink(fifo_str);
	if (mkfifo(fifo_str, S_IRUSR | S_IWUSR | S_IWGRP) == -1)
	{
		free(fifo_str);
		return ERROR_FIFO_CREAT;
	}

	wprintw(state.display, "Abriendo FIFO(out) servidor: %s\n", SERVER_FIFO_IN);
	wrefresh(state.display);

	state.sv_fifo = open(SERVER_FIFO_IN, O_WRONLY);
	if (state.sv_fifo == -1)
	{
		free(fifo_str);
		return ERROR_SERVER_CONNECTION;
	}

	status = send_server_login(&state, password);
	if (status == ERROR_SV_SEND)
	{
		free(fifo_str);
		return ERROR_SV_SEND;
	}

	wprintw(state.display, "Abriendo FIFO(in) cliente: %s\n", fifo_str);
	wrefresh(state.display);

	state.in_fifo = open(fifo_str, O_RDONLY);
	if (state.in_fifo == -1)
	{
		free(fifo_str);
		return ERROR_FIFO_OPEN;
	}

	enum db_type_code code = read_server_login(state.in_fifo, &status);
	if (status == -1)
	{
		free(fifo_str);
		return ERROR_SV_READ;
	}

	if (status != SV_LOGIN_SUCCESS)
	{
		free(fifo_str);
		if (status == SV_LOGIN_ERROR_CRD)
		{
			return ERROR_SV_CREDENTIALS;
		}
		if (status == SV_LOGIN_ERROR_ACTIVE)
		{
			return ERROR_SV_USER_ACTIVE;
		}
	}

	wprintw(state.display, "Login completado.  Tipo de usuario: %d\n", code);
	wrefresh(state.display);

	status = start_client(&state);

	send_server_exit(&state);

	close(state.sv_fifo);
	close(state.in_fifo);
	unlink(fifo_str);

	free(fifo_str);
	msgctl(state.mq_in, IPC_RMID, NULL);
	semctl(state.sem,0 ,IPC_RMID ,NULL);
	shmctl(state.shm, IPC_RMID, NULL);
	shmdt(state.shm_addr);
	wprintw(state.display, "Presionar ENTER para salir.");
	wrefresh(state.display);
	wgetch(state.input);

	endwin();
	return status;

}

int init_ncurses(client_state_t *st)
{
	initscr();
	cbreak();
	intrflush(stdscr, FALSE);

	st->display = newwin(LINES - 1, COLS, 0, 0);
	st->input = newwin(1, COLS, LINES - 1, 0);

	if (st->display == NULL || st->input == NULL)
	{
		return -1;
	}

	idlok(st->input, TRUE);
  	scrollok(st->input, TRUE);
  	keypad(st->input, TRUE);
  	idlok(st->display, TRUE);
  	scrollok(st->display, TRUE);

  	refresh();
  	return 0;
}

int start_client(client_state_t *st)
{
	char cht_name[CHT_MAX_NAME_LEN + 1];
	int cmd, key, status = 0, quit = FALSE;

	while (!quit)
	{
		cmd = get_usr_command(st, cht_name);
		switch (cmd)
		{
			case USR_EXIT:
				wprintw(st->display, "Cerrando sesion en servidor.\n");
				wrefresh(st->display);
				status = send_server_exit(st);
				quit = TRUE;
			break;

			case USR_JOIN:
				wprintw(st->display, "Uniendose a chatroom: %s\n", cht_name);
				wrefresh(st->display);

				status = send_server_join(st, cht_name);
				if (status != 0)
				{
					wprintw(st->display, "Error al unirse a chatroom.\n");
					wrefresh(st->display);
				}

				status = read_create_join_res(st, &key);
				if (status == 0)
				{
					wprintw(st->display, "--> mq_key: %d.\n", cht_name, key);
					wrefresh(st->display);			
					st->chat_name = strdup(cht_name);
					status = enter_chat_mode(st, key);
					if (status != 0)
					{
						wprintw(st->display, "Error al entrar modo chat.\n");
						wrefresh(st->display);
						quit = TRUE;
					}
				}
				else
				{
					wprintw(st->display, "Error al unirse al chatroom.\n");
					wrefresh(st->display);				
				}

			break;

			case USR_CREATE:

				wprintw(st->display, "Creando chatroom '%s'\n", cht_name);
				wrefresh(st->display);
				status = send_server_create(st, cht_name);
				if (status != 0)
				{
					wprintw(st->display, "Client: error al enviar sv_create_req.\n");
					wrefresh(st->display);
					quit = TRUE;
					break;
				}

				status = read_create_join_res(st, &key);
				if (status == 0)
				{
					wprintw(st->display, "Chatroom creado.  Uniendo... (mq_key: %d)\n", key);
					wrefresh(st->display);
					st->chat_name = strdup(cht_name);
					status = enter_chat_mode(st, key);
					if (status != 0)
					{
						wprintw(st->display, "Error al entrar modo chat.\n");
						wrefresh(st->display);
						quit = TRUE;
					}

				}
				else
				{
					wprintw(st->display, "Error al intentar crear chatroom.\n");
					wrefresh(st->display);
				}


			break;

			case USR_CLEAR:
				wclear(st->display);
				wrefresh(st->display);
			break;
		}
	}

	return status;
}

int enter_chat_mode(client_state_t *st, int key)
{

	char *sem_str, *shm_str;
	int status,sem_key, shm_key;
	pid_t pid;
	msg_t msg;
	char *content;

	st->mq_out = msgget(key, IPC_CREAT | S_IRUSR | S_IWUSR);

	if (st->mq_out == -1)
	{
		wprintw(st->display, "Error al abrir chat MQ.\n");
		wrefresh(st->display);
		return -1;
	}

	wprintw(st->display, "Chatroom MQ abierto.\n");
	wrefresh(st->display);

	content = pack_content(msg.mtext, st->mq_key);
	strcpy(content, st->username);

	pack_msg(&msg, st->pid, CHT_MSG_JOIN);

	status = msgsnd(st->mq_out, &msg, CHT_TEXT_SIZE + 1, 0);


	if (status == -1)
	{
		return -1;
	}
	status = msgrcv(st->mq_in, &msg, CHT_TEXT_SIZE + 1, 0, 0);

	if (status == -1)
	{
		return -1;
	}



	if (msg.code != CHT_MSG_JOIN)
	{
		return -1;
	}

	content = unpack_content(msg.mtext, &sem_key);
	unpack_content(content, &shm_key);

	st->sem = semget(sem_key, 0, S_IRUSR | S_IWUSR);


	if (st->sem == -1)
	{
		return -1;
	}

	st->shm = shmget(shm_key, 0, 0);

	if (st->shm == -1)
	{
		return -1;
	}


	st->shm_addr = shmat(st->shm, NULL, SHM_RDONLY); 

	if (st->shm_addr == (void *) -1){
		return -1;
	}


	status = enter_chat_loop(st);



	wclear(st->display);
	wrefresh(st->display);	

	return status;
}


int enter_chat_loop(client_state_t * st)
{
	int quit = FALSE;
	char text[CHT_TEXT_SIZE + 1];
	char content[CHT_TEXT_SIZE + 1];
	int status, has_exited = FALSE;
	msg_t msg;

	st->thread_ended = FALSE;

	st->in_chatroom = TRUE;

	wclear(st->display);
	wprintw(st->display, "Conectado al chatroom %s.\n", st->chat_name);
	wprintw(st->display, "Comandos validos:.\n");
	wprintw(st->display, "   /history   /exit\n");
	wrefresh(st->display);

	/* Thread para recibir mensajes de MQ */
	pthread_create(&(st->rec_thread), NULL, read_mq_loop, st);

	while (!quit)
	{
		pthread_mutex_lock(&st->thread_m);
		if (st->thread_ended)
		{
			pthread_mutex_unlock(&st->thread_m);
			break;
		}
		pthread_mutex_unlock(&st->thread_m);

		if (has_exited)
		{
			continue;
		}

		fill_zeros(text, CHT_TEXT_SIZE + 1);
		read_input_ncurses(st, text, CHT_TEXT_SIZE);

		pthread_mutex_lock(&st->thread_m);
		if (st->thread_ended)
		{
			pthread_mutex_unlock(&st->thread_m);
			break;
		}
		pthread_mutex_unlock(&st->thread_m);

		if (text[0] == '/')
		{
			if (strcmp(text, "/exit") == 0)
			{
				st->in_chatroom = FALSE;
				pack_msg(&msg, st->pid, CHT_MSG_EXIT);
				status = msgsnd(st->mq_out, &msg, CHT_TEXT_SIZE + 1, 0);

				if (status == -1)
				{
					pthread_mutex_lock(&st->thread_m);
					pthread_cancel(st->rec_thread);
					pthread_mutex_unlock(&st->thread_m);
					return -1;
				}

				has_exited = TRUE;
			}
			else if (strcmp(text, "/history") == 0)
			{
				pack_msg(&msg, st->pid, CHT_MSG_HIST);
				status = msgsnd(st->mq_out, &msg, CHT_TEXT_SIZE + 1, 0);

				if (status == -1)
				{
					pthread_mutex_lock(&st->thread_m);
					pthread_cancel(st->rec_thread);
					pthread_mutex_unlock(&st->thread_m);
					return -1;
				}
			}
			else
			{
				pthread_mutex_lock(&st->screen_m);
				wprintw(st->display, "Comando invalido.\n");
				wrefresh(st->display);
				pthread_mutex_unlock(&st->screen_m);
			}
		}
		else
		{
			strcpy(msg.mtext, text);

			pack_msg(&msg, st->pid, CHT_MSG_TEXT);
			if (strlen(text) !=0)
			{
				status = msgsnd(st->mq_out, &msg, CHT_TEXT_SIZE + 1, 0);
			}
			
			if (status == -1)
			{
				pthread_mutex_lock(&st->thread_m);
				pthread_cancel(st->rec_thread);
				pthread_mutex_unlock(&st->thread_m);
				return -1;
			}

		}
	}

	return 0;
}

void *read_mq_loop(void *arg)
{
	char content[CHT_TEXT_SIZE + 1];
	int quit = FALSE;
	int status, i, hist_empty;
	client_state_t *st = (client_state_t*)arg;
	msg_t msg;
	struct sembuf op;
	
	while (!quit)
	{
		status =  msgrcv(st->mq_in, &msg, CHT_TEXT_SIZE + 1, 0, 0);

		if (status == -1)
		{
			pthread_mutex_lock(&st->thread_m);
			st->thread_ended = TRUE;
			
			pthread_mutex_unlock(&st->thread_m);
			return NULL;
		}

		switch (msg.code)
		{
			case CHT_MSG_TEXT:
				pthread_mutex_lock(&st->screen_m);
				wprintw(st->display, "%s\n", msg.mtext);
				wrefresh(st->display);
				pthread_mutex_unlock(&st->screen_m);
			break;

			case CHT_MSG_HIST:
				pthread_mutex_lock(&st->screen_m);
				
				op.sem_num = 0;
				op.sem_op = -1;
				op.sem_flg = SEM_UNDO;

				semop(st->sem,&op,1);

				wprintw(st->display, "\n-- HISTORIAL --\n\n");


				hist_empty = TRUE;

				wrefresh(st->display);

				for(i =0; i < CHT_HIST_SIZE; i++){
					if(strlen(st->shm_addr + (i * CHT_MSG_SIZE)) != 0 )
					{
						hist_empty = FALSE; 
						wprintw(st->display, "%d: %s\n", i + 1 ,st->shm_addr + (i * CHT_MSG_SIZE));
					}
				}

				if (hist_empty)
				{
					wprintw(st->display, "El historial se encuentra vacio \n");
				}


				wprintw(st->display, "\n---------------\n\n");

				wrefresh(st->display);
				op.sem_op = 1;
				semop(st->sem,&op,1);
				pthread_mutex_unlock(&st->screen_m);
			break;

			case CHT_MSG_EXIT:
				pthread_mutex_lock(&st->screen_m);
				wprintw(st->display, "Saliendo de chatroom.\n");
				wprintw(st->display, "Presione enter para cerrar la ventana de chat.\n");
				wrefresh(st->display);
				pthread_mutex_unlock(&st->screen_m);

				pthread_mutex_lock(&st->thread_m);
				st->thread_ended = TRUE;
				st->in_chatroom = FALSE;
				pthread_mutex_unlock(&st->thread_m);
				return NULL;
			break;
			default:

			break;
		}
	}	

	return NULL;
}

int read_create_join_res(client_state_t *st, int *key)
{
	struct sv_create_join_res res;
	int res_type = -1, status;

	status = read(st->in_fifo, &res_type, sizeof(int));
	if (status != sizeof(int) || res_type != SV_CREATE_JOIN_RES)
	{
		wprintw(st->display, "Client: respuesta no es join/create.\n");
		wrefresh(st->display);
		return -1;
	}

	status = read(st->in_fifo, &res, sizeof(struct sv_create_join_res));
	if (status != sizeof(struct sv_create_join_res))
	{
		wprintw(st->display, "Client: error al leer sv_create_join_res.\n");
		wrefresh(st->display);
		return -1;
	}

	//manejar errores

	if (res.status == SV_CREATE_SUCCESS || res.status == SV_JOIN_SUCCESS)
	{
		*key = res.mq_key;
		return 0;
	}
	
	return -1;
}

int send_server_exit(client_state_t *st)
{
	int status, req_type = SV_EXIT_REQ;
	struct sv_exit_req req;
	req.pid = st->pid;
	
	status = write_server(st->sv_fifo, &req, sizeof(struct sv_exit_req), req_type);
	
	if (status != 0)
	{
		return ERROR_SV_SEND;
	}



	return 0;
}

int send_server_create(client_state_t *st, char *name)
{
	int status, req_type = SV_CREATE_REQ;
	struct sv_create_req req;
	req.pid = st->pid;
	strcpy(req.name, name);

	status = write_server(st->sv_fifo, &req, sizeof(struct sv_create_req), req_type);

	if (status != 0)
	{
		return ERROR_SV_SEND;
	}

	return 0;
}

int send_server_join(client_state_t *st, char *name)
{
	int status, req_type = SV_JOIN_REQ;
	struct sv_join_req req;
	req.pid = st->pid;
	strcpy(req.name, name);

	status = write_server(st->sv_fifo, &req, sizeof(struct sv_join_req), req_type);

	if (status != 0)
	{
		return ERROR_SV_SEND;
	}

	return 0;
}

int get_usr_command(client_state_t *st, char *cht_name)
{
	char buf[CHT_MAX_NAME_LEN + 9];
	int len = sizeof(buf) / sizeof(char);

	wprintw(st->display, "Comandos:\n   /exit\n   /join [nombre]\n   /create [nombre]\n");
	wrefresh(st->display);
	while (TRUE)
	{
		int i;
		for (i = 0; i < len; i++)
		{
			buf[i] = 0;
		}

		read_input_ncurses(st, buf, CHT_MAX_NAME_LEN + 8);

		if (strcmp(buf, "/exit") == 0)
		{
			return USR_EXIT;
		}

		if (strncmp(buf, "/join ", 6) == 0)
		{
			strcpy(cht_name, buf + 6);
			return USR_JOIN;
		}

		if (strncmp(buf, "/create ", 8) == 0)
		{
			strcpy(cht_name, buf + 8);
			return USR_CREATE;
		}

		if (strcmp(buf, "/clear") == 0)
		{
			return USR_CLEAR;
		}

		wprintw(st->display, "Comando invalido.\n");
		wrefresh(st->display);
	}
}

int read_input_ncurses(client_state_t *st, char *buf, size_t max_length)
{
	int err;
	pthread_mutex_lock(&st->screen_m);
	wclear(st->input);
	wprintw(st->input, "%s", CHT_PS1);
	wrefresh(st->input);
	pthread_mutex_unlock(&st->screen_m);
	err =  wgetnstr(st->input, buf, max_length);
	return err;
}

int read_input(char * buff, size_t min_length, size_t max_length)
{
	int i = 0;
	char c;

	while (i < max_length && (c = getchar()) != EOF && c != '\n') 
	{
		buff[i++] = c;
	}

	buff[i] = 0;

	if (i == max_length && (c = getchar()) != EOF && c != '\n')
	{
		while ((c = getchar()) != EOF && c != '\n')
			;
		return ERROR_LENGTH;
	}

	if (i < min_length)
	{
		return ERROR_LENGTH;
	}

	return 0;	
}

enum db_type_code read_server_login(int fifo, int *status)
{
	struct sv_login_res res;
	int res_type = -1;
	printf("Client: leyendo respuesta...\n");
	int bytes = read(fifo, &res_type, sizeof(int));
	if (bytes != sizeof(int) || res_type != SV_LOGIN_RES)
	{
		printf("Client: error al leer tipo de mensaje, o tipo de mensaje no es SV_LOGIN_RES: %d.\n", res_type);
		*status = -1;
		return -1;
	}

	bytes = read(fifo, &res, sizeof(struct sv_login_res));
	if (bytes != sizeof(struct sv_login_res))
	{
		printf("Client: error al leer respuesta de servidor.\n");
		*status = -1;
		return -1;
	}

	printf("Client: listo.\n");

	*status = res.status;
	return res.usr_type;
}

int send_server_login(client_state_t *st, char *password)
{
	int req_type = SV_LOGIN_REQ;
	int status;
	struct sv_login_req req;
	req.pid = st->pid;
	strcpy(req.username, st->username);
	strcpy(req.password, password);
	
	status = write_server(st->sv_fifo, &req, sizeof(struct sv_login_req), req_type);
	
	if (status != 0)
	{
		return ERROR_SV_SEND;
	}

	return 0;
}

void fill_zeros(char *buf, int length)
{
	int i;
	for (i =0; i< length; i++)
	{
		buf[i] = 0;
	}
}

void exit_cleanup(int sig)
{
	if (sig == SIGINT)
	{
		if (gbl_state == NULL)
		{
			return;
		}

		wprintw(gbl_state->display,"\nCerrando sesion...\n");
		wrefresh(gbl_state->display);

		msg_t msg;
		char *content;

		if (gbl_state->in_chatroom)
		{
			pack_msg(&msg, gbl_state->pid, CHT_MSG_EXIT);
			msgsnd(gbl_state->mq_out, &msg, CHT_TEXT_SIZE + 1, 0);
		}

		char *fifo_str = gen_client_fifo_str(gbl_state->pid);

		send_server_exit(gbl_state);

		close(gbl_state->sv_fifo);
		close(gbl_state->in_fifo);
		unlink(fifo_str);
		msgctl(gbl_state->mq_in, IPC_RMID, NULL);
		free(fifo_str);

		endwin();

		exit(0);
	}
}