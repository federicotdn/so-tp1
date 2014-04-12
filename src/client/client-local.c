#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "client-local.h"
#include "protocol.h"

#include <ncurses.h>
#include <pthread.h>

#define TRUE 1
#define FALSE 0

WINDOW *receive;
WINDOW *text;

void putch(WINDOW *win, char ch)
{
     // Add the character ch to the specified "window".  The
     // WINDOW is one of the two ncurses windows, me and them,
     // that are used in this program.
	if (ch == 4 || ch == 7) // Translate left-arrow, backspace to CTL-H
    {
    	ch = '\b';
    }

  	if (ch < ' ' && ch != '\t' && ch != '\n' && ch != '\b') 
  	{
        // \t, \n, and \b are the only control characters that
        // are interpreted by wechochar().  Others should be ignored.
    	return;
	}

	wechochar(win,ch);
	if (ch == '\b')
	{
      	// \b only moves the cursor -- I also want to erase the character.
    	wdelch(win);
    	refresh();
	}
}


void putchars(WINDOW *win, const char *str) 
{
     // Put all the chars in str into the specified WINDOW.
	while (*str) 
	{
    	putch(win, *str);
    	str++;
	}
}

void *receive_thread(void *arg)
{
    while (1) 
    {
    	sleep(1);
    	putchars(receive, "mensaje\n");
    }
}

void enter_chat_mode()
{
	/* ncurses tests */

	initscr();
	cbreak();
	noecho();
	intrflush(stdscr, FALSE);

	receive = newwin(10, COLS, 0, 0);
	text = newwin(10, COLS, 11, 0);

	idlok(text, TRUE);
  	scrollok(text, TRUE);
  	keypad(text, TRUE);
  	idlok(receive, TRUE);
  	scrollok(receive, TRUE);

  	refresh();

  	pthread_t rec_thread;
  	pthread_create(&rec_thread, NULL, receive_thread, NULL);

  	char ch;

  	while (1) 
  	{
         ch = wgetch(text);  // Get character typed by user.
         if (ch == '.')
            break;
         putch(text, ch);
    }

    endwin();
}


int init_client_local(char *username, char *password)
{
	client_state_t state;
	state.pid = getpid();
	state.username = username;
	int status;

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

	printf("Abriendo FIFO(out) servidor: %s\n", SERVER_FIFO_IN);

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

	printf("Abriendo FIFO(in) cliente: %s\n", fifo_str);

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

	printf("Login completado.  Tipo de usuario: %d\n", code);
	status = start_client(&state);

	close(state.sv_fifo);
	close(state.in_fifo);
	unlink(fifo_str);
	free(fifo_str);

	return status;

}

int start_client(client_state_t *st)
{
	char cht_name[CHT_MAX_NAME_LEN + 1];
	int cmd, status = 0, quit = FALSE;

	while (!quit)
	{
		cmd = get_usr_command(cht_name);
		switch (cmd)
		{
			case USR_EXIT:
				printf("Cerrando sesion en servidor.\n");
				status = send_server_exit(st);
				quit = TRUE;
			break;

			case USR_JOIN:
				enter_chat_mode();
				quit = TRUE;
			break;

			case USR_CREATE:

				if (st->type != DB_TEACHER)
				{
					printf("Solo los profesores pueden crear chatrooms.\n");
				}
				else
				{
					printf("Creando chatroom '%s'\n", cht_name);
					status = send_server_create(st, cht_name);
					if (status != 0)
					{
						printf("Client: error al enviar sv_create_req.\n");
						quit = TRUE;
					}

					status = read_create_join_res(st);
				}

			break;
		}
	}

	return status;
}

int read_create_join_res(client_state_t *st)
{
	struct sv_create_join_res res;
	int res_type = -1, status;

	status = read(st->in_fifo, &res_type, sizeof(int));
	if (status != sizeof(int) || res_type != SV_CREATE_JOIN_RES)
	{
		printf("Client: respuesta no es join/create.\n");
		return -1;
	}

	status = read(st->in_fifo, &res, sizeof(struct sv_create_join_res));
	if (status != sizeof(struct sv_create_join_res))
	{
		printf("Client: error al leer sv_create_join_res.\n");
		return -1;
	}

	//manejar errores

	if (res.status == SV_CREATE_SUCCESS || res.status == SV_JOIN_SUCCESS)
	{
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

int get_usr_command(char *cht_name)
{
	char buf[CHT_MAX_NAME_LEN + 9];

	printf("Comandos:\n   /exit\n   /join [nombre]\n   /create [nombre]\n");
	while (TRUE)
	{
		printf("Ingrese un comando:\n--> ");
		read_input(buf, 5, CHT_MAX_NAME_LEN + 8);

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

		printf("Comando invalido.\n");
	}
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




