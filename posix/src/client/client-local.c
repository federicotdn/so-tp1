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
	mqd_t mq_in;
	mqd_t mq_out;
	sem_t *sem;
	void *shm_addr;


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
int enter_chat_mode(client_state_t *st, char *mq_name);
enum db_type_code read_server_login(int fifo, int *status);
int send_server_login(client_state_t *st, char *password);
int send_server_exit(client_state_t *st);
int send_server_create(client_state_t *st, char *name);
int send_server_join(client_state_t *st, char *name);
int read_create_join_res(client_state_t *st, char *buf);
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

	char *mq_name = gen_mq_name_str(state.pid);

	if (mq_name == NULL)
	{
		endwin();
		return ERROR_OTHER;
	}

	struct mq_attr attr;
	attr.mq_maxmsg = CHT_MSG_Q_COUNT;
	attr.mq_msgsize = CHT_MSG_SIZE;

	state.mq_in = mq_open(mq_name, O_CREAT | O_RDONLY, S_IRUSR | S_IWUSR | S_IWGRP, &attr);


	if (state.mq_in == -1)
	{
		free(mq_name);
		endwin();
		return ERROR_OTHER;
	}

	char *fifo_str = gen_client_fifo_str(state.pid);
	if (fifo_str == NULL)
	{
		free(mq_name);
		endwin();
		return ERROR_OTHER;
	}
	
	unlink(fifo_str);
	if (mkfifo(fifo_str, S_IRUSR | S_IWUSR | S_IWGRP) == -1)
	{
		free(mq_name);
		free(fifo_str);
		endwin();
		return ERROR_FIFO_CREAT;
	}

	wprintw(state.display, "Abriendo FIFO(out) servidor: %s\n", SERVER_FIFO_IN);
	wrefresh(state.display);

	state.sv_fifo = open(SERVER_FIFO_IN, O_WRONLY);
	if (state.sv_fifo == -1)
	{
		free(mq_name);
		free(fifo_str);
		endwin();
		return ERROR_SERVER_CONNECTION;
	}

	status = send_server_login(&state, password);
	if (status == ERROR_SV_SEND)
	{
		free(mq_name);
		free(fifo_str);
		endwin();
		return ERROR_SV_SEND;
	}

	wprintw(state.display, "Abriendo FIFO(in) cliente: %s\n", fifo_str);
	wrefresh(state.display);

	state.in_fifo = open(fifo_str, O_RDONLY);
	if (state.in_fifo == -1)
	{
		free(mq_name);
		free(fifo_str);
		endwin();
		return ERROR_FIFO_OPEN;
	}

	enum db_type_code code = read_server_login(state.in_fifo, &status);
	if (status == -1)
	{
		free(mq_name);
		free(fifo_str);
		endwin();
		return ERROR_SV_READ;
	}

	if (status != SV_LOGIN_SUCCESS)
	{
		free(mq_name);
		free(fifo_str);
		if (status == SV_LOGIN_ERROR_CRD)
		{
			endwin();
			return ERROR_SV_CREDENTIALS;
		}
		if (status == SV_LOGIN_ERROR_ACTIVE)
		{
			endwin();
			return ERROR_SV_USER_ACTIVE;
		}
	}

	wprintw(state.display, "Login completado.  Tipo de usuario: %d\n", code);
	wrefresh(state.display);

	status = start_client(&state);

	close(state.sv_fifo);
	close(state.in_fifo);
	unlink(fifo_str);
	mq_close(state.mq_in);
	mq_close(state.mq_out);
	mq_unlink(mq_name);
	free(mq_name);
	free(fifo_str);

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
	char mq_name[CHT_MAX_MQ_NAME + 1];
	int cmd, status = 0, quit = FALSE;

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

				status = read_create_join_res(st, mq_name);
				if (status == 0)
				{
					wprintw(st->display, "--> mq_name: %s.\n", cht_name, mq_name);
					wrefresh(st->display);			
					st->chat_name = strdup(cht_name);
					status = enter_chat_mode(st, mq_name);
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

				status = read_create_join_res(st, mq_name);
				if (status == 0)
				{
					wprintw(st->display, "Chatroom creado.  Uniendo... (mq_name: %s)\n", mq_name);
					wrefresh(st->display);
					st->chat_name = strdup(cht_name);
					status = enter_chat_mode(st, mq_name);
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

int enter_chat_mode(client_state_t *st, char *mq_name)
{
	char msg_buf[CHT_MSG_SIZE];
	char *content, *sem_str, *shm_str;
	int status;
	pid_t pid;
	char code;

	struct mq_attr attr;
	attr.mq_maxmsg = CHT_MSG_Q_COUNT;
	attr.mq_msgsize = CHT_MSG_SIZE;
	st->mq_out = mq_open(mq_name, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IWGRP, &attr);

	if (st->mq_out == -1)
	{
		wprintw(st->display, "Error al abrir chat MQ.\n");
		wrefresh(st->display);
		return -1;
	}

	wprintw(st->display, "Chatroom MQ abierto.\n", mq_name);
	wrefresh(st->display);

	content = pack_msg(msg_buf, st->pid, CHT_MSG_JOIN);
	strcpy(content, st->username);

	status = mq_send(st->mq_out, msg_buf, CHT_MSG_SIZE, 0);

	if (status == -1)
	{
		return -1;
	}

	status = mq_receive(st->mq_in, msg_buf, CHT_MSG_SIZE, NULL);

	if (status == -1)
	{
		return -1;
	}

	content  = unpack_msg(msg_buf, &pid, &code);

	if (code != CHT_MSG_JOIN)
	{
		return -1;
	}

	shm_str = gen_shm_name_str(pid);
	sem_str = gen_sem_name_str(pid);

	if (shm_str == NULL || sem_str == NULL)
	{
		free(shm_str), free(sem_str);
		return -1;
	}



	status = shm_open(shm_str, O_CREAT | O_RDWR, 0);
	int shm_fd = status;

	if (status == -1)
	{
		free(shm_str), free(sem_str);
		return -1;
	}

	if (ftruncate(shm_fd, CHT_SHM_SIZE) == -1)
	{
		free(shm_str), free(sem_str);
		close(shm_fd);
       	return -1;
	}


    void *addr = mmap(NULL, CHT_SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    if (addr == MAP_FAILED)
    {
    	free(shm_str), free(sem_str);
    	close(shm_fd);
    	return -1;
    }

    st->shm_addr = addr;


	sem_t *sem = sem_open(sem_str, 0);

	st->sem = sem;


	if (sem == SEM_FAILED)
    {
        free(shm_str), free(sem_str);
        close(shm_fd);
	    munmap(addr, CHT_SHM_SIZE);
        return -1;
    }

	status = enter_chat_loop(st);

	mq_close(st->mq_out);

	sem_close(sem);

	close(shm_fd);
	munmap(addr, CHT_SHM_SIZE);

	free(shm_str), free(sem_str);

	wclear(st->display);
	wrefresh(st->display);	

	return status;
}


int enter_chat_loop(client_state_t * st)
{
	int quit = FALSE;
	char text[CHT_TEXT_SIZE + 1];
	char msg_buf[CHT_MSG_SIZE];
	char *content;
	int status, has_exited = FALSE;

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
				pack_msg(msg_buf, st->pid, CHT_MSG_EXIT);
				status = mq_send(st->mq_out, msg_buf, CHT_MSG_SIZE, 0);

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
				pack_msg(msg_buf, st->pid, CHT_MSG_HIST);
				status = mq_send(st->mq_out, msg_buf, CHT_MSG_SIZE, 0);

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
			content = pack_msg(msg_buf,st->pid, CHT_MSG_TEXT);
			strcpy(content, text);
			if (strlen(text) !=0)
			{
				status = mq_send(st->mq_out, msg_buf, CHT_MSG_SIZE, 0);
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
	char msg_buf[CHT_MSG_SIZE];
	char *content;
	int quit = FALSE;
	int status, i, hist_empty;
	pid_t pid;
	char code;
	client_state_t *st = (client_state_t*)arg;
	
	while (!quit)
	{
		status = mq_receive(st->mq_in, msg_buf, CHT_MSG_SIZE, NULL);

		if (status == -1)
		{
			pthread_mutex_lock(&st->thread_m);
			st->thread_ended = TRUE;
			pthread_mutex_unlock(&st->thread_m);
			return NULL;
		}
		content = unpack_msg(msg_buf, &pid, &code);

		switch (code)
		{
			case CHT_MSG_TEXT:
				pthread_mutex_lock(&st->screen_m);
				wprintw(st->display, "%s\n", content);
				wrefresh(st->display);
				pthread_mutex_unlock(&st->screen_m);
			break;

			case CHT_MSG_HIST:
				pthread_mutex_lock(&st->screen_m);
				sem_wait(st->sem);

				wprintw(st->display, "\n-- HISTORIAL --\n\n");

				hist_empty = TRUE;

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

				sem_post(st->sem);
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

int read_create_join_res(client_state_t *st, char *buf)
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
		strcpy(buf, res.mq_name);
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

		char msg_buf[CHT_MSG_SIZE];

		if (gbl_state->in_chatroom)
		{
			pack_msg(msg_buf, gbl_state->pid, CHT_MSG_EXIT);
			mq_send(gbl_state->mq_out, msg_buf, CHT_MSG_SIZE, 0);
		}

		char *mq_name = gen_mq_name_str(gbl_state->pid);
		char *fifo_str = gen_client_fifo_str(gbl_state->pid);

		send_server_exit(gbl_state);

		close(gbl_state->sv_fifo);
		close(gbl_state->in_fifo);
		unlink(fifo_str);
		mq_close(gbl_state->mq_in);
		mq_close(gbl_state->mq_out);;
		mq_unlink(mq_name);
		free(mq_name);
		free(fifo_str);


		


		endwin();

		exit(0);
	}
}