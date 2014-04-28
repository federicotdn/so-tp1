#include "client-remote.h"
#include "protocol-remote.h"
#include "dbaccess.h"

#include <sys/stat.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define ERROR_LENGTH 1
#define ERROR_SERVER_CONNECTION 2
#define ERROR_FIFO_CREAT 3
#define ERROR_SV_SEND 4
#define ERROR_OTHER 5
#define ERROR_FIFO_OPEN 6
#define ERROR_SV_READ 7
#define ERROR_SV_CREDENTIALS 8
#define ERROR_SV_USER_ACTIVE 9

#define TRUE 1
#define FALSE 0
#define CHT_PS1 "-->: "

enum usr_commands { USR_EXIT, USR_JOIN, USR_CREATE, USR_CLEAR };

struct client_state {
	char *username;
	char *chat_name;
	enum db_type_code type;
	int socket_fd;
	struct sockaddr_in ssocket;
	pthread_mutex_t socket_m;

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

typedef struct client_state client_state_t;

int setup_sockets(client_state_t *st);
static int start_client(client_state_t *st);
static int enter_chat_mode(client_state_t *st, port_t port);
static enum db_type_code read_server_login(client_state_t *st, int *status);
static int send_server_login(client_state_t *st, char *password);
static int send_server_exit(client_state_t *st);
static int send_server_create(client_state_t *st, char *name);
static int send_server_join(client_state_t *st, char *name);
static int read_create_join_res(client_state_t *st, port_t *cht_port);
static int get_usr_command(client_state_t *st, char *cht_name);
static int init_ncurses(client_state_t *st);
static int read_input_ncurses(client_state_t *st, char *buf, size_t max_length);
static int enter_chat_loop(client_state_t * st, struct sockaddr_in *cht_addr);
static void *read_socket_loop(void *arg);
static void fill_zeros(char *buf, int length);
static void exit_cleanup(int sig);

static struct client_state *gbl_state = NULL;

int init_client_remote(char *username, char *password, char *ip, unsigned short port)
{
	if (signal(SIGINT, exit_cleanup) == SIG_ERR)
	{
		return -1;
	}

	client_state_t state;
	gbl_state = &state;
	state.username = username;
	int status;

	state.in_chatroom = FALSE;

	pthread_mutex_init(&state.thread_m, NULL);
	pthread_mutex_init(&state.screen_m, NULL);
	pthread_mutex_init(&state.socket_m, NULL);
	
	if (init_ncurses(&state) != 0)
	{
		return ERROR_OTHER;
	}

	wprintw(state.display, "Iniciando cliente.\n");
	wrefresh(state.display);

	if (setup_sockets(&state) != 0)
	{
		return ERROR_OTHER;
	}

	wprintw(state.display, "Iniciando comunicacion con el servidor.\n");
	wprintw(state.display, "IP: %s, puerto: %u.\n", ip, port);
	wrefresh(state.display);

	state.ssocket.sin_family = AF_INET;
	state.ssocket.sin_port = htons(port);

	inet_pton(AF_INET, ip, &state.ssocket.sin_addr);

	status = send_server_login(&state, password);
	if (status != 0)
	{
		return ERROR_SV_SEND;
	}

	wprintw(state.display, "Mensaje de login enviado al servidor.\n");
	wrefresh(state.display);

	enum db_type_code code = read_server_login(&state, &status);
	if (status == -1)
	{
		close(state.socket_fd);
		endwin();
		return ERROR_SV_READ;
	}

	if (status != SV_LOGIN_SUCCESS)
	{
		close(state.socket_fd);
		endwin();

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

	state.type = code;

	status = start_client(&state);

	close(state.socket_fd);

	wprintw(state.display, "Presionar ENTER para salir.");
	wrefresh(state.display);
	wgetch(state.input);

	endwin();
	return status;
}

int setup_sockets(client_state_t *st)
{
	wprintw(st->display, "Iniciando socket cliente.\n");
	wrefresh(st->display);

	int status;

	struct sockaddr_in sock_addr;
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_port = htons(CLIENT_IN_PORT);
	inet_pton(AF_INET, CLIENT_IP_ANY, &sock_addr.sin_addr);

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

int send_server_login(client_state_t *st, char *password)
{
	char buf[SV_MSG_SIZE];
	char *content = buf;
	*content++ = SV_LOGIN_REQ;
	ssize_t status;

	strcpy(content, st->username);
	content += (strlen(st->username) + 1);
	strcpy(content, password);
	
	status = sendto(st->socket_fd, buf, SV_MSG_SIZE, 0, (struct sockaddr*)&st->ssocket, sizeof(struct sockaddr_in));

	if (status != SV_MSG_SIZE)
	{
		return -1;
	}

	return 0;
}


int send_server_create(client_state_t *st, char *name)
{
	char buf[SV_MSG_SIZE];
	char *content = buf;
	*content++ = SV_CREATE_REQ;
	strcpy(content, name);
	ssize_t status;

	status = sendto(st->socket_fd, buf, SV_MSG_SIZE, 0, (struct sockaddr*)&st->ssocket, sizeof(struct sockaddr_in));
	if (status != SV_MSG_SIZE)
	{
		return ERROR_SV_SEND;
	}

	return 0;
}

int send_server_exit(client_state_t *st)
{
	char buf[SV_MSG_SIZE];
	buf[0] = SV_EXIT_REQ;
	ssize_t status;

	status = sendto(st->socket_fd, buf, SV_MSG_SIZE, 0, (struct sockaddr*)&st->ssocket, sizeof(struct sockaddr_in));
	if (status != SV_MSG_SIZE)
	{
		return ERROR_SV_SEND;
	}

	return 0;
}

int send_server_join(client_state_t *st, char *name)
{
	char buf[SV_MSG_SIZE];
	buf[0] = SV_JOIN_REQ;
	ssize_t status;

	strcpy(&buf[1], name);

	status = sendto(st->socket_fd, buf, SV_MSG_SIZE, 0, (struct sockaddr*)&st->ssocket, sizeof(struct sockaddr_in));
	if (status != SV_MSG_SIZE)
	{
		return ERROR_SV_SEND;
	}

	return 0;
}

enum db_type_code read_server_login(client_state_t *st, int *status)
{
	char buf[SV_MSG_SIZE];
	ssize_t received;

	received = recvfrom(st->socket_fd, buf, SV_MSG_SIZE, 0, NULL, NULL);
	if (received != SV_MSG_SIZE || buf[0] != SV_LOGIN_RES)
	{
		printf("Client: error al leer tipo de mensaje, o tipo de mensaje no es SV_LOGIN_RES.\n");
		*status = -1;
		return -1;
	}

	*status = buf[1];
	return buf[2];
}

int read_create_join_res(client_state_t *st, port_t *cht_port)
{
	char buf[SV_MSG_SIZE];
	ssize_t received;
	int status = 0;

	received = recvfrom(st->socket_fd, buf, SV_MSG_SIZE, 0, NULL, NULL);
	if (received != SV_MSG_SIZE || buf[0] != SV_CREATE_JOIN_RES)
	{
		return -1;
	}

	status = buf[1];
	char *content = &buf[2];

	if (status == SV_CREATE_SUCCESS || status == SV_JOIN_SUCCESS)
	{
		memcpy(cht_port, content, sizeof(port_t));
		return 0;
	}
	
	return -1;
}

int start_client(client_state_t *st)
{
	int cmd, status = 0, quit = FALSE;
	char cht_name[CHT_MAX_NAME_LEN + 1];
	port_t cht_port;

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

				wprintw(st->display, "Leyendo respuesta del servidor...\n");
				wrefresh(st->display);				

				status = read_create_join_res(st, &cht_port);
				if (status == 0)
				{
					wprintw(st->display, "--> nombre: %s, puerto: %u.\n", cht_name, cht_port);
					wrefresh(st->display);			
					st->chat_name = strdup(cht_name);
					status = enter_chat_mode(st, cht_port);
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

				wprintw(st->display, "Leyendo respuesta del servidor...\n");
				wrefresh(st->display);

				status = read_create_join_res(st, &cht_port);
				if (status == 0)
				{
					wprintw(st->display, "Chatroom creado.  Uniendo... (port: %u)\n", cht_port);
					wrefresh(st->display);
					st->chat_name = strdup(cht_name);

					status = enter_chat_mode(st, cht_port);
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

int enter_chat_mode(client_state_t *st, port_t port)
{
	char buf[SV_MSG_SIZE];
	struct sockaddr_in cht_addr = st->ssocket;
	cht_addr.sin_port = htons(port);

	ssize_t status;

	wprintw(st->display, "Abriendo chatroom...\n");
	wrefresh(st->display);

	buf[0] = CHT_MSG_JOIN;
	strcpy(&buf[1], st->username);

	status = sendto(st->socket_fd, buf, SV_MSG_SIZE, 0, (struct sockaddr*)&cht_addr, sizeof(struct sockaddr_in));
	if (status != SV_MSG_SIZE)
	{
		return -1;
	}

	wprintw(st->display, "Leyendo respuesta del servidor...\n");
	wrefresh(st->display);

	status = recvfrom(st->socket_fd, buf, SV_MSG_SIZE, 0, NULL, NULL);
	if (status != SV_MSG_SIZE || buf[0] != CHT_MSG_JOIN)
	{
		return -1;
	}

	wprintw(st->display, "Respuesta recibida.\n");
	wrefresh(st->display);

	status = enter_chat_loop(st, &cht_addr);

	wclear(st->display);
	wrefresh(st->display);	

	return status;
}


int enter_chat_loop(client_state_t *st, struct sockaddr_in *cht_addr)
{
	int quit = FALSE;
	char text[CHT_MSG_SIZE + 1];
	char msg_buf[SV_MSG_SIZE];
	int has_exited = FALSE;
	ssize_t status;

	st->thread_ended = FALSE;
	st->in_chatroom = TRUE;

	wclear(st->display);
	wprintw(st->display, "Conectado al chatroom %s.\n", st->chat_name);
	wprintw(st->display, "Comandos validos:.\n");
	wprintw(st->display, "   /exit\n");
	wrefresh(st->display);

	/* Thread para recibir mensajes del socket */
	pthread_create(&(st->rec_thread), NULL, read_socket_loop, st);

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

		fill_zeros(text, CHT_MSG_SIZE + 1);
		read_input_ncurses(st, text, CHT_MSG_SIZE);

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
				msg_buf[0] = CHT_MSG_EXIT;
				status = sendto(st->socket_fd, msg_buf, SV_MSG_SIZE, 0, (struct sockaddr*)cht_addr, sizeof(struct sockaddr));
				if (status != SV_MSG_SIZE)
				{
					pthread_mutex_lock(&st->thread_m);
					pthread_cancel(st->rec_thread);
					pthread_mutex_unlock(&st->thread_m);
					return -1;
				}

				has_exited = TRUE;
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
			if (strlen(text) > 0)
			{
				msg_buf[0] = CHT_MSG_TEXT;
				strcpy(&msg_buf[1], text);
				status = sendto(st->socket_fd, msg_buf, SV_MSG_SIZE, 0, (struct sockaddr*)cht_addr, sizeof(struct sockaddr));
				if (status == -1)
				{
					pthread_mutex_lock(&st->thread_m);
					pthread_cancel(st->rec_thread);
					pthread_mutex_unlock(&st->thread_m);
					return -1;
				}
			}
		}
	}

	return 0;
}

void *read_socket_loop(void *arg)
{
	char msg_buf[SV_MSG_SIZE];
	int quit = FALSE;
	ssize_t status;
	char code, *content;
	client_state_t *st = (client_state_t*)arg;
	
	while (!quit)
	{
		status = recvfrom(st->socket_fd, msg_buf, SV_MSG_SIZE, 0, NULL, NULL);
		if (status != SV_MSG_SIZE)
		{
			pthread_mutex_lock(&st->thread_m);
			st->thread_ended = TRUE;
			pthread_mutex_unlock(&st->thread_m);
			return NULL;
		}
		
		code = msg_buf[0];
		content = &msg_buf[1];

		switch (code)
		{
			case CHT_MSG_TEXT:
				pthread_mutex_lock(&st->screen_m);
				wprintw(st->display, "%s\n", content);
				wrefresh(st->display);
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
				pthread_mutex_lock(&st->screen_m);
				wprintw(st->display, "Codigo invalido recibido (enter para continuar).\n");
				wrefresh(st->display);
				pthread_mutex_unlock(&st->screen_m);

				pthread_mutex_lock(&st->thread_m);
				st->thread_ended = TRUE;
				st->in_chatroom = FALSE;
				pthread_mutex_unlock(&st->thread_m);
				return NULL;
			break;
		}
	}	

	return NULL;
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

		//char msg_buf[CHT_MSG_SIZE];

		if (gbl_state->in_chatroom)
		{
			//pack_msg(msg_buf, gbl_state->pid, CHT_MSG_EXIT);
			//mq_send(gbl_state->mq_out, msg_buf, CHT_MSG_SIZE, 0);
		}

		send_server_exit(gbl_state);
		endwin();
		exit(0);
	}
}