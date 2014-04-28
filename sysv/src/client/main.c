#include <stdio.h>
#include <string.h>
#include "client-local.h"
#include "dbaccess.h"

int main(int argc, char *argv[])
{
	char password[DB_MAX_PASSLEN + 1];
	char username[DB_MAX_USERLEN + 1];
	bzero(username, DB_MAX_USERLEN + 1);
	int status;

	printf("Ingrese el nombre de usuario:\n");
	status = read_input(username, DB_MIN_USERLEN, DB_MAX_USERLEN);

	if (status != 0)
	{
		printf("Usuario invalido.\n");
		endwin();
		return 1;
	}

	printf("Ingrese la clave:\n");
	status = read_input(password, DB_MIN_PASSLEN, DB_MAX_PASSLEN);
	if (status != 0)
	{
		printf("Clave invalida.\n");
		endwin();
		return 1;
	}

	printf("\n-- Login usuario: %s --\n", username);
	

	status = init_client_local(username, password);

	if (status != 0)
	{
		printf("Error init_client. Codigo de error: %d\n", status);
	}

	if (status == ERROR_SERVER_CONNECTION)
	{
		printf("Error de comunicacion con el servidor.\n");
		endwin();
		return -1;
	}
	

	return 0;
}