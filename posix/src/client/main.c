#include <stdio.h>
#include <string.h>
#include "client-local.h"
#include "client-remote.h"
#include "dbaccess.h"


void print_error(int error);

int main(int argc, char *argv[])
{
	char password[DB_MAX_PASSLEN + 1];
	char username[DB_MAX_USERLEN + 1];
	int status;

	if (argc < 2)
	{
		printf("Uso: %s [ local | remote ]\n", argv[0]);
		return 1;
	}

	char *mode = argv[1];
	if (strcmp("local", mode) != 0 && strcmp("remote", mode) != 0)
	{
		printf("Uso: %s [ local | remote ]\n", argv[0]);
		return 1;		
	}

	printf("Ingrese el nombre de usuario:\n");
	status = read_input(username, DB_MIN_USERLEN, DB_MAX_USERLEN);

	if (status != 0)
	{
		printf("Usuario invalido.\n");
		return 1;
	}

	printf("Ingrese la clave:\n");
	status = read_input(password, DB_MIN_PASSLEN, DB_MAX_PASSLEN);
	if (status != 0)
	{
		printf("Clave invalida.\n");
		return 1;
	}

	printf("\n-- Login usuario: %s --\n", username);
	
	if (strcmp("local", mode) == 0)
	{
		status = init_client_local(username, password);

		if (status != 0)
		{
			print_error(status);
			return -1;
		}
	}
	else
	{
		if (argc != 4)
		{
			printf("Uso: %s remote [ IP ] [ puerto ]\n", argv[0]);
			return 1;
		}

		unsigned short port;

		if (!sscanf(argv[3], "%hu", &port))
		{
			return 1;
		}

		int status = init_client_remote(username, password, argv[2], port);
		if (status != 0)
		{
			print_error(status);
			return -1;
		}
	}

	return 0;
}

void print_error(int error)
{
	switch(error)
	{
		case ERROR_SERVER_CONNECTION:
			printf("Error de conexion con el servidor(%d)\n", error);
		break;

		case ERROR_FIFO_CREAT:
			printf("Error al crear fifo(%d)\n", error);
		break;

		case ERROR_SV_SEND:
			printf("Error al enviar los datos  al servidor(%d)\n", error);
		break;

		case ERROR_SV_READ:
			printf("Error al recibir los datos del servidor(%d)\n", error);
		break;

		case ERROR_FIFO_OPEN:
			printf("Error al abrir fifo(%d)\n", error);
		break;

		case ERROR_SV_CREDENTIALS:
			printf("Credenciales incorrectas. Verifique que su usuario y contrasena seran correctos(%d)\n", error);
		break;

		case ERROR_SV_USER_ACTIVE:
			printf("El usuario ya se encuentra activo(%d)\n", error);
		break;

		default:
			printf("Error al inicializar al cliente(%d)\n", error);
		break;
	}
}
