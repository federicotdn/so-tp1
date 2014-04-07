#include <stdio.h>
#include <string.h>
#include "client-local.h"
#include "dbaccess.h"

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
	status = read_input(username, DB_MAX_USERLEN);
	printf("%s\n", username);
	
	if (status == ERROR_MAX_LENGTH)
	{
		printf("Usuario invalido.\n");
		return ERROR_MAX_LENGTH;
	}


	printf("Ingrese la clave:\n");
	status = read_input(password, DB_MAX_PASSLEN);
	printf("%s\n", password);
	if (status == ERROR_MAX_LENGTH)
	{
		printf("Clave invalida.\n");
		return ERROR_MAX_LENGTH;
	}
	


	
	if (strcmp("local", mode) == 0)
	{
	}
	else
	{
		
	}

	return 0;
}

int read_input(char * buff, size_t max_length)
{
	int i = 0;
	char c;

	while (i < max_length && (c = gethchar()) != EOF) 
	{
		buff[i++] = c;
	}

	buff[i] = 0;


	if (i > max_length)
	{
		while (gethchar() != EOF);
		return ERROR_MAX_LENGTH;
	}

	return 0;
	
}