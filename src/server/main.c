#include <stdio.h>
#include "server-local.h"

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		printf("Uso: %s [ local | remote ]\n", argv[0]);
		return 1;
	}
	
	return 0;
}
