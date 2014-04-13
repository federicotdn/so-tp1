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
	enum db_type_code type;
	pid_t pid;
	mqd_t mq_in;

	/* ncurses */
	WINDOW *display;
	WINDOW *input;

	/* receive window pthread */
	pthread_t rec_thread;
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
int read_input_ncurses(WINDOW *input, char *buf, size_t max_length);

int init_client_local(char *username, char *password)
{
	client_state_t state;
	state.pid = getpid();
	state.username = username;
	int status;

	if (init_ncurses(&state) != 0)
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

	close(state.sv_fifo);
	close(state.in_fifo);
	unlink(fifo_str);
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

					status = enter_chat_mode(st, mq_name);
					if (status == -1)
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
					quit = TRUE;
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
	char *content;

	struct mq_attr attr;
	attr.mq_maxmsg = CHT_MSG_Q_COUNT;
	attr.mq_msgsize = CHT_MSG_SIZE;
	mqd_t mq_out = mq_open(mq_name, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IWGRP, &attr);

	if (mq_out == -1)
	{
		wprintw(st->display, "Error al abrir chat MQ.\n");
		wrefresh(st->display);
		return -1;
	}

	wprintw(st->display, "Chatroom MQ abierto.\n", mq_name);
	wrefresh(st->display);

	content = pack_msg(msg_buf, st->pid, CHT_MSG_JOIN);
	strcpy(content, st->username);

	mq_send(mq_out, msg_buf, CHT_MSG_SIZE, 0);

	return 0;
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

		read_input_ncurses(st->input, buf, CHT_MAX_NAME_LEN + 8);

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

int read_input_ncurses(WINDOW *input, char *buf, size_t max_length)
{
	wclear(input);
	wprintw(input, "%s", CHT_PS1);
	wrefresh(input);
	return wgetnstr(input, buf, max_length);
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




